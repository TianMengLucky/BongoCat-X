/* Vulkan RHI backend: device and swapchain lifetime (see rhi_vk_frame.c
   for frame submission and the unavailable-platform stubs). Loads the
   system loader through SDL at runtime so the build needs no Vulkan SDK. */
#include "rhi_vk_internal.h"

#if defined(_WIN32) || (defined(__linux__) && defined(__x86_64__))

#include <stdio.h>
#include <stdlib.h>

/* Instance-level entry points come from vkGetInstanceProcAddr; device-level
   calls use the same dispatch for simplicity, the frame rate is modest. */
static bool load_fn(BongoCatRhiVk *vk, void **slot, const char *name) {
    *slot = (void *)vk->instance_gpa(vk->instance, name);
    if (!*slot) {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO,
            "[render] Vulkan entry point %s is unavailable", name);
        return false;
    }
    return true;
}

#define BONGO_CAT_VK_LOAD(name) \
    do { \
        if (!load_fn(vk, (void **)&vk->name, #name)) return false; \
    } while (0)

static bool load_global_fns(BongoCatRhiVk *vk, BongoCatError *error) {
    vk->instance_gpa = (PFN_vkGetInstanceProcAddr)
        SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!vk->instance_gpa) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "SDL cannot resolve vkGetInstanceProcAddr: %s", SDL_GetError());
        return false;
    }
    vk->vkCreateInstance = (PFN_vkCreateInstance)vk->instance_gpa(
        VK_NULL_HANDLE, "vkCreateInstance");
    if (!vk->vkCreateInstance) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "The Vulkan loader does not provide vkCreateInstance");
        return false;
    }
    return true;
}

static bool load_instance_fns(BongoCatRhiVk *vk) {
    BONGO_CAT_VK_LOAD(vkDestroyInstance);
    BONGO_CAT_VK_LOAD(vkEnumeratePhysicalDevices);
    BONGO_CAT_VK_LOAD(vkGetPhysicalDeviceProperties);
    BONGO_CAT_VK_LOAD(vkGetPhysicalDeviceQueueFamilyProperties);
    BONGO_CAT_VK_LOAD(vkGetPhysicalDeviceSurfaceSupportKHR);
    BONGO_CAT_VK_LOAD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    BONGO_CAT_VK_LOAD(vkGetPhysicalDeviceSurfaceFormatsKHR);
    BONGO_CAT_VK_LOAD(vkGetPhysicalDeviceFormatProperties);
    BONGO_CAT_VK_LOAD(vkCreateDevice);
    BONGO_CAT_VK_LOAD(vkDestroyDevice);
    BONGO_CAT_VK_LOAD(vkDeviceWaitIdle);
    BONGO_CAT_VK_LOAD(vkGetDeviceQueue);
    BONGO_CAT_VK_LOAD(vkCreateSwapchainKHR);
    BONGO_CAT_VK_LOAD(vkDestroySwapchainKHR);
    BONGO_CAT_VK_LOAD(vkGetSwapchainImagesKHR);
    BONGO_CAT_VK_LOAD(vkCreateImageView);
    BONGO_CAT_VK_LOAD(vkDestroyImageView);
    BONGO_CAT_VK_LOAD(vkCreateRenderPass);
    BONGO_CAT_VK_LOAD(vkDestroyRenderPass);
    BONGO_CAT_VK_LOAD(vkCreateFramebuffer);
    BONGO_CAT_VK_LOAD(vkDestroyFramebuffer);
    BONGO_CAT_VK_LOAD(vkCreateCommandPool);
    BONGO_CAT_VK_LOAD(vkDestroyCommandPool);
    BONGO_CAT_VK_LOAD(vkAllocateCommandBuffers);
    BONGO_CAT_VK_LOAD(vkCmdBeginRenderPass);
    BONGO_CAT_VK_LOAD(vkCmdEndRenderPass);
    BONGO_CAT_VK_LOAD(vkEndCommandBuffer);
    BONGO_CAT_VK_LOAD(vkQueueSubmit);
    BONGO_CAT_VK_LOAD(vkQueuePresentKHR);
    BONGO_CAT_VK_LOAD(vkWaitForFences);
    BONGO_CAT_VK_LOAD(vkResetFences);
    BONGO_CAT_VK_LOAD(vkCreateFence);
    BONGO_CAT_VK_LOAD(vkDestroyFence);
    BONGO_CAT_VK_LOAD(vkCreateSemaphore);
    BONGO_CAT_VK_LOAD(vkDestroySemaphore);
    BONGO_CAT_VK_LOAD(vkAcquireNextImageKHR);
    BONGO_CAT_VK_LOAD(vkBeginCommandBuffer);
    BONGO_CAT_VK_LOAD(vkQueueWaitIdle);
    BONGO_CAT_VK_LOAD(vkFreeCommandBuffers);
    return true;
}

