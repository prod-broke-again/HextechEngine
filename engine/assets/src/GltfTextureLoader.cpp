#include "engine/assets/GltfTextureLoader.hpp"

#include "engine/core/Log.hpp"

namespace engine {

AssetManager::TexturePtr GltfTextureLoader::loadImage(AssetManager& assets, const LoadedGltfCpu& gltf,
                                                        const cgltf_image* image) {
    if (!gltf.data || !image) {
        return nullptr;
    }

    if (image->buffer_view) {
        const cgltf_buffer_view* view = image->buffer_view;
        if (!view->buffer || !view->buffer->data) {
            return nullptr;
        }
        const unsigned char* bytes =
            static_cast<const unsigned char*>(view->buffer->data) + view->offset;
        const int byteCount = static_cast<int>(view->size);
        return assets.loadTextureFromMemory(bytes, byteCount);
    }

    if (image->uri && image->uri[0] != '\0') {
        log(LogLevel::Warn, "GltfTextureLoader: external URI textures are not supported yet");
    }
    return nullptr;
}

AssetManager::TexturePtr GltfTextureLoader::loadBaseColorTexture(AssetManager& assets,
                                                                   const LoadedGltfCpu& gltf,
                                                                   const cgltf_texture* texture) {
    if (!texture || !texture->image) {
        return nullptr;
    }
    return loadImage(assets, gltf, texture->image);
}

} // namespace engine
