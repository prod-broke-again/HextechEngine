#include "engine/core/Config.hpp"

#include "engine/core/Log.hpp"

#include <fstream>
#include <string>

namespace engine {

bool Config::loadFromFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        log(LogLevel::Warn, "Config: could not open file");
        return false;
    }
    try {
        in >> m_json;
    } catch (const std::exception& e) {
        log(LogLevel::Error, std::string("Config JSON parse error: ") + e.what());
        return false;
    }
    return true;
}

} // namespace engine
