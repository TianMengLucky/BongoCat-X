/* Cache the composed desk canvas on the GPU; animated frames only copy it. */
#include "rhi_vk_internal.h"
#include <limits.h>
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

void bongo_cat_rhi_vk_release_background(BongoCatRhiVk *vk) {
    if (vk->background) vk->vkDestroyImage(vk->device, vk->background, NULL);
    if (vk->background_memory) vk->vkFreeMemory(vk->device, vk->background_memory, NULL);
    vk->background = VK_NULL_HANDLE;
    vk->background_memory = VK_NULL_HANDLE;
    vk->background_ready = false;
}
static bool allocate_memory(BongoCatRhiVk *vk, VkMemoryRequirements required,
    VkMemoryPropertyFlags flags, VkDeviceMemory *memory) {
    VkPhysicalDeviceMemoryProperties properties;
    vk->vkGetPhysicalDeviceMemoryProperties(vk->physical, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if (!(required.memoryTypeBits & (1u << i)) ||
            (properties.memoryTypes[i].propertyFlags & flags) != flags) continue;
        VkMemoryAllocateInfo allocation = {0};
        allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize = required.size;
        allocation.memoryTypeIndex = i;
        return vk->vkAllocateMemory(vk->device, &allocation, NULL, memory) == VK_SUCCESS;
    }
    return false;
}
static void transition(BongoCatRhiVk *vk, VkCommandBuffer command, VkImage image,
    VkImageLayout from, VkImageLayout to, VkAccessFlags source, VkAccessFlags target) {
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = source; barrier.dstAccessMask = target;
    barrier.oldLayout = from; barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vk->vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
}
bool bongo_cat_rhi_vk_set_background(BongoCatRhi *rhi, const void *pixels,
    int width, int height, int pitch, uint64_t revision) {
    BongoCatRhiVk *vk = rhi ? rhi->impl : NULL;
    if (!vk || !vk->device) return false;
    if (!pixels) { bongo_cat_rhi_vk_release_background(vk); return true; }
    if (width <= 0 || height <= 0 || width > INT_MAX / 4 || pitch < width * 4 ||
        width != (int)vk->extent.width || height != (int)vk->extent.height ||
        (uint64_t)width * height > SIZE_MAX / 4) return false;
    if (vk->background_ready && vk->background_revision == revision) return true;
    if (vk->vkQueueWaitIdle(vk->queue) != VK_SUCCESS) return false;
    if (!vk->background) {
        VkImageCreateInfo image = {0};
        image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image.imageType = VK_IMAGE_TYPE_2D; image.format = vk->format;
        image.extent = (VkExtent3D){(uint32_t)width, (uint32_t)height, 1};
        image.mipLevels = image.arrayLayers = 1; image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        image.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (vk->vkCreateImage(vk->device, &image, NULL, &vk->background) != VK_SUCCESS)
            return false;
        VkMemoryRequirements required;
        vk->vkGetImageMemoryRequirements(vk->device, vk->background, &required);
        if (!allocate_memory(vk, required, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &vk->background_memory) || vk->vkBindImageMemory(vk->device,
                vk->background, vk->background_memory, 0) != VK_SUCCESS) {
            bongo_cat_rhi_vk_release_background(vk); return false;
        }
    }
    VkDeviceSize bytes = (VkDeviceSize)width * height * 4;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBufferCreateInfo info = {0};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = bytes; info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bool ok = vk->vkCreateBuffer(vk->device, &info, NULL, &buffer) == VK_SUCCESS;
    if (ok) {
        VkMemoryRequirements required;
        vk->vkGetBufferMemoryRequirements(vk->device, buffer, &required);
        ok = allocate_memory(vk, required, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory) &&
            vk->vkBindBufferMemory(vk->device, buffer, memory, 0) == VK_SUCCESS;
    }
    void *mapped = NULL;
    if (ok) ok = vk->vkMapMemory(vk->device, memory, 0, bytes, 0, &mapped) == VK_SUCCESS;
    if (ok) {
        for (int y = 0; y < height; ++y) {
            const uint8_t *source = (const uint8_t *)pixels + (size_t)y * pitch;
            uint8_t *dest = (uint8_t *)mapped + (size_t)y * width * 4;
            memcpy(dest, source, (size_t)width * 4);
            if (vk->format == VK_FORMAT_B8G8R8A8_UNORM)
                for (int x = 0; x < width; ++x) {
                    dest[x * 4] = source[x * 4 + 2];
                    dest[x * 4 + 2] = source[x * 4];
                }
        }
        vk->vkUnmapMemory(vk->device, memory);
        VkCommandBuffer command = bongo_cat_rhi_vk_begin_commands(rhi);
        ok = command != VK_NULL_HANDLE;
        if (ok) {
            transition(vk, command, vk->background, vk->background_ready
                ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                vk->background_ready ? VK_ACCESS_TRANSFER_READ_BIT : 0,
                VK_ACCESS_TRANSFER_WRITE_BIT);
            VkBufferImageCopy region = {0};
            region.imageSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = (VkExtent3D){(uint32_t)width, (uint32_t)height, 1};
            vk->vkCmdCopyBufferToImage(command, buffer, vk->background,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            transition(vk, command, vk->background, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT);
            ok = bongo_cat_rhi_vk_submit_commands(rhi, command);
        }
    }
    if (buffer) vk->vkDestroyBuffer(vk->device, buffer, NULL);
    if (memory) vk->vkFreeMemory(vk->device, memory, NULL);
    if (ok) { vk->background_ready = true; vk->background_revision = revision; }
    else bongo_cat_rhi_vk_release_background(vk);
    return ok;
}
bool bongo_cat_rhi_vk_draw_background(BongoCatRhiVk *vk) {
    if (!vk->background_ready) return true;
    VkCommandBuffer command = bongo_cat_rhi_vk_begin_commands(vk->owner);
    if (!command) return false;
    transition(vk, command, vk->current_image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy region = {0};
    region.srcSubresource = region.dstSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.extent = (VkExtent3D){vk->extent.width, vk->extent.height, 1};
    vk->vkCmdCopyImage(command, vk->background, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        vk->current_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    transition(vk, command, vk->current_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    return bongo_cat_rhi_vk_submit_commands(vk->owner, command);
}
#endif
