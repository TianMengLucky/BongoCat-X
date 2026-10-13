/* Two independently fenced upload slots own staging until GPU completion. */
#include "rhi_vk_internal.h"
#include <string.h>
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))
static void release_staging(BongoCatRhiVk *vk, BongoCatRhiVkUpload *slot) {
    if (slot->buffer) vk->vkDestroyBuffer(vk->device, slot->buffer, NULL);
    if (slot->memory) vk->vkFreeMemory(vk->device, slot->memory, NULL);
    slot->buffer = VK_NULL_HANDLE;
    slot->memory = VK_NULL_HANDLE;
}
bool bongo_cat_rhi_vk_collect_uploads(BongoCatRhiVk *vk) {
    if (vk->failed) return false;
    for (unsigned i = 0; i < BONGO_CAT_VK_FRAMES; ++i) {
        BongoCatRhiVkUpload *slot = &vk->uploads[i];
        if (!slot->pending) continue;
        VkResult result = vk->vkWaitForFences(vk->device, 1, &slot->fence, VK_TRUE, 0);
        if (result == VK_TIMEOUT) continue;
        if (result != VK_SUCCESS) { vk->failed = true; return false; }
        slot->pending = false;
        release_staging(vk, slot);
    }
    return true;
}
BongoCatRhiVkUpload *bongo_cat_rhi_vk_begin_upload(BongoCatRhiVk *vk) {
    if (!bongo_cat_rhi_vk_collect_uploads(vk)) return NULL;
    BongoCatRhiVkUpload *slot = &vk->uploads[vk->upload_slot];
    if (slot->pending) {
        if (vk->vkWaitForFences(vk->device, 1, &slot->fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            vk->failed = true;
            return NULL;
        }
        slot->pending = false;
        release_staging(vk, slot);
    }
    if (!slot->pool) {
        VkCommandPoolCreateInfo pool = {0};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.queueFamilyIndex = vk->queue_family;
        pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        if (vk->vkCreateCommandPool(vk->device, &pool, NULL, &slot->pool) != VK_SUCCESS)
            return NULL;
    }
    if (!slot->command) {
        VkCommandBufferAllocateInfo commands = {0};
        commands.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commands.commandPool = slot->pool;
        commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commands.commandBufferCount = 1;
        if (vk->vkAllocateCommandBuffers(vk->device, &commands, &slot->command) != VK_SUCCESS)
            return NULL;
    }
    if (!slot->fence) {
        VkFenceCreateInfo fence = {0}; fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vk->vkCreateFence(vk->device, &fence, NULL, &slot->fence) != VK_SUCCESS) return NULL;
    }
    if (vk->vkResetCommandPool(vk->device, slot->pool,
            VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS) return NULL;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    return vk->vkBeginCommandBuffer(slot->command, &begin) == VK_SUCCESS ? slot : NULL;
}
bool bongo_cat_rhi_vk_submit_upload(BongoCatRhiVk *vk, BongoCatRhiVkUpload *slot,
    VkBuffer buffer, VkDeviceMemory memory) {
    if (vk->failed || slot->pending) return false;
    VkSubmitInfo submit = {0}; submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1; submit.pCommandBuffers = &slot->command;
    VkResult result = vk->vkEndCommandBuffer(slot->command);
    if (result == VK_SUCCESS) result = vk->vkResetFences(vk->device, 1, &slot->fence);
    if (result == VK_SUCCESS) result = vk->vkQueueSubmit(vk->queue, 1, &submit, slot->fence);
    if (result != VK_SUCCESS) {
        vk->failed = true;
        return false;
    }
    slot->buffer = buffer; slot->memory = memory; slot->pending = true;
    vk->upload_slot = (vk->upload_slot + 1) % BONGO_CAT_VK_FRAMES;
    return true; // Ownership transferred; do not wait here.
}
void bongo_cat_rhi_vk_destroy_uploads(BongoCatRhiVk *vk) {
    /* Device was waited at teardown. Also handles incomplete initialization. */
    for (unsigned i = 0; i < BONGO_CAT_VK_FRAMES; ++i) {
        BongoCatRhiVkUpload *slot = &vk->uploads[i];
        release_staging(vk, slot);
        if (slot->fence) vk->vkDestroyFence(vk->device, slot->fence, NULL);
        if (slot->pool) vk->vkDestroyCommandPool(vk->device, slot->pool, NULL);
        memset(slot, 0, sizeof(*slot));
    }
}
#endif
