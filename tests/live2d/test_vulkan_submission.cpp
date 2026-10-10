#include <volk.h>
#include "cubism_vulkan_submission.hpp"
#include "cubism_vulkan_copy_ranges.hpp"
#include <cstdio>
#include <stdexcept>
#include <type_traits>
using namespace bongo_cat;
#define CHECK(value) do { if (!(value)) { std::fprintf(stderr, "Failed line %d: %s\n", __LINE__, #value); return 1; } } while (0)
namespace {
template<class T> T handle(uintptr_t value) {
    if constexpr (std::is_pointer_v<T>) return reinterpret_cast<T>(value);
    else return static_cast<T>(value);
}
unsigned submits, waits, queue_idles, frees, destroyed, pipeline_binds, descriptor_binds, culls;
unsigned submitted_infos, submitted_commands;
bool fail_submit, fail_cache;
VkPipelineCache used_cache;
VKAPI_ATTR VkResult VKAPI_CALL create_fence(VkDevice, const VkFenceCreateInfo *, const VkAllocationCallbacks *, VkFence *out) {
    *out = handle<VkFence>(1); return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL reset(VkDevice, uint32_t, const VkFence *) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL wait(VkDevice, uint32_t, const VkFence *, VkBool32, uint64_t) { ++waits; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue, uint32_t count, const VkSubmitInfo *info, VkFence fence) {
    ++submits; submitted_infos = count; submitted_commands = 0;
    if (!fence) return VK_ERROR_INITIALIZATION_FAILED;
    for (uint32_t i = 0; i < count; ++i) submitted_commands += info[i].commandBufferCount;
    if (count == 2 && (info[0].signalSemaphoreCount != 1 || info[1].waitSemaphoreCount != 1 ||
        info[0].pSignalSemaphores[0] != info[1].pWaitSemaphores[0])) return VK_ERROR_INITIALIZATION_FAILED;
    return fail_submit ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL idle(VkQueue) { ++queue_idles; return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL device_idle(VkDevice) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL end(VkCommandBuffer) { return VK_SUCCESS; }
VKAPI_ATTR VkResult VKAPI_CALL begin(VkCommandBuffer, const VkCommandBufferBeginInfo *) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL free_commands(VkDevice, VkCommandPool, uint32_t count, const VkCommandBuffer *) { frees += count; }
VKAPI_ATTR void VKAPI_CALL destroy_buffer(VkDevice, VkBuffer, const VkAllocationCallbacks *) { ++destroyed; }
VKAPI_ATTR void VKAPI_CALL free_memory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *) {}
VKAPI_ATTR void VKAPI_CALL destroy_fence(VkDevice, VkFence, const VkAllocationCallbacks *) {}
VKAPI_ATTR VkResult VKAPI_CALL create_cache(VkDevice, const VkPipelineCacheCreateInfo *, const VkAllocationCallbacks *, VkPipelineCache *out) {
    *out = fail_cache ? VK_NULL_HANDLE : handle<VkPipelineCache>(2);
    return fail_cache ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_cache(VkDevice, VkPipelineCache, const VkAllocationCallbacks *) {}
VKAPI_ATTR void VKAPI_CALL bind_pipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) { ++pipeline_binds; }
VKAPI_ATTR void VKAPI_CALL bind_descriptors(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t,
    uint32_t, const VkDescriptorSet *, uint32_t, const uint32_t *) { ++descriptor_binds; }
VKAPI_ATTR void VKAPI_CALL set_cull(VkCommandBuffer, VkCullModeFlags) { ++culls; }
VKAPI_ATTR VkResult VKAPI_CALL create_pipelines(VkDevice, VkPipelineCache cache, uint32_t,
    const VkGraphicsPipelineCreateInfo *, const VkAllocationCallbacks *, VkPipeline *) {
    used_cache = cache; return VK_SUCCESS;
}
}
int main() {
    std::vector<VkBufferCopy> ranges;
    vulkan_copy_range_append(ranges, 32, 16);
    vulkan_copy_range_append(ranges, 48, 16);
    vulkan_copy_range_append(ranges, 64, 0);
    vulkan_copy_range_append(ranges, 96, 16);
    CHECK(ranges.size() == 2 && ranges[0].size == 32 && ranges[1].size == 16);
    CHECK(ranges[0].srcOffset == 32 && ranges[1].srcOffset == 96);
    vulkan_copy_ranges_rebase(ranges, 32);
    CHECK(ranges[0].srcOffset == 0 && ranges[0].dstOffset == 32);
    CHECK(ranges[1].srcOffset == 64 && ranges[1].dstOffset == 96);
    CHECK(ranges[0].srcOffset + ranges[0].size <= ranges[1].srcOffset);

    vkCreateFence = create_fence; vkDestroyFence = destroy_fence;
    vkResetFences = reset; vkWaitForFences = wait; vkQueueSubmit = submit;
    vkQueueWaitIdle = idle; vkDeviceWaitIdle = device_idle;
    vkBeginCommandBuffer = begin; vkEndCommandBuffer = end; vkFreeCommandBuffers = free_commands;
    vkDestroyBuffer = destroy_buffer; vkFreeMemory = free_memory;
    vkCreatePipelineCache = create_cache; vkDestroyPipelineCache = destroy_cache;
    vkCmdBindPipeline = bind_pipeline; vkCmdBindDescriptorSets = bind_descriptors; vkCmdSetCullMode = set_cull;
    vkCreateGraphicsPipelines = create_pipelines;
    const auto device = handle<VkDevice>(1);
    const auto queue = handle<VkQueue>(1);
    vulkan_submit_initialize(device, queue, handle<VkCommandPool>(1));
    const auto update = handle<VkCommandBuffer>(1), draw = handle<VkCommandBuffer>(2);
    const auto semaphore = handle<VkSemaphore>(3);
    vulkan_submit(update, semaphore, VK_NULL_HANDLE);
    CHECK(submits == 0);
    vulkan_submit(draw, VK_NULL_HANDLE, semaphore);
    CHECK(submits == 1 && submitted_infos == 2 && waits == 1 && queue_idles == 0 && frees == 0);
    vulkan_upload_batch_begin();
    vulkan_submit(update, VK_NULL_HANDLE, VK_NULL_HANDLE);
    CHECK(vulkan_upload_defer_buffer(handle<VkBuffer>(3), handle<VkDeviceMemory>(3)));
    vulkan_submit(draw, VK_NULL_HANDLE, VK_NULL_HANDLE);
    CHECK(submits == 1 && destroyed == 0 && frees == 0);
    vulkan_upload_batch_end();
    CHECK(submits == 2 && submitted_commands == 2 && destroyed == 1 && frees == 2);
    vulkan_upload_batch_begin();
    vulkan_submit(update, VK_NULL_HANDLE, VK_NULL_HANDLE);
    CHECK(vulkan_upload_defer_buffer(handle<VkBuffer>(4), handle<VkDeviceMemory>(4)));
    vulkan_submit_cancel();
    CHECK(submits == 2 && destroyed == 2 && frees == 3);
    fail_submit = true;
    try { vulkan_submit(draw, VK_NULL_HANDLE, VK_NULL_HANDLE); CHECK(false); }
    catch (const std::runtime_error &) {}
    CHECK(queue_idles == 1 && frees == 4);
    fail_submit = false;
    const auto pipeline = handle<VkPipeline>(7);
    const auto layout = handle<VkPipelineLayout>(7);
    VkDescriptorSet descriptor = handle<VkDescriptorSet>(8);
    VkCommandBufferBeginInfo info{};
    vulkan_begin_commands(draw, &info);
    for (int i = 0; i < 2; ++i) {
        vulkan_bind_pipeline(draw, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vulkan_bind_descriptors(draw, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptor, 0, nullptr);
        vulkan_set_cull(draw, VK_CULL_MODE_BACK_BIT);
    }
    CHECK(pipeline_binds == 1 && descriptor_binds == 1 && culls == 1);
    vulkan_begin_commands(draw, &info);
    vulkan_bind_pipeline(draw, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vulkan_bind_descriptors(draw, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptor, 0, nullptr);
    CHECK(pipeline_binds == 2 && descriptor_binds == 2);
    vulkan_create_pipelines(device, VK_NULL_HANDLE, 0, nullptr, nullptr, nullptr);
    CHECK(used_cache != VK_NULL_HANDLE);
    vulkan_submit_release(); fail_cache = true;
    vulkan_submit_initialize(device, queue, handle<VkCommandPool>(1));
    vulkan_create_pipelines(device, handle<VkPipelineCache>(9), 0, nullptr, nullptr, nullptr);
    CHECK(used_cache == handle<VkPipelineCache>(9));
    vulkan_submit_release();
    return 0;
}
