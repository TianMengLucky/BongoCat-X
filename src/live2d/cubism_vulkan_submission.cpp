#include "cubism_vulkan_submission.hpp"
#include "cubism_vulkan_memory.hpp"
#include <array>
#include <limits>
#include <stdexcept>
#include <vector>
namespace bongo_cat {
namespace {
VkDevice device;
VkQueue queue;
VkCommandPool pool;
VkFence fence;
VkPipelineCache pipeline_cache;
VkCommandBuffer pending;
VkSemaphore pending_signal;
bool batch, batch_failed;
VkDeviceSize upload_bytes;
std::vector<VkCommandBuffer> uploads;
struct DeferredBuffer { VkBuffer buffer; VkDeviceMemory memory; };
std::vector<DeferredBuffer> deferred;
void retire_buffers() {
    for (const auto &item : deferred) {
        if (!vulkan_buffer_destroy(item.buffer)) {
            vkDestroyBuffer(device, item.buffer, nullptr);
            vkFreeMemory(device, item.memory, nullptr);
        }
    }
    deferred.clear();
}
void checked(VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Live2D Vulkan submission failed");
}
struct State {
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineBindPoint point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> sets{};
    bool cull_valid = false;
    VkCullModeFlags cull = VK_CULL_MODE_NONE;
};
thread_local State state;
void select(VkCommandBuffer command) {
    if (state.command != command) { state = {}; state.command = command; }
}
void submit_wait(uint32_t count, const VkSubmitInfo *info) {
    checked(vkResetFences(device, 1, &fence));
    checked(vkQueueSubmit(queue, count, info, fence));
    const VkResult result = vkWaitForFences(device, 1, &fence, VK_TRUE,
        std::numeric_limits<uint64_t>::max());
    if (result != VK_SUCCESS) vkQueueWaitIdle(queue);
    checked(result);
}
}
void vulkan_submit_release() {
    if (device) {
        vkDeviceWaitIdle(device);
        vulkan_submit_cancel();
        if (fence) vkDestroyFence(device, fence, nullptr);
        if (pipeline_cache) vkDestroyPipelineCache(device, pipeline_cache, nullptr);
    }
    device = VK_NULL_HANDLE; fence = VK_NULL_HANDLE; pipeline_cache = VK_NULL_HANDLE;
    pending = VK_NULL_HANDLE; pending_signal = VK_NULL_HANDLE;
    batch = false; uploads.clear(); state = {};
}
void vulkan_submit_initialize(VkDevice value, VkQueue graphics, VkCommandPool commands) {
    if (device == value && fence && queue == graphics && pool == commands) return;
    vulkan_submit_release();
    device = value; queue = graphics; pool = commands;
    VkFenceCreateInfo create{}; create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    checked(vkCreateFence(device, &create, nullptr, &fence));
    VkPipelineCacheCreateInfo cache{}; cache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    // Cache creation is optional; pipeline creation retains the uncached fallback.
    if (vkCreatePipelineCache(device, &cache, nullptr, &pipeline_cache) != VK_SUCCESS)
        pipeline_cache = VK_NULL_HANDLE;
}
void vulkan_submit_cancel() {
    pending = VK_NULL_HANDLE; pending_signal = VK_NULL_HANDLE;
    // Batch commands have not been submitted; safe to free after cancellation.
    if (!uploads.empty()) vkFreeCommandBuffers(device, pool,
        (uint32_t)uploads.size(), uploads.data());
    uploads.clear(); batch = false; upload_bytes = 0; retire_buffers();
}
VkCommandBuffer vulkan_upload_begin() {
    VkCommandBufferAllocateInfo allocate{};
    allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate.commandPool = pool; allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    checked(vkAllocateCommandBuffers(device, &allocate, &command));
    VkCommandBufferBeginInfo begin{}; begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    const auto result = vkBeginCommandBuffer(command, &begin);
    if (result != VK_SUCCESS) { vkFreeCommandBuffers(device, pool, 1, &command); checked(result); }
    return command;
}
void vulkan_upload_budget(VkDeviceSize bytes) {
    if (!batch) return;
    if (upload_bytes && (bytes > 16ull * 1024 * 1024 ||
        upload_bytes > 16ull * 1024 * 1024 - bytes)) {
        vulkan_upload_batch_end(); batch = true;
    }
    upload_bytes += bytes;
}
void vulkan_upload_batch_begin() {
    if (batch || pending) throw std::runtime_error("Nested Live2D upload batch");
    batch_failed = false; batch = true;
}
void vulkan_upload_batch_end() {
    if (batch_failed) throw std::runtime_error("Cannot retain Live2D upload staging storage");
    batch = false; upload_bytes = 0;
    if (uploads.empty()) { retire_buffers(); return; }
    VkSubmitInfo info{}; info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = (uint32_t)uploads.size(); info.pCommandBuffers = uploads.data();
    try { submit_wait(1, &info); }
    catch (...) { vkQueueWaitIdle(queue); vulkan_submit_cancel(); throw; }
    vkFreeCommandBuffers(device, pool, (uint32_t)uploads.size(), uploads.data());
    uploads.clear(); retire_buffers();
}
bool vulkan_upload_defer_buffer(VkBuffer buffer, VkDeviceMemory memory) noexcept {
    if (!batch || !buffer) return false;
    try { deferred.push_back({buffer, memory}); return true; }
    catch (...) { vulkan_submit_cancel(); batch_failed = true; return false; }
}
void vulkan_submit(VkCommandBuffer command, VkSemaphore signal, VkSemaphore wait) {
    const auto ended = vkEndCommandBuffer(command);
    if (ended != VK_SUCCESS) {
        if (!signal && !wait) vkFreeCommandBuffers(device, pool, 1, &command);
        checked(ended);
    }
    if (signal) {
        if (pending) throw std::runtime_error("Unconsumed Live2D update submission");
        pending = command; pending_signal = signal;
        return;
    }
    if (batch && !wait) {
        try { uploads.push_back(command); }
        catch (...) { vkFreeCommandBuffers(device, pool, 1, &command); throw; }
        if (uploads.size() >= 32) { vulkan_upload_batch_end(); batch = true; }
        return;
    }
    VkSubmitInfo infos[2]{};
    infos[0].sType = infos[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    infos[0].commandBufferCount = 1; infos[0].pCommandBuffers = &command;
    VkPipelineStageFlags stages = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    uint32_t count = 1;
    if (wait) {
        if (!pending || pending_signal != wait) throw std::runtime_error("Missing Live2D update submission");
        infos[0].pCommandBuffers = &pending;
        infos[0].signalSemaphoreCount = 1; infos[0].pSignalSemaphores = &pending_signal;
        infos[1].commandBufferCount = 1; infos[1].pCommandBuffers = &command;
        infos[1].waitSemaphoreCount = 1; infos[1].pWaitSemaphores = &wait;
        infos[1].pWaitDstStageMask = &stages;
        count = 2;
    }
    try { submit_wait(count, infos); }
    catch (...) {
        vkQueueWaitIdle(queue); pending = VK_NULL_HANDLE;
        if (!wait) vkFreeCommandBuffers(device, pool, 1, &command);
        throw;
    }
    pending = VK_NULL_HANDLE; pending_signal = VK_NULL_HANDLE;
    if (!wait) vkFreeCommandBuffers(device, pool, 1, &command);
}
VkResult vulkan_begin_commands(VkCommandBuffer command, const VkCommandBufferBeginInfo *info) {
    state = {}; state.command = command;
    return vkBeginCommandBuffer(command, info);
}
void vulkan_bind_pipeline(VkCommandBuffer command, VkPipelineBindPoint point, VkPipeline pipeline) {
    select(command);
    if (state.pipeline == pipeline && state.point == point) return;
    vkCmdBindPipeline(command, point, pipeline);
    state.pipeline = pipeline; state.point = point;
}
void vulkan_bind_descriptors(VkCommandBuffer command, VkPipelineBindPoint point,
    VkPipelineLayout layout, uint32_t first, uint32_t count, const VkDescriptorSet *sets,
    uint32_t dynamic_count, const uint32_t *dynamic_offsets) {
    select(command);
    const bool cacheable = point == VK_PIPELINE_BIND_POINT_GRAPHICS && !dynamic_count &&
        count == 1 && first < state.sets.size();
    if (state.layout != layout) { state.layout = layout; state.sets = {}; }
    if (cacheable && state.sets[first] == sets[0]) return;
    vkCmdBindDescriptorSets(command, point, layout, first, count, sets, dynamic_count, dynamic_offsets);
    if (cacheable) state.sets[first] = sets[0];
    else state.sets = {};
}
void vulkan_set_cull(VkCommandBuffer command, VkCullModeFlags flags) {
    select(command);
    if (state.cull_valid && state.cull == flags) return;
    vkCmdSetCullMode(command, flags); state.cull = flags; state.cull_valid = true;
}
VkResult vulkan_create_pipelines(VkDevice value, VkPipelineCache fallback, uint32_t count,
    const VkGraphicsPipelineCreateInfo *info, const VkAllocationCallbacks *callbacks, VkPipeline *pipelines) {
    return vkCreateGraphicsPipelines(value, device == value && pipeline_cache ? pipeline_cache : fallback,
        count, info, callbacks, pipelines);
}
}
