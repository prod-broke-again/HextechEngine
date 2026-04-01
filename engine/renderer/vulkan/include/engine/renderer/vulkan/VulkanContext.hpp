#pragma once

#include "engine/core/Events.hpp"
#include "engine/renderer/vulkan/VmaAllocator.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace engine {

constexpr uint32_t kMaxFramesInFlight = 2;

class VulkanContext {
public:
    bool init(GLFWwindow* window, bool enableValidationLayers);
    void shutdown();

    void handleResize(const WindowResizeEvent& e);

    VkResult acquireNextImage(uint32_t* imageIndex);
    VkResult submitAndPresent(uint32_t imageIndex);

    [[nodiscard]] VkInstance instance() const { return m_instance; }
    [[nodiscard]] VkDevice device() const { return m_device; }
    [[nodiscard]] VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] VkQueue graphicsQueue() const { return m_graphicsQueue; }
    [[nodiscard]] VkQueue presentQueue() const { return m_presentQueue; }
    [[nodiscard]] uint32_t graphicsFamily() const { return m_graphicsFamily; }
    [[nodiscard]] uint32_t presentFamily() const { return m_presentFamily; }
    [[nodiscard]] VkRenderPass renderPass() const { return m_renderPass; }
    [[nodiscard]] VkSwapchainKHR swapchain() const { return m_swapchain; }
    [[nodiscard]] VkFormat swapchainFormat() const { return m_swapchainFormat; }
    [[nodiscard]] VkExtent2D swapchainExtent() const { return m_swapchainExtent; }
    [[nodiscard]] const std::vector<VkFramebuffer>& framebuffers() const { return m_swapchainFramebuffers; }
    [[nodiscard]] VkCommandPool commandPool() const { return m_commandPool; }
    [[nodiscard]] VkCommandBuffer commandBuffer(uint32_t frame) const { return m_commandBuffers[frame]; }
    [[nodiscard]] VkSemaphore imageAvailableSemaphore(uint32_t frame) const {
        return m_imageAvailableSemaphores[frame];
    }
    [[nodiscard]] VkSemaphore renderFinishedSemaphore(uint32_t frame) const {
        return m_renderFinishedSemaphores[frame];
    }
    [[nodiscard]] VkFence inFlightFence(uint32_t frame) const { return m_inFlightFences[frame]; }
    [[nodiscard]] uint32_t currentFrame() const { return m_currentFrame; }
    void advanceFrame() { m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight; }

    [[nodiscard]] VmaAllocatorHolder& vma() { return m_vma; }
    [[nodiscard]] GLFWwindow* window() const { return m_window; }

    [[nodiscard]] VkDescriptorPool imguiDescriptorPool() const { return m_imguiDescriptorPool; }

private:
    bool createInstance(bool enableValidationLayers);
    bool createSurface();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain();
    void cleanupSwapchain();
    bool createImageViews();
    bool createRenderPass();
    bool createDepthResources();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createImGuiDescriptorPool();

    GLFWwindow* m_window = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsFamily = 0;
    uint32_t m_presentFamily = 0;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat{};
    VkExtent2D m_swapchainExtent{};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkImage m_depthImage = VK_NULL_HANDLE;
    void* m_depthAllocation = nullptr;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VkFormat m_depthFormat{};
    std::vector<VkFramebuffer> m_swapchainFramebuffers;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;

    VmaAllocatorHolder m_vma;
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;

    uint32_t m_currentFrame = 0;
    bool m_validationLayers = false;
};

} // namespace engine
