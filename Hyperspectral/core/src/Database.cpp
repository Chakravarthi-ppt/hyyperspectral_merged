#include "hsi/Database.h"
#include "hsi/RasterIO.h"
#include "hsi/Logger.h"

#include <libpq-fe.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace hsi {

namespace {

// Small, fast, non-cryptographic 64-bit hash (FNV-1a). This only needs to
// satisfy "same bytes in -> same key out" for cache lookups, not stand up
// against a malicious actor -- so no need to pull in OpenSSL just for this.
uint64_t fnv1a64(const unsigned char* data, size_t len, uint64_t h) {
    constexpr uint64_t prime = 1099511628211ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= prime;
    }
    return h;
}

std::string toHex(uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
    return std::string(buf);
}

std::string tempFilePath(const std::string& suffix) {
    static std::atomic<long long> counter{0};
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << std::filesystem::temp_directory_path().string()
        << "/hsi_db_" << stamp << "_" << counter.fetch_add(1) << suffix;
    return oss.str();
}

std::string geoTransformToArrayLiteral(const std::array<double, 6>& gt) {
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < gt.size(); ++i) {
        if (i) oss << ",";
        oss << gt[i];
    }
    oss << "}";
    return oss.str();
}

} // namespace

Database::Database() : Database(ConnectionConfig()) {}

Database::Database(ConnectionConfig cfg) : cfg_(std::move(cfg)) {}

Database::~Database() { disconnect(); }

bool Database::isConnected() const {
    return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
}

void Database::setError(const std::string& msg) {
    lastError_ = msg;
    Logger::log("Database", "ERROR: " + msg);
}

bool Database::connect() {
    if (isConnected()) return true;

    std::ostringstream conninfo;
    if (!cfg_.host.empty()) conninfo << "host=" << cfg_.host << " ";
    conninfo << "port=" << cfg_.port
              << " dbname=" << cfg_.dbname
              << " user=" << cfg_.user;
    if (!cfg_.password.empty()) conninfo << " password=" << cfg_.password;

    conn_ = PQconnectdb(conninfo.str().c_str());
    if (PQstatus(conn_) != CONNECTION_OK) {
        setError(std::string("connection failed: ") + PQerrorMessage(conn_));
        PQfinish(conn_);
        conn_ = nullptr;
        return false;
    }

    if (!ensureSchema()) {
        disconnect();
        return false;
    }

    Logger::log("Database", "Connected to '" + cfg_.dbname + "'"
                + (cfg_.host.empty() ? " (local socket)." : (" on " + cfg_.host + ":" + cfg_.port + ".")));
    return true;
}

void Database::disconnect() {
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

bool Database::ensureSchema() {
    const char* ddl =
        "CREATE EXTENSION IF NOT EXISTS postgis;"
        "CREATE EXTENSION IF NOT EXISTS postgis_raster;"
        "CREATE TABLE IF NOT EXISTS raster_products ("
        "    id             BIGSERIAL PRIMARY KEY,"
        "    source_key     TEXT NOT NULL,"
        "    product_tag    TEXT NOT NULL,"
        "    original_path  TEXT,"
        "    width          INTEGER NOT NULL,"
        "    height         INTEGER NOT NULL,"
        "    bands          INTEGER NOT NULL,"
        "    band_names     TEXT,"
        "    geo_transform  DOUBLE PRECISION[] NOT NULL,"
        "    projection_wkt TEXT,"
        "    nodata_value   DOUBLE PRECISION,"
        "    has_nodata     BOOLEAN NOT NULL DEFAULT FALSE,"
        "    gtiff_data     BYTEA NOT NULL,"
        "    rast           RASTER,"
        "    created_at     TIMESTAMPTZ NOT NULL DEFAULT now(),"
        "    UNIQUE (source_key, product_tag)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_raster_products_lookup "
        "    ON raster_products (source_key, product_tag);";

    PGresult* res = PQexec(conn_, ddl);
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (!ok) setError(std::string("schema setup failed: ") + PQerrorMessage(conn_));
    PQclear(res);
    return ok;
}

std::string Database::computeSourceKey(const std::string& filePath) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f) throw HsiError("Database: cannot open '" + filePath + "' to compute its cache key");

    uint64_t h = 1469598103934665603ULL; // FNV offset basis
    std::vector<unsigned char> buf(1 << 16);
    uint64_t totalBytes = 0;
    while (f) {
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        std::streamsize n = f.gcount();
        if (n <= 0) break;
        h = fnv1a64(buf.data(), static_cast<size_t>(n), h);
        totalBytes += static_cast<uint64_t>(n);
    }
    // Hash + raw byte count: belt-and-suspenders against accidental
    // collisions for a cache key (doesn't need to be cryptographically
    // strong, just practically unique across whatever files this app sees).
    return toHex(h) + "_" + toHex(totalBytes);
}

bool Database::tryLoad(const std::string& sourceKey, const std::string& productTag, RasterCube& outCube) {
    if (!isConnected() && !connect()) return false;

    const char* sql =
        "SELECT gtiff_data FROM raster_products WHERE source_key = $1 AND product_tag = $2";
    const char* params[2] = { sourceKey.c_str(), productTag.c_str() };
    // resultFormat=1 (binary): we want the raw GeoTIFF bytes back exactly
    // as stored, not the text-escaped form bytea would otherwise produce.
    PGresult* res = PQexecParams(conn_, sql, 2, nullptr, params, nullptr, nullptr, 1);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        PQclear(res);
        return false;
    }

    const char* blob = PQgetvalue(res, 0, 0);
    int blobLen = PQgetlength(res, 0, 0);

    std::string tmpPath = tempFilePath(".tif");
    {
        std::ofstream out(tmpPath, std::ios::binary);
        out.write(blob, blobLen);
    }
    PQclear(res);

    try {
        outCube = RasterIO::loadCube(tmpPath);
    } catch (const std::exception& e) {
        std::filesystem::remove(tmpPath);
        setError(std::string("cached blob for '") + productTag + "' was unreadable: " + e.what());
        return false;
    }
    std::filesystem::remove(tmpPath);

    Logger::log("Database", "Cache HIT: '" + productTag + "' (" + sourceKey.substr(0, 12)
                + "...) loaded from database -- local file not touched.");
    return true;
}

