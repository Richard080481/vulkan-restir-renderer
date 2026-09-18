#include "vulkan/VulkanContext.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace {

#if defined(VRR_ENABLE_VALIDATION)
constexpr bool validationRequested = true;
#else
constexpr bool validationRequested = false;
#endif

constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";
constexpr std::array requiredDeviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

void require(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(operation) + " failed with VkResult " + std::to_string(result));
    }
}

bool hasValidationLayer() {
    std::uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    return std::ranges::any_of(layers, [](const VkLayerProperties& layer) {
        return std::strcmp(layer.layerName, validationLayer) == 0;
    });
}

VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo() {
    VkDebugUtilsMessengerCreateInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = VulkanContext::debugCallback;
    return info;
}

} // namespace

VulkanContext::VulkanContext(Win32Window& window) : window_(window) {
    createInstance();
    createDebugMessenger();
    createSurface();
    selectPhysicalDevice();
    createDevice();
    createSwapchain();
    createCommandResources();
    createSynchronization();
    reportRayTracingSupport();
}

VulkanContext::~VulkanContext() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        gpuProfiler_.shutdown();
        for (std::uint32_t index = 0; index < framesInFlight; ++index) {
            vkDestroyFence(device_, inFlight_[index], nullptr);
            vkDestroySemaphore(device_, imageAvailable_[index], nullptr);
        }
        if (commandPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, commandPool_, nullptr);
        }
        destroySwapchain();
        vkDestroyDevice(device_, nullptr);
    }

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }
    if (debugMessenger_ != VK_NULL_HANDLE) {
        const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy != nullptr) {
            destroy(instance_, debugMessenger_, nullptr);
        }
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
    }
}

void VulkanContext::createInstance() {
    std::uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion != nullptr) {
        require(vkEnumerateInstanceVersion(&loaderVersion), "vkEnumerateInstanceVersion");
    }
    if (loaderVersion < VK_API_VERSION_1_3) {
        throw std::runtime_error("Vulkan 1.3 or newer is required");
    }

    const bool enableValidation = validationRequested && hasValidationLayer();
    if (validationRequested && !enableValidation) {
        std::cerr << "Warning: validation requested, but VK_LAYER_KHRONOS_validation is unavailable\n";
    }

    VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    application.pApplicationName = "Vulkan ReSTIR Renderer";
    application.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    application.pEngineName = "VRR";
    application.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
    application.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };
    if (enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    auto debugInfo = debugCreateInfo();
    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &application;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    if (enableValidation) {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayer;
        createInfo.pNext = &debugInfo;
    }

    require(vkCreateInstance(&createInfo, nullptr, &instance_), "vkCreateInstance");
}

void VulkanContext::createDebugMessenger() {
    if (!validationRequested) {
        return;
    }
    const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (create == nullptr) {
        return;
    }
    auto createInfo = debugCreateInfo();
    require(create(instance_, &createInfo, nullptr, &debugMessenger_), "vkCreateDebugUtilsMessengerEXT");
}

void VulkanContext::createSurface() {
    VkWin32SurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    createInfo.hinstance = window_.instance();
    createInfo.hwnd = window_.handle();
    require(vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_), "vkCreateWin32SurfaceKHR");
}

VulkanContext::QueueFamilies VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> properties(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

    QueueFamilies result;
    for (std::uint32_t index = 0; index < count; ++index) {
        if ((properties[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            result.graphics = index;
        }
        VkBool32 presentSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface_, &presentSupported);
        if (presentSupported == VK_TRUE) {
            result.present = index;
        }
        if (result.complete()) {
            break;
        }
    }
    return result;
}

bool VulkanContext::supportsDeviceExtensions(VkPhysicalDevice device) const {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    return std::ranges::all_of(requiredDeviceExtensions, [&](const char* required) {
        return std::ranges::any_of(available, [&](const VkExtensionProperties& extension) {
            return std::strcmp(extension.extensionName, required) == 0;
        });
    });
}

VulkanContext::SwapchainSupport VulkanContext::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupport support;
    require(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &support.capabilities),
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    std::uint32_t count = 0;
    require(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, nullptr),
            "vkGetPhysicalDeviceSurfaceFormatsKHR");
    support.formats.resize(count);
    require(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &count, support.formats.data()),
            "vkGetPhysicalDeviceSurfaceFormatsKHR");

    count = 0;
    require(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, nullptr),
            "vkGetPhysicalDeviceSurfacePresentModesKHR");
    support.presentModes.resize(count);
    require(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &count, support.presentModes.data()),
            "vkGetPhysicalDeviceSurfacePresentModesKHR");
    return support;
}

