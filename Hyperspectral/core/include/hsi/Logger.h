#pragma once
#include <string>
#include <functional>
#include <mutex>

namespace hsi {

enum class LogLevel { Debug, Info, Warning, Error };

// Process-wide log sink. The UI installs a callback that forwards messages
// into a QPlainTextEdit / status bar; the CLI tool installs one that prints
// to stdout. Core stages never touch Qt or iostream directly.
class Logger {
public:
    using Callback = std::function<void(const std::string& stage, const std::string& message)>;
    using LeveledCallback = std::function<void(LogLevel level, const std::string& stage, const std::string& message)>;

    static void setCallback(Callback cb) {
        std::lock_guard<std::mutex> lock(mutex());
        callback() = std::move(cb);
    }

    // Optional: if set, receives level info too (e.g. to color-code
    // warnings/errors differently from routine progress messages in the UI
    // log panel). Falls back to `callback()` (Info-level) if unset, so
    // existing call sites and UI wiring keep working unchanged.
    static void setLeveledCallback(LeveledCallback cb) {
        std::lock_guard<std::mutex> lock(mutex());
        leveledCallback() = std::move(cb);
    }

    static void log(const std::string& stage, const std::string& message) {
        logAt(LogLevel::Info, stage, message);
    }
    static void logWarn(const std::string& stage, const std::string& message) {
        logAt(LogLevel::Warning, stage, "WARNING: " + message);
    }
    static void logError(const std::string& stage, const std::string& message) {
        logAt(LogLevel::Error, stage, "ERROR: " + message);
    }
    // Compiles to nothing in release builds (QT_NO_DEBUG_OUTPUT / NDEBUG,
    // set for the release CONFIG in the .pro file) -- for high-frequency
    // per-block/per-pixel diagnostics that would otherwise spam the log
    // panel and cost real time in a release build.
    static void logDebug(const std::string& stage, const std::string& message) {
#if !defined(QT_NO_DEBUG_OUTPUT) && !defined(NDEBUG)
        logAt(LogLevel::Debug, stage, message);
#else
        (void)stage; (void)message;
#endif
    }

    static void logAt(LogLevel level, const std::string& stage, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex());
        if (leveledCallback()) { leveledCallback()(level, stage, message); return; }
        if (callback()) callback()(stage, message);
    }

private:
    static Callback& callback() {
        static Callback cb;
        return cb;
    }
    static LeveledCallback& leveledCallback() {
        static LeveledCallback cb;
        return cb;
    }
    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }
};

} // namespace hsi
