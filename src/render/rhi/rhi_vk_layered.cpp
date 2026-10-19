/* Windows already presents readback pixels through its layered presenter.
   A single owned Vulkan target avoids a second, unused WSI presentation path. */
#include "rhi_vk_internal.h"
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))
bool bongo_cat_rhi_vk_create_layered_target(BongoCatRhiVk *vk, int width, int height) {
    if (!vk || width <= 0 || height <= 0) return false;
    VkPhysicalDeviceProperties device;
    vk->vkGetPhysicalDeviceProperties(vk->physical, &device);
    if ((uint32_t)width > device.limits.maxImageDimension2D ||
        (uint32_t)height > device.limits.maxImageDimension2D) return false;
    vk->layered_target = true;
    if (!vk->borrowed_device) vk->format = VK_FORMAT_B8G8R8A8_UNORM;
    vk->extent = VkExtent2D{(uint32_t)width, (uint32_t)height};
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = vk->format;
    image.extent = VkExtent3D{(uint32_t)width, (uint32_t)height, 1};
    image.mipLevels = image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (vk->vkCreateImage(vk->device, &image, NULL, &vk->layered_image) != VK_SUCCESS) return false;
    VkMemoryRequirements required;
    vk->vkGetImageMemoryRequirements(vk->device, vk->layered_image, &required);
    VkPhysicalDeviceMemoryProperties memory;
    vk->vkGetPhysicalDeviceMemoryProperties(vk->physical, &memory);
    for (uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        if (!(required.memoryTypeBits & (1u << i)) ||
            !(memory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) continue;
        VkMemoryAllocateInfo allocate = {};
        allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate.allocationSize = required.size;
        allocate.memoryTypeIndex = i;
        if (vk->vkAllocateMemory(vk->device, &allocate, NULL, &vk->layered_memory) != VK_SUCCESS)
            return false;
        return vk->vkBindImageMemory(vk->device, vk->layered_image, vk->layered_memory, 0) == VK_SUCCESS;
    }
    return false;
}
#endif