void VulkanContext::selectPhysicalDevice() {
    std::uint32_t count = 0;
    require(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
    if (count == 0) {
        throw std::runtime_error("No Vulkan physical device was found");
    }

    std::vector<VkPhysicalDevice> devices(count);
    require(vkEnumeratePhysicalDevices(instance_, &count, devices.data()), "vkEnumeratePhysicalDevices");

    int bestScore = -1;
    for (VkPhysicalDevice candidate : devices) {
        const auto families = findQueueFamilies(candidate);
        if (!families.complete() || !supportsDeviceExtensions(candidate)) {
            continue;
        }
        const auto swapchain = querySwapchainSupport(candidate);
        if (swapchain.formats.empty() || swapchain.presentModes.empty() ||
            (swapchain.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
            continue;
        }

        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features.pNext = &features13;
        vkGetPhysicalDeviceFeatures2(candidate, &features);
        if (features13.synchronization2 != VK_TRUE) {
            continue;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);
        const int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 100 : 10;
        if (score > bestScore) {
            bestScore = score;
            physicalDevice_ = candidate;
            queueFamilies_ = families;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        throw std::runtime_error("No device satisfies the Vulkan 1.3 swapchain requirements");
    }

    VkPhysicalDeviceProperties selected{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &selected);
    std::cout << "GPU: " << selected.deviceName << '\n';
}

void VulkanContext::createDevice() {
    const std::set uniqueFamilies{queueFamilies_.graphics, queueFamilies_.present};
    constexpr float priority = 1.0F;
    std::vector<VkDeviceQueueCreateInfo> queues;
    for (const std::uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex = family;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        queues.push_back(queue);
    }

    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.synchronization2 = VK_TRUE;

    VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.pNext = &features13;
    createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queues.size());
    createInfo.pQueueCreateInfos = queues.data();
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(requiredDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
    require(vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_), "vkCreateDevice");

    vkGetDeviceQueue(device_, queueFamilies_.graphics, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, queueFamilies_.present, 0, &presentQueue_);
    beginDebugLabel_ = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device_, "vkCmdBeginDebugUtilsLabelEXT"));
    endDebugLabel_ = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(device_, "vkCmdEndDebugUtilsLabelEXT"));
}

VkSurfaceFormatKHR VulkanContext::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    const auto preferred = std::ranges::find_if(formats, [](const VkSurfaceFormatKHR& format) {
        return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
               format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    });
    return preferred != formats.end() ? *preferred : formats.front();
}

