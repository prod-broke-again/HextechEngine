#include "engine/modules/data/DataRegistry.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace engine {

int DataRegistry::offsetToLine(std::string_view content, size_t offset) {
    int line = 1;
    const size_t limit = std::min(offset, content.size());
    for (size_t i = 0; i < limit; ++i) {
        if (content[i] == '\n') {
            ++line;
        }
    }
    return line;
}

int DataRegistry::findLineNumber(std::string_view content, std::string_view token, size_t searchFrom) {
    if (token.empty() || searchFrom >= content.size()) return -1;
    const size_t pos = content.find(token, searchFrom);
    if (pos == std::string_view::npos) return -1;
    return offsetToLine(content, pos);
}

Result<nlohmann::json, DataError> DataRegistry::parseJsonString(std::string_view content, std::string_view filename) {
    try {
        nlohmann::json doc = nlohmann::json::parse(content);
        return doc;
    } catch (const nlohmann::json::parse_error& e) {
        int line = offsetToLine(content, e.byte);
        return Result<nlohmann::json, DataError>::error(DataError{
            .filename = std::string(filename),
            .line = line,
            .key = "",
            .reason = e.what()
        });
    } catch (const std::exception& e) {
        return Result<nlohmann::json, DataError>::error(DataError{
            .filename = std::string(filename),
            .line = -1,
            .key = "",
            .reason = e.what()
        });
    }
}

Result<nlohmann::json, DataError> DataRegistry::loadJson(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream.is_open()) {
        return Result<nlohmann::json, DataError>::error(DataError{
            .filename = path.string(),
            .line = -1,
            .key = "",
            .reason = "Could not open file: " + path.string()
        });
    }

    std::ostringstream ss;
    ss << stream.rdbuf();
    const std::string content = ss.str();

    return parseJsonString(content, path.string());
}

void DataRegistry::registerFile(std::string_view id, const std::filesystem::path& path, LoadHandler handler) {
    for (auto& file : m_files) {
        if (file.id == id) {
            file.path = path;
            file.handler = std::move(handler);
            std::error_code ec;
            if (std::filesystem::exists(path, ec)) {
                file.lastWriteTime = std::filesystem::last_write_time(path, ec);
            }
            return;
        }
    }

    TrackedFile tf;
    tf.id = std::string(id);
    tf.path = path;
    tf.handler = std::move(handler);
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        tf.lastWriteTime = std::filesystem::last_write_time(path, ec);
    }
    m_files.push_back(std::move(tf));
}

Result<void, std::vector<DataError>> DataRegistry::loadAll() {
    std::vector<DataError> allErrors;

    for (auto& file : m_files) {
        std::ifstream stream(file.path, std::ios::in | std::ios::binary);
        if (!stream.is_open()) {
            allErrors.push_back(DataError{
                .filename = file.path.string(),
                .line = -1,
                .key = "",
                .reason = "Failed to open file"
            });
            continue;
        }

        std::ostringstream ss;
        ss << stream.rdbuf();
        const std::string content = ss.str();

        auto parseRes = parseJsonString(content, file.path.string());
        if (!parseRes) {
            allErrors.push_back(parseRes.error());
            continue;
        }

        if (file.handler) {
            auto handlerErrors = file.handler(parseRes.value(), file.path.string(), content);
            if (!handlerErrors.empty()) {
                allErrors.insert(allErrors.end(), handlerErrors.begin(), handlerErrors.end());
                continue;
            }
        }

        file.document = std::move(parseRes.value());
        file.isLoaded = true;
        std::error_code ec;
        if (std::filesystem::exists(file.path, ec)) {
            file.lastWriteTime = std::filesystem::last_write_time(file.path, ec);
        }
    }

    if (!allErrors.empty()) {
        return Result<void, std::vector<DataError>>::error(std::move(allErrors));
    }

    for (const auto& listener : m_listeners) {
        if (listener) listener();
    }

    return Result<void, std::vector<DataError>>::ok();
}

Result<void, std::vector<DataError>> DataRegistry::reloadFile(std::string_view id) {
    for (auto& file : m_files) {
        if (file.id != id) continue;

        std::ifstream stream(file.path, std::ios::in | std::ios::binary);
        if (!stream.is_open()) {
            std::vector<DataError> errs;
            errs.push_back(DataError{
                .filename = file.path.string(),
                .line = -1,
                .key = "",
                .reason = "Failed to open file"
            });
            return Result<void, std::vector<DataError>>::error(std::move(errs));
        }

        std::ostringstream ss;
        ss << stream.rdbuf();
        const std::string content = ss.str();

        auto parseRes = parseJsonString(content, file.path.string());
        if (!parseRes) {
            std::vector<DataError> errs;
            errs.push_back(parseRes.error());
            return Result<void, std::vector<DataError>>::error(std::move(errs));
        }

        if (file.handler) {
            auto handlerErrors = file.handler(parseRes.value(), file.path.string(), content);
            if (!handlerErrors.empty()) {
                return Result<void, std::vector<DataError>>::error(std::move(handlerErrors));
            }
        }

        file.document = std::move(parseRes.value());
        file.isLoaded = true;
        std::error_code ec;
        if (std::filesystem::exists(file.path, ec)) {
            file.lastWriteTime = std::filesystem::last_write_time(file.path, ec);
        }

        for (const auto& listener : m_listeners) {
            if (listener) listener();
        }

        return Result<void, std::vector<DataError>>::ok();
    }

    std::vector<DataError> notFound;
    notFound.push_back(DataError{
        .filename = std::string(id),
        .line = -1,
        .key = "",
        .reason = "File id not registered"
    });
    return Result<void, std::vector<DataError>>::error(std::move(notFound));
}

bool DataRegistry::checkForModifications() {
    for (const auto& file : m_files) {
        std::error_code ec;
        if (std::filesystem::exists(file.path, ec)) {
            auto time = std::filesystem::last_write_time(file.path, ec);
            if (!ec && time > file.lastWriteTime) {
                return true;
            }
        }
    }
    return false;
}

bool DataRegistry::reloadIfModified() {
    if (checkForModifications()) {
        auto res = loadAll();
        return res.isOk();
    }
    return false;
}

const nlohmann::json* DataRegistry::getJson(std::string_view id) const {
    for (const auto& file : m_files) {
        if (file.id == id && file.isLoaded) {
            return &file.document;
        }
    }
    return nullptr;
}

void DataRegistry::addReloadListener(ReloadListener listener) {
    m_listeners.push_back(std::move(listener));
}

} // namespace engine
