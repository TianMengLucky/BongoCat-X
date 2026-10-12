/* Vulkan frame drawing, readback and presentation. The RHI acquires an
   image, calls the Cubism hook, then hands real pixels to the presenter. */
#include "rhi_vk_internal.h"
#include <stdlib.h>

#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

/* Failure recovery always clears; normal frames may copy a complete background. */
static bool initialize_frame(BongoCatRhiVk *vk, uint32_t index, bool background) {
    VkCommandBuffer command = vk->frames[vk->frame_slot].initialize;
    /* A new slot was pool-reset after its fence. Error recovery first places
       a queue checkpoint, then can reset this initialization buffer safely. */
    if (vk->vkResetCommandBuffer(command, 0) != VK_SUCCESS) return false;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vk->vkBeginCommandBuffer(command, &begin) != VK_SUCCESS)
        return false;
    if (background) {
        bongo_cat_rhi_vk_record_background(vk, command);
    } else {
        VkClearValue clear;
        clear.color = (VkClearColorValue){{
            vk->clear[0] * vk->clear[3], vk->clear[1] * vk->clear[3],
            vk->clear[2] * vk->clear[3], vk->clear[3]}};
        VkRenderPassBeginInfo pass = {0};
        pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass = vk->render_pass;
        pass.framebuffer = vk->framebuffers[index];
        pass.renderArea.extent = vk->extent;
        pass.clearValueCount = 1;
        pass.pClearValues = &clear;
        vk->vkCmdBeginRenderPass(command, &pass, VK_SUBPASS_CONTENTS_INLINE);
        vk->vkCmdEndRenderPass(command);
    }
    /* Initialization and plugin drawing use separate submissions on the same
       queue. Make the initialized color visible without a CPU completion wait. */
    VkMemoryBarrier ready = {0};
    ready.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    ready.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    ready.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vk->vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &ready, 0, NULL, 0, NULL);
    if (vk->vkEndCommandBuffer(command) != VK_SUCCESS) return false;
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    if (vk->vkQueueSubmit(vk->queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) {
        vk->failed = true;
        return false;
    }
    return true;
}

bool bongo_cat_rhi_vk_render_frame(BongoCatRhi *rhi) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk || !vk->device || (!vk->swapchain && !vk->layered_target) || vk->failed) return false;
    if (vk->frame_ready && !bongo_cat_rhi_vk_frame_present(rhi->window, vk))
        return false;
    if (!bongo_cat_rhi_vk_collect_uploads(vk)) return false;
    vk->pixels_valid = false;
    BongoCatRhiVkFrame *frame = &vk->frames[vk->frame_slot];
    if (!bongo_cat_rhi_vk_wait_frame(vk, frame) ||
        vk->vkResetCommandPool(vk->device, frame->pool,
            VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT) != VK_SUCCESS) return false;
    /* CPU acquisition remains explicit for existing plugins. Final submission
       is asynchronous; only reuse/readback waits for the slot's fence. */
    uint32_t index = 0;
    if (vk->layered_target) {
        // One color target is shared. Wait its previous writer, not merely this slot.
        unsigned previous = (vk->frame_slot + BONGO_CAT_VK_FRAMES - 1) % BONGO_CAT_VK_FRAMES;
        if (!bongo_cat_rhi_vk_wait_frame(vk, &vk->frames[previous])) return false;
    } else {
        if (vk->vkResetFences(vk->device, 1, &vk->fence) != VK_SUCCESS) return false;
        VkResult acquired = vk->vkAcquireNextImageKHR(vk->device, vk->swapchain,
            UINT64_MAX, VK_NULL_HANDLE, vk->fence, &index);
        if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
            bongo_cat_rhi_vk_recreate_swapchain(vk, (int)vk->extent.width,
                (int)vk->extent.height);
            return false;
        }
        if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
            vk->failed = true;
            return false;
        }
        if (vk->vkWaitForFences(vk->device, 1, &vk->fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
            vk->failed = true; // Acquisition completion is unknown; never reset this fence again.
            return false;
        }
    }
    vk->acquired_index = index;
    vk->current_image = vk->images[index];
    vk->current_view = vk->views[index];
    const BongoCatRhiPresentOps *ops = &vk->owner->present;
    /* Background copy and initialization share one asynchronous submission. */
    if (!initialize_frame(vk, index, vk->background_ready)) {
        vk->failed = true; // The acquired image cannot be abandoned and acquired again forever.
        return false;
    }
    bool drawn = !ops->draw_frame || ops->draw_frame(ops->hook_user);
    if (!drawn) {
        /* A failed callback may already have submitted work. Retire it before
           resetting our command buffer and clearing the acquired image. */
        if (!bongo_cat_rhi_vk_queue_checkpoint(vk) || !initialize_frame(vk, index, false)) {
            vk->failed = true;
            return false;
        }
    }
    vk->frame_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vk->frame_ready = true;
    if (!bongo_cat_rhi_vk_capture_frame(vk)) {
        /* Initialization may already be queued. Stop reusing this slot until
           teardown instead of waiting on a fence never submitted. */
        vk->failed = true;
        return false;
    }
    if (!drawn) {
        (void)bongo_cat_rhi_vk_frame_present(rhi->window, vk);
        return false;
    }
    SDL_Rect changed;
    if (bongo_cat_rhi_compose_foreground(rhi, vk->pixels, (int)vk->extent.width,
        (int)vk->extent.height, (size_t)vk->extent.width * 4,
        vk->format == VK_FORMAT_B8G8R8A8_UNORM, &changed) &&
        !bongo_cat_rhi_vk_upload_composed_frame(vk, changed)) return false;
    return true;
}

