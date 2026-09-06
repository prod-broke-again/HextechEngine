#include "engine/assets/AssetManager.hpp"

#include "engine/core/Log.hpp"
#include "engine/core/Path.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cgltf.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

namespace engine {

namespace {

void cgltfFreeData(cgltf_data* data) {
    if (data) {
        cgltf_free(data);
    }
}

std::shared_ptr<LoadedGltfCpu> parseGltfFile(const std::filesystem::path& path) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    const std::string gltfPath = path.string();
    if (cgltf_parse_file(&options, gltfPath.c_str(), &data) != cgltf_result_success || !data) {
        log(LogLevel::Error, "cgltf_parse_file failed for " + gltfPath);
        return nullptr;
    }
    if (cgltf_load_buffers(&options, data, gltfPath.c_str()) != cgltf_result_success) {
        log(LogLevel::Error, "cgltf_load_buffers failed for " + gltfPath);
        cgltf_free(data);
        return nullptr;
    }
    if (cgltf_validate(data) != cgltf_result_success) {
        log(LogLevel::Warn, "cgltf_validate reported issues for " + gltfPath);
    }
    return std::make_shared<LoadedGltfCpu>(
        LoadedGltfCpu{std::unique_ptr<cgltf_data, void (*)(cgltf_data*)>(data, cgltfFreeData)});
}

std::shared_ptr<LoadedTextureCpu> loadTextureFile(const std::filesystem::path& path, bool hdr) {
    auto tex = std::make_shared<LoadedTextureCpu>();
    if (hdr) {
        int w = 0;
        int h = 0;
        int c = 0;
        float* data = stbi_loadf(path.string().c_str(), &w, &h, &c, 4);
        if (!data) {
            log(LogLevel::Error, "stbi_loadf failed");
            return nullptr;
        }
        tex->width = w;
        tex->height = h;
        tex->channels = 4;
        const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
        tex->pixels.resize(count * sizeof(float));
        std::memcpy(tex->pixels.data(), data, count * sizeof(float));
        stbi_image_free(data);
    } else {
        int w = 0;
        int h = 0;
        int c = 0;
        unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &c, 4);
        if (!data) {
            log(LogLevel::Error, "stbi_load failed");
            return nullptr;
        }
        tex->width = w;
        tex->height = h;
        tex->channels = 4;
        const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
        tex->pixels.assign(data, data + count);
        stbi_image_free(data);
    }
    return tex;
}

} // namespace

AssetManager::GltfPtr AssetManager::getOrLoadGltf(const std::filesystem::path& path) {
    const auto resolved = resolvePath(path);
    const std::string key = resolved.generic_string();
    std::lock_guard lock(m_mutex);
    if (auto it = m_gltf.find(key); it != m_gltf.end()) {
        return it->second;
    }
    if (!std::filesystem::exists(resolved)) {
        log(LogLevel::Warn, "getOrLoadGltf: file not found " + resolved.string());
        return nullptr;
    }
    auto loaded = parseGltfFile(resolved);
    if (!loaded) {
        return nullptr;
    }
    m_gltf[key] = loaded;
    return loaded;
}

AssetManager::TexturePtr AssetManager::getOrLoadTexture(const std::filesystem::path& path, bool hdr) {
    const auto resolved = resolvePath(path);
    const std::string key = resolved.generic_string();
    std::lock_guard lock(m_mutex);
    if (auto it = m_textures.find(key); it != m_textures.end()) {
        return it->second;
    }
    auto loaded = loadTextureFile(resolved, hdr);
    if (!loaded) {
        return nullptr;
    }
    m_textures[key] = loaded;
    return loaded;
}

void AssetManager::unload(const std::filesystem::path& path) {
    const auto resolved = resolvePath(path);
    const std::string key = resolved.generic_string();
    std::lock_guard lock(m_mutex);
    m_gltf.erase(key);
    m_textures.erase(key);
}

std::future<AssetManager::GltfPtr> AssetManager::loadGltfAsync(const std::filesystem::path& path) {
    return std::async(std::launch::async, [this, path]() { return getOrLoadGltf(path); });
}

std::future<AssetManager::TexturePtr> AssetManager::loadTextureAsync(const std::filesystem::path& path, bool hdr) {
    return std::async(std::launch::async, [this, path, hdr]() {
        return getOrLoadTexture(path, hdr);
    });
}

AssetManager::TexturePtr AssetManager::loadTextureFromMemory(const unsigned char* data, int size) {
    if (!data || size <= 0) {
        return nullptr;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
    if (!pixels) {
        log(LogLevel::Error, "stbi_load_from_memory failed");
        return nullptr;
    }
    auto tex = std::make_shared<LoadedTextureCpu>();
    tex->width = width;
    tex->height = height;
    tex->channels = 4;
    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    tex->pixels.assign(pixels, pixels + count);
    stbi_image_free(pixels);
    return tex;
}

} // namespace engine
