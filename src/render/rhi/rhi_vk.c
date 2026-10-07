/* Vulkan RHI backend: device and swapchain lifetime (see rhi_vk_loader.c
   for entry-point resolution, rhi_vk_swapchain.c for surface lifetime and
   rhi_vk_frame.c for frame submission). */
#include "rhi_vk_internal.h"

#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

#include <stdio.h>
#include <stdlib.h>

BongoCatResult bongo_cat_rhi_vk_create_window(const char *title, int width, int height,
    SDL_Window **window, BongoCatRhi *rhi, BongoCatError *error) {
    /* Initializes the volk table's global loader; the framework's Vulkan
       renderer sources call vk* through it (VK_NO_PROTOTYPES). */
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    if (volkInitialize() != VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "The Vulkan loader is unavailable");
        return BONGO_CAT_ERROR_PLATFORM;
    }
#endif
    if (!SDL_Vulkan_LoadLibrary(NULL)) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "SDL cannot load the Vulkan loader: %s", SDL_GetError());
        return BONGO_CAT_ERROR_PLATFORM;
    }
    /* Swapchain presentation has no window alpha; the window stays opaque
       and Windows composes the frame through the layered presenter. */
    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN;
#ifndef _WIN32
    flags |= SDL_WINDOW_TRANSPARENT;
#endif
    *window = SDL_CreateWindow(title, width, height, flags);
    if (!*window) {
        SDL_Vulkan_UnloadLibrary();
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "Window creation: %s", SDL_GetError());
        return BONGO_CAT_ERROR_PLATFORM;
    }
    BongoCatRhiVk *vk = calloc(1, sizeof(*vk));
    if (!vk) {
        SDL_DestroyWindow(*window);
        *window = NULL;
        SDL_Vulkan_UnloadLibrary();
        bongo_cat_error_set(error, BONGO_CAT_ERROR_MEMORY,
            "Cannot allocate the Vulkan RHI state");
        return BONGO_CAT_ERROR_MEMORY;
    }
    /* Shutdown must see partially initialized handles on every error path. */
    rhi->impl = vk;
    if (!bongo_cat_rhi_vk_load_global_fns(vk, error)) goto failed;
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
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    app.apiVersion = VK_API_VERSION_1_3;
#else
    app.apiVersion = VK_API_VERSION_1_0;
#endif
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
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    volkLoadInstance(vk->instance);
#endif
    if (!bongo_cat_rhi_vk_load_instance_fns(vk)) {
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
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
        if (!bongo_cat_rhi_vk_cubism_features(vk, devices[i], NULL)) continue;
#endif
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
            "No compatible Vulkan device supports graphics and presentation");
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
    const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    VkPhysicalDeviceVulkan13Features cubism_features = {0};
    bongo_cat_rhi_vk_cubism_features(vk, chosen, &cubism_features);
#endif
    VkDeviceCreateInfo device_info = {0};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    device_info.pNext = &cubism_features;
#endif
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
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
    volkLoadDevice(vk->device);
#endif
    vk->vkGetDeviceQueue(vk->device, queue_family, 0, &vk->queue);
    if (!bongo_cat_rhi_vk_pick_depth_format(vk) ||
        !bongo_cat_rhi_vk_create_swapchain(vk, width, height, error)) goto failed;
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

    if (vk->vkCreateFence(vk->device, &fence_info, NULL, &vk->fence) !=
        VK_SUCCESS) {
        bongo_cat_error_set(error, BONGO_CAT_ERROR_PLATFORM,
            "vkCreateFence failed");
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
    vk->owner = rhi;
    return BONGO_CAT_OK;

failed:
    bongo_cat_rhi_vk_shutdown(rhi);
    /* The backend owns its window only until creation succeeds; the runtime
       teardown takes it from there. */
    if (*window) {
        SDL_DestroyWindow(*window);
        *window = NULL;
    }
    return error && error->code != BONGO_CAT_OK ? error->code :
        BONGO_CAT_ERROR_PLATFORM;
}

#endif