VkPresentModeKHR VulkanContext::choosePresentMode(const std::vector<VkPresentModeKHR>& modes) const {
    return std::ranges::find(modes, VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()
               ? VK_PRESENT_MODE_MAILBOX_KHR
               : VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    return {
        std::clamp(window_.width(), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(window_.height(), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
}

void VulkanContext::createSwapchain() {
    const auto support = querySwapchainSupport(physicalDevice_);
    const auto format = chooseSurfaceFormat(support.formats);
    const auto presentMode = choosePresentMode(support.presentModes);
    const auto extent = chooseExtent(support.capabilities);

    std::uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, support.capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = format.format;
    createInfo.imageColorSpace = format.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const std::array familyIndices{queueFamilies_.graphics, queueFamilies_.present};
    if (queueFamilies_.graphics != queueFamilies_.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(familyIndices.size());
        createInfo.pQueueFamilyIndices = familyIndices.data();
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    require(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_), "vkCreateSwapchainKHR");

    require(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr), "vkGetSwapchainImagesKHR");
    swapchainImages_.resize(imageCount);
    require(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data()),
            "vkGetSwapchainImagesKHR");
    swapchainLayouts_.assign(imageCount, VK_IMAGE_LAYOUT_UNDEFINED);
    imagesInFlight_.assign(imageCount, VK_NULL_HANDLE);
    renderFinished_.resize(imageCount);
    const VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (VkSemaphore& semaphore : renderFinished_) {
        require(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &semaphore),
                "vkCreateSemaphore(renderFinished)");
    }
    swapchainFormat_ = format.format;
    swapchainExtent_ = extent;
    std::cout << "Swapchain: " << extent.width << 'x' << extent.height << ", " << imageCount << " images\n";
}

void VulkanContext::destroySwapchain() {
    for (const VkSemaphore semaphore : renderFinished_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
    renderFinished_.clear();
    swapchainImages_.clear();
    swapchainLayouts_.clear();
    imagesInFlight_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::createCommandResources() {
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilies_.graphics;
    require(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = commandPool_;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = framesInFlight;
    require(vkAllocateCommandBuffers(device_, &allocate, commandBuffers_.data()), "vkAllocateCommandBuffers");

    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> familyProperties(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &familyCount, familyProperties.data());
    const std::uint32_t validBits = familyProperties[queueFamilies_.graphics].timestampValidBits;
    if (validBits == 0) {
        throw std::runtime_error("The selected graphics queue does not support timestamp queries");
    }
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    gpuProfiler_.initialize(device_, properties.limits.timestampPeriod, validBits, framesInFlight);
}

void VulkanContext::createSynchronization() {
    const VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::uint32_t index = 0; index < framesInFlight; ++index) {
        require(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailable_[index]),
                "vkCreateSemaphore(imageAvailable)");
        require(vkCreateFence(device_, &fenceInfo, nullptr, &inFlight_[index]), "vkCreateFence");
    }
}

void VulkanContext::recordClearCommands(VkCommandBuffer commandBuffer, std::uint32_t imageIndex) {
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    require(vkBeginCommandBuffer(commandBuffer, &begin), "vkBeginCommandBuffer");
    gpuProfiler_.beginFrame(commandBuffer, frameIndex_);
    beginDebugLabel(commandBuffer, "Clear swapchain", {0.15F, 0.35F, 0.8F, 1.0F});

    VkImageMemoryBarrier2 toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    toTransfer.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    toTransfer.srcAccessMask = VK_ACCESS_2_NONE;
    toTransfer.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = swapchainLayouts_[imageIndex];
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = swapchainImages_[imageIndex];
    toTransfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &toTransfer;
    vkCmdPipelineBarrier2(commandBuffer, &dependency);

    const float phase = static_cast<float>(frameIndex_) / static_cast<float>(framesInFlight);
    const VkClearColorValue clearColor{{0.025F + phase * 0.01F, 0.04F, 0.075F, 1.0F}};
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(commandBuffer, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &clearColor, 1, &range);

    VkImageMemoryBarrier2 toPresent{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    toPresent.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toPresent.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toPresent.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
    toPresent.dstAccessMask = VK_ACCESS_2_NONE;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = swapchainImages_[imageIndex];
    toPresent.subresourceRange = range;
    dependency.pImageMemoryBarriers = &toPresent;
    vkCmdPipelineBarrier2(commandBuffer, &dependency);

    endDebugLabel(commandBuffer);
    gpuProfiler_.endFrame(commandBuffer, frameIndex_);
    require(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
    swapchainLayouts_[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

void VulkanContext::drawFrame() {
    require(vkWaitForFences(device_, 1, &inFlight_[frameIndex_], VK_TRUE, UINT64_MAX), "vkWaitForFences");
    if (const auto elapsed = gpuProfiler_.resolveMilliseconds(frameIndex_)) {
        lastGpuMilliseconds_ = *elapsed;
    }

    std::uint32_t imageIndex = 0;
    const VkResult acquire = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX, imageAvailable_[frameIndex_], VK_NULL_HANDLE, &imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
        require(acquire, "vkAcquireNextImageKHR");
    }

    if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
        require(vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX),
                "vkWaitForFences(image)");
    }
    imagesInFlight_[imageIndex] = inFlight_[frameIndex_];
    require(vkResetFences(device_, 1, &inFlight_[frameIndex_]), "vkResetFences");
    require(vkResetCommandBuffer(commandBuffers_[frameIndex_], 0), "vkResetCommandBuffer");
    recordClearCommands(commandBuffers_[frameIndex_], imageIndex);

    VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    wait.semaphore = imageAvailable_[frameIndex_];
    wait.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    VkCommandBufferSubmitInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    command.commandBuffer = commandBuffers_[frameIndex_];
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signal.semaphore = renderFinished_[imageIndex];
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &wait;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &command;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signal;
    require(vkQueueSubmit2(graphicsQueue_, 1, &submit, inFlight_[frameIndex_]), "vkQueueSubmit2");

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished_[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &imageIndex;
    const VkResult result = vkQueuePresentKHR(presentQueue_, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || acquire == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        require(result, "vkQueuePresentKHR");
    }

    ++submittedFrameCount_;
    if (submittedFrameCount_ % 120 == 0) {
        std::cout << "GPU frame time (clear/present workload): " << lastGpuMilliseconds_ << " ms\n";
    }
    frameIndex_ = (frameIndex_ + 1) % framesInFlight;
}

void VulkanContext::recreateSwapchain() {
    if (window_.width() == 0 || window_.height() == 0) {
        return;
    }
    require(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle(recreateSwapchain)");
    destroySwapchain();
    createSwapchain();
}

void VulkanContext::waitIdle() const {
    if (device_ != VK_NULL_HANDLE) {
        require(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle");
    }
}

void VulkanContext::reportRayTracingSupport() const {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &count, extensions.data());
    const auto supports = [&](const char* name) {
        return std::ranges::any_of(extensions, [&](const VkExtensionProperties& extension) {
            return std::strcmp(extension.extensionName, name) == 0;
        });
    };
    std::cout << "Ray query support: "
              << (supports(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
                          supports(VK_KHR_RAY_QUERY_EXTENSION_NAME)
                      ? "yes"
                      : "no")
              << '\n';
}

void VulkanContext::beginDebugLabel(VkCommandBuffer commandBuffer, const char* name,
                                    const std::array<float, 4>& color) const {
    if (beginDebugLabel_ == nullptr) {
        return;
    }
    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    std::ranges::copy(color, label.color);
    beginDebugLabel_(commandBuffer, &label);
}

void VulkanContext::endDebugLabel(VkCommandBuffer commandBuffer) const {
    if (endDebugLabel_ != nullptr) {
        endDebugLabel_(commandBuffer);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*) {
    std::cerr << (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "[Vulkan error] "
                                                                          : "[Vulkan warning] ")
              << callbackData->pMessage << '\n';
    return VK_FALSE;
}
