#include "rhi_vk_resources.hpp"
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))
BongoCatVulkanBackend::~BongoCatVulkanBackend() {
    // Scope exit is not GPU completion: wait before destroying any owned object.
    if (device && vkDeviceWaitIdle) vkDeviceWaitIdle(device);
    bongo_cat_rhi_vk_release_corners(this);
    bongo_cat_rhi_vk_destroy_swapchain_objects(this);
    bongo_cat_rhi_vk_destroy_frames(this);
    bongo_cat_rhi_vk_destroy_uploads(this);
    if (submit_fence && vkDestroyFence) vkDestroyFence(device, submit_fence, nullptr);
    if (fence && vkDestroyFence) vkDestroyFence(device, fence, nullptr);
    if (pool && vkDestroyCommandPool) vkDestroyCommandPool(device, pool, nullptr);
    if (!borrowed_device && device && vkDestroyDevice) vkDestroyDevice(device, nullptr);
    if (surface && instance) SDL_Vulkan_DestroySurface(instance, surface, nullptr);
    if (!borrowed_device) bongo_cat_rhi_vk_debug_stop(this);
    if (!borrowed_device && instance && vkDestroyInstance) vkDestroyInstance(instance, nullptr);
    if (!borrowed_device) SDL_Vulkan_UnloadLibrary();
}
#endif
