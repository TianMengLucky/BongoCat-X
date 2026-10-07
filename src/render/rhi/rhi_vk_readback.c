/* Capture before presentation, while the application owns the acquired image. */
#include "rhi_vk_internal.h"
#include "rhi_pixels.h"

#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

void bongo_cat_rhi_vk_release_readback(BongoCatRhiVk *vk) {
    if (vk->readback_buffer)
        vk->vkDestroyBuffer(vk->device, vk->readback_buffer, NULL);
    if (vk->readback_memory)
        vk->vkFreeMemory(vk->device, vk->readback_memory, NULL);
    vk->readback_buffer = VK_NULL_HANDLE;
    vk->readback_memory = VK_NULL_HANDLE;
    vk->readback_size = 0;
    free(vk->pixels);
    vk->pixels = NULL;
    vk->pixels_valid = false;
}

static bool allocate_readback(BongoCatRhiVk *vk, VkDeviceSize bytes) {
    if (vk->readback_buffer && vk->readback_size == bytes) return true;
    bongo_cat_rhi_vk_release_readback(vk);
    VkBufferCreateInfo buffer = {0};
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size = bytes;
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vk->vkCreateBuffer(vk->device, &buffer, NULL, &vk->readback_buffer) !=
        VK_SUCCESS) return false;
    VkMemoryRequirements required;
    vk->vkGetBufferMemoryRequirements(vk->device, vk->readback_buffer, &required);
    VkPhysicalDeviceMemoryProperties properties;
    vk->vkGetPhysicalDeviceMemoryProperties(vk->physical, &properties);
    uint32_t type = UINT32_MAX;
    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if ((required.memoryTypeBits & (1u << i)) &&
            (properties.memoryTypes[i].propertyFlags & flags) == flags) {
            type = i;
            break;
        }
    }
    if (type == UINT32_MAX) return false;
    VkMemoryAllocateInfo allocation = {0};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = required.size;
    allocation.memoryTypeIndex = type;
    if (vk->vkAllocateMemory(vk->device, &allocation, NULL, &vk->readback_memory) !=
        VK_SUCCESS || vk->vkBindBufferMemory(vk->device, vk->readback_buffer,
            vk->readback_memory, 0) != VK_SUCCESS) return false;
    vk->pixels = malloc((size_t)bytes);
    if (!vk->pixels) return false;
    vk->readback_size = bytes;
    return true;
}

bool bongo_cat_rhi_vk_capture_frame(BongoCatRhiVk *vk) {
    if (!vk->extent.width || !vk->extent.height ||
        (uint64_t)vk->extent.width * vk->extent.height > SIZE_MAX / 4) return false;
    VkDeviceSize bytes = (VkDeviceSize)vk->extent.width * vk->extent.height * 4;
    if (!allocate_readback(vk, bytes)) return false;
    VkCommandBuffer command = bongo_cat_rhi_vk_begin_commands(vk->owner);
    if (!command) return false;
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vk->current_image;
    barrier.subresourceRange = (VkImageSubresourceRange){
        VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vk->vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    VkBufferImageCopy region = {0};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = (VkExtent3D){vk->extent.width, vk->extent.height, 1};
    vk->vkCmdCopyImageToBuffer(command, vk->current_image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk->readback_buffer, 1, &region);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = 0;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vk->vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    if (!bongo_cat_rhi_vk_submit_commands(vk->owner, command)) return false;
    vk->frame_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    void *mapped = NULL;
    if (vk->vkMapMemory(vk->device, vk->readback_memory, 0, bytes, 0, &mapped) !=
        VK_SUCCESS) return false;
    memcpy(vk->pixels, mapped, (size_t)bytes);
    vk->vkUnmapMemory(vk->device, vk->readback_memory);
    vk->pixels_valid = true;
    return true;
}

bool bongo_cat_rhi_vk_read_bgra(int width, int height, void *pixels, void *user) {
    const BongoCatRhiVk *vk = user;
    return vk && vk->pixels_valid && width == (int)vk->extent.width &&
        height == (int)vk->extent.height && bongo_cat_rhi_copy_pixels(vk->pixels,
            width, height, (size_t)width * 4, vk->format == VK_FORMAT_B8G8R8A8_UNORM,
            0, 0, width, height, true, pixels);
}

bool bongo_cat_rhi_vk_read_rgba(int x, int y, int width, int height,
    bool back_buffer, void *pixels, void *user) {
    (void)back_buffer;
    const BongoCatRhiVk *vk = user;
    return vk && vk->pixels_valid && bongo_cat_rhi_copy_pixels(vk->pixels,
        (int)vk->extent.width, (int)vk->extent.height, (size_t)vk->extent.width * 4,
        vk->format == VK_FORMAT_B8G8R8A8_UNORM, x, y, width, height, false, pixels);
}

#endif
