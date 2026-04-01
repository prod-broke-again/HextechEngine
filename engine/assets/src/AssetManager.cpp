#include "engine/assets/AssetManager.hpp"

#include "engine/core/Log.hpp"

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

std::vector<char> readFileBinary(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const auto size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
}

void cgltfFreeData(cgltf_data* data) {
    if (data) {
        cgltf_free(data);
    }
}

std::shared_ptr<LoadedGltfCpu> parseGltfFromMemory(const std::vector<char>& bytes,
                                                   const std::filesystem::path& pathForUri) {
    cgltf_options options{};
    cgltf_data* data = nullptr;
    const cgltf_result r =
        cgltf_parse(&options, bytes.data(), static_cast<cgltf_size>(bytes.size()), &data);
    if (r != cgltf_result_success || !data) {
        log(LogLevel::Error, "cgltf_parse failed");
        return nullptr;
    }
    const std::string base = pathForUri.parent_path().string();
    if (cgltf_load_buffers(&options, data, base.c_str()) != cgltf_result_success) {
        log(LogLevel::Error, "cgltf_load_buffers failed");
        cgltf_free(data);
        return nullptr;
    }
    if (cgltf_validate(data) != cgltf_result_success) {
        log(LogLevel::Warn, "cgltf_validate reported issues");
    }
    return std::make_shared<LoadedGltfCpu>(
        LoadedGltfCpu{std::unique_ptr<cgltf_data, void (*)(cgltf_data*)>(data, cgltfFreeData)});
}

TexturePtr loadTextureFile(const std::filesystem::path& path, bool hdr) {
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

GltfPtr AssetManager::getOrLoadGltf(const std::filesystem::path& path) {
    const std::string key = path.generic_string();
    std::lock_guard lock(m_mutex);
    if (auto it = m_gltf.find(key); it != m_gltf.end()) {
        return it->second;
    }
    const auto bytes = readFileBinary(path);
    if (bytes.empty()) {
        return nullptr;
    }
    auto loaded = parseGltfFromMemory(bytes, path);
    if (!loaded) {
        return nullptr;
    }
    m_gltf[key] = loaded;
    return loaded;
}

TexturePtr AssetManager::getOrLoadTexture(const std::filesystem::path& path, bool hdr) {
    const std::string key = path.generic_string();
    std::lock_guard lock(m_mutex);
    if (auto it = m_textures.find(key); it != m_textures.end()) {
        return it->second;
    }
    auto loaded = loadTextureFile(path, hdr);
    if (!loaded) {
        return nullptr;
    }
    m_textures[key] = loaded;
    return loaded;
}

void AssetManager::unload(const std::filesystem::path& path) {
    const std::string key = path.generic_string();
    std::lock_guard lock(m_mutex);
    m_gltf.erase(key);
    m_textures.erase(key);
}

std::future<GltfPtr> AssetManager::loadGltfAsync(const std::filesystem::path& path) {
    return std::async(std::launch::async, [this, path]() { return getOrLoadGltf(path); });
}

std::future<TexturePtr> AssetManager::loadTextureAsync(const std::filesystem::path& path, bool hdr) {
    return std::async(std::launch::async, [this, path, hdr]() {
        return getOrLoadTexture(path, hdr);
    });
}

} // namespace engine
