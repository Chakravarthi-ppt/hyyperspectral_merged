#include "Utils.h"
#include "hsi/Types.h"
#include "hsi/RasterIO.h"
#include "hsi/BandSelector.h"
#include "hsi/Logger.h"

#include <cpl_vsi.h>
#include <cpl_conv.h>
#include <cpl_string.h>
#include <functional>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <thread>
#include <mutex>
#include <vector>

#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QProcess>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>

namespace ui_util {

namespace {
bool endsWithIgnoreCase(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin(), [](char a, char b) {
        return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    });
}

// Cache directories under Hyperspectral_Data/ used to be keyed on the zip's
// base filename alone. That silently reused a stale merged_band_stack.tif
// (or a stale single-scene extraction) whenever a *different* delivery
// happened to share the same filename (e.g. every EOS-04/Sentinel-2 download
// named "data.zip") -- the app looked like it was "stuck" always loading the
// same old scene no matter which new file was picked. Folding in the file
// size and last-modified time makes the cache key specific to *this*
// archive's actual contents, not just its name, while still letting a
// genuinely-unchanged file reuse its cache on repeated loads.
QString sceneCacheKey(const std::string& path) {
    QFileInfo fi(QString::fromStdString(path));
    QString base = fi.completeBaseName();
    qint64 size = fi.size();
    qint64 mtime = fi.lastModified().toMSecsSinceEpoch();
    return QString("%1_%2_%3").arg(base).arg(size).arg(mtime);
}

// /tmp is frequently a small, RAM-backed tmpfs -- left unattended, every
// Step 1 run (a full Hyperion archive extraction plus a multi-GB
// merged_band_stack.tif) accumulates there forever and eventually
// exhausts it. That surfaces as a confusing "RasterIO: write error" deep
// inside whatever GDAL operation happens to run out of room next, not as
// an obvious "disk full" message -- so sweep away any previous run's
// leftover hsi_extract_* folder before starting a new extraction. Safe to
// do unconditionally: by the time a *new* archive is being extracted, an
// older run's extracted files are never referenced again.
void cleanupOldExtractions(const QString& base) {
    QDir baseDir(base);
    QStringList old = baseDir.entryList({ "hsi_extract_*" }, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : old) {
        QDir(base + "/" + name).removeRecursively();
    }
    if (!old.isEmpty()) {
        hsi::Logger::log("Archive", "Cleaned up " + std::to_string(old.size()) +
                          " leftover extraction folder(s) from previous run(s) to free disk space.");
    }
}

// (Previously: deleteEverythingExcept() cleaned up extracted per-band source
// files after merging. No longer needed -- per-band merging now reads
// directly from the zip via /vsizip/ and never extracts those files to
// disk in the first place, so there's nothing left over to clean up.)


// USGS EarthExplorer ships EO-1 Hyperion L1T scenes as a .zip containing
// one single-band GeoTIFF *per band* (named like "..._B125_L1T.TIF")
// rather than one combined multi-band file. Detects that pattern among
// the files listed *inside the zip* (no disk extraction performed) and
// returns (sensor band number, /vsizip/ path) for every match that
// BandSelector would actually keep -- so the caller stacks only those
// into one cube, instead of either treating any single file as "the"
// scene, or spending time reading the ~44 files (the 1-7 head, the
// 58-76 VNIR/SWIR overlap zone, and the uncalibrated 225-242 SWIR tail)
// that selectCalibratedBands would throw away again immediately
// afterward anyway.
std::vector<std::pair<int, std::string>> findPerBandTifs(const std::vector<std::string>& vsizipFiles) {
    static const QRegularExpression bandPattern("_[Bb](\\d{1,4})_");
    static const hsi::BandSelector::Rule keepRule; // same default BandSelector itself applies
    std::vector<std::pair<int, std::string>> result;
    for (const auto& f : vsizipFiles) {
        QString qf = QString::fromStdString(f);
        QString suffix = QFileInfo(qf).suffix().toLower();
        if (suffix != "tif" && suffix != "tiff") continue;
        QRegularExpressionMatch m = bandPattern.match(QFileInfo(qf).fileName());
        if (!m.hasMatch()) continue;

        int sensorBand = m.captured(1).toInt();
        bool inVnir = sensorBand >= keepRule.vnirStartBand && sensorBand <= keepRule.vnirEndBand;
        bool inSwir = sensorBand >= keepRule.swirStartBand && sensorBand <= keepRule.swirEndBand;
        if (inVnir || inSwir) result.emplace_back(sensorBand, f);
    }
    return result;
}

// Loads each (sensor band number, /vsizip/ path) pair -- each expected to
// be a single-band raster -- and stacks them into one RasterCube, sorted
// by true sensor band number. Each output band is tagged with
// hsi::kSensorBandTag so the true band number survives the save-to-disk +
// reload that Pipeline::preprocess does immediately afterward, instead of
// collapsing back to a plain 1..N file position.
//
// Performance: this is the step that used to take 10-20 minutes. Two
// fixes applied:
//   1. No `unzip` subprocess and no disk extraction at all -- GDAL reads
//      each single-band TIFF directly out of the zip via its built-in
//      /vsizip/ virtual filesystem (zlib inflate happens in-memory, once,
//      per band actually needed -- not once for the whole archive up
//      front via an external process).
//   2. The ~198 kept band files are loaded in parallel across all CPU
//      cores instead of one at a time. Each thread loads into its own
//      pre-allocated slice of the final cube, so there's no merge/copy
//      step afterward and no lock contention on the pixel data itself.
std::string mergePerBandFiles(std::vector<std::pair<int, std::string>> bands,
                               const QString& workDirPath,
                               const std::function<void(int,int)>& onProgress) {
    std::sort(bands.begin(), bands.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const int n = static_cast<int>(bands.size());

    hsi::Logger::log("Archive", "Merging " + std::to_string(n) +
        " per-band files into one cube directly from the zip (no extraction step) -- "
        "bands outside VNIR 8-57 / SWIR 77-224 were skipped entirely.");

    // Probe the first band to get grid dimensions, then allocate the full
    // cube up front so every worker thread writes into its own slice with
    // no further allocation or copying.
    hsi::RasterCube first = hsi::RasterIO::loadCube(bands[0].second);
    if (first.bands != 1) {
        throw hsi::HsiError("Expected '" + bands[0].second + "' to be a single-band per-band-archive "
                            "file, but it has " + std::to_string(first.bands) + " bands.");
    }

    hsi::RasterCube merged;
    merged.allocate(first.width, first.height, n);
    merged.geoTransform  = first.geoTransform;
    merged.projectionWkt = first.projectionWkt;
    merged.hasNoData     = first.hasNoData;
    merged.noDataValue   = first.noDataValue;

    std::copy(first.data.begin(), first.data.end(), merged.data.begin());
    merged.bandNumbers[0] = bands[0].first;
    merged.bandNames[0]   = std::string(hsi::kSensorBandTag) + std::to_string(bands[0].first);

    std::atomic<int> nextIndex{1};   // bands[0] already done above
    std::atomic<int> completed{1};
    std::atomic<bool> failed{false};
    std::string firstError;
    std::mutex errorMutex;

    auto worker = [&]() {
        while (!failed.load()) {
            int i = nextIndex.fetch_add(1);
            if (i >= n) break;
            try {
                hsi::RasterCube single = hsi::RasterIO::loadCube(bands[i].second);
                if (single.bands != 1) {
                    throw hsi::HsiError("Expected '" + bands[i].second + "' to be a single-band "
                                        "per-band-archive file, but it has " +
                                        std::to_string(single.bands) + " bands.");
                }
                if (!merged.sameGridAs(single)) {
                    throw hsi::HsiError("Band file '" + bands[i].second + "' (" +
                                        std::to_string(single.width) + "x" + std::to_string(single.height) +
                                        ") does not match the grid of band " + std::to_string(bands[0].first) +
                                        " (" + std::to_string(merged.width) + "x" +
                                        std::to_string(merged.height) + ").");
                }
                std::copy(single.data.begin(), single.data.end(),
                          merged.data.begin() + static_cast<size_t>(i) * merged.pixelCount());
                merged.bandNumbers[i] = bands[i].first;
                merged.bandNames[i]   = std::string(hsi::kSensorBandTag) + std::to_string(bands[i].first);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lk(errorMutex);
                if (firstError.empty()) firstError = e.what();
                failed.store(true);
                break;
            }
            int done = completed.fetch_add(1) + 1;
            if (onProgress) onProgress(done, n);
        }
    };

    unsigned hwThreads = std::thread::hardware_concurrency();
    int nThreads = std::max(2, std::min(static_cast<int>(hwThreads > 0 ? hwThreads : 4), n));

    std::vector<std::thread> pool;
    pool.reserve(nThreads);
    for (int t = 0; t < nThreads; ++t) pool.emplace_back(worker);
    for (auto& th : pool) th.join();

    if (failed.load()) {
        throw hsi::HsiError("Failed while merging per-band files: " + firstError);
    }

    QDir().mkpath(workDirPath);
    std::string mergedPath = (workDirPath + "/merged_band_stack.tif").toStdString();
    hsi::RasterIO::saveCube(merged, mergedPath, "GTiff");

    return mergedPath;
}
// Lists every regular file inside `zipPath` using GDAL's VSI layer
// (libtiff/zlib's own zip reader) -- no external `unzip` process, no
// bytes written to disk. Returns paths already prefixed with /vsizip/,
// ready to hand straight to GDALOpen.
std::vector<std::string> listZipContentsVsi(const std::string& zipPath) {
    std::string vsiRoot = "/vsizip/" + zipPath;
    std::vector<std::string> found;

    std::function<void(const std::string&)> recurse = [&](const std::string& dir) {
        char** entries = VSIReadDir(dir.c_str());
        if (!entries) return;
        for (int i = 0; entries[i] != nullptr; ++i) {
            std::string name = entries[i];
            if (name == "." || name == "..") continue;
            std::string full = dir + "/" + name;

            VSIStatBufL st;
            if (VSIStatL(full.c_str(), &st) == 0 && VSI_ISDIR(st.st_mode)) {
                recurse(full);
            } else {
                found.push_back(full);
            }
        }
        CSLDestroy(entries);
    };
    recurse(vsiRoot);
    return found;
}
} // namespace

std::map<std::string, std::vector<std::pair<int, int>>> loadSampleCsv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw hsi::HsiError("Cannot open sample CSV '" + path + "'.");

