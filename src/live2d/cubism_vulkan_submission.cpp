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
bool draw_in_flight;
struct DeferredBuffer { VkBuffer buffer; VkDeviceMemory memory; };
struct UploadSlot {
    VkFence fence = VK_NULL_HANDLE;
    bool in_flight = false;
    std::vector<VkCommandBuffer> commands;
    std::vector<DeferredBuffer> buffers;
};
std::array<UploadSlot, 2> upload_slots;
size_t next_upload;
int last_upload = -1;
VkPipelineCache pipeline_cache;
VkCommandBuffer pending;
VkSemaphore pending_signal;
bool batch, batch_failed;
VkDeviceSize upload_bytes;
std::vector<VkCommandBuffer> uploads;
std::vector<DeferredBuffer> deferred;
void retire_buffers(std::vector<DeferredBuffer> &buffers) {
    for (const auto &item : buffers) {
        if (!vulkan_buffer_destroy(item.buffer)) {
            vkDestroyBuffer(device, item.buffer, nullptr);
            vkFreeMemory(device, item.memory, nullptr);
        }
    }
    buffers.clear();
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
void wait_draw() {
    if (!draw_in_flight) return;
    checked(vkWaitForFences(device, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
    draw_in_flight = false;
}
void wait_upload(UploadSlot &slot) {
    if (slot.in_flight) {
        checked(vkWaitForFences(device, 1, &slot.fence, VK_TRUE,
            std::numeric_limits<uint64_t>::max()));
        slot.in_flight = false;
    }
    if (!slot.commands.empty()) vkFreeCommandBuffers(device, pool,
        (uint32_t)slot.commands.size(), slot.commands.data());
    slot.commands.clear();
    retire_buffers(slot.buffers);
}
void submit_uploads() {
    auto &slot = upload_slots[next_upload];
    wait_upload(slot);
    VkSubmitInfo info{}; info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = (uint32_t)uploads.size(); info.pCommandBuffers = uploads.data();
    checked(vkResetFences(device, 1, &slot.fence));
    checked(vkQueueSubmit(queue, 1, &info, slot.fence));
    // Moving vectors transfers ownership without allocating after GPU submission.
    slot.commands.swap(uploads); slot.buffers.swap(deferred);
    slot.in_flight = true;
    last_upload = (int)next_upload;
    next_upload = (next_upload + 1) % upload_slots.size();
}
}
void vulkan_submit_release() {
    if (device) {
        vkDeviceWaitIdle(device);
        draw_in_flight = false;
        for (auto &slot : upload_slots) slot.in_flight = false;
        vulkan_submit_cancel();
        for (auto &slot : upload_slots) {
            slot.in_flight = false;
            wait_upload(slot);
            if (slot.fence) vkDestroyFence(device, slot.fence, nullptr);
            slot.fence = VK_NULL_HANDLE;
        }
        if (fence) vkDestroyFence(device, fence, nullptr);
        if (pipeline_cache) vkDestroyPipelineCache(device, pipeline_cache, nullptr);
    }
    device = VK_NULL_HANDLE; fence = VK_NULL_HANDLE; pipeline_cache = VK_NULL_HANDLE;
    pending = VK_NULL_HANDLE; pending_signal = VK_NULL_HANDLE;
    batch = false; draw_in_flight = false; next_upload = 0; last_upload = -1;
    uploads.clear(); state = {};
}
void vulkan_submit_initialize(VkDevice value, VkQueue graphics, VkCommandPool commands, bool retain_pipeline_cache) {
    // Pipeline caches serve CPU compilation only; ordinary GPU draws do not use them.
    if (!retain_pipeline_cache && pipeline_cache) {
        vkDestroyPipelineCache(device, pipeline_cache, nullptr);
        pipeline_cache = VK_NULL_HANDLE;
    }
    if (device == value && fence && upload_slots[0].fence && upload_slots[1].fence &&
        queue == graphics && pool == commands) return;
    vulkan_submit_release();
    device = value; queue = graphics; pool = commands;
    VkFenceCreateInfo create{}; create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    checked(vkCreateFence(device, &create, nullptr, &fence));
    for (auto &slot : upload_slots)
        checked(vkCreateFence(device, &create, nullptr, &slot.fence));
    VkPipelineCacheCreateInfo cache{}; cache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    // Cache creation is optional; pipeline creation retains the uncached fallback.
    if (retain_pipeline_cache &&
        vkCreatePipelineCache(device, &cache, nullptr, &pipeline_cache) != VK_SUCCESS)
        pipeline_cache = VK_NULL_HANDLE;
}
void vulkan_draw_wait() {
    wait_draw();
    // Release completed staging even when no further upload reuses its slot.
    for (auto &slot : upload_slots) if (slot.in_flight) {
        const auto result = vkWaitForFences(device, 1, &slot.fence, VK_TRUE, 0);
        if (result == VK_SUCCESS) wait_upload(slot);
        else if (result != VK_TIMEOUT) checked(result);
    }
}
void vulkan_submit_wait() {
    wait_draw();
    for (auto &slot : upload_slots) wait_upload(slot);
    last_upload = -1;
}
void vulkan_submit_cancel() {
    // Cancellation may be followed by image destruction: drain submitted work.
    vulkan_submit_wait();
    pending = VK_NULL_HANDLE; pending_signal = VK_NULL_HANDLE;
    // Batch commands have not been submitted; safe to free after cancellation.
    if (!uploads.empty()) vkFreeCommandBuffers(device, pool,
        (uint32_t)uploads.size(), uploads.data());
    uploads.clear(); batch = false; upload_bytes = 0; retire_buffers(deferred);
}
VkCommandBuffer vulkan_upload_begin() {
    last_upload = -1;
    if (uploads.empty()) wait_upload(upload_slots[next_upload]);
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
    const bool compact = vulkan_memory_prefers_device();
    if (!batch) {
        if (compact) vulkan_submit_wait();
        else wait_upload(upload_slots[next_upload]);
        return;
    }
    const VkDeviceSize budget = compact ? 1024ull * 1024 : 16ull * 1024 * 1024;
    if (upload_bytes && (bytes > budget || upload_bytes > budget - bytes)) {
        vulkan_upload_batch_end(); batch = true;
    }
    if (!upload_bytes) {
        if (compact) vulkan_submit_wait();
        else wait_upload(upload_slots[next_upload]);
    }
    upload_bytes += bytes;
}
void vulkan_upload_batch_begin() {
    if (batch || pending) throw std::runtime_error("Nested Live2D upload batch");
    vulkan_submit_wait();
    batch_failed = false; batch = true;
}
void vulkan_upload_batch_end() {
    if (batch_failed) throw std::runtime_error("Cannot retain Live2D upload staging storage");
    batch = false; upload_bytes = 0;
    if (uploads.empty()) { retire_buffers(deferred); return; }
    submit_uploads(); // Fence completion is checked only when this slot is reused.
}
bool vulkan_upload_defer_buffer(VkBuffer buffer, VkDeviceMemory memory) noexcept {
    if (last_upload >= 0 && !upload_slots[(size_t)last_upload].in_flight) last_upload = -1;
    if (!buffer || (!batch && last_upload < 0)) return false;
    try {
        if (last_upload >= 0)
            upload_slots[(size_t)last_upload].buffers.push_back({buffer, memory});
        else deferred.push_back({buffer, memory});
        return true;
    } catch (...) {
        // Exceptional allocation failure must not free a submitted staging buffer.
        try { vulkan_submit_cancel(); }
        catch (...) { vkDeviceWaitIdle(device); }
        batch_failed = true;
        return false;
    }
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
    if (!wait) {
        try { uploads.push_back(command); }
        catch (...) { vkFreeCommandBuffers(device, pool, 1, &command); throw; }
        submit_uploads();
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
    wait_draw();
    checked(vkResetFences(device, 1, &fence));
    checked(vkQueueSubmit(queue, count, infos, fence));
    draw_in_flight = true;
    pending = VK_NULL_HANDLE; pending_signal = VK_NULL_HANDLE;
    last_upload = -1;
}
VkResult vulkan_begin_commands(VkCommandBuffer command, const VkCommandBufferBeginInfo *info) {
    wait_draw(); // SDK reuses its small backing set within multi-pass draws.
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
