// VMA owns suballocation, alignment, buffer/image granularity and mapping.
#include "cubism_vulkan_memory.hpp"
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <map>
#include <stdexcept>

namespace bongo_cat {
int vulkan_texture_rebind_slot = -1;
namespace {
VmaAllocator allocator;
VkDevice allocator_device;
bool use_pool;
bool allocator_large_allocation;
bool allocator_system_memory;
uint32_t host_memory_types;
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
    VkDevice device, bool enabled, bool large_allocation, bool system_memory) {
    // Policy changes occur before loading, after the previous model is released.
    if (!buffers.empty() || !images.empty()) return false;
    if (allocator && (allocator_device != device || !enabled ||
        allocator_large_allocation != large_allocation || allocator_system_memory != system_memory)) vulkan_memory_release();
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
        info.preferredLargeHeapBlockSize = large_allocation ? 16ull * 1024 * 1024 : 1ull * 1024 * 1024;
        info.pVulkanFunctions = &functions;
        if (vmaCreateAllocator(&info, &allocator) != VK_SUCCESS) return false;
        allocator_device = device;
        allocator_large_allocation = large_allocation;
        allocator_system_memory = system_memory;
        host_memory_types = 0;
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vkGetPhysicalDeviceMemoryProperties(physical, &memory_properties);
        for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
            const auto flags = memory_properties.memoryTypes[i].propertyFlags;
            if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                !(flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) host_memory_types |= 1u << i;
        }
    }
    use_pool = true;
    return true;
}
bool vulkan_memory_prefers_device() {
    return use_pool && !allocator_system_memory;
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
    // Prefer device-local heaps even for CPU-visible buffers (BAR/UMA), while
    // preserving required host access/coherence and allowing compatible fallback.
    if (!allocator_system_memory)
        allocation_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (allocator_system_memory && !transient) {
        allocation_info.requiredFlags &= ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        allocation_info.preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        allocation_info.memoryTypeBits = host_memory_types;
    }
    VmaAllocation allocation{};
    VmaAllocationInfo details{};
    VkBuffer raw{};
    VkResult result = vmaCreateBuffer(allocator, &info, &allocation_info, &raw, &allocation, &details);
    if (result != VK_SUCCESS && allocation_info.memoryTypeBits) {
        // Some resources cannot use a non-device-local heap. Keep required CPU access.
        allocation_info.memoryTypeBits = 0;
        result = vmaCreateBuffer(allocator, &info, &allocation_info, &raw, &allocation, &details);
    }
    check(result);
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
    if (allocator_system_memory) {
        allocation_info.preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        allocation_info.memoryTypeBits = host_memory_types;
    } else allocation_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VmaAllocation allocation{};
    VmaAllocationInfo details{};
    VkImage raw{};
    VkResult result = vmaCreateImage(allocator, &info, &allocation_info, &raw, &allocation, &details);
    if (result != VK_SUCCESS && allocation_info.memoryTypeBits) {
        // Optimal images often require device-local memory; preference must not break loading.
        allocation_info.memoryTypeBits = 0;
        result = vmaCreateImage(allocator, &info, &allocation_info, &raw, &allocation, &details);
    }
    check(result);
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
