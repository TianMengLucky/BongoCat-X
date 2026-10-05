/* Vulkan RHI backend: frame submission and present ops (see rhi_vk.c for
   device and swapchain lifetime). Frames carry the recorded clear color
   until the draw phases port to the RHI; the tail of the file provides
   the stub symbols for platforms without the backend. */
#include "rhi_vk_internal.h"

#if defined(_WIN32) || (defined(__linux__) && defined(__x86_64__))

bool bongo_cat_rhi_vk_frame_present(void *user) {
    BongoCatRhiVk *vk = user;
    if (!vk || !vk->device) return false;
    if (vk->vkWaitForFences(vk->device, 1, &vk->fence, VK_TRUE,
            UINT64_MAX) != VK_SUCCESS) return false;
    if (vk->vkResetFences(vk->device, 1, &vk->fence) != VK_SUCCESS)
        return false;
    uint32_t index = 0;
    VkResult acquired = vk->vkAcquireNextImageKHR(vk->device, vk->swapchain,
        UINT64_MAX, vk->image_available, VK_NULL_HANDLE, &index);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) return false;
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
        return false;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vk->vkBeginCommandBuffer(vk->command, &begin) != VK_SUCCESS)
        return false;
    VkClearValue clear;
    clear.color = (VkClearColorValue){{
        vk->clear[0], vk->clear[1], vk->clear[2], vk->clear[3]}};
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
    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit = {0};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &vk->image_available;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &vk->command;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &vk->render_finished;
    if (vk->vkQueueSubmit(vk->queue, 1, &submit, vk->fence) != VK_SUCCESS)
        return false;
    VkPresentInfoKHR present = {0};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &vk->render_finished;
    present.swapchainCount = 1;
    present.pSwapchains = &vk->swapchain;
    present.pImageIndices = &index;
    VkResult presented = vk->vkQueuePresentKHR(vk->queue, &present);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR) {
        bongo_cat_rhi_vk_recreate_swapchain(vk, (int)vk->extent.width,
            (int)vk->extent.height);
        return false;
    }
    return presented == VK_SUCCESS || presented == VK_SUBOPTIMAL_KHR;
}

static bool read_fill(void *user, int width, int height, void *pixels,
    bool bottom_up) {
    BongoCatRhiVk *vk = user;
    if (!vk || width <= 0 || height <= 0 || !pixels) return false;
    /* Premultiplied BGRA synthesized from the recorded clear color: until
       the draw phases port, that color is the complete frame content, so a
       device roundtrip would return the same bytes. */
    uint8_t b = (uint8_t)(vk->clear[2] * 255.0f * vk->clear[3]);
    uint8_t g = (uint8_t)(vk->clear[1] * 255.0f * vk->clear[3]);
    uint8_t r = (uint8_t)(vk->clear[0] * 255.0f * vk->clear[3]);
    uint8_t a = (uint8_t)(vk->clear[3] * 255.0f);
    uint8_t *rows = pixels;
    for (int y = 0; y < height; ++y) {
        uint8_t *row = rows + (size_t)(bottom_up ? height - 1 - y : y) *
            (size_t)width * 4;
        for (int x = 0; x < width; ++x) {
            row[x * 4 + 0] = b;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = r;
            row[x * 4 + 3] = a;
        }
    }
    return true;
}

bool bongo_cat_rhi_vk_read_bgra(int width, int height, void *pixels,
    void *user) {
    return read_fill(user, width, height, pixels, true);
}

bool bongo_cat_rhi_vk_read_rgba(int x, int y, int width, int height,
    bool back_buffer, void *pixels, void *user) {
    (void)x; (void)y; (void)back_buffer;
    return read_fill(user, width, height, pixels, false);
}

const char *bongo_cat_rhi_vk_present_name(void *user) {
    BongoCatRhiVk *vk = user;
    return vk && vk->describe[0] ? vk->describe : "Vulkan";
}

bool bongo_cat_rhi_vk_make_current(BongoCatRhi *rhi) {
    (void)rhi;
    return true;
}

void bongo_cat_rhi_vk_detach(const BongoCatRhi *rhi) { (void)rhi; }

void bongo_cat_rhi_vk_prepare_frame(BongoCatRhi *rhi, int width, int height) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk || !vk->device) return;
    if (vk->extent.width != (uint32_t)width ||
        vk->extent.height != (uint32_t)height)
        bongo_cat_rhi_vk_recreate_swapchain(vk, width, height);
}

void bongo_cat_rhi_vk_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height) {
    /* Recorded with the draw commands once the draw phases port. */
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
    if (vk->device) vk->vkDeviceWaitIdle(vk->device);
    bongo_cat_rhi_vk_destroy_swapchain_objects(vk);
    if (vk->render_finished && vk->vkDestroySemaphore)
        vk->vkDestroySemaphore(vk->device, vk->render_finished, NULL);
    if (vk->image_available && vk->vkDestroySemaphore)
        vk->vkDestroySemaphore(vk->device, vk->image_available, NULL);
    if (vk->fence && vk->vkDestroyFence)
        vk->vkDestroyFence(vk->device, vk->fence, NULL);
    if (vk->pool && vk->vkDestroyCommandPool)
        vk->vkDestroyCommandPool(vk->device, vk->pool, NULL);
    if (vk->render_pass && vk->vkDestroyRenderPass)
        vk->vkDestroyRenderPass(vk->device, vk->render_pass, NULL);
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

#else

/* Unreachable through the availability gate; keeps the symbols so the
   dispatcher links on every platform. */
bool bongo_cat_rhi_vk_create_window(const char *title, int width, int height,
    SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error) {
    (void)title; (void)width; (void)height; (void)window; (void)rhi;
    bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
        "Vulkan is not supported on this platform");
    return BONGO_CAT_ERROR_PLATFORM;
}

void bongo_cat_rhi_vk_shutdown(BongoCatRhi *rhi) { (void)rhi; }

bool bongo_cat_rhi_vk_make_current(BongoCatRhi *rhi) {
    (void)rhi;
    return false;
}

void bongo_cat_rhi_vk_detach(const BongoCatRhi *rhi) { (void)rhi; }

void bongo_cat_rhi_vk_prepare_frame(BongoCatRhi *rhi, int width, int height) {
    (void)rhi; (void)width; (void)height;
}

void bongo_cat_rhi_vk_viewport(BongoCatRhi *rhi, int x, int y, int width,
    int height) {
    (void)rhi; (void)x; (void)y; (void)width; (void)height;
}

void bongo_cat_rhi_vk_clear(BongoCatRhi *rhi, float red, float green,
    float blue, float alpha) {
    (void)rhi; (void)red; (void)green; (void)blue; (void)alpha;
}

const char *bongo_cat_rhi_vk_describe(const BongoCatRhi *rhi) {
    (void)rhi;
    return "Vulkan";
}

#endif
