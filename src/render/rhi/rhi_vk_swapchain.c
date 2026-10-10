/* Surface format, render-pass and swapchain lifetime. */
#include "rhi_vk_internal.h"
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))
#include <stdlib.h>

static bool create_render_pass(BongoCatRhiVk *vk, BongoCatError *error);

void bongo_cat_rhi_vk_destroy_swapchain_objects(BongoCatRhiVk *vk) {
    bongo_cat_rhi_vk_release_corner_framebuffers(vk);
    if (vk->framebuffers) {
        for (uint32_t i = 0; i < vk->image_count; ++i)
            if (vk->framebuffers[i])
                vk->vkDestroyFramebuffer(vk->device, vk->framebuffers[i], NULL);
        free(vk->framebuffers);
        vk->framebuffers = NULL;
    }
    if (vk->views) {
        for (uint32_t i = 0; i < vk->image_count; ++i)
            if (vk->views[i])
                vk->vkDestroyImageView(vk->device, vk->views[i], NULL);
        free(vk->views);
        vk->views = NULL;
    }
    if (vk->images) {
        free(vk->images);
        vk->images = NULL;
    }
    if (vk->swapchain) {
        vk->vkDestroySwapchainKHR(vk->device, vk->swapchain, NULL);
        vk->swapchain = VK_NULL_HANDLE;
    }
    vk->frame_ready = false;
    bongo_cat_rhi_vk_release_readback(vk);
    bongo_cat_rhi_vk_release_background(vk);
    vk->image_count = 0;
    vk->current_image = VK_NULL_HANDLE;
    vk->current_view = VK_NULL_HANDLE;
    if (vk->render_pass) {
        vk->vkDestroyRenderPass(vk->device, vk->render_pass, NULL);
        vk->render_pass = VK_NULL_HANDLE;
    }
}

bool bongo_cat_rhi_vk_create_swapchain(BongoCatRhiVk *vk, int width, int height,
    BongoCatError *error) {
    VkSurfaceCapabilitiesKHR caps;
    if (vk->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical,
            vk->surface, &caps) != VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");
        return false;
    }
    uint32_t format_count = 0;
    if (vk->vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical, vk->surface,
            &format_count, NULL) != VK_SUCCESS || !format_count) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "The Vulkan surface exposes no formats");
        return false;
    }
    VkSurfaceFormatKHR *formats = malloc(format_count * sizeof(*formats));
    if (!formats) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
            "Cannot list the Vulkan surface formats");
        return false;
    }
    if (vk->vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical, vk->surface,
            &format_count, formats) != VK_SUCCESS) {
        free(formats);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkGetPhysicalDeviceSurfaceFormatsKHR failed");
        return false;
    }
    VkFormat chosen = formats[0].format == VK_FORMAT_UNDEFINED ?
        VK_FORMAT_B8G8R8A8_UNORM : formats[0].format;
    VkColorSpaceKHR color_space = formats[0].colorSpace;
    for (uint32_t i = 0; i < format_count; ++i) {
        if ((formats[i].format == VK_FORMAT_B8G8R8A8_UNORM ||
                formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = formats[i].format;
            color_space = formats[i].colorSpace;
            break;
        }
    }
    free(formats);
    if (chosen != VK_FORMAT_B8G8R8A8_UNORM && chosen != VK_FORMAT_R8G8B8A8_UNORM) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Vulkan surface requires an unsupported readback format");
        return false;
    }
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = (uint32_t)width;
        extent.height = (uint32_t)height;
        extent.width = SDL_clamp(extent.width, caps.minImageExtent.width,
            caps.maxImageExtent.width);
        extent.height = SDL_clamp(extent.height, caps.minImageExtent.height,
            caps.maxImageExtent.height);
    }
    if (!extent.width || !extent.height) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "The Vulkan surface has a zero-sized extent");
        return false;
    }
    VkSwapchainCreateInfoKHR info = {0};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = vk->surface;
    info.minImageCount = image_count;
    info.imageFormat = chosen;
    info.imageColorSpace = color_space;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    if ((caps.supportedUsageFlags & (VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT)) != (VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Vulkan surface does not support frame readback");
        return false;
    }
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    static const VkCompositeAlphaFlagBitsKHR alpha_modes[] = {
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR};
    for (size_t i = 0; i < SDL_arraysize(alpha_modes); ++i) {
        if (caps.supportedCompositeAlpha & alpha_modes[i]) {
            info.compositeAlpha = alpha_modes[i];
            break;
        }
    }
#ifndef _WIN32
    if (info.compositeAlpha != VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR &&
        info.compositeAlpha != VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "The Vulkan surface cannot composite a transparent pet");
        return false;
    }
