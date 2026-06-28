#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace engine {

class Config {
public:
    bool loadFromFile(const std::filesystem::path& path);

    [[nodiscard]] const nlohmann::json& json() const { return m_json; }
    [[nodiscard]] nlohmann::json& json() { return m_json; }

    [[nodiscard]] float getFloat(std::string_view key, float defaultValue) const;
    [[nodiscard]] int getInt(std::string_view key, int defaultValue) const;
    [[nodiscard]] bool getBool(std::string_view key, bool defaultValue) const;
    [[nodiscard]] std::string getString(std::string_view key,
                                          std::string_view defaultValue) const;

private:
    [[nodiscard]] const nlohmann::json* find(std::string_view key) const;

    nlohmann::json m_json = nlohmann::json::object();
};

} // namespace engine
