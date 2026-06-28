#include "engine/core/Config.hpp"

#include "engine/core/Log.hpp"

#include <fstream>

namespace engine {

namespace {

const nlohmann::json* jsonAtPath(const nlohmann::json& root, std::string_view key) {
    if (!root.is_object()) {
        return nullptr;
    }
    const auto it = root.find(std::string(key));
    if (it == root.end()) {
        return nullptr;
    }
    return &(*it);
}

} // namespace

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

const nlohmann::json* Config::find(std::string_view key) const {
    return jsonAtPath(m_json, key);
}

float Config::getFloat(std::string_view key, float defaultValue) const {
    const nlohmann::json* value = find(key);
    return value && value->is_number() ? value->get<float>() : defaultValue;
}

int Config::getInt(std::string_view key, int defaultValue) const {
    const nlohmann::json* value = find(key);
    return value && value->is_number_integer() ? value->get<int>() : defaultValue;
}

bool Config::getBool(std::string_view key, bool defaultValue) const {
    const nlohmann::json* value = find(key);
    return value && value->is_boolean() ? value->get<bool>() : defaultValue;
}

std::string Config::getString(std::string_view key, std::string_view defaultValue) const {
    const nlohmann::json* value = find(key);
    return value && value->is_string() ? value->get<std::string>() : std::string(defaultValue);
}

} // namespace engine
