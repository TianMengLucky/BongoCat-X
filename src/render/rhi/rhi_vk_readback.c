/* Capture before presentation, while the application owns the acquired image. */
#include "rhi_vk_internal.h"
#include "rhi_pixels.h"

#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

static void release_slot(BongoCatRhiVk *vk, BongoCatRhiVkReadback *frame) {
    if (frame->pixels) vk->vkUnmapMemory(vk->device, frame->readback_memory);
    if (frame->readback_buffer)
        vk->vkDestroyBuffer(vk->device, frame->readback_buffer, NULL);
    if (frame->readback_memory)
        vk->vkFreeMemory(vk->device, frame->readback_memory, NULL);
    frame->readback_buffer = VK_NULL_HANDLE;
    frame->readback_memory = VK_NULL_HANDLE;
    frame->readback_size = 0;
    frame->pixels = NULL;
}

void bongo_cat_rhi_vk_release_readback(BongoCatRhiVk *vk) {
    for (unsigned i = 0; i < BONGO_CAT_VK_FRAMES; ++i)
        release_slot(vk, &vk->readbacks[i]);
    vk->pixels_valid = false;
}

static bool allocate_readback(BongoCatRhiVk *vk, VkDeviceSize bytes) {
    BongoCatRhiVkReadback *frame = &vk->readbacks[vk->readback_slot];
    if (frame->pixels && frame->readback_size == bytes) return true;
    release_slot(vk, frame);
    VkBufferCreateInfo buffer = {0};
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size = bytes;
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vk->vkCreateBuffer(vk->device, &buffer, NULL, &frame->readback_buffer) !=
        VK_SUCCESS) goto failed;
    VkMemoryRequirements required;
    vk->vkGetBufferMemoryRequirements(vk->device, frame->readback_buffer, &required);
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
    if (type == UINT32_MAX) goto failed;
    VkMemoryAllocateInfo allocation = {0};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = required.size;
    allocation.memoryTypeIndex = type;
    if (vk->vkAllocateMemory(vk->device, &allocation, NULL, &frame->readback_memory) !=
        VK_SUCCESS || vk->vkBindBufferMemory(vk->device, frame->readback_buffer,
            frame->readback_memory, 0) != VK_SUCCESS) goto failed;
    void *mapped = NULL;
    if (vk->vkMapMemory(vk->device, frame->readback_memory, 0, bytes, 0, &mapped) !=
        VK_SUCCESS) goto failed;
    frame->pixels = mapped;
    frame->readback_size = bytes;
    return true;
failed:
    release_slot(vk, frame);
    return false;
}

bool bongo_cat_rhi_vk_capture_frame(BongoCatRhiVk *vk) {
    if (!vk->extent.width || !vk->extent.height ||
        (uint64_t)vk->extent.width * vk->extent.height > SIZE_MAX / 4) return false;
    VkDeviceSize bytes = (VkDeviceSize)vk->extent.width * vk->extent.height * 4;
    if (vk->owner->present.requires_layered &&
        !bongo_cat_rhi_vk_wait_frame(vk, &vk->frames[vk->readback_frame])) return false;
    // The layered presenter consumes each frame before the next one. Keep one
    // mapping there; independent swap presentation uses the full two-slot ring.
    vk->readback_slot = vk->owner->present.requires_layered ? 0 : vk->frame_slot;
    if (!allocate_readback(vk, bytes)) return false;
    BongoCatRhiVkReadback *frame = &vk->readbacks[vk->readback_slot];
    VkCommandBuffer command = vk->frames[vk->frame_slot].finish;
    VkCommandBufferBeginInfo begin = {0};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vk->vkBeginCommandBuffer(command, &begin) != VK_SUCCESS ||
        !bongo_cat_rhi_vk_draw_corners(vk, command)) return false;
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
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, frame->readback_buffer, 1, &region);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = 0;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = vk->layered_target ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    if (vk->layered_target) barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    /* Coherent mapping removes invalidation, not the transfer-to-host
       dependency. Batch it with the final image transition. */
    VkBufferMemoryBarrier host = {0};
    host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    host.buffer = frame->readback_buffer; host.size = bytes;
    vk->vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT | (vk->layered_target ?
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT),
        0, 0, NULL, 1, &host, 1, &barrier);
    if (!bongo_cat_rhi_vk_submit_frame(vk, command)) return false;
    vk->frame_layout = barrier.newLayout;
    vk->readback_frame = vk->frame_slot;
    vk->pixels_valid = true;
    return true;
}

bool bongo_cat_rhi_vk_upload_composed_frame(BongoCatRhiVk *vk, SDL_Rect changed) {
    if (changed.x < 0 || changed.y < 0 || changed.w <= 0 || changed.h <= 0 ||
        (uint64_t)changed.x + changed.w > vk->extent.width ||
        (uint64_t)changed.y + changed.h > vk->extent.height) return false;
    VkCommandBuffer command = bongo_cat_rhi_vk_begin_commands(vk->owner);
    if (!command) return false;
    VkBufferMemoryBarrier host = {0};
    host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    host.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    host.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    host.buffer = vk->readback_buffer; host.size = vk->readback_size;
    VkImageMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = vk->frame_layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vk->current_image;
    barrier.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vk->vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1, &host, 1, &barrier);
    VkBufferImageCopy region = {0};
    /* Keep the full readback row stride, but transfer only foreground pixels. */
    region.bufferOffset = ((VkDeviceSize)changed.y * vk->extent.width + changed.x) * 4;
    region.bufferRowLength = vk->extent.width;
    region.imageSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = (VkOffset3D){changed.x, changed.y, 0};
    region.imageExtent = (VkExtent3D){(uint32_t)changed.w, (uint32_t)changed.h, 1};
    vk->vkCmdCopyBufferToImage(command, vk->readback_buffer, vk->current_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vk->vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
    return bongo_cat_rhi_vk_submit_commands(vk->owner, command);
}

bool bongo_cat_rhi_vk_read_bgra(int width, int height, void *pixels, void *user) {
    BongoCatRhiVk *vk = user;
    return vk && vk->pixels_valid && width == (int)vk->extent.width &&
        height == (int)vk->extent.height && bongo_cat_rhi_vk_wait_frame(vk, &vk->frames[vk->readback_frame]) &&
        bongo_cat_rhi_copy_pixels(vk->readbacks[vk->readback_slot].pixels,
            width, height, (size_t)width * 4, vk->format == VK_FORMAT_B8G8R8A8_UNORM,
            0, 0, width, height, true, pixels);
}

bool bongo_cat_rhi_vk_read_rgba(int x, int y, int width, int height,
    bool back_buffer, void *pixels, void *user) {
    (void)back_buffer;
    BongoCatRhiVk *vk = user;
    return vk && vk->pixels_valid && bongo_cat_rhi_vk_wait_frame(vk, &vk->frames[vk->readback_frame]) &&
        bongo_cat_rhi_copy_pixels(vk->readbacks[vk->readback_slot].pixels,
        (int)vk->extent.width, (int)vk->extent.height, (size_t)vk->extent.width * 4,
        vk->format == VK_FORMAT_B8G8R8A8_UNORM, x, y, width, height, false, pixels);
}

#endif