bool Database::store(const std::string& sourceKey, const std::string& productTag,
                      const RasterCube& cube, const std::string& originalPath) {
    if (!isConnected() && !connect()) return false;

    // Serialize via the exact same GeoTIFF writer RasterIO already uses for
    // disk output, so round-tripping through the DB is byte-for-byte the
    // same code path as round-tripping through a file.
    std::string tmpPath = tempFilePath(".tif");
    try {
        RasterIO::saveCube(cube, tmpPath, "GTiff");
    } catch (const std::exception& e) {
        setError(std::string("could not serialize cube for caching: ") + e.what());
        return false;
    }

    std::ifstream in(tmpPath, std::ios::binary | std::ios::ate);
    if (!in) {
        setError("could not reopen temp GeoTIFF for caching");
        std::filesystem::remove(tmpPath);
        return false;
    }
    std::streamsize size = in.tellg();
    in.seekg(0);
    std::vector<char> blob(static_cast<size_t>(size));
    in.read(blob.data(), size);
    in.close();
    std::filesystem::remove(tmpPath);

    std::string gt = geoTransformToArrayLiteral(cube.geoTransform);
    std::string bandNamesJoined;
    for (size_t i = 0; i < cube.bandNames.size(); ++i) {
        if (i) bandNamesJoined += "|";
        bandNamesJoined += cube.bandNames[i];
    }
    std::string widthStr  = std::to_string(cube.width);
    std::string heightStr = std::to_string(cube.height);
    std::string bandsStr  = std::to_string(cube.bands);
    std::string ndStr     = std::to_string(cube.noDataValue);
    std::string hasNdStr  = cube.hasNoData ? "true" : "false";

    const char* sql =
        "INSERT INTO raster_products "
        "  (source_key, product_tag, original_path, width, height, bands, "
        "   band_names, geo_transform, projection_wkt, nodata_value, has_nodata, gtiff_data) "
        "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12) "
        "ON CONFLICT (source_key, product_tag) DO UPDATE SET "
        "  original_path = EXCLUDED.original_path, width = EXCLUDED.width, "
        "  height = EXCLUDED.height, bands = EXCLUDED.bands, band_names = EXCLUDED.band_names, "
        "  geo_transform = EXCLUDED.geo_transform, projection_wkt = EXCLUDED.projection_wkt, "
        "  nodata_value = EXCLUDED.nodata_value, has_nodata = EXCLUDED.has_nodata, "
        "  gtiff_data = EXCLUDED.gtiff_data, rast = NULL, created_at = now() "
        "RETURNING id";

    const char* params[12] = {
        sourceKey.c_str(), productTag.c_str(), originalPath.c_str(),
        widthStr.c_str(), heightStr.c_str(), bandsStr.c_str(),
        bandNamesJoined.c_str(), gt.c_str(), cube.projectionWkt.c_str(),
        ndStr.c_str(), hasNdStr.c_str(), blob.data()
    };
    int lengths[12] = { 0,0,0,0,0,0,0,0,0,0,0, static_cast<int>(blob.size()) };
    int formats[12] = { 0,0,0,0,0,0,0,0,0,0,0, 1 }; // only the blob param is binary

    PGresult* res = PQexecParams(conn_, sql, 12, nullptr, params, lengths, formats, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        setError(std::string("insert failed: ") + PQerrorMessage(conn_));
        PQclear(res);
        return false;
    }
    std::string newId = PQgetvalue(res, 0, 0);
    PQclear(res);

    // Best-effort: also populate the real PostGIS `raster` column from the
    // same bytes, so this is queryable with ordinary PostGIS SQL / QGIS,
    // not just retrievable through this class. Not fatal if it fails.
    std::string updateSql = "UPDATE raster_products SET rast = ST_FromGDALRaster(gtiff_data) WHERE id = " + newId;
    PGresult* res2 = PQexec(conn_, updateSql.c_str());
    if (PQresultStatus(res2) != PGRES_COMMAND_OK) {
        Logger::log("Database", "Note: PostGIS raster column not populated ("
                     + std::string(PQerrorMessage(conn_)) + "); blob cache was still saved fine.");
    }
    PQclear(res2);

    Logger::log("Database", "Cached '" + productTag + "' (" + widthStr + "x" + heightStr + "x" + bandsStr
                + ") -> raster_products.id=" + newId);
    return true;
}

RasterCube Database::loadOrIngest(const std::string& localPath, const std::string& productTag) {
    lastLoadFromCache_ = false;
    std::string key = computeSourceKey(localPath);

    RasterCube cached;
    if (tryLoad(key, productTag, cached)) {
        lastLoadFromCache_ = true;
        return cached;
    }

    Logger::log("Database", "Cache MISS: '" + productTag + "' -- loading from local file '" + localPath + "'.");
    RasterCube raw = RasterIO::loadCube(localPath);
    if (!store(key, productTag, raw, localPath)) {
        Logger::log("Database", "Warning: loaded '" + productTag + "' but could NOT cache it ("
                     + lastError_ + "). It will be re-read from the local file next time.");
    }
    return raw;
}

} // namespace hsi
