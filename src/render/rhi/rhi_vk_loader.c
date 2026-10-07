/* Vulkan RHI backend: entry-point resolution (see rhi_vk.c for device and
   swapchain lifetime, rhi_vk_frame.c for frame submission and present).
   Split into its own translation unit to keep rhi_vk.c within the source
   size policy. The system loader is reached through SDL at runtime, so the
   build needs no Vulkan SDK. */
#include "rhi_vk_internal.h"

#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

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

bool bongo_cat_rhi_vk_load_global_fns(BongoCatRhiVk *vk,
    BongoCatError *error) {
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

bool bongo_cat_rhi_vk_load_instance_fns(BongoCatRhiVk *vk) {
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
    BONGO_CAT_VK_LOAD(vkAcquireNextImageKHR);
    BONGO_CAT_VK_LOAD(vkBeginCommandBuffer);
    BONGO_CAT_VK_LOAD(vkQueueWaitIdle);
    BONGO_CAT_VK_LOAD(vkFreeCommandBuffers);
    BONGO_CAT_VK_LOAD(vkResetCommandBuffer);
    BONGO_CAT_VK_LOAD(vkCreateBuffer);
    BONGO_CAT_VK_LOAD(vkDestroyBuffer);
    BONGO_CAT_VK_LOAD(vkGetBufferMemoryRequirements);
    BONGO_CAT_VK_LOAD(vkAllocateMemory);
    BONGO_CAT_VK_LOAD(vkFreeMemory);
    BONGO_CAT_VK_LOAD(vkBindBufferMemory);
    BONGO_CAT_VK_LOAD(vkMapMemory);
    BONGO_CAT_VK_LOAD(vkUnmapMemory);
    BONGO_CAT_VK_LOAD(vkGetPhysicalDeviceMemoryProperties);
    BONGO_CAT_VK_LOAD(vkCmdPipelineBarrier);
    BONGO_CAT_VK_LOAD(vkCmdCopyImageToBuffer);

    return true;
}

#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
bool bongo_cat_rhi_vk_cubism_features(BongoCatRhiVk *vk,
    VkPhysicalDevice device, VkPhysicalDeviceVulkan13Features *enabled) {
    VkPhysicalDeviceProperties properties;
    vk->vkGetPhysicalDeviceProperties(device, &properties);
    if (properties.apiVersion < VK_API_VERSION_1_3) return false;
    VkPhysicalDeviceVulkan13Features features = {0};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceFeatures2 query = {0};
    query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    query.pNext = &features;
    vkGetPhysicalDeviceFeatures2(device, &query);
    if (!features.dynamicRendering || !features.synchronization2) return false;
    if (enabled) {
        memset(enabled, 0, sizeof(*enabled));
        enabled->sType = features.sType;
        enabled->dynamicRendering = VK_TRUE;
        enabled->synchronization2 = VK_TRUE;
    }
    return true;
}
#endif

#endif
