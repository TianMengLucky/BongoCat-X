/* Vulkan frame drawing, readback and presentation. The RHI acquires an
   image, calls the Cubism hook, then hands real pixels to the presenter. */
#include "rhi_vk_internal.h"
#include <stdlib.h>

#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

/* Clear is also used to release an acquired image after a failed draw hook. */
static bool clear_frame(BongoCatRhiVk *vk, uint32_t index) {
    if (vk->vkResetCommandBuffer(vk->command, 0) != VK_SUCCESS) return false;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vk->vkBeginCommandBuffer(vk->command, &begin) != VK_SUCCESS)
        return false;
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
    vk->vkCmdBeginRenderPass(vk->command, &pass, VK_SUBPASS_CONTENTS_INLINE);
    vk->vkCmdEndRenderPass(vk->command);
    if (vk->vkEndCommandBuffer(vk->command) != VK_SUCCESS) return false;
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &vk->command;
    return vk->vkQueueSubmit(vk->queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS;
}

bool bongo_cat_rhi_vk_render_frame(BongoCatRhi *rhi) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk || !vk->device || !vk->swapchain) return false;
    if (vk->frame_ready && !bongo_cat_rhi_vk_frame_present(rhi->window, vk))
        return false;
    vk->pixels_valid = false;
    /* Single frame in flight for this milestone. The acquire fence is waited
       on the CPU before any draw, and all rendering completes before present.
       No binary present semaphore is reused while the presentation engine
       might still own it. A future asynchronous path needs per-image signals. */
    if (vk->vkQueueWaitIdle(vk->queue) != VK_SUCCESS ||
        vk->vkResetFences(vk->device, 1, &vk->fence) != VK_SUCCESS) return false;
    uint32_t index = 0;
    VkResult acquired = vk->vkAcquireNextImageKHR(vk->device, vk->swapchain,
        UINT64_MAX, VK_NULL_HANDLE, vk->fence, &index);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        bongo_cat_rhi_vk_recreate_swapchain(vk, (int)vk->extent.width,
            (int)vk->extent.height);
        return false;
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) return false;
    if (vk->vkWaitForFences(vk->device, 1, &vk->fence, VK_TRUE, UINT64_MAX) !=
        VK_SUCCESS) return false;
    vk->acquired_index = index;
    vk->current_image = vk->images[index];
    vk->current_view = vk->views[index];
    const BongoCatRhiPresentOps *ops = &vk->owner->present;
    if (!clear_frame(vk, index) || vk->vkQueueWaitIdle(vk->queue) != VK_SUCCESS)
        return false;
    bool drawn = bongo_cat_rhi_vk_draw_background(vk) &&
        (!ops->draw_frame || ops->draw_frame(ops->hook_user));
    if (!drawn) {
        /* A failed callback may already have submitted work. Retire it before
           resetting our command buffer and clearing the acquired image. */
        if (vk->vkQueueWaitIdle(vk->queue) != VK_SUCCESS ||
            !clear_frame(vk, index)) return false;
    }
    if (vk->vkQueueWaitIdle(vk->queue) != VK_SUCCESS) return false;
    vk->frame_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vk->frame_ready = true;
    if (!bongo_cat_rhi_vk_capture_frame(vk) || !drawn) {
        (void)bongo_cat_rhi_vk_frame_present(rhi->window, vk);
        return false;
    }
    return true;
}

bool bongo_cat_rhi_vk_frame_present(SDL_Window *window, void *user) {
    BongoCatRhiVk *vk = user;
    if (!vk || !vk->owner) return false;
    if (!vk->frame_ready &&
        !bongo_cat_rhi_vk_render_frame((BongoCatRhi *)vk->owner)) return false;
    if (vk->frame_layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        VkCommandBuffer command = bongo_cat_rhi_vk_begin_commands(vk->owner);
        if (!command) return false;
        VkImageMemoryBarrier barrier = {0};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = vk->frame_layout;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = vk->current_image;
        barrier.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vk->vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
        if (!bongo_cat_rhi_vk_submit_commands(vk->owner, command)) return false;
        vk->frame_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
    uint32_t index = vk->acquired_index;
    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.swapchainCount = 1;
    present.pSwapchains = &vk->swapchain;
    present.pImageIndices = &index;
    VkResult presented = vk->vkQueuePresentKHR(vk->queue, &present);
    vk->current_image = VK_NULL_HANDLE;
    vk->current_view = VK_NULL_HANDLE;
    vk->frame_ready = false;
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
        bongo_cat_rhi_vk_recreate_swapchain(vk, (int)vk->extent.width,
            (int)vk->extent.height);
        return false;
    }
    (void)window;
    return presented == VK_SUCCESS;
}

void *bongo_cat_rhi_vk_begin_commands(const BongoCatRhi *rhi) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk || !vk->device) return NULL;
    VkCommandBufferAllocateInfo info = {0};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = vk->pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    if (vk->vkAllocateCommandBuffers(vk->device, &info, &command) !=
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
    if (!vk || !buffer) return false;
    if (vk->vkEndCommandBuffer(buffer) != VK_SUCCESS) {
        vk->vkFreeCommandBuffers(vk->device, vk->pool, 1, &buffer);
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Cannot end Vulkan upload commands");
        return false;
    }
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &buffer;
    VkResult submitted = vk->vkQueueSubmit(vk->queue, 1, &submit, VK_NULL_HANDLE);
    VkResult waited = submitted == VK_SUCCESS ? vk->vkQueueWaitIdle(vk->queue) :
        submitted;
    if (waited != VK_SUCCESS)
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Vulkan upload failed: %d", (int)waited);
    vk->vkFreeCommandBuffers(vk->device, vk->pool, 1, &buffer);
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
    VkFormatProperties properties;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        vk->vkGetPhysicalDeviceFormatProperties(vk->physical, candidates[i],
            &properties);
        if (properties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            vk->depth_format = candidates[i];
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
    info->vulkan_command_pool = vk->pool;
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
    if (!vk->swapchain || vk->extent.width != (uint32_t)width ||
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
    bongo_cat_rhi_vk_destroy_swapchain_objects(vk);
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