void bongo_cat_rhi_vk_destroy_swapchain_objects(BongoCatRhiVk *vk) {
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
    vk->image_count = 0;
}

static bool create_swapchain(BongoCatRhiVk *vk, int width, int height,
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
    VkFormat chosen = formats[0].format;
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
    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = (uint32_t)width;
        extent.height = (uint32_t)height;
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
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
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
    if (vk->vkGetSwapchainImagesKHR(vk->device, vk->swapchain,
            &vk->image_count, NULL) != VK_SUCCESS || !vk->image_count) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkGetSwapchainImagesKHR failed");
        return false;
    }
    vk->images = malloc(vk->image_count * sizeof(*vk->images));
    vk->views = calloc(vk->image_count, sizeof(*vk->views));
    vk->framebuffers = calloc(vk->image_count, sizeof(*vk->framebuffers));
    if (!vk->images || !vk->views || !vk->framebuffers ||
        vk->vkGetSwapchainImagesKHR(vk->device, vk->swapchain,
            &vk->image_count, vk->images) != VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
            "Cannot query the Vulkan swapchain images");
        return false;
    }
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
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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
    vk->vkDeviceWaitIdle(vk->device);
    bongo_cat_rhi_vk_destroy_swapchain_objects(vk);
    BongoCatError error = {0};
    return create_swapchain(vk, width, height, &error);
}

