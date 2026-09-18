#pragma once

#include "platform/Win32Window.hpp"
#include "vulkan/GpuProfiler.hpp"

#include <array>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

class VulkanContext {
public:
    explicit VulkanContext(Win32Window& window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void drawFrame();
    void waitIdle() const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData);

private:
    static constexpr std::uint32_t framesInFlight = 2;

    struct QueueFamilies {
        std::uint32_t graphics = UINT32_MAX;
        std::uint32_t present = UINT32_MAX;
        [[nodiscard]] bool complete() const { return graphics != UINT32_MAX && present != UINT32_MAX; }
    };

    struct SwapchainSupport {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    void createInstance();
    void createDebugMessenger();
    void createSurface();
    void selectPhysicalDevice();
    void createDevice();
    void createSwapchain();
    void createCommandResources();
    void createSynchronization();
    void recreateSwapchain();
    void destroySwapchain();
    void recordClearCommands(VkCommandBuffer commandBuffer, std::uint32_t imageIndex);

    [[nodiscard]] QueueFamilies findQueueFamilies(VkPhysicalDevice device) const;
    [[nodiscard]] bool supportsDeviceExtensions(VkPhysicalDevice device) const;
    [[nodiscard]] SwapchainSupport querySwapchainSupport(VkPhysicalDevice device) const;
    [[nodiscard]] VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    [[nodiscard]] VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    [[nodiscard]] VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    void reportRayTracingSupport() const;
    void beginDebugLabel(VkCommandBuffer commandBuffer, const char* name,
                         const std::array<float, 4>& color) const;
    void endDebugLabel(VkCommandBuffer commandBuffer) const;

    Win32Window& window_;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    QueueFamilies queueFamilies_{};
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    PFN_vkCmdBeginDebugUtilsLabelEXT beginDebugLabel_ = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT endDebugLabel_ = nullptr;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageLayout> swapchainLayouts_;

    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    GpuProfiler gpuProfiler_;
    std::array<VkCommandBuffer, framesInFlight> commandBuffers_{};
    std::array<VkSemaphore, framesInFlight> imageAvailable_{};
    std::vector<VkSemaphore> renderFinished_;
    std::array<VkFence, framesInFlight> inFlight_{};
    std::vector<VkFence> imagesInFlight_;
    std::uint32_t frameIndex_ = 0;
    std::uint64_t submittedFrameCount_ = 0;
    double lastGpuMilliseconds_ = 0.0;
};
