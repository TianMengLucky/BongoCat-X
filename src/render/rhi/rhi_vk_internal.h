#ifndef BONGO_CAT_RHI_VK_INTERNAL_H
#define BONGO_CAT_RHI_VK_INTERNAL_H

/* Shared state of the Vulkan RHI backend (see rhi_vk.c). Split across
   rhi_vk.c (device and swapchain lifetime) and rhi_vk_frame.c (frame
   submission and present ops) to keep both within the source size
   policy. The file is only compiled where the backend is available. */

#include "rhi_internal.h"

#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
#include <volk.h>
#else
#include <vulkan/vulkan.h>
#endif

typedef struct BongoCatRhiVk {
    char describe[160];
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical;
    VkDevice device;
    uint32_t queue_family;
    VkQueue queue;
    VkSwapchainKHR swapchain;
    VkFormat format;
    VkFormat depth_format;
    VkExtent2D extent;
    VkImage *images;
    uint32_t image_count;
    VkImageView *views;
    VkFramebuffer *framebuffers;
    VkRenderPass render_pass;
    VkCommandPool pool;
    VkCommandBuffer command;
    VkFence fence;
    bool frame_ready;
    VkImageLayout frame_layout;
    VkBuffer readback_buffer;
    VkDeviceMemory readback_memory;
    VkDeviceSize readback_size;
    uint8_t *pixels;
    bool pixels_valid;
    const BongoCatRhi *owner;
    uint32_t acquired_index;
    VkImage current_image;
    VkImageView current_view;
    float clear[4];
    VkImage background;
    VkDeviceMemory background_memory;
    uint64_t background_revision;
    bool background_ready;
    PFN_vkGetInstanceProcAddr instance_gpa;
#define BONGO_CAT_VK_FN(name) PFN_##name name;
    BONGO_CAT_VK_FN(vkCreateInstance)
    BONGO_CAT_VK_FN(vkDestroyInstance)
    BONGO_CAT_VK_FN(vkEnumeratePhysicalDevices)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceProperties)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceQueueFamilyProperties)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceSurfaceSupportKHR)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceSurfaceFormatsKHR)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceFormatProperties)
    BONGO_CAT_VK_FN(vkCreateDevice)
    BONGO_CAT_VK_FN(vkDestroyDevice)
    BONGO_CAT_VK_FN(vkDeviceWaitIdle)
    BONGO_CAT_VK_FN(vkGetDeviceQueue)
    BONGO_CAT_VK_FN(vkCreateSwapchainKHR)
    BONGO_CAT_VK_FN(vkDestroySwapchainKHR)
    BONGO_CAT_VK_FN(vkGetSwapchainImagesKHR)
    BONGO_CAT_VK_FN(vkCreateImageView)
    BONGO_CAT_VK_FN(vkDestroyImageView)
    BONGO_CAT_VK_FN(vkCreateRenderPass)
    BONGO_CAT_VK_FN(vkDestroyRenderPass)
    BONGO_CAT_VK_FN(vkCreateFramebuffer)
    BONGO_CAT_VK_FN(vkDestroyFramebuffer)
    BONGO_CAT_VK_FN(vkCreateCommandPool)
    BONGO_CAT_VK_FN(vkDestroyCommandPool)
    BONGO_CAT_VK_FN(vkAllocateCommandBuffers)
    BONGO_CAT_VK_FN(vkCmdBeginRenderPass)
    BONGO_CAT_VK_FN(vkCmdEndRenderPass)
    BONGO_CAT_VK_FN(vkEndCommandBuffer)
    BONGO_CAT_VK_FN(vkQueueSubmit)
    BONGO_CAT_VK_FN(vkQueuePresentKHR)
    BONGO_CAT_VK_FN(vkWaitForFences)
    BONGO_CAT_VK_FN(vkResetFences)
    BONGO_CAT_VK_FN(vkCreateFence)
    BONGO_CAT_VK_FN(vkDestroyFence)
    BONGO_CAT_VK_FN(vkAcquireNextImageKHR)
    BONGO_CAT_VK_FN(vkBeginCommandBuffer)
    BONGO_CAT_VK_FN(vkQueueWaitIdle)
    BONGO_CAT_VK_FN(vkFreeCommandBuffers)
    BONGO_CAT_VK_FN(vkResetCommandBuffer)
    BONGO_CAT_VK_FN(vkCreateBuffer)
    BONGO_CAT_VK_FN(vkDestroyBuffer)
    BONGO_CAT_VK_FN(vkGetBufferMemoryRequirements)
    BONGO_CAT_VK_FN(vkAllocateMemory)
    BONGO_CAT_VK_FN(vkFreeMemory)
    BONGO_CAT_VK_FN(vkBindBufferMemory)
    BONGO_CAT_VK_FN(vkMapMemory)
    BONGO_CAT_VK_FN(vkUnmapMemory)
    BONGO_CAT_VK_FN(vkGetPhysicalDeviceMemoryProperties)
    BONGO_CAT_VK_FN(vkCmdPipelineBarrier)
    BONGO_CAT_VK_FN(vkCmdCopyImageToBuffer)
    BONGO_CAT_VK_FN(vkCreateImage)
    BONGO_CAT_VK_FN(vkDestroyImage)
    BONGO_CAT_VK_FN(vkGetImageMemoryRequirements)
    BONGO_CAT_VK_FN(vkBindImageMemory)
    BONGO_CAT_VK_FN(vkCmdCopyBufferToImage)
    BONGO_CAT_VK_FN(vkCmdCopyImage)
#undef BONGO_CAT_VK_FN
} BongoCatRhiVk;

bool bongo_cat_rhi_vk_load_global_fns(BongoCatRhiVk *vk,
    BongoCatError *error);
bool bongo_cat_rhi_vk_load_instance_fns(BongoCatRhiVk *vk);
#ifdef BONGO_CAT_HAS_CUBISM_VULKAN
bool bongo_cat_rhi_vk_cubism_features(BongoCatRhiVk *vk,
    VkPhysicalDevice device, VkPhysicalDeviceVulkan13Features *enabled);
#endif

void bongo_cat_rhi_vk_release_background(BongoCatRhiVk *vk);
bool bongo_cat_rhi_vk_draw_background(BongoCatRhiVk *vk);
void bongo_cat_rhi_vk_release_readback(BongoCatRhiVk *vk);
bool bongo_cat_rhi_vk_capture_frame(BongoCatRhiVk *vk);
/* Chooses and records a depth format for the Cubism render passes. */
bool bongo_cat_rhi_vk_pick_depth_format(BongoCatRhiVk *vk);
/* Destroys views, framebuffers and the swapchain (device idle required). */
void bongo_cat_rhi_vk_destroy_swapchain_objects(BongoCatRhiVk *vk);
/* Re-creates the swapchain for the requested pixel size. */
bool bongo_cat_rhi_vk_create_swapchain(BongoCatRhiVk *vk, int width, int height,
    BongoCatError *error);
bool bongo_cat_rhi_vk_recreate_swapchain(BongoCatRhiVk *vk, int width,
    int height);
/* Presents the prepared acquired frame; also the swap present op. */
bool bongo_cat_rhi_vk_frame_present(SDL_Window *window, void *user);
/* Premultiplied-BGRA / RGBA window readback helpers for present ops. */
bool bongo_cat_rhi_vk_read_bgra(int width, int height, void *pixels,
    void *user);
bool bongo_cat_rhi_vk_read_rgba(int x, int y, int width, int height,
    bool back_buffer, void *pixels, void *user);
const char *bongo_cat_rhi_vk_present_name(void *user);

#endif

#endif