bool bongo_cat_rhi_vk_create_window(const char *title, int width, int height,
    SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error) {
    /* Initializes the volk table's global loader; the framework's Vulkan
       renderer sources call vk* through it (VK_NO_PROTOTYPES). */
    if (volkInitialize() != VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "The Vulkan loader is unavailable");
        return BONGO_CAT_ERROR_PLATFORM;
    }
    if (!SDL_Vulkan_LoadLibrary(NULL)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "SDL cannot load the Vulkan loader: %s", SDL_GetError());
        return BONGO_CAT_ERROR_PLATFORM;
    }
    /* Swapchain presentation has no window alpha; the window stays opaque
       and Windows composes the frame through the layered presenter. */
    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
    *window = SDL_CreateWindow(title, width, height, flags);
    if (!*window) {
        SDL_Vulkan_UnloadLibrary();
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Window creation: %s", SDL_GetError());
        return BONGO_CAT_ERROR_PLATFORM;
    }
    BongoCatRhiVk *vk = calloc(1, sizeof(*vk));
    if (!vk) {
        SDL_Vulkan_UnloadLibrary();
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
            "Cannot allocate the Vulkan RHI state");
        return BONGO_CAT_ERROR_MEMORY;
    }
    if (!load_global_fns(vk, error)) goto failed;
    unsigned extension_count = 0;
    /* SDL owns the returned array; the instance creation only borrows it. */
    const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(
        &extension_count);
    if (!extensions || !extension_count) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "SDL cannot list Vulkan instance extensions: %s", SDL_GetError());
        goto failed;
    }
    VkApplicationInfo app = {0};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "BongoCat";
    app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app.pEngineName = "BongoCat";
    app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo instance_info = {0};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app;
    instance_info.enabledExtensionCount = extension_count;
    instance_info.ppEnabledExtensionNames = extensions;
    if (vk->vkCreateInstance(&instance_info, NULL, &vk->instance) !=
        VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkCreateInstance failed");
        goto failed;
    }
    /* Populate volk's instance-level table for the framework sources. */
    volkLoadInstance(vk->instance);
    if (!load_instance_fns(vk)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Required Vulkan entry points are unavailable");
        goto failed;
    }
    if (!SDL_Vulkan_CreateSurface(*window, vk->instance, NULL,
            &vk->surface)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "SDL cannot create the Vulkan surface: %s", SDL_GetError());
        goto failed;
    }
    uint32_t device_count = 0;
    if (vk->vkEnumeratePhysicalDevices(vk->instance, &device_count, NULL) !=
        VK_SUCCESS || !device_count) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "No Vulkan physical devices found");
        goto failed;
    }
    VkPhysicalDevice *devices = malloc(device_count * sizeof(*devices));
    if (!devices) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
            "Cannot list the Vulkan physical devices");
        goto failed;
    }
    if (vk->vkEnumeratePhysicalDevices(vk->instance, &device_count,
            devices) != VK_SUCCESS) {
        free(devices);
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkEnumeratePhysicalDevices failed");
        goto failed;
    }
    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    uint32_t queue_family = UINT32_MAX;
    for (uint32_t i = 0; i < device_count && !chosen; ++i) {
        uint32_t family_count = 0;
        vk->vkGetPhysicalDeviceQueueFamilyProperties(devices[i],
            &family_count, NULL);
        VkQueueFamilyProperties *families =
            malloc(family_count * sizeof(*families));
        if (!families) continue;
        vk->vkGetPhysicalDeviceQueueFamilyProperties(devices[i],
            &family_count, families);
        for (uint32_t f = 0; f < family_count; ++f) {
            VkBool32 supported = VK_FALSE;
            if ((families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                vk->vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], f,
                    vk->surface, &supported) == VK_SUCCESS &&
                supported == VK_TRUE) {
                chosen = devices[i];
                queue_family = f;
                break;
            }
        }
        free(families);
        if (chosen) {
            VkPhysicalDeviceProperties properties;
            vk->vkGetPhysicalDeviceProperties(devices[i], &properties);
            snprintf(vk->describe, sizeof(vk->describe), "Vulkan %s",
                properties.deviceName);
        }
    }
    free(devices);
    if (!chosen) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "No Vulkan device supports graphics and presentation");
        goto failed;
    }
    vk->physical = chosen;
    vk->queue_family = queue_family;
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {0};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    const char *device_extensions[] = {"VK_KHR_swapchain"};
    VkDeviceCreateInfo device_info = {0};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = device_extensions;
    if (vk->vkCreateDevice(vk->physical, &device_info, NULL, &vk->device) !=
        VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkCreateDevice failed");
        goto failed;
    }
    vk->vkGetDeviceQueue(vk->device, queue_family, 0, &vk->queue);
    if (!bongo_cat_rhi_vk_pick_depth_format(vk) ||
        !create_render_pass(vk, error) ||
        !create_swapchain(vk, width, height, error)) goto failed;
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family;
    if (vk->vkCreateCommandPool(vk->device, &pool_info, NULL, &vk->pool) !=
        VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkCreateCommandPool failed");
        goto failed;
    }
    VkCommandBufferAllocateInfo command_info = {0};
    command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_info.commandPool = vk->pool;
    command_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_info.commandBufferCount = 1;
    if (vk->vkAllocateCommandBuffers(vk->device, &command_info,
            &vk->command) != VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkAllocateCommandBuffers failed");
        goto failed;
    }
    VkFenceCreateInfo fence_info = {0};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vk->vkCreateFence(vk->device, &fence_info, NULL, &vk->fence) !=
        VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkCreateFence failed");
        goto failed;
    }
    VkSemaphoreCreateInfo semaphore_info = {0};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vk->vkCreateSemaphore(vk->device, &semaphore_info, NULL,
            &vk->image_available) != VK_SUCCESS ||
        vk->vkCreateSemaphore(vk->device, &semaphore_info, NULL,
            &vk->render_finished) != VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkCreateSemaphore failed");
        goto failed;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_VIDEO, "[render] %s ready", vk->describe);
    rhi->backend = BONGO_CAT_RHI_VULKAN;
    rhi->window = *window;
    rhi->impl = vk;
    rhi->present.swap = bongo_cat_rhi_vk_frame_present;
    rhi->present.read_bgra = bongo_cat_rhi_vk_read_bgra;
    rhi->present.read_rgba = bongo_cat_rhi_vk_read_rgba;
    rhi->present.name = bongo_cat_rhi_vk_present_name;
#ifdef _WIN32
    /* Vulkan swapchains cannot present with window alpha on Windows; the
       layered presenter composes the frame from readbacks instead. */
    rhi->present.requires_layered = true;
#else
    rhi->present.requires_layered = false;
#endif
    rhi->present.user = vk;
    return BONGO_CAT_OK;

failed:
    bongo_cat_rhi_vk_shutdown(rhi);
    /* The backend owns its window only until creation succeeds; the runtime
       teardown takes it from there. */
    if (*window) {
        SDL_DestroyWindow(*window);
        *window = NULL;
    }
    return BONGO_CAT_ERROR_PLATFORM;
}

#endif
