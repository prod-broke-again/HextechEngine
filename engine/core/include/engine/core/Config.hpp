#pragma once

#include <nlohmann/json.hpp>
#include <filesystem>
#include <optional>

namespace engine {

class Config {
public:
    bool loadFromFile(const std::filesystem::path& path);
    [[nodiscard]] const nlohmann::json& json() const { return m_json; }
    [[nodiscard]] nlohmann::json& json() { return m_json; }

private:
    nlohmann::json m_json = nlohmann::json::object();
};

} // namespace engine
