#ifndef BONGO_CAT_RHI_VK_INTERNAL_H
#define BONGO_CAT_RHI_VK_INTERNAL_H

/* Shared state of the Vulkan RHI backend (see rhi_vk.c). Split across
   rhi_vk.c (device and swapchain lifetime) and rhi_vk_frame.c (frame
   submission and present ops) to keep both within the source size
   policy. The file is only compiled where the backend is available. */

#include "rhi_internal.h"

#if defined(_WIN32) || (defined(__linux__) && defined(__x86_64__))

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

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
    VkSemaphore image_available, render_finished;
    uint32_t acquired_index;
    VkImage current_image;
    VkImageView current_view;
    bool (*hook_draw)(void *hook_user);
    void *hook_user;
    float clear[4];
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
    BONGO_CAT_VK_FN(vkCreateSemaphore)
    BONGO_CAT_VK_FN(vkDestroySemaphore)
    BONGO_CAT_VK_FN(vkAcquireNextImageKHR)
    BONGO_CAT_VK_FN(vkBeginCommandBuffer)
    BONGO_CAT_VK_FN(vkQueueWaitIdle)
    BONGO_CAT_VK_FN(vkFreeCommandBuffers)
#undef BONGO_CAT_VK_FN
} BongoCatRhiVk;

/* Chooses and records a depth format for the Cubism render passes. */
bool bongo_cat_rhi_vk_pick_depth_format(BongoCatRhiVk *vk);
/* Destroys views, framebuffers and the swapchain (device idle required). */
void bongo_cat_rhi_vk_destroy_swapchain_objects(BongoCatRhiVk *vk);
/* Re-creates the swapchain for the requested pixel size. */
bool bongo_cat_rhi_vk_recreate_swapchain(BongoCatRhiVk *vk, int width,
    int height);
/* Presents one cleared frame; also the backend's swap present op. */
bool bongo_cat_rhi_vk_frame_present(void *user);
/* Premultiplied-BGRA / RGBA window readback helpers for present ops. */
bool bongo_cat_rhi_vk_read_bgra(int width, int height, void *pixels,
    void *user);
bool bongo_cat_rhi_vk_read_rgba(int x, int y, int width, int height,
    bool back_buffer, void *pixels, void *user);
const char *bongo_cat_rhi_vk_present_name(void *user);

#endif

#endif
