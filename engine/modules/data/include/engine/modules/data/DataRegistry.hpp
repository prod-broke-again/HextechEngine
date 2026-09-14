#pragma once

#include "DataError.hpp"
#include "engine/foundation/Result.hpp"
#include "engine/foundation/StringHash.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace engine {

class DataRegistry {
public:
    using LoadHandler = std::function<std::vector<DataError>(const nlohmann::json& doc, const std::string& filename, std::string_view rawContent)>;

    DataRegistry() = default;
    ~DataRegistry() = default;

    // Direct JSON loading with syntax error localization
    static Result<nlohmann::json, DataError> loadJson(const std::filesystem::path& path);
    static Result<nlohmann::json, DataError> parseJsonString(std::string_view content, std::string_view filename = "memory");

    // Helper: calculate 1-based line number for a character offset in text
    static int offsetToLine(std::string_view content, size_t offset);

    // Helper: find line number of first occurrence of a token starting from searchFrom
    static int findLineNumber(std::string_view content, std::string_view token, size_t searchFrom = 0);

    // Track a file for loading, schema validation, and hot reloading
    void registerFile(std::string_view id, const std::filesystem::path& path, LoadHandler handler);

    // Load / reload all registered files
    Result<void, std::vector<DataError>> loadAll();

    // Reload a single file by id
    Result<void, std::vector<DataError>> reloadFile(std::string_view id);

    // Check if any tracked files have been modified on disk
    bool checkForModifications();

    // Reload all files if modified
    bool reloadIfModified();

    // Raw JSON access
    const nlohmann::json* getJson(std::string_view id) const;

    // Listeners for hot reload events
    using ReloadListener = std::function<void()>;
    void addReloadListener(ReloadListener listener);

private:
    struct TrackedFile {
        std::string id;
        std::filesystem::path path;
        std::filesystem::file_time_type lastWriteTime{};
        LoadHandler handler;
        nlohmann::json document;
        bool isLoaded = false;
    };

    std::vector<TrackedFile> m_files;
    std::vector<ReloadListener> m_listeners;
};

} // namespace engine
