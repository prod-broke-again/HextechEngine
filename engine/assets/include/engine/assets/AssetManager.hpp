#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <cgltf.h>

namespace engine {

struct LoadedTextureCpu {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<unsigned char> pixels;
};

struct LoadedGltfCpu {
    std::unique_ptr<cgltf_data, void (*)(cgltf_data*)> data;
};

class AssetManager {
public:
    using GltfPtr = std::shared_ptr<LoadedGltfCpu>;
    using TexturePtr = std::shared_ptr<LoadedTextureCpu>;

    [[nodiscard]] GltfPtr getOrLoadGltf(const std::filesystem::path& path);
    [[nodiscard]] TexturePtr getOrLoadTexture(const std::filesystem::path& path, bool hdr = false);
    [[nodiscard]] TexturePtr loadTextureFromMemory(const unsigned char* data, int size);

    void unload(const std::filesystem::path& path);

    [[nodiscard]] std::future<GltfPtr> loadGltfAsync(const std::filesystem::path& path);
    [[nodiscard]] std::future<TexturePtr> loadTextureAsync(const std::filesystem::path& path,
                                                           bool hdr = false);

private:
    std::mutex m_mutex;
    std::unordered_map<std::string, GltfPtr> m_gltf;
    std::unordered_map<std::string, TexturePtr> m_textures;
};

} // namespace engine
