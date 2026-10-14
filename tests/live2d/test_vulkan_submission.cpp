/* Exercise asynchronous production submission and staging ownership without a GPU/SDK. */
#include "cubism_vulkan_submission.hpp"
#include "test.h"
#include <map>
#include <stdexcept>
int bongo_cat_test_failures;
namespace {
uintptr_t next_fence = 1, next_command = 100;
unsigned submissions, waits, freed_buffers, live_commands, live_fences, live_caches;
std::map<uintptr_t, bool> pending;
std::map<uintptr_t, uintptr_t> command_fence, buffer_fence;
VkFence last_fence;
VkResult submit_result = VK_SUCCESS, wait_result = VK_SUCCESS;
template<class T> uintptr_t id(T handle) { return (uintptr_t)handle; }
}
extern "C" {
VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice, const VkFenceCreateInfo *,
    const VkAllocationCallbacks *, VkFence *out) {
    *out = (VkFence)next_fence++; live_fences++; return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice, VkFence f, const VkAllocationCallbacks *) {
    CHECK(!pending[id(f)]); live_fences--;
}
VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice, uint32_t count, const VkFence *f) {
    CHECK(count == 1 && !pending[id(*f)]); return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t count,
    const VkFence *f, VkBool32 all, uint64_t timeout) {
    CHECK(count == 1 && all == VK_TRUE);
    if (!timeout) return pending[id(*f)] ? VK_TIMEOUT : VK_SUCCESS;
    waits++;
    if (wait_result == VK_SUCCESS) pending[id(*f)] = false;
    return wait_result;
}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue, uint32_t count,
    const VkSubmitInfo *info, VkFence f) {
    CHECK(!pending[id(f)]);
    if (submit_result != VK_SUCCESS) return submit_result;
    for (uint32_t i = 0; i < count; ++i)
        for (uint32_t j = 0; j < info[i].commandBufferCount; ++j)
            command_fence[id(info[i].pCommandBuffers[j])] = id(f);
    pending[id(f)] = true; last_fence = f; submissions++; return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice) {
    for (auto &item : pending) item.second = false;
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice,
    const VkCommandBufferAllocateInfo *info, VkCommandBuffer *out) {
    for (uint32_t i = 0; i < info->commandBufferCount; ++i) out[i] = (VkCommandBuffer)next_command++;
    live_commands += info->commandBufferCount; return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice, VkCommandPool,
    uint32_t count, const VkCommandBuffer *commands) {
    for (uint32_t i = 0; i < count; ++i) {
        CHECK(!pending[command_fence[id(commands[i])]]);
        command_fence.erase(id(commands[i]));
    }
    live_commands -= count;
}
VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer command,
    const VkCommandBufferBeginInfo *) {
    CHECK(!pending[command_fence[id(command)]]); return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer) { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks *) {}
VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice, VkDeviceMemory, const VkAllocationCallbacks *) {}
VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineCache(VkDevice, const VkPipelineCacheCreateInfo *,
    const VkAllocationCallbacks *, VkPipelineCache *out) { *out = (VkPipelineCache)(uintptr_t)99; live_caches++; return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineCache(VkDevice, VkPipelineCache cache, const VkAllocationCallbacks *) {
    CHECK(cache != VK_NULL_HANDLE && live_caches == 1); live_caches--;
}
VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline) {}
VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint,
    VkPipelineLayout, uint32_t, uint32_t, const VkDescriptorSet *, uint32_t, const uint32_t *) {}
