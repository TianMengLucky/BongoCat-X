#ifndef BONGO_CAT_RHI_VK_DEBUG_H
#define BONGO_CAT_RHI_VK_DEBUG_H
struct BongoCatRhiVk;
void bongo_cat_rhi_vk_debug_configure(struct BongoCatRhiVk *vk, VkInstanceCreateInfo *info);
void bongo_cat_rhi_vk_debug_start(struct BongoCatRhiVk *vk);
void bongo_cat_rhi_vk_debug_stop(struct BongoCatRhiVk *vk);
#endif
