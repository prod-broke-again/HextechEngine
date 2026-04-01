#include "engine/renderer/vulkan/ShaderHotReload.hpp"

#include <filesystem>
#include <mutex>
#include <string>
#include <string>
#include <unordered_map>

namespace engine {

class ShaderHotReloadWatcher {
public:
    void watch(const std::filesystem::path& path) {
        std::lock_guard lock(m_mutex);
        try {
            if (std::filesystem::exists(path)) {
                m_times[path.string()] = std::filesystem::last_write_time(path);
            }
        } catch (...) {
        }
    }

    bool pollChanged(const std::filesystem::path& path) {
        std::lock_guard lock(m_mutex);
        try {
            if (!std::filesystem::exists(path)) {
                return false;
            }
            const auto t = std::filesystem::last_write_time(path);
            const auto key = path.string();
            auto it = m_times.find(key);
            if (it == m_times.end()) {
                m_times[key] = t;
                return false;
            }
            if (t != it->second) {
                it->second = t;
                return true;
            }
        } catch (...) {
        }
        return false;
    }

private:
    std::mutex m_mutex;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_times;
};

ShaderHotReloadWatcher& shaderHotReloadWatcher() {
    static ShaderHotReloadWatcher w;
    return w;
}

void shaderHotReloadWatchPath(const char* path) {
    shaderHotReloadWatcher().watch(path);
}

bool shaderHotReloadPollChanged(const char* path) {
    return shaderHotReloadWatcher().pollChanged(path);
}

} // namespace engine
