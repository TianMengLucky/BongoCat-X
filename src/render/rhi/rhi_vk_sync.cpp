/* Fixed frame slots and fence checkpoints; no retirement list or frame IDs. */
#include "rhi_vk_internal.h"
#include <string.h>
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

bool bongo_cat_rhi_vk_background_matches(const BongoCatRhi *rhi, int width, int height,
    uint64_t revision) {
    const BongoCatRhiVk *vk=rhi ? static_cast<BongoCatRhiVk *>(rhi->impl) : nullptr;
    return vk && !vk->failed && vk->background_ready && vk->background &&
        width > 0 && height > 0 && (uint32_t)width == vk->extent.width &&
        (uint32_t)height == vk->extent.height && revision == vk->background_revision;
}

bool bongo_cat_rhi_vk_release_idle_memory(BongoCatRhi *rhi) {
    BongoCatRhiVk *vk = rhi ? static_cast<BongoCatRhiVk *>(rhi->impl) : nullptr;
    if (!vk || !vk->device || vk->failed) return false;
    /* A WSI frame must still consume its presentation semaphore normally. */
    if (vk->frame_ready && !vk->layered_target) return false;
    bool allocated = false;
    for (unsigned i = 0; i < BONGO_CAT_VK_FRAMES; ++i) {
        allocated |= vk->readbacks[i].readback_memory != VK_NULL_HANDLE;
        allocated |= vk->uploads[i].pool != VK_NULL_HANDLE ||
            vk->uploads[i].buffer != VK_NULL_HANDLE;
    }
    if (!allocated) return true; /* No repeated queue waits while hidden. */
    for (unsigned i = 0; i < BONGO_CAT_VK_FRAMES; ++i)
        if (!bongo_cat_rhi_vk_wait_frame(vk, &vk->frames[i])) return false;
    /* Foreground uploads can also reference the readback buffers. Fence all
       earlier queue work, including plugin submissions, before destruction. */
    if (!bongo_cat_rhi_vk_queue_checkpoint(vk)) return false;
    if (vk->frame_ready) {
        vk->frame_ready = false;
        vk->current_image = VK_NULL_HANDLE;
        vk->current_view = VK_NULL_HANDLE;
        vk->frame_slot = (vk->frame_slot + 1) % BONGO_CAT_VK_FRAMES;
    }
    bongo_cat_rhi_vk_release_readback(vk);
    // The checkpoint covers texture uploads too. Release staging and empty
    // command pools now; a hidden model may not submit another frame.
    bongo_cat_rhi_vk_destroy_uploads(vk);
    return true;
}

bool bongo_cat_rhi_vk_create_frames(BongoCatRhiVk *vk) {
    for (unsigned i = 0; i < BONGO_CAT_VK_FRAMES; ++i) {
        BongoCatRhiVkFrame *frame = &vk->frames[i];
        VkCommandPoolCreateInfo pool = {};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.queueFamilyIndex = vk->queue_family;
        pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vk->vkCreateCommandPool(vk->device, &pool, NULL, &frame->pool) != VK_SUCCESS)
            return false;
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = frame->pool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = 2;
        VkCommandBuffer commands[2];
        if (vk->vkAllocateCommandBuffers(vk->device, &info, commands) != VK_SUCCESS)
            return false;
        frame->initialize = commands[0];
        frame->finish = commands[1];
        VkFenceCreateInfo fence = {};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vk->vkCreateFence(vk->device, &fence, NULL, &frame->fence) != VK_SUCCESS)
            return false;
    }
    return true;
}

/* Caller has waited for the device at teardown, including partial startup. */
void bongo_cat_rhi_vk_destroy_frames(BongoCatRhiVk *vk) {
    bongo_cat_rhi_vk_release_readback(vk);
    for (unsigned i = 0; i < BONGO_CAT_VK_FRAMES; ++i) {
        BongoCatRhiVkFrame *frame = &vk->frames[i];
        if (frame->fence) vk->vkDestroyFence(vk->device, frame->fence, NULL);
        if (frame->pool) vk->vkDestroyCommandPool(vk->device, frame->pool, NULL);
        *frame = {};
    }
}

BongoCatRhiAsyncState bongo_cat_rhi_vk_async_frame_state(const BongoCatRhi *rhi) {
    BongoCatRhiVk *vk = rhi ? static_cast<BongoCatRhiVk *>(rhi->impl) : nullptr;
    if (!vk || !vk->layered_target) return BONGO_CAT_RHI_ASYNC_DISABLED;
    if (vk->failed) return BONGO_CAT_RHI_ASYNC_FAILED;
    if (!vk->frame_ready) return BONGO_CAT_RHI_ASYNC_IDLE;
    BongoCatRhiVkFrame *frame = &vk->frames[vk->readback_frame];
    if (!frame->pending) return BONGO_CAT_RHI_ASYNC_READY;
    VkResult result = vk->vkWaitForFences(vk->device, 1, &frame->fence, VK_TRUE, 0);
    if (result == VK_TIMEOUT) return BONGO_CAT_RHI_ASYNC_PENDING;
    if (result != VK_SUCCESS) { vk->failed = true; return BONGO_CAT_RHI_ASYNC_FAILED; }
    frame->pending = false;
    return BONGO_CAT_RHI_ASYNC_READY;
}

bool bongo_cat_rhi_vk_wait_frame(BongoCatRhiVk *vk, BongoCatRhiVkFrame *frame) {
    if (vk->failed) return false;
    if (!frame->pending) return true;
    VkResult result = vk->vkWaitForFences(vk->device, 1, &frame->fence,
        VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        vk->failed = true; /* Never reset/free resources whose completion is unknown. */
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Vulkan frame wait failed: %d", (int)result);
        return false;
    }
    frame->pending = false;
    return true;
}

/* Empty submission fences all earlier work on this queue, including plugin
   submissions. Needed only before changing shared resources/error recovery. */
bool bongo_cat_rhi_vk_queue_checkpoint(BongoCatRhiVk *vk) {
    if (vk->failed) return false;
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (!vk->submit_fence && vk->vkCreateFence(vk->device, &info, NULL,
            &vk->submit_fence) != VK_SUCCESS) return false;
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkResult result = vk->vkResetFences(vk->device, 1, &vk->submit_fence);
    if (result == VK_SUCCESS)
        result = vk->vkQueueSubmit(vk->queue, 1, &submit, vk->submit_fence);
    if (result == VK_SUCCESS)
        result = vk->vkWaitForFences(vk->device, 1, &vk->submit_fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        vk->failed = true;
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Vulkan queue checkpoint failed: %d", (int)result);
    }
    return result == VK_SUCCESS;
}

bool bongo_cat_rhi_vk_submit_frame(BongoCatRhiVk *vk, VkCommandBuffer command) {
    if (vk->failed) return false;
    BongoCatRhiVkFrame *frame = &vk->frames[vk->frame_slot];
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    /* Presentation owns this semaphore until this same image is acquired
       again. A frame fence alone does not prove presentation consumed it. */
    if (!vk->layered_target) {
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &vk->present_semaphores[vk->acquired_index];
    }
    VkResult result = vk->vkEndCommandBuffer(command);
    if (result == VK_SUCCESS)
        result = vk->vkResetFences(vk->device, 1, &frame->fence);
    if (result == VK_SUCCESS)
        result = vk->vkQueueSubmit(vk->queue, 1, &submit, frame->fence);
    if (result != VK_SUCCESS) {
        vk->failed = true;
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Vulkan frame submission failed: %d", (int)result);
        return false;
    }
    frame->pending = true; /* Only successfully submitted fences may be waited. */
    return true;
}
#endif
