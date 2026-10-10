// VMA owns suballocation, alignment, buffer/image granularity and mapping.
#include "cubism_vulkan_memory.hpp"
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <map>
#include <stdexcept>

namespace bongo_cat {
namespace {
VmaAllocator allocator;
VkDevice allocator_device;
bool use_pool;
struct BufferAllocation {
    VmaAllocation allocation;
    unsigned maps = 0;
    void *mapped = nullptr;
};
std::map<VkBuffer, BufferAllocation> buffers;
std::map<VkImage, VmaAllocation> images;
void check(VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Live2D Vulkan pooled allocation failed");
}
}
bool vulkan_memory_configure(VkInstance instance, VkPhysicalDevice physical,
    VkDevice device, bool enabled) {
    // Policy changes occur before loading, after the previous model is released.
    if (!buffers.empty() || !images.empty()) return false;
    if (allocator && (allocator_device != device || !enabled)) vulkan_memory_release();
    use_pool = false;
    if (!enabled) return true;
    if (!instance || !physical || !device) return false;
    if (!allocator) {
        VmaVulkanFunctions functions{};
        functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        VmaAllocatorCreateInfo info{};
        info.instance = instance;
        info.physicalDevice = physical;
        info.device = device;
        info.vulkanApiVersion = VK_API_VERSION_1_3;
        // Keep small models from reserving large mostly empty blocks per memory type.
        // VMA still grows the pool and dedicates oversized resources as needed.
        info.preferredLargeHeapBlockSize = 16ull * 1024 * 1024;
        info.pVulkanFunctions = &functions;
        if (vmaCreateAllocator(&info, &allocator) != VK_SUCCESS) return false;
        allocator_device = device;
    }
    use_pool = true;
    return true;
}
void vulkan_memory_release() {
    // Resources must be destroyed before the SDK's final device release.
    if (!buffers.empty() || !images.empty()) return;
    if (allocator) vmaDestroyAllocator(allocator);
    allocator = nullptr;
    allocator_device = VK_NULL_HANDLE;
    use_pool = false;
}
bool vulkan_buffer_create(VkDevice device, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
    VkBuffer *buffer, VkDeviceMemory *memory, bool transient) {
    if (!use_pool || device != allocator_device) return false;
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo allocation_info{};
    // Uploads must not expand pools retained by long-lived mesh/uniform buffers.
    allocation_info.flags = transient ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT :
        VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocation_info.requiredFlags = properties;
    VmaAllocation allocation{};
    VmaAllocationInfo details{};
    VkBuffer raw{};
    check(vmaCreateBuffer(allocator, &info, &allocation_info, &raw, &allocation, &details));
    try { buffers.emplace(raw, BufferAllocation{allocation, 0}); }
    catch (...) { vmaDestroyBuffer(allocator, raw, allocation); throw; }
    *buffer = raw;
    *memory = details.deviceMemory;
    return true;
}
bool vulkan_image_create(VkDevice device, const VkImageCreateInfo &info,
    VkImage *image, VkDeviceMemory *memory) {
    if (!use_pool || device != allocator_device) return false;
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocation_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VmaAllocation allocation{};
    VmaAllocationInfo details{};
    VkImage raw{};
    check(vmaCreateImage(allocator, &info, &allocation_info, &raw, &allocation, &details));
    try { images.emplace(raw, allocation); }
    catch (...) { vmaDestroyImage(allocator, raw, allocation); throw; }
    *image = raw;
    *memory = details.deviceMemory;
    return true;
}
bool vulkan_buffer_map(VkBuffer buffer, void **mapped) {
    auto found = buffers.find(buffer);
    if (found == buffers.end()) return false;
    check(vmaMapMemory(allocator, found->second.allocation, mapped));
    found->second.mapped = *mapped;
    ++found->second.maps;
    return true;
}
void *vulkan_buffer_mapped_data(VkBuffer buffer) {
    auto found = buffers.find(buffer);
    return found != buffers.end() && found->second.maps ? found->second.mapped : nullptr;
}
bool vulkan_buffer_unmap(VkBuffer buffer) {
    auto found = buffers.find(buffer);
    if (found == buffers.end()) return false;
    if (found->second.maps) {
        vmaUnmapMemory(allocator, found->second.allocation);
        if (!--found->second.maps) found->second.mapped = nullptr;
    }
    return true;
}
bool vulkan_buffer_destroy(VkBuffer buffer) {
    auto found = buffers.find(buffer);
    if (found == buffers.end()) return false;
    // SDK uniform buffers stay mapped until Destroy; balance VMA map counts.
    for (unsigned i = 0; i < found->second.maps; ++i)
        vmaUnmapMemory(allocator, found->second.allocation);
    vmaDestroyBuffer(allocator, buffer, found->second.allocation);
    buffers.erase(found);
    return true;
}
bool vulkan_image_destroy(VkImage image) {
    auto found = images.find(image);
    if (found == images.end()) return false;
    vmaDestroyImage(allocator, image, found->second);
    images.erase(found);
    return true;
}
}
