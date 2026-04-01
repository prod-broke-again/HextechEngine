#include "engine/core/Log.hpp"

#include <chrono>
#include <iostream>
#include <mutex>

namespace engine {

namespace {

std::mutex g_logMutex;

const char* levelStr(LogLevel l) {
    switch (l) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "?";
}

} // namespace

void log(LogLevel level, std::string_view message) {
    std::lock_guard lock(g_logMutex);
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::cerr << '[' << t << "] [" << levelStr(level) << "] " << message << '\n';
}

} // namespace engine
