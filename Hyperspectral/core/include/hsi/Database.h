#pragma once
#include "hsi/Types.h"
#include <string>

// Forward-declared so this header never pulls <libpq-fe.h> into the rest
// of the codebase -- same pattern RasterIO.h uses to keep <gdal_priv.h>
// out of every other translation unit.
struct pg_conn;
typedef struct pg_conn PGconn;

namespace hsi {

// ---------------------------------------------------------------------------
// Generic "load it from disk once, then serve it out of PostGIS forever
// after" cache. One table (raster_products), keyed by a content hash of
// the source file plus a caller-supplied product tag, holds every cached
// RasterCube -- the Hyperion surface-reflectance result, the Sentinel-2
// merge input, the EOS-04 SAR merge input, or anything added later. No
// schema change is needed when a new step or a new sensor starts using it.
//
// Policy implemented everywhere this is used:
//   1. The very first time a given local file is used for a given product,
//      it is read from disk (and, where relevant, processed) as normal.
//   2. The result is written to the database under (contentHash, tag).
//   3. Every subsequent run recognises the same file by its content hash
//      and loads the cached result straight from the database -- the
//      local file is not reopened at all.
// ---------------------------------------------------------------------------
class Database {
public:
    struct ConnectionConfig {
        // Empty host => connect over the local Unix domain socket, which is
        // what initdb's "trust" authentication actually covers by default.
        // Switch to "127.0.0.1"/TCP only once pg_hba.conf has a real auth
        // rule (and password) for that route.
        std::string host     = "";
        std::string port     = "5432";
        std::string dbname   = "hsi_db";
        std::string user     = "postgres";
        std::string password = "";
    };

    Database();
    explicit Database(ConnectionConfig cfg);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Opens the connection and makes sure raster_products exists. Safe to
    // call more than once. Returns false on failure -- see lastError().
    // Deliberately never throws: a database outage should degrade the app
    // to "local files only", not crash it.
    bool connect();
    void disconnect();
    bool isConnected() const;

    // Stable cache key for a local file: same bytes always produce the
    // same key, regardless of the path it happens to be loaded from.
    static std::string computeSourceKey(const std::string& filePath);

    // Cache lookup. Returns true and fills `outCube` on a hit; false (and
    // leaves outCube untouched) on a miss or if the database is unreachable.
    bool tryLoad(const std::string& sourceKey, const std::string& productTag, RasterCube& outCube);

    // Stores (or replaces) `cube` under (sourceKey, productTag).
    bool store(const std::string& sourceKey, const std::string& productTag,
               const RasterCube& cube, const std::string& originalPath = "");

    // Convenience wrapper for the simple case -- no extra processing
    // between "read the file" and "cache the result" (this is what the
    // Sentinel-2 / EOS-04 SAR merge inputs need): cache hit -> returned
    // immediately, localPath is NOT reopened; cache miss -> RasterIO::
    // loadCube(localPath) once, the result is cached, then returned.
    //
    // Steps with real processing in between (e.g. the Hyperion ortho ->
    // radiance -> reflectance chain) should call tryLoad()/store() around
    // their own processing instead -- see PreprocessingDialog::run().
    RasterCube loadOrIngest(const std::string& localPath, const std::string& productTag);

    bool wasLastLoadFromCache() const { return lastLoadFromCache_; }
    std::string lastError() const { return lastError_; }

private:
    bool ensureSchema();
    void setError(const std::string& msg);

    ConnectionConfig cfg_;
    PGconn* conn_ = nullptr;
    std::string lastError_;
    bool lastLoadFromCache_ = false;
};

} // namespace hsi
