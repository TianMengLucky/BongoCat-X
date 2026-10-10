include("${CMAKE_CURRENT_LIST_DIR}/WriteIfDifferent.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/CubismVulkanRecording.cmake")

function(bongo_cat_vulkan_bindless_patch variable)
  set(source "${${variable}}")
  bongo_cat_recording_replace(source [=[#include "CubismRenderer_Vulkan.hpp"]=]
    [=[#include "CubismRenderer_Vulkan.hpp"
#include "cubism_vulkan_bindless.hpp"
#include <string>]=])
  bongo_cat_recording_replace(source
    [=[    VkShaderModule vertShaderModule = modules.vertex = CreateShaderModule(s_device, vertFileName);]=]
    [=[    if (bongo_cat::vulkan_bindless_active()) {
        std::string file(fragFileName.GetRawString());
        const auto extension = file.rfind(".spv");
        if (extension == std::string::npos) throw std::runtime_error("Invalid Cubism shader name");
        file.insert(extension, "_Bindless");
        fragFileName = csmString(file.c_str());
    }
    VkShaderModule vertShaderModule = modules.vertex = CreateShaderModule(s_device, vertFileName);]=])
  bongo_cat_recording_replace(source
    [=[    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;]=]
    [=[    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    const VkDescriptorSetLayout atlasLayouts[] = {descriptorSetLayout, bongo_cat::vulkan_bindless_layout()};
    const VkPushConstantRange atlasIndex{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t)};
    if (bongo_cat::vulkan_bindless_active()) {
        pipelineLayoutInfo.setLayoutCount = 2;
        pipelineLayoutInfo.pSetLayouts = atlasLayouts;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &atlasIndex;
    }]=])
  bongo_cat_recording_function(source UpdateDescriptorSet
    [=[    descriptorWrites.PushBack(image1);]=]
    [=[    if (!bongo_cat::vulkan_bindless_active()) descriptorWrites.PushBack(image1);]=])
  bongo_cat_recording_function(source UpdateDescriptorSetForMask
    [=[    descriptorWrites.PushBack(image);]=]
    [=[    if (!bongo_cat::vulkan_bindless_active()) descriptorWrites.PushBack(image);]=])
  # Prepare the atlas binding only for inline draws. Parallel workers bind
  # the same immutable table from their copied texture index instead.
  bongo_cat_recording_function(source ExecuteDrawForDrawable
    [=[    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,]=]
    [=[    bongo_cat::vulkan_bindless_bind(cmdBuffer, pipelineLayout, textureIndex);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,]=])
  bongo_cat_recording_function(source ExecuteDrawForMask
    [=[    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,]=]
    [=[    bongo_cat::vulkan_bindless_bind(cmdBuffer, pipelineLayout, textureIndex);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,]=])
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_vulkan_bindless_common variable)
  set(source "${${variable}}")
  bongo_cat_recording_replace(source
    [=[layout(binding = 1) uniform sampler2D s_texture0;]=]
    [=[#ifdef BONGO_CAT_BINDLESS
layout(set = 1, binding = 0) uniform sampler2D bongo_atlases[64];
layout(push_constant) uniform BongoAtlasIndex { uint texture; } bongo_atlas;
#define s_texture0 bongo_atlases[bongo_atlas.texture]
#else
layout(binding = 1) uniform sampler2D s_texture0;
#endif]=])
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_prepare_vulkan_bindless_shaders input output)
  file(MAKE_DIRECTORY "${output}")
  file(GLOB shaders CONFIGURE_DEPENDS "${input}/*.vert" "${input}/*.frag" "${input}/*.glsl")
  foreach(shader IN LISTS shaders)
    get_filename_component(name "${shader}" NAME)
    if(name STREQUAL "common.glsl")
      file(READ "${shader}" source)
      string(REPLACE "\r\n" "\n" source "${source}")
      bongo_cat_vulkan_bindless_common(source)
      bongo_cat_write_if_different("${output}/${name}" "${source}")
    else()
      configure_file("${shader}" "${output}/${name}" COPYONLY)
    endif()
  endforeach()
endfunction()
