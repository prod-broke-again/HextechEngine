#pragma once

#include "engine/assets/AssetManager.hpp"

#include <cgltf.h>

namespace engine {

class GltfTextureLoader {
public:
    [[nodiscard]] static AssetManager::TexturePtr loadImage(AssetManager& assets, const LoadedGltfCpu& gltf,
                                                            const cgltf_image* image);
    [[nodiscard]] static AssetManager::TexturePtr loadBaseColorTexture(AssetManager& assets,
                                                                       const LoadedGltfCpu& gltf,
                                                                       const cgltf_texture* texture);
};

} // namespace engine
