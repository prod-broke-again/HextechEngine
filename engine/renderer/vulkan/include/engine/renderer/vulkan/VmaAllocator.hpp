#pragma once

#include <vulkan/vulkan.h>

namespace engine {

class VmaAllocatorHolder {
public:
    bool init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
    void shutdown();

    [[nodiscard]] void* get() const { return m_allocator; }

private:
    void* m_allocator = nullptr;
};

} // namespace engine
