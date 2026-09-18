#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

class GpuProfiler {
public:
    GpuProfiler() = default;
    ~GpuProfiler();

    GpuProfiler(const GpuProfiler&) = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;

    void initialize(VkDevice device, float timestampPeriodNanoseconds, std::uint32_t validBits,
                    std::uint32_t frameCount);
    void shutdown();
    void beginFrame(VkCommandBuffer commandBuffer, std::uint32_t frameIndex);
    void endFrame(VkCommandBuffer commandBuffer, std::uint32_t frameIndex);
    [[nodiscard]] std::optional<double> resolveMilliseconds(std::uint32_t frameIndex) const;

private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueryPool queryPool_ = VK_NULL_HANDLE;
    float timestampPeriodNanoseconds_ = 0.0F;
    std::uint32_t validBits_ = 0;
    std::uint32_t frameCount_ = 0;
    std::vector<bool> recorded_;
};