bool bongo_cat_rhi_vk_frame_present(SDL_Window *window, void *user) {
    BongoCatRhiVk *vk = user;
    if (!vk || !vk->owner) return false;
    if (!vk->frame_ready &&
        !bongo_cat_rhi_vk_render_frame((BongoCatRhi *)vk->owner)) return false;
    if (vk->failed) return false;
    if (vk->layered_target) {
        if (vk->frame_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) return false;
        vk->current_image = VK_NULL_HANDLE; vk->current_view = VK_NULL_HANDLE;
        vk->frame_ready = false;
        vk->frame_slot = (vk->frame_slot + 1) % BONGO_CAT_VK_FRAMES;
        return true;
    }
    if (vk->frame_layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) return false;
    uint32_t index = vk->acquired_index;
    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &vk->present_semaphores[index];
    present.swapchainCount = 1;
    present.pSwapchains = &vk->swapchain;
    present.pImageIndices = &index;
    VkResult presented = vk->vkQueuePresentKHR(vk->queue, &present);
    vk->current_image = VK_NULL_HANDLE;
    vk->current_view = VK_NULL_HANDLE;
    vk->frame_ready = false;
    vk->frame_slot = (vk->frame_slot + 1) % BONGO_CAT_VK_FRAMES;
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
        bongo_cat_rhi_vk_recreate_swapchain(vk, (int)vk->extent.width,
            (int)vk->extent.height);
        return false;
    }
    (void)window;
    if (presented != VK_SUCCESS) vk->failed = true;
    return presented == VK_SUCCESS;
}

void *bongo_cat_rhi_vk_begin_commands(const BongoCatRhi *rhi) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk || !vk->device || vk->failed) return NULL;
    VkCommandBufferAllocateInfo info = {0};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = vk->pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    /* Synchronous bridge submissions retire before returning. Recycle one
       command buffer without assuming callers never nest command recording. */
    VkCommandBuffer command = vk->idle_command;
    vk->idle_command = VK_NULL_HANDLE;
    if (command && vk->vkResetCommandBuffer(command, 0) != VK_SUCCESS) {
        vk->vkFreeCommandBuffers(vk->device, vk->pool, 1, &command);
        return NULL;
    }
    if (!command && vk->vkAllocateCommandBuffers(vk->device, &info, &command) !=
        VK_SUCCESS) return NULL;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vk->vkBeginCommandBuffer(command, &begin) != VK_SUCCESS) {
        vk->vkFreeCommandBuffers(vk->device, vk->pool, 1, &command);
        return NULL;
    }
    return command;
}

bool bongo_cat_rhi_vk_submit_commands(const BongoCatRhi *rhi, void *command) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    VkCommandBuffer buffer = (VkCommandBuffer)command;
    if (!vk || !buffer || vk->failed) return false;
    if (vk->vkEndCommandBuffer(buffer) != VK_SUCCESS) {
        vk->vkFreeCommandBuffers(vk->device, vk->pool, 1, &buffer);
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Cannot end Vulkan upload commands");
        return false;
    }
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &buffer;
    VkFenceCreateInfo fence = {0};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if ((!vk->submit_fence && vk->vkCreateFence(vk->device, &fence, NULL,
            &vk->submit_fence) != VK_SUCCESS) ||
        vk->vkResetFences(vk->device, 1, &vk->submit_fence) != VK_SUCCESS) {
        vk->vkFreeCommandBuffers(vk->device, vk->pool, 1, &buffer);
        return false;
    }
    VkResult submitted = vk->vkQueueSubmit(vk->queue, 1, &submit, vk->submit_fence);
    VkResult waited = submitted == VK_SUCCESS ? vk->vkWaitForFences(vk->device,
        1, &vk->submit_fence, VK_TRUE, UINT64_MAX) : submitted;
    if (waited != VK_SUCCESS) {
        vk->failed = true;
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Vulkan upload failed: %d", (int)waited);
        /* The pool owns the buffer until device teardown if completion is unknown. */
    } else if (!vk->idle_command) vk->idle_command = buffer;
    else vk->vkFreeCommandBuffers(vk->device, vk->pool, 1, &buffer);
    return waited == VK_SUCCESS;
}

