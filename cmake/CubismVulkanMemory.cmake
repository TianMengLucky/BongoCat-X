# Adapt generated SDK sources; the user's proprietary SDK remains untouched.
set(BONGO_CAT_VMA_INCLUDE_DIR "" CACHE PATH "Directory containing vk_mem_alloc.h")
if(BONGO_CAT_FETCH_DEPS)
  FetchContent_Declare(bongo_vma URL
    https://codeload.github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/tar.gz/refs/tags/v3.3.0
    URL_HASH SHA256=c4f6bbe6b5a45c2eb610ca9d231158e313086d5b1a40c9922cb42b597419b14e)
  FetchContent_GetProperties(bongo_vma)
  if(NOT bongo_vma_POPULATED)
    FetchContent_Populate(bongo_vma)
  endif()
  set(BONGO_CAT_VMA_INCLUDE_DIR "${bongo_vma_SOURCE_DIR}/include")
endif()
if(NOT EXISTS "${BONGO_CAT_VMA_INCLUDE_DIR}/vk_mem_alloc.h")
  message(FATAL_ERROR "Live2D Vulkan requires VMA; enable BONGO_CAT_FETCH_DEPS or set BONGO_CAT_VMA_INCLUDE_DIR")
endif()
target_include_directories(Framework PRIVATE "${PROJECT_SOURCE_DIR}/src/live2d")
target_include_directories(Framework SYSTEM PRIVATE "${BONGO_CAT_VMA_INCLUDE_DIR}")
target_sources(Framework PRIVATE "${PROJECT_SOURCE_DIR}/src/live2d/cubism_vulkan_memory.cpp")

function(bongo_cat_memory_replace variable anchor replacement)
  set(source "${${variable}}")
  string(FIND "${source}" "${anchor}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan memory patch mismatch: ${anchor}")
  endif()
  string(REPLACE "${anchor}" "${replacement}" source "${source}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_vulkan_memory_patch variable)
  set(source "${${variable}}")
  bongo_cat_memory_replace(source [=[#include <stdexcept>]=]
    [=[#include <stdexcept>
#include "cubism_vulkan_memory.hpp"]=])
  bongo_cat_memory_replace(source [=[    VkBufferCreateInfo bufferInfo{};]=]
    [=[    if (bongo_cat::vulkan_buffer_create(device, size, usage, properties, &buffer, &memory)) return;
    VkBufferCreateInfo bufferInfo{};]=])
  bongo_cat_memory_replace(source [=[    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)]=]
    [=[    if (bongo_cat::vulkan_image_create(device, imageInfo, &image, &memory)) return;
    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)]=])
  bongo_cat_memory_replace(source [=[    if (vkMapMemory(device, memory, 0, size, 0, &mapped) != VK_SUCCESS)]=]
    [=[    if (bongo_cat::vulkan_buffer_map(buffer, &mapped)) return;
    if (vkMapMemory(device, memory, 0, size, 0, &mapped) != VK_SUCCESS)]=])
  bongo_cat_memory_replace(source [=[    vkUnmapMemory(device, memory);]=]
    [=[    if (!bongo_cat::vulkan_buffer_unmap(buffer)) vkUnmapMemory(device, memory);]=])
  bongo_cat_memory_replace(source [=[    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);]=]
    [=[    if (!bongo_cat::vulkan_buffer_destroy(buffer)) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }
    buffer = VK_NULL_HANDLE; memory = VK_NULL_HANDLE; mapped = nullptr;]=])
  # Destroy views before images and free memory even when view creation failed.
  set(anchor [=[void CubismImageVulkan::Destroy(VkDevice device)
{]=])
  string(FIND "${source}" "${anchor}" begin)
  if(begin EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan image cleanup patch mismatch")
  endif()
  string(SUBSTRING "${source}" "${begin}" -1 tail)
  string(FIND "${tail}" "\n}\n" length)
  if(length EQUAL -1)
    message(FATAL_ERROR "Cubism Vulkan image cleanup terminator mismatch")
  endif()
  math(EXPR length "${length} + 3")
  string(SUBSTRING "${tail}" 0 "${length}" original)
  bongo_cat_memory_replace(source "${original}" [=[void CubismImageVulkan::Destroy(VkDevice device)
{
    if (sampler) vkDestroySampler(device, sampler, nullptr);
    if (view) vkDestroyImageView(device, view, nullptr);
    if (!bongo_cat::vulkan_image_destroy(image)) {
        if (image) vkDestroyImage(device, image, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
    }
    image = VK_NULL_HANDLE; memory = VK_NULL_HANDLE;
    view = VK_NULL_HANDLE; sampler = VK_NULL_HANDLE;
}
]=])
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()
