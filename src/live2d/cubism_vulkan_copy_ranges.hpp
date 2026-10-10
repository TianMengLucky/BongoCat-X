#pragma once
#include <vulkan/vulkan.h>
#include <vector>
namespace bongo_cat {
// Meshes arrive in ascending buffer order; merge adjacency, retain static gaps.
inline void vulkan_copy_range_append(std::vector<VkBufferCopy> &ranges,
    VkDeviceSize offset, VkDeviceSize size) {
    if (!size) return;
    if (!ranges.empty() && ranges.back().dstOffset + ranges.back().size == offset)
        ranges.back().size += size;
    else ranges.push_back({offset, offset, size});
}
inline void vulkan_copy_ranges_rebase(std::vector<VkBufferCopy> &ranges,
    VkDeviceSize staging_origin) {
    for (auto &range : ranges) range.srcOffset -= staging_origin;
}
}
