#include "vulkan/GpuProfiler.hpp"

#include <array>
#include <stdexcept>

GpuProfiler::~GpuProfiler() {
    shutdown();
}

void GpuProfiler::shutdown() {
    if (queryPool_ != VK_NULL_HANDLE) {
        vkDestroyQueryPool(device_, queryPool_, nullptr);
        queryPool_ = VK_NULL_HANDLE;
    }
    device_ = VK_NULL_HANDLE;
}

void GpuProfiler::initialize(VkDevice device, float timestampPeriodNanoseconds, std::uint32_t validBits,
                             std::uint32_t frameCount) {
    device_ = device;
    timestampPeriodNanoseconds_ = timestampPeriodNanoseconds;
    validBits_ = validBits;
    frameCount_ = frameCount;
    recorded_.assign(frameCount, false);

    VkQueryPoolCreateInfo createInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = frameCount * 2;
    if (vkCreateQueryPool(device_, &createInfo, nullptr, &queryPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create the GPU timestamp query pool");
    }
}

void GpuProfiler::beginFrame(VkCommandBuffer commandBuffer, std::uint32_t frameIndex) {
    const std::uint32_t firstQuery = frameIndex * 2;
    vkCmdResetQueryPool(commandBuffer, queryPool_, firstQuery, 2);
    vkCmdWriteTimestamp2(commandBuffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, queryPool_, firstQuery);
    recorded_[frameIndex] = true;
}

void GpuProfiler::endFrame(VkCommandBuffer commandBuffer, std::uint32_t frameIndex) {
    vkCmdWriteTimestamp2(commandBuffer, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, queryPool_, frameIndex * 2 + 1);
}

std::optional<double> GpuProfiler::resolveMilliseconds(std::uint32_t frameIndex) const {
    if (!recorded_[frameIndex]) {
        return std::nullopt;
    }

    std::array<std::uint64_t, 2> timestamps{};
    const VkResult result = vkGetQueryPoolResults(
        device_, queryPool_, frameIndex * 2, 2, sizeof(timestamps), timestamps.data(),
        sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT);
    if (result == VK_NOT_READY) {
        return std::nullopt;
    }
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to resolve GPU timestamps");
    }

    const std::uint64_t mask = validBits_ == 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << validBits_) - 1;
    const std::uint64_t elapsedTicks = (timestamps[1] - timestamps[0]) & mask;
    const double elapsedNanoseconds = static_cast<double>(elapsedTicks) * timestampPeriodNanoseconds_;
    return elapsedNanoseconds / 1'000'000.0;
}
