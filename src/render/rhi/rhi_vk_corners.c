/* Four corner quads multiply existing premultiplied color and alpha in place.
   No intermediate image, vertex buffer, descriptor set or CPU pixel pass. */
#include "rhi_vk_internal.h"
#include "rhi_corners.h"
#include "bongo_cat/safe_ffi.h"
#include <stdlib.h>
#if defined(BONGO_CAT_HAS_VULKAN_RHI) && \
    (defined(_WIN32) || (defined(__linux__) && defined(__x86_64__)))

void bongo_cat_rhi_vk_release_corner_framebuffers(BongoCatRhiVk *vk) {
    if (vk->corner_framebuffers) {
        for (uint32_t i = 0; i < vk->image_count; ++i)
            if (vk->corner_framebuffers[i])
                vk->vkDestroyFramebuffer(vk->device, vk->corner_framebuffers[i], NULL);
        free(vk->corner_framebuffers);
        vk->corner_framebuffers = NULL;
    }
}
void bongo_cat_rhi_vk_release_corners(BongoCatRhiVk *vk) {
    bongo_cat_rhi_vk_release_corner_framebuffers(vk);
    if (vk->corner_pipeline) vk->vkDestroyPipeline(vk->device, vk->corner_pipeline, NULL);
    if (vk->corner_layout) vk->vkDestroyPipelineLayout(vk->device, vk->corner_layout, NULL);
    if (vk->corner_pass) vk->vkDestroyRenderPass(vk->device, vk->corner_pass, NULL);
    vk->corner_pipeline = VK_NULL_HANDLE;
    vk->corner_layout = VK_NULL_HANDLE;
    vk->corner_pass = VK_NULL_HANDLE;
}
static bool prepare_framebuffers(BongoCatRhiVk *vk) {
    if (vk->corner_framebuffers) return true;
    vk->corner_framebuffers = calloc(vk->image_count, sizeof(VkFramebuffer));
    if (!vk->corner_framebuffers) return false;
    for (uint32_t i = 0; i < vk->image_count; ++i) {
        VkFramebufferCreateInfo fb = {0};
        fb.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb.renderPass = vk->corner_pass;
        fb.attachmentCount = 1; fb.pAttachments = &vk->views[i];
        fb.width = vk->extent.width; fb.height = vk->extent.height; fb.layers = 1;
        if (vk->vkCreateFramebuffer(vk->device, &fb, NULL,
            &vk->corner_framebuffers[i]) != VK_SUCCESS) {
            bongo_cat_rhi_vk_release_corner_framebuffers(vk);
            return false;
        }
    }
    return true;
}
static bool prepare(BongoCatRhiVk *vk) {
    if (vk->corner_pipeline && vk->corner_format == vk->format)
        return prepare_framebuffers(vk);
    bongo_cat_rhi_vk_release_corners(vk);
    VkAttachmentDescription attachment = {0};
    attachment.format = vk->format; attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference reference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &reference;
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask = dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo pass = {0};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass.attachmentCount = 1; pass.pAttachments = &attachment;
    pass.subpassCount = 1; pass.pSubpasses = &subpass;
    pass.dependencyCount = 1; pass.pDependencies = &dependency;
    if (vk->vkCreateRenderPass(vk->device, &pass, NULL, &vk->corner_pass) != VK_SUCCESS) goto failed;
    VkPushConstantRange range = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 32};
    VkPipelineLayoutCreateInfo layout = {0};
    layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &range;
    if (vk->vkCreatePipelineLayout(vk->device, &layout, NULL, &vk->corner_layout) != VK_SUCCESS) goto failed;
    VkShaderModule modules[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkPipelineShaderStageCreateInfo stages[2] = {{0}, {0}};
    bool ok = true;
    for (int i = 0; i < 2; ++i) {
        VkShaderModuleCreateInfo shader = {0};
        shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader.pCode = bongo_safe_corner_shader(i != 0, &shader.codeSize);
        if (vk->vkCreateShaderModule(vk->device, &shader, NULL, &modules[i]) != VK_SUCCESS) { ok = false; break; }
        stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[i].stage = i ? VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT;
        stages[i].module = modules[i]; stages[i].pName = i ? "fragment" : "vertex";
    }
    VkPipelineVertexInputStateCreateInfo vertex = {0};
    vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly = {0};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport = {0};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster = {0};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL; raster.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo samples = {0};
    samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend = {0};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.dstColorBlendFactor = blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.colorBlendOp = blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blending = {0};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1; blending.pAttachments = &blend;
    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic = {0};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2; dynamic.pDynamicStates = dynamic_states;
    VkGraphicsPipelineCreateInfo pipeline = {0};
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.stageCount = 2; pipeline.pStages = stages;
    pipeline.pVertexInputState = &vertex; pipeline.pInputAssemblyState = &assembly;
    pipeline.pViewportState = &viewport; pipeline.pRasterizationState = &raster;
    pipeline.pMultisampleState = &samples; pipeline.pColorBlendState = &blending;
    pipeline.pDynamicState = &dynamic;
    pipeline.layout = vk->corner_layout; pipeline.renderPass = vk->corner_pass;
    if (ok) ok = vk->vkCreateGraphicsPipelines(vk->device, VK_NULL_HANDLE,
        1, &pipeline, NULL, &vk->corner_pipeline) == VK_SUCCESS;
    for (int i = 0; i < 2; ++i)
        if (modules[i]) vk->vkDestroyShaderModule(vk->device, modules[i], NULL);
    if (ok) {
        vk->corner_format = vk->format;
        if (prepare_framebuffers(vk)) return true;
    }
failed:
    bongo_cat_rhi_vk_release_corners(vk);
    return false;
}
bool bongo_cat_rhi_vk_draw_corners(BongoCatRhiVk *vk, VkCommandBuffer command) {
    float params[8];
    if (!bongo_cat_rhi_corner_params(vk->owner->window, (int)vk->extent.width,
        (int)vk->extent.height, params)) return true;
    if (!prepare(vk)) return false;
    VkRenderPassBeginInfo pass = {0};
    pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass.renderPass = vk->corner_pass; pass.framebuffer = vk->corner_framebuffers[vk->acquired_index];
    pass.renderArea.extent = vk->extent;
    vk->vkCmdBeginRenderPass(command, &pass, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = {0, 0, (float)vk->extent.width, (float)vk->extent.height, 0, 1};
    VkRect2D scissor = {{0, 0}, vk->extent};
    vk->vkCmdSetViewport(command, 0, 1, &viewport);
    vk->vkCmdSetScissor(command, 0, 1, &scissor);
    vk->vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->corner_pipeline);
    vk->vkCmdPushConstants(command, vk->corner_layout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(params), params);
    vk->vkCmdDraw(command, 24, 1, 0, 0);
    vk->vkCmdEndRenderPass(command);
    return true;
}
#endif
