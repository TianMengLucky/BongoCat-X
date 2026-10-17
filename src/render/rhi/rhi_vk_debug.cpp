#include "rhi_vk_internal.h"
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))
#include "bongo_cat/log.h"
#include <stdlib.h>
#include <string.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_message(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *message, void *userdata) {
    (void)type; (void)userdata;
    SDL_LogMessage(BONGO_CAT_LOG_LIFECYCLE,
        severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ?
            SDL_LOG_PRIORITY_ERROR : SDL_LOG_PRIORITY_WARN,
        "[vulkan-validation] %s", message && message->pMessage ? message->pMessage : "Unknown message");
    return VK_FALSE;
}

void bongo_cat_rhi_vk_debug_configure(BongoCatRhiVk *vk, VkInstanceCreateInfo *info) {
    const char *requested = SDL_getenv("BONGOCAT_VULKAN_VALIDATION");
    if (!requested || strcmp(requested, "1") != 0) return;
    PFN_vkEnumerateInstanceLayerProperties layers = (PFN_vkEnumerateInstanceLayerProperties)
        vk->instance_gpa(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties");
    uint32_t count = 0;
    if (!layers || layers(&count, NULL) != VK_SUCCESS || !count) return;
    VkLayerProperties *available = static_cast<VkLayerProperties *>(calloc(count, sizeof(*available)));
    bool found = false;
    if (available && layers(&count, available) == VK_SUCCESS)
        for (uint32_t i = 0; i < count; ++i)
            if (strcmp(available[i].layerName, "VK_LAYER_KHRONOS_validation") == 0) found = true;
    free(available);
    if (!found) {
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[vulkan-validation] Requested layer is unavailable");
        return;
    }
    static const char *layer = "VK_LAYER_KHRONOS_validation";
    info->enabledLayerCount = 1; info->ppEnabledLayerNames = &layer;
    PFN_vkEnumerateInstanceExtensionProperties extensions = (PFN_vkEnumerateInstanceExtensionProperties)
        vk->instance_gpa(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    count = 0;
    if (!extensions || extensions(NULL, &count, NULL) != VK_SUCCESS || !count) return;
    VkExtensionProperties *properties = static_cast<VkExtensionProperties *>(calloc(count, sizeof(*properties)));
    found = false;
    if (properties && extensions(NULL, &count, properties) == VK_SUCCESS)
        for (uint32_t i = 0; i < count; ++i)
            if (strcmp(properties[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) found = true;
    free(properties);
    if (!found) return;
    bool enabled = false;
    for (uint32_t i = 0; i < info->enabledExtensionCount; ++i)
        if (strcmp(info->ppEnabledExtensionNames[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) enabled = true;
    if (!enabled) {
        vk->debug_extensions = static_cast<const char **>(calloc((size_t)info->enabledExtensionCount + 1, sizeof(char *)));
        if (!vk->debug_extensions) return;
        memcpy(vk->debug_extensions, info->ppEnabledExtensionNames,
            info->enabledExtensionCount * sizeof(char *));
        vk->debug_extensions[info->enabledExtensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        info->ppEnabledExtensionNames = vk->debug_extensions;
    }
    vk->debug_create.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    vk->debug_create.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    vk->debug_create.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    vk->debug_create.pfnUserCallback = debug_message;
    vk->debug_create.pNext = info->pNext;
    info->pNext = &vk->debug_create;
}

void bongo_cat_rhi_vk_debug_start(BongoCatRhiVk *vk) {
    free(vk->debug_extensions); vk->debug_extensions = NULL;
    if (!vk->debug_create.sType) return;
    PFN_vkCreateDebugUtilsMessengerEXT create = (PFN_vkCreateDebugUtilsMessengerEXT)
        vk->instance_gpa(vk->instance, "vkCreateDebugUtilsMessengerEXT");
    if (create && create(vk->instance, &vk->debug_create, NULL, &vk->debug_messenger) != VK_SUCCESS)
        SDL_LogWarn(BONGO_CAT_LOG_LIFECYCLE, "[vulkan-validation] Cannot create debug messenger");
}

void bongo_cat_rhi_vk_debug_stop(BongoCatRhiVk *vk) {
    free(vk->debug_extensions); vk->debug_extensions = NULL;
    if (!vk->debug_messenger) return;
    PFN_vkDestroyDebugUtilsMessengerEXT destroy = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vk->instance_gpa(vk->instance, "vkDestroyDebugUtilsMessengerEXT");
    if (destroy) destroy(vk->instance, vk->debug_messenger, NULL);
    vk->debug_messenger = VK_NULL_HANDLE;
}
#endif