#endif
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    info.clipped = VK_TRUE;
    if (vk->vkCreateSwapchainKHR(vk->device, &info, NULL, &vk->swapchain) !=
        VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkCreateSwapchainKHR failed");
        return false;
    }
    vk->format = chosen;
    vk->extent = extent;
    /* The render pass must use the surface's selected color format. */
    if (!create_render_pass(vk, error)) return false;
    if (vk->vkGetSwapchainImagesKHR(vk->device, vk->swapchain,
            &vk->image_count, NULL) != VK_SUCCESS || !vk->image_count) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkGetSwapchainImagesKHR failed");
        return false;
    }
    vk->images = malloc(vk->image_count * sizeof(VkImage));
    vk->views = calloc(vk->image_count, sizeof(VkImageView));
    vk->framebuffers = calloc(vk->image_count, sizeof(VkFramebuffer));
    uint32_t queried_count = vk->image_count;
    if (!vk->images || !vk->views || !vk->framebuffers ||
        vk->vkGetSwapchainImagesKHR(vk->device, vk->swapchain,
            &queried_count, vk->images) != VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
            "Cannot query the Vulkan swapchain images");
        return false;
    }
    vk->image_count = queried_count;
    for (uint32_t i = 0; i < vk->image_count; ++i) {
        VkImageViewCreateInfo view = {0};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = vk->images[i];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = vk->format;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.layerCount = 1;
        if (vk->vkCreateImageView(vk->device, &view, NULL, &vk->views[i]) !=
            VK_SUCCESS) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "vkCreateImageView failed");
            return false;
        }
        VkFramebufferCreateInfo fb = {0};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = vk->render_pass;
        fb.attachmentCount = 1;
        fb.pAttachments = &vk->views[i];
        fb.width = vk->extent.width;
        fb.height = vk->extent.height;
        fb.layers = 1;
        if (vk->vkCreateFramebuffer(vk->device, &fb, NULL,
                &vk->framebuffers[i]) != VK_SUCCESS) {
            bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
                "vkCreateFramebuffer failed");
            return false;
        }
    }
    return true;
}

static bool create_render_pass(BongoCatRhiVk *vk, BongoCatError *error) {
    VkAttachmentDescription attachment = {0};
    attachment.format = vk->format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference reference = {0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo info = {0};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;
    if (vk->vkCreateRenderPass(vk->device, &info, NULL, &vk->render_pass) !=
        VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkCreateRenderPass failed");
        return false;
    }
    return true;
}

bool bongo_cat_rhi_vk_recreate_swapchain(BongoCatRhiVk *vk, int width,
    int height) {
    if (!vk || !vk->device || width <= 0 || height <= 0) return false;
    if (vk->vkDeviceWaitIdle(vk->device) != VK_SUCCESS) return false;
    bongo_cat_rhi_vk_destroy_swapchain_objects(vk);
    BongoCatError error = {0};
    if (bongo_cat_rhi_vk_create_swapchain(vk, width, height, &error)) return true;
    SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Swapchain rebuild failed: %s",
        error.message);
    bongo_cat_rhi_vk_destroy_swapchain_objects(vk);
    return false;
}


#endif
