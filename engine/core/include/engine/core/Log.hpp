#pragma once

#include <filesystem>
#include <string_view>

namespace engine {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

void setMinLogLevel(LogLevel level);
void setLogFile(const std::filesystem::path& path);
void log(LogLevel level, std::string_view message);

} // namespace engine
