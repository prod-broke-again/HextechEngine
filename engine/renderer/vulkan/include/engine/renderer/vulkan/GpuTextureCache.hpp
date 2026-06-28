#pragma once

#include "engine/assets/AssetManager.hpp"
#include "engine/assets/MeshData.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"

#include <vulkan/vulkan.h>

#include <vector>

namespace engine {

struct GpuTexture {
    VkImage image = VK_NULL_HANDLE;
    void* allocation = nullptr;
    VkImageView view = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
};

class GpuTextureCache {
public:
    explicit GpuTextureCache(VulkanContext& ctx);

    bool init(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool,
              VkSampler sampler);
    void shutdown();

    [[nodiscard]] GpuTextureId upload(const LoadedTextureCpu& texture);
    [[nodiscard]] const GpuTexture* get(GpuTextureId id) const;
    [[nodiscard]] VkDescriptorSet defaultDescriptorSet() const { return m_defaultDescriptorSet; }

private:
    bool createImage2D(const LoadedTextureCpu& texture, GpuTexture& out);
    bool createDefaultTexture();

    VulkanContext* m_ctx = nullptr;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSet m_defaultDescriptorSet = VK_NULL_HANDLE;
    std::vector<GpuTexture> m_textures;
};

} // namespace engine