    std::map<std::string, std::vector<std::pair<int, int>>> samples;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string className, rowTok, colTok;
        std::getline(ss, className, ',');
        std::getline(ss, rowTok, ',');
        std::getline(ss, colTok, ',');
        if (className.empty() || rowTok.empty() || colTok.empty()) continue;
        try {
            int row = std::stoi(rowTok);
            int col = std::stoi(colTok);
            samples[className].push_back({ row, col });
        } catch (...) {
            continue; // skip header row or malformed line
        }
    }
    if (samples.empty()) throw hsi::HsiError("Sample CSV '" + path + "' produced no valid (class,row,col) rows.");
    return samples;
}

std::string resolveRasterPath(const std::string& path) {
    if (!endsWithIgnoreCase(path, ".zip")) return path;

    hsi::RasterIO::init(); // ensures GDAL (and its VSI layer) is registered

    static std::atomic<int> counter{0};
    QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (base.isEmpty()) base = QDir::tempPath();

    cleanupOldExtractions(base);

    // Persistent output goes to ~/Documents/Hyperspectral_Data/ so the merged
    // stack is reusable across sessions without re-extracting the zip each time.
    QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docsPath.isEmpty()) docsPath = QDir::homePath() + "/Documents";
    QString persistDir = docsPath + "/Hyperspectral_Data";
    QDir().mkpath(persistDir);

    // Temporary working dir (for extraction scratch space — single-file zips only)
    QString workDirPath = base + "/hsi_extract_" + QString::number(QDateTime::currentMSecsSinceEpoch())
                              + "_" + QString::number(counter.fetch_add(1));

    // List the archive's contents in-memory via GDAL's VSI zip reader --
    // this replaces the old `unzip` subprocess call entirely. Nothing is
    // written to disk by this step; it's just reading the zip's central
    // directory, which is fast even for large archives.
    std::vector<std::string> vsiFiles = listZipContentsVsi(path);
    if (vsiFiles.empty()) {
        throw hsi::HsiError("'" + path + "' could not be read as a zip archive, or is empty.");
    }

    // If this archive is a "one TIFF per band" delivery (USGS EO-1 Hyperion
    // L1T style), read every kept band straight out of the zip in parallel
    // via /vsizip/ -- no extraction to disk at all.
    auto perBandTifs = findPerBandTifs(vsiFiles);
    if (perBandTifs.size() > 1) {
        // Derive a scene-specific name from the zip's filename + size + mtime
        // (not filename alone -- see sceneCacheKey()) so multiple different
        // scenes, even ones sharing a filename, coexist in Hyperspectral_Data/
        // without overwriting each other or being confused for one another.
        QString zipName = sceneCacheKey(path);
        QString sceneDir = persistDir + "/" + zipName;
        QDir().mkpath(sceneDir);
        QString cachedMerged = sceneDir + "/merged_band_stack.tif";

        // Re-use the previously merged file if it exists — saves re-reading
        // all ~198 per-band TIFFs from inside the zip on repeated loads.
        if (QFile::exists(cachedMerged)) {
            hsi::Logger::log("Archive",
                "Reusing previously merged stack from: " + cachedMerged.toStdString());
            return cachedMerged.toStdString();
        }

        std::string merged = mergePerBandFiles(std::move(perBandTifs), sceneDir, nullptr);
        hsi::Logger::log("Archive",
            "Merged stack saved to Hyperspectral_Data: " + sceneDir.toStdString());
        return merged;
    }

    // Otherwise this is a normal single-scene archive (one combined
    // multi-band file, possibly with sidecar files like .hdr/.aux/.xml
    // that some GDAL drivers expect to find as real files next to the
    // main one). Extract it for real in that case, since /vsizip/ access
    // to a file whose driver needs to *write* sidecars, or that some
    // non-GDAL tool downstream needs as a real path, isn't safe to skip.
    QString unzipBin = QStandardPaths::findExecutable("unzip");
    if (unzipBin.isEmpty()) {
        throw hsi::HsiError("Cannot auto-extract '" + path + "': the 'unzip' command was not found on PATH. "
                            "Install it (e.g. 'sudo dnf install unzip') and try again.");
    }

    // For single-file zips, extract to Hyperspectral_Data/<cachekey>/ so
    // the result is also persistent and reusable -- keyed on content
    // (name+size+mtime), not just filename, so a different archive with the
    // same name is never mistaken for a cached one (see sceneCacheKey()).
    QString zipName2 = sceneCacheKey(path);
    QString singleSceneDir = persistDir + "/" + zipName2;
    QDir().mkpath(singleSceneDir);

    // Check if we already extracted this zip before
    static const std::vector<std::string> priorityExtCheck = { "tif", "tiff", "img", "bil", "dat", "bin", "hdr" };
    {
        std::vector<std::string> existingFiles;
        QDirIterator chk(singleSceneDir, QDir::Files, QDirIterator::Subdirectories);
        while (chk.hasNext()) existingFiles.push_back(chk.next().toStdString());
        for (const auto& ext : priorityExtCheck) {
            for (const auto& f : existingFiles) {
                if (QFileInfo(QString::fromStdString(f)).suffix().toLower().toStdString() == ext) {
                    hsi::Logger::log("Archive",
                        "Reusing previously extracted file from Hyperspectral_Data: " + f);
                    return f;
                }
            }
        }
    }

    if (!QDir().mkpath(workDirPath)) {
        throw hsi::HsiError("Could not create extraction directory '" + workDirPath.toStdString() + "'.");
    }

    QProcess proc;
    proc.start(unzipBin, { "-o", "-q", QString::fromStdString(path), "-d", workDirPath });
    if (!proc.waitForFinished(30 * 60 * 1000)) { // 30 min ceiling -- 1.5GB+ defence deliveries can legitimately take longer than 2 min
        proc.kill();
        proc.waitForFinished(5000);
        throw hsi::HsiError("Extracting '" + path + "' timed out after 30 minutes.");
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        throw hsi::HsiError("Failed to extract '" + path + "': " +
                            QString::fromUtf8(proc.readAllStandardError()).toStdString());
    }

    // Search the extracted tree for a file GDAL is likely to open directly,
    // preferring the most common interchange formats first. ENVI's .hdr is
    // listed last since it's the sidecar header, not the pixel data itself
    // -- only used as a last resort if nothing else matched.
    static const std::vector<std::string> priorityExt = { "tif", "tiff", "img", "bil", "dat", "bin", "hdr" };
    std::vector<std::string> found;
    QDirIterator it(workDirPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) found.push_back(it.next().toStdString());

    for (const auto& ext : priorityExt) {
        for (const auto& f : found) {
            if (QFileInfo(QString::fromStdString(f)).suffix().toLower().toStdString() == ext) {
                // Copy the found file to ~/Documents/Hyperspectral_Data/<scene>/ for reuse
                QString destPath = singleSceneDir + "/" + QFileInfo(QString::fromStdString(f)).fileName();
                if (!QFile::exists(destPath))
                    QFile::copy(QString::fromStdString(f), destPath);
                hsi::Logger::log("Archive",
                    "Saved to Hyperspectral_Data: " + destPath.toStdString());
                return f; // return original path (already valid on disk)
            }
        }
    }

    std::ostringstream listing;
    for (size_t i = 0; i < found.size() && i < 20; ++i) listing << "\n  " << found[i];
    throw hsi::HsiError("Extracted '" + path + "' to '" + workDirPath.toStdString() +
                        "' but found no recognizable raster file inside it "
                        "(.tif/.tiff/.img/.bil/.dat/.bin/.hdr). Contents found:" + listing.str());
}

} // namespace ui_util
