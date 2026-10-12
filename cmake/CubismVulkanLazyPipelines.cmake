# Store shader descriptions at initialization; materialize only used variants.
# All changes apply to the generated SDK translation unit, never the SDK itself.
function(bongo_cat_vulkan_lazy_pipelines variable)
  set(source "${${variable}}")
  bongo_cat_recording_replace(source [=[#include "CubismRenderer_Vulkan.hpp"]=]
    [=[#include "CubismRenderer_Vulkan.hpp"
#include <map>
#include <mutex>]=])
  bongo_cat_recording_replace(source [=[CubismPipeline_Vulkan::CubismPipeline_Vulkan()
{}]=] [=[namespace {
struct BongoPipelineDescription {
    CubismPipeline_Vulkan::PipelineResource *resource;
    csmString vertex, fragment;
    VkDescriptorSetLayout layout;
    csmUint32 shader;
    csmInt32 color, alpha;
    csmUint32 ready = 0;
};
using BongoPipelineKey = std::pair<CubismPipeline_Vulkan *, csmUint32>;
std::map<BongoPipelineKey, BongoPipelineDescription> bongoPipelineDescriptions;
std::mutex bongoPipelineMutex;
thread_local int bongoPipelineBlend = 0;
void BongoEnsurePipeline(CubismPipeline_Vulkan *owner, csmInt32 shader, csmInt32 blend) {
    // Add/multiply aliases share the six normal shader resources.
    if (shader >= ShaderNames_Add && shader <= ShaderNames_MultMaskedInvertedPremultipliedAlpha)
        shader = ShaderNames_Normal + (shader - ShaderNames_Add) % 6;
    std::lock_guard<std::mutex> lock(bongoPipelineMutex);
    auto found = bongoPipelineDescriptions.find({owner, (csmUint32)shader});
    if (found == bongoPipelineDescriptions.end())
        throw std::runtime_error("Missing Live2D Vulkan pipeline description");
    auto &description = found->second;
    if (blend < 0 || blend > 3) throw std::runtime_error("Invalid Vulkan blend variant");
    const bool single = description.shader == ShaderNames_Copy ||
        (description.color != ColorBlendMode_None && description.alpha != AlphaBlendMode_None);
    const csmUint32 bit = 1u << (single ? 0 : blend);
    if (description.ready & bit) return;
    bongoPipelineBlend = blend;
    try {
        if (!description.resource->CreateGraphicsPipeline(description.vertex,
                description.fragment, description.layout, description.shader,
                description.color, description.alpha))
            throw std::runtime_error("Cannot create Live2D Vulkan pipeline");
        description.ready |= bit;
    } catch (...) {
        // No command can reference this failed creation. Dispose partial handles
        // before propagating; shader modules use the existing RAII guard.
        if (!description.ready) description.resource->Release();
        throw;
    }
}
}
CubismPipeline_Vulkan::CubismPipeline_Vulkan()
{}]=])
  bongo_cat_recording_replace(source [=[    if (!_pipelineResource[shaderName]->CreateGraphicsPipeline(vertShaderFileName, fragShaderFileName, descriptorSetLayout, shaderName, colorBlendMode, alphaBlendMode))
    {
        _pipelineResource[shaderName] = NULL;
        CubismLogError("failed to create shader %d", shaderName);
    }]=] [=[    std::lock_guard<std::mutex> lock(bongoPipelineMutex);
    bongoPipelineDescriptions.emplace(BongoPipelineKey{this, shaderName},
        BongoPipelineDescription{_pipelineResource[shaderName], vertShaderFileName,
            fragShaderFileName, descriptorSetLayout, shaderName, colorBlendMode, alphaBlendMode});]=])
  bongo_cat_recording_replace(source [=[void CubismPipeline_Vulkan::ReleaseShaderProgram()
{]=] [=[void CubismPipeline_Vulkan::ReleaseShaderProgram()
{
    // The existing device/worker shutdown waits precede this release.
    std::lock_guard<std::mutex> lock(bongoPipelineMutex);
    for (auto it = bongoPipelineDescriptions.begin(); it != bongoPipelineDescriptions.end();) {
        if (it->first.first == this) it = bongoPipelineDescriptions.erase(it);
        else ++it;
    }]=])
  bongo_cat_recording_replace(source [=[void CubismPipeline_Vulkan::CreatePipelines(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSetLayout copyDescriptorSetLayout)
{]=] [=[void CubismPipeline_Vulkan::CreatePipelines(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSetLayout copyDescriptorSetLayout)
{
    // Reload may destroy the old renderer's descriptor layouts. Refresh pending
    // descriptions before first use; already created layouts remain compatible.
    {
        std::lock_guard<std::mutex> lock(bongoPipelineMutex);
        for (auto &entry : bongoPipelineDescriptions) {
            if (entry.first.first == this)
                entry.second.layout = entry.second.shader == ShaderNames_Copy ?
                    copyDescriptorSetLayout : descriptorSetLayout;
        }
    }]=])
  set(anchor "    VkPipelineLayout pipelineLayout = _deviceInfo->GetPipeline()->GetPipelineLayout(shaderIndex, blendIndex);")
  string(REGEX MATCHALL "VkPipelineLayout pipelineLayout = " matches "${source}")
  list(LENGTH matches count)
  if(NOT count EQUAL 3)
    message(FATAL_ERROR "Cubism Vulkan lazy pipeline draw sites changed")
  endif()
  string(REPLACE "${anchor}" "    BongoEnsurePipeline(_deviceInfo->GetPipeline(), shaderIndex, blendIndex);
${anchor}" source "${source}")
  bongo_cat_recording_replace(source [=[    Descriptor &descriptor = _copyDescriptorSets[_commandBufferCurrent];]=]
    [=[    BongoEnsurePipeline(_deviceInfo->GetPipeline(), ShaderNames_Copy, 0);
    Descriptor &descriptor = _copyDescriptorSets[_commandBufferCurrent];]=])
  bongo_cat_vulkan_pipeline_variants(source)
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

# Keep vector slots stable; a failed new variant must not destroy live siblings.
function(bongo_cat_vulkan_pipeline_variants variable)
  set(source "${${variable}}")
  set(marker "                                                                     csmInt32 colorBlendMode, csmInt32 alphaBlendMode)\n{")
  bongo_cat_recording_replace(source "${marker}" [=[                                                                     csmInt32 colorBlendMode, csmInt32 alphaBlendMode)
{
    struct NewVariant {
        VkPipeline *pipeline = nullptr;
        VkPipelineLayout *layout = nullptr;
        bool committed = false;
        ~NewVariant() {
            if (committed) return;
            if (pipeline && *pipeline) { vkDestroyPipeline(s_device, *pipeline, nullptr); *pipeline = VK_NULL_HANDLE; }
            if (layout && *layout) { vkDestroyPipelineLayout(s_device, *layout, nullptr); *layout = VK_NULL_HANDLE; }
        }
    } variant;]=])
  foreach(blend IN ITEMS 0 CompatibleBlend::Blend_Normal CompatibleBlend::Blend_Add CompatibleBlend::Blend_Mult CompatibleBlend::Blend_Mask)
    set(anchor "        if (vkCreatePipelineLayout(s_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout[${blend}]) != VK_SUCCESS)")
    string(FIND "${source}" "${anchor}" begin)
    if(begin LESS 0)
      message(FATAL_ERROR "Vulkan blend variant creation changed: ${blend}")
    endif()
    string(SUBSTRING "${source}" ${begin} -1 tail)
    string(FIND "${tail}" "\n        }\n" first_end)
    math(EXPR second_begin "${first_end} + 11")
    string(SUBSTRING "${tail}" ${second_begin} -1 rest)
    string(FIND "${rest}" "\n        }\n" second_end)
    if(first_end LESS 0 OR second_end LESS 0)
      message(FATAL_ERROR "Vulkan blend variant error boundary changed")
    endif()
    math(EXPR length "${second_begin} + ${second_end} + 11")
    string(SUBSTRING "${tail}" 0 ${length} body)
    set(wrapped "        variant.pipeline = &_pipeline[${blend}];\n        variant.layout = &_pipelineLayout[${blend}];\n${body}")
    if(NOT blend STREQUAL "0")
      set(wrapped "        if (bongoPipelineBlend == ${blend}) {\n${wrapped}        }\n")
    endif()
    string(REPLACE "${body}" "${wrapped}" source "${source}")
  endforeach()
  # Commit this RAII guard only in CreateGraphicsPipeline, not other SDK methods.
  set(anchor "    return true;\n}\n\nvoid CubismPipeline_Vulkan::PipelineResource::Release()")
  bongo_cat_recording_replace(source "${anchor}" "    variant.committed = true;\n${anchor}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

