#include "engine/core/Log.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>

namespace engine {

namespace {

std::mutex g_logMutex;
LogLevel g_minLevel = LogLevel::Trace;
std::ofstream g_logFile;

const char* levelStr(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "?";
}

} // namespace

void setMinLogLevel(LogLevel level) { g_minLevel = level; }

void setLogFile(const std::filesystem::path& path) {
    std::lock_guard lock(g_logMutex);
    g_logFile.close();
    g_logFile.open(path, std::ios::out | std::ios::app);
}

void log(LogLevel level, std::string_view message) {
    if (level < g_minLevel) {
        return;
    }

    std::lock_guard lock(g_logMutex);
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::ostream& out = g_logFile.is_open() ? static_cast<std::ostream&>(g_logFile) : std::cerr;
    out << '[' << t << "] [" << levelStr(level) << "] " << message << '\n';
    if (g_logFile.is_open()) {
        g_logFile.flush();
    }
}

} // namespace engine