bool bongo_cat_rhi_vk_wait_idle(const BongoCatRhi *rhi) {
    const BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    return vk && vk->device && vk->vkDeviceWaitIdle(vk->device) == VK_SUCCESS;
}

const char *bongo_cat_rhi_vk_present_name(void *user) {
    BongoCatRhiVk *vk = user;
    return vk && vk->describe[0] ? vk->describe : "Vulkan";
}

bool bongo_cat_rhi_vk_pick_depth_format(BongoCatRhiVk *vk) {
    /* The SDK uses depth-only views/barriers; avoid combined formats that
       require separateDepthStencilLayouts for depth-only transitions. */
    static const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM};
    bool compact = vk->layered_target;
    VkFormatProperties properties;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        VkFormat candidate = candidates[compact ? 1 - i : i];
        vk->vkGetPhysicalDeviceFormatProperties(vk->physical, candidate,
            &properties);
        if (properties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            vk->depth_format = candidate;
            return true;
        }
    }
    return false;
}

bool bongo_cat_rhi_vk_get_device_info(const BongoCatRhi *rhi,
    BongoCatRhiDeviceInfo *info) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk || !info || !vk->device) return false;
    memset(info, 0, sizeof(*info));
    info->backend = BONGO_CAT_RHI_VULKAN;
    info->vulkan_instance = vk->instance;
    info->vulkan_device = vk->device;
    info->vulkan_physical_device = vk->physical;
    /* VkCommandPool is a non-dispatchable handle (uint64_t on 32-bit Vulkan),
       so it needs the same explicit conversion as the image/view handles. */
    info->vulkan_command_pool = (void *)(uintptr_t)vk->pool;
    info->vulkan_queue = vk->queue;
    info->queue_family = vk->queue_family;
    info->image_count = vk->image_count;
    info->extent_width = vk->extent.width;
    info->extent_height = vk->extent.height;
    info->color_format = (int)vk->format;
    info->depth_format = (int)vk->depth_format;
    info->swapchain_views = (void **)vk->views;
    info->current_image = (void *)(uintptr_t)vk->current_image;
    info->current_view = (void *)(uintptr_t)vk->current_view;
    info->rhi_handle = rhi;
    return true;
}

bool bongo_cat_rhi_vk_get_frame_info(const BongoCatRhi *rhi,
    BongoCatRhiVulkanFrameInfo *info) {
    const BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk || !vk->current_image || !info) return false;
    info->image = (uint64_t)vk->current_image;
    info->view = (uint64_t)vk->current_view;
    return true;
}

bool bongo_cat_rhi_vk_make_current(BongoCatRhi *rhi) {
    const BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    return vk && vk->device;
}

void bongo_cat_rhi_vk_detach(const BongoCatRhi *rhi) { (void)rhi; }

void bongo_cat_rhi_vk_prepare_frame(BongoCatRhi *rhi, int width, int height) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk || !vk->device) return;
    if ((!vk->swapchain && !vk->layered_target) || vk->extent.width != (uint32_t)width ||
        vk->extent.height != (uint32_t)height)
        bongo_cat_rhi_vk_recreate_swapchain(vk, width, height);
}

void bongo_cat_rhi_vk_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height) {
    /* Cubism records the full drawable viewport in its draw commands. */
    (void)rhi; (void)x; (void)y; (void)width; (void)height;
}

void bongo_cat_rhi_vk_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk) return;
    vk->clear[0] = red;
    vk->clear[1] = green;
    vk->clear[2] = blue;
    vk->clear[3] = alpha;
}

const char *bongo_cat_rhi_vk_describe(const BongoCatRhi *rhi) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    return vk && vk->describe[0] ? vk->describe : "Vulkan";
}

void bongo_cat_rhi_vk_shutdown(BongoCatRhi *rhi) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk) return;
    if (vk->device && vk->vkDeviceWaitIdle) vk->vkDeviceWaitIdle(vk->device);
    bongo_cat_rhi_vk_release_corners(vk);
    bongo_cat_rhi_vk_destroy_swapchain_objects(vk);
    bongo_cat_rhi_vk_destroy_frames(vk);
    bongo_cat_rhi_vk_destroy_uploads(vk);
    if (vk->submit_fence && vk->vkDestroyFence)
        vk->vkDestroyFence(vk->device, vk->submit_fence, NULL);
    if (vk->fence && vk->vkDestroyFence)
        vk->vkDestroyFence(vk->device, vk->fence, NULL);
    if (vk->pool && vk->vkDestroyCommandPool)
        vk->vkDestroyCommandPool(vk->device, vk->pool, NULL);
    if (vk->device && vk->vkDestroyDevice)
        vk->vkDestroyDevice(vk->device, NULL);
    if (vk->surface && vk->instance)
        SDL_Vulkan_DestroySurface(vk->instance, vk->surface, NULL);
    if (vk->instance && vk->vkDestroyInstance)
        vk->vkDestroyInstance(vk->instance, NULL);
    free(vk);
    rhi->impl = NULL;
    SDL_Vulkan_UnloadLibrary();
}

#endif