VKAPI_ATTR void VKAPI_CALL vkCmdSetCullMode(VkCommandBuffer, VkCullModeFlags) {}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice, VkPipelineCache, uint32_t,
    const VkGraphicsPipelineCreateInfo *, const VkAllocationCallbacks *, VkPipeline *) { return VK_SUCCESS; }
}
namespace bongo_cat {
bool vulkan_memory_prefers_device() { return false; }
bool vulkan_buffer_destroy(VkBuffer buffer) {
    CHECK(!pending[buffer_fence[id(buffer)]]);
    freed_buffers++; return true;
}
}
static void draw() {
    VkCommandBuffer update = (VkCommandBuffer)(uintptr_t)10, paint = (VkCommandBuffer)(uintptr_t)11;
    VkSemaphore signal = (VkSemaphore)(uintptr_t)12;
    unsigned before = waits, submits = submissions;
    bongo_cat::vulkan_submit(update, signal, VK_NULL_HANDLE);
    CHECK(submissions == submits);
    bongo_cat::vulkan_submit(paint, VK_NULL_HANDLE, signal);
    CHECK(submissions == submits + 1 && waits == before && pending[id(last_fence)]);
    VkCommandBufferBeginInfo begin{}; begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CHECK(bongo_cat::vulkan_begin_commands(update, &begin) == VK_SUCCESS);
    CHECK(waits == before + 1 && !pending[id(last_fence)]);
}
static void uploads() {
    unsigned before = waits, freed = freed_buffers;
    for (uintptr_t i = 0; i < 5; ++i) {
        auto command = bongo_cat::vulkan_upload_begin();
        unsigned at_submit = waits;
        bongo_cat::vulkan_submit(command, VK_NULL_HANDLE, VK_NULL_HANDLE);
        CHECK(waits == at_submit); // Backpressure was handled at slot acquisition.
        auto buffer = (VkBuffer)(uintptr_t)(200 + i);
        buffer_fence[id(buffer)] = id(last_fence);
        CHECK(bongo_cat::vulkan_upload_defer_buffer(buffer, VK_NULL_HANDLE));
        CHECK(freed_buffers == freed + (i >= 2 ? i - 1 : 0));
        CHECK(live_commands <= 2 && live_fences == 3);
    }
    CHECK(waits == before + 3);
    bongo_cat::vulkan_submit_wait();
    CHECK(freed_buffers == freed + 5 && live_commands == 0);
}
static void batch_and_cancel() {
    bongo_cat::vulkan_upload_batch_begin();
    unsigned before = waits, freed = freed_buffers;
    for (uintptr_t i = 0; i < 32; ++i) {
        auto command = bongo_cat::vulkan_upload_begin();
        bongo_cat::vulkan_submit(command, VK_NULL_HANDLE, VK_NULL_HANDLE);
        auto buffer = (VkBuffer)(uintptr_t)(300 + i);
        CHECK(bongo_cat::vulkan_upload_defer_buffer(buffer, VK_NULL_HANDLE));
    }
    for (uintptr_t i = 0; i < 32; ++i) buffer_fence[300 + i] = id(last_fence);
    CHECK(waits == before && freed_buffers == freed && live_commands == 32);
    bongo_cat::vulkan_upload_batch_end();
    CHECK(waits == before); // Batch end returns while its resources remain in use.
    bongo_cat::vulkan_submit_wait();
    CHECK(freed_buffers == freed + 32 && live_commands == 0);
    bongo_cat::vulkan_upload_batch_begin();
    auto command = bongo_cat::vulkan_upload_begin();
    bongo_cat::vulkan_submit(command, VK_NULL_HANDLE, VK_NULL_HANDLE);
    unsigned submits = submissions;
    bongo_cat::vulkan_submit_cancel();
    CHECK(submissions == submits && live_commands == 0); // Abandoned work is never queued.
}
static void failures() {
    auto command = bongo_cat::vulkan_upload_begin();
    submit_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    bool rejected = false;
    try { bongo_cat::vulkan_submit(command, VK_NULL_HANDLE, VK_NULL_HANDLE); }
    catch (const std::runtime_error &) { rejected = true; }
    CHECK(rejected);
    unsigned before = waits;
    bongo_cat::vulkan_submit_cancel();
    CHECK(waits == before && live_commands == 0);
    submit_result = VK_SUCCESS;
    command = bongo_cat::vulkan_upload_begin();
    bongo_cat::vulkan_submit(command, VK_NULL_HANDLE, VK_NULL_HANDLE);
    auto buffer = (VkBuffer)(uintptr_t)400;
    buffer_fence[id(buffer)] = id(last_fence);
    CHECK(bongo_cat::vulkan_upload_defer_buffer(buffer, VK_NULL_HANDLE));
    unsigned freed = freed_buffers;
    wait_result = VK_ERROR_DEVICE_LOST;
    rejected = false;
    try { bongo_cat::vulkan_submit_wait(); }
    catch (const std::runtime_error &) { rejected = true; }
    CHECK(rejected && freed_buffers == freed && live_commands == 1);
    wait_result = VK_SUCCESS;
    bongo_cat::vulkan_submit_wait();
    CHECK(freed_buffers == freed + 1 && live_commands == 0);
}
int main() {
    bongo_cat::vulkan_submit_initialize((VkDevice)(uintptr_t)1,
        (VkQueue)(uintptr_t)2, (VkCommandPool)(uintptr_t)3);
    CHECK(live_fences == 3 && live_caches == 1);
    // Disabling pipeline cache retention drops only the CPU cache, preserving draw fences.
    bongo_cat::vulkan_submit_initialize((VkDevice)(uintptr_t)1,
        (VkQueue)(uintptr_t)2, (VkCommandPool)(uintptr_t)3, false);
    CHECK(live_fences == 3 && live_caches == 0);
    draw(); uploads(); batch_and_cancel(); failures();
    bongo_cat::vulkan_submit_release();
    CHECK(!live_fences && !live_commands && !live_caches);
    bongo_cat::vulkan_submit_initialize((VkDevice)(uintptr_t)1,
        (VkQueue)(uintptr_t)2, (VkCommandPool)(uintptr_t)3, false);
    CHECK(live_fences == 3 && !live_caches);
    bongo_cat::vulkan_submit_release();
    CHECK(!live_fences && !live_caches);
    return bongo_cat_test_failures ? 1 : 0;
}
