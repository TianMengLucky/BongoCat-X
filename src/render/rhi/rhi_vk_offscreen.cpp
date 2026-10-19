/* Independent target and command pools; device/queue remain owned by the pet. */
#include "rhi_vk_resources.hpp"
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))
bool bongo_cat_rhi_vk_create_offscreen(const BongoCatRhi *source, SDL_Window *window,
    int width, int height, BongoCatRhi *target, BongoCatError *error) {
    const auto *parent = static_cast<const BongoCatRhiVk *>(source->impl);
    if (!parent || parent->failed || !parent->device) return false;
    std::unique_ptr<BongoCatVulkanBackend> owned(new (std::nothrow) BongoCatVulkanBackend);
    if (!owned) return false;
    auto *vk = owned.get();
    // This flag precedes every failure path: no child may destroy the parent's device.
    vk->borrowed_device = true;
    vk->instance = parent->instance; vk->physical = parent->physical;
    vk->device = parent->device; vk->queue = parent->queue;
    vk->queue_family = parent->queue_family; vk->depth_format = parent->depth_format;
    vk->format = parent->format; vk->bridge_pool = parent->pool;
    if (!bongo_cat_rhi_vk_load_global_fns(vk, error) ||
        !bongo_cat_rhi_vk_load_instance_fns(vk) ||
        !bongo_cat_rhi_vk_create_swapchain(vk, width, height, error)) return false;
    VkCommandPoolCreateInfo pool{};
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.queueFamilyIndex = vk->queue_family;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vk->vkCreateCommandPool(vk->device, &pool, nullptr, &vk->pool) != VK_SUCCESS) return false;
    VkFenceCreateInfo fence{};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vk->vkCreateFence(vk->device, &fence, nullptr, &vk->fence) != VK_SUCCESS ||
        !bongo_cat_rhi_vk_create_frames(vk)) return false;
    target->backend = BONGO_CAT_RHI_VULKAN; target->window = window;
    target->present.swap = bongo_cat_rhi_vk_frame_present;
    target->present.read_bgra = bongo_cat_rhi_vk_read_bgra;
    target->present.read_rgba = bongo_cat_rhi_vk_read_rgba;
    target->present.requires_layered = true;
    target->present.user = vk; vk->owner = target;
    target->impl = static_cast<BongoCatRhiVk *>(owned.release());
    return true;
}
#endif
