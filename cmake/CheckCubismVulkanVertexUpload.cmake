cmake_minimum_required(VERSION 3.24)
include("${CMAKE_CURRENT_LIST_DIR}/CubismVulkanVertexUpload.cmake")
if(NOT DEFINED SDK OR SDK STREQUAL "")
  get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
  set(SDK "${ROOT}/vendor/CubismSdkForNative")
endif()
set(renderer "${SDK}/Framework/src/Rendering/Vulkan/CubismRenderer_Vulkan.cpp")
if(NOT EXISTS "${renderer}")
  message(STATUS "Cubism Vulkan vertex upload check skipped: local SDK absent")
  return()
endif()
file(SHA256 "${renderer}" original_hash)
file(READ "${renderer}" source)
string(REPLACE "\r\n" "\n" source "${source}")
if(MUTATION STREQUAL "missing_targets")
  string(REPLACE "csmInt32 createSize = 1;" "csmInt32 createSize = 2;" source "${source}")
elseif(MUTATION STREQUAL "missing_descriptors")
  string(REPLACE "descriptorSetCountPerBuffer = drawableCount * drawModeCount"
    "descriptorSetCountPerBuffer = drawableCount * 4" source "${source}")
elseif(MUTATION STREQUAL "missing_descriptor_slots")
  string(REPLACE "descriptorSets[drawAssign * 3 + 2]"
    "descriptorSets[drawAssign * 4 + 2]" source "${source}")
elseif(MUTATION STREQUAL "missing_pipeline")
  string(REPLACE "_pipelineResource[shaderName]->CreateGraphicsPipeline" "changed_pipeline_creation" source "${source}")
elseif(MUTATION STREQUAL "missing_binding")
  string(REPLACE "_textures.PushBack(image);" "changed_texture_binding();" source "${source}")
elseif(MUTATION STREQUAL "missing_wait")
  string(REPLACE "vkQueueWaitIdle(s_queue);" "changed_async_submit();" source "${source}")
elseif(MUTATION STREQUAL "missing_slots")
  string(REPLACE "s_bufferSetNum = swapchainImageCount;" "s_bufferSetNum = 3;" source "${source}")
elseif(MUTATION STREQUAL "missing")
  string(REPLACE "vertices.PushBack(vertex);" "changed_sdk_upload();" source "${source}")
elseif(MUTATION STREQUAL "missing_pass")
  string(REPLACE "void CubismRenderer_Vulkan::DoDrawModel()"
    "void CubismRenderer_Vulkan::ChangedDrawModel()" source "${source}")
elseif(MUTATION STREQUAL "ambiguous")
  string(APPEND source "\n        vertices.PushBack(vertex);\n")
endif()
bongo_cat_vulkan_vertex_upload_patch(source)
foreach(required IN ITEMS "_copyVertexBuffer[buffer] = copyVertexBuffer;\n        }"
    "stagingBuffer.Destroy(s_device);\n        }"
    "descriptorSetCountPerBuffer = drawableCount;"
    "csmInt32 createSize = model->IsBlendModeEnabled() ? 1 : 0;"
    "if (GetModel()->IsBlendModeEnabled() || GetModel()->GetOffscreenCount() > 0)"
    "if (!GetModel()->IsBlendModeEnabled() && offscreenCount == 0) continue;"
    "_copyVertexBuffer[buffer] = copyVertexBuffer;\n        }"
    "_copyIndexBuffer[buffer] = copyIndexBuffer;\n        stagingBuffer.Destroy(s_device);\n        }"
    "_drawableMaskBuffers[buffer].Resize(drawableMaskCount);"
    "_offscreenMaskBuffers[buffer].Resize(offscreenMaskCount);"
    "GetModel()->GetOffscreenMaskCounts()" "GetModel()->GetOffscreenMasks()"
    "if (!maskSources[source])" "if (source < 0 || source >= drawableCount)"
    "descriptor.descriptorSetMasked = descriptor.descriptorSet;"
    "if (maskSources[drawAssign])"
    "descriptor.descriptorSetForMask = descriptorSets[nextDescriptor++];")
  string(FIND "${source}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Compact drawable descriptors missing: ${required}")
  endif()
endforeach()
foreach(required IN ITEMS "if (!descriptor.uniformBuffer.GetBuffer())" "if (!descriptor.uniformBufferMask.GetBuffer())" "descriptor.uniformBuffer = CubismBufferVulkan();" "descriptor.uniformBufferMask = CubismBufferVulkan();" "_textures[slot] = image;"
    "isDescriptorSetUpdated = false;" "isDescriptorSetMaskedUpdated = false;"
    "isDescriptorSetForMaskUpdated = false;" "s_bufferSetNum = 1;" "vulkan_buffer_mapped_data(" "if (!mapped) vertices.PrepareCapacity(vcount);"
    "if (vcount <= 0) return;" "if (mapped) std::memcpy(mapped +"
    "if (!mapped)\n        _stagingBuffers" "vkCmdCopyBuffer(commandBuffer,"
    "if (!_drawableClippingManager && !_offscreenClippingManager)"
    "DrawObjectLoop(updateCommandBuffer, drawCommandBuffer, beginInfo);\n        return;")
  string(FIND "${source}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Vertex upload behavior missing: ${required}")
  endif()
endforeach()
# Verify that the independent secondary-recording adaptation still composes.
include("${CMAKE_CURRENT_LIST_DIR}/CubismVulkanRecording.cmake")
bongo_cat_vulkan_recording_patch(source)
include("${CMAKE_CURRENT_LIST_DIR}/CubismVulkanLazyPipelines.cmake")
bongo_cat_vulkan_lazy_pipelines(source)
foreach(required IN ITEMS "bongoPipelineDescriptions.emplace" "variant.committed = true;" "if (bongoPipelineBlend == CompatibleBlend::Blend_Mask)" "if (description.ready & bit) return;"
    "if (!description.ready) description.resource->Release();" "std::lock_guard<std::mutex> lock(bongoPipelineMutex);"
    "BongoEnsurePipeline(_deviceInfo->GetPipeline(), ShaderNames_Copy, 0);")
  string(FIND "${source}" "${required}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Lazy pipeline ownership behavior missing: ${required}")
  endif()
endforeach()
foreach(mutation IN ITEMS missing ambiguous missing_pass missing_wait missing_slots missing_binding missing_pipeline missing_descriptors missing_descriptor_slots missing_targets)
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DSDK=${SDK}" "-DMUTATION=${mutation}"
    -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
  if(result EQUAL 0 OR NOT error MATCHES "Cubism Vulkan (vertex upload|recording) patch")
    message(FATAL_ERROR "Vertex upload SDK mismatch check failed: ${mutation}: ${error}")
  endif()
endforeach()
file(SHA256 "${renderer}" current_hash)
if(NOT current_hash STREQUAL original_hash)
  message(FATAL_ERROR "Vertex upload patch modified the user SDK")
endif()
message(STATUS "Vulkan direct vertex upload, fallback and SDK mismatch checks passed")
