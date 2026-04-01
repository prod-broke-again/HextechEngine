#include "engine/renderer/vulkan/VmaAllocator.hpp"

#include "engine/core/Log.hpp"

#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace engine {

bool VmaAllocatorHolder::init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
    VmaVulkanFunctions vmaFunctions{};
    vmaFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vmaFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.pVulkanFunctions = &vmaFunctions;

    VmaAllocator allocator{};
    const VkResult r = vmaCreateAllocator(&allocatorInfo, &allocator);
    if (r != VK_SUCCESS) {
        log(LogLevel::Error, "vmaCreateAllocator failed");
        m_allocator = nullptr;
        return false;
    }
    m_allocator = allocator;
    return true;
}

void VmaAllocatorHolder::shutdown() {
    if (m_allocator) {
        vmaDestroyAllocator(static_cast<VmaAllocator>(m_allocator));
        m_allocator = nullptr;
    }
}

} // namespace engine
