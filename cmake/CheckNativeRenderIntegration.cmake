cmake_minimum_required(VERSION 3.24)
set(root "${CMAKE_CURRENT_LIST_DIR}/..")
include("${CMAKE_CURRENT_LIST_DIR}/CubismNativeShaderIO.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/CubismVulkanSafety.cmake")
set(depth "view(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)")
bongo_cat_vulkan_depth_aspects(depth)
if(depth MATCHES "STENCIL" OR NOT depth MATCHES "DEPTH_BIT")
  message(FATAL_ERROR "Depth-only Vulkan targets must not request stencil")
endif()
set(vulkan [=[
    std::ifstream file(filename.GetRawString(), std::ios::ate | std::ios::binary);
    csmInt32 fileSize = 0;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.pCode = reinterpret_cast<const csmUint32*>(buffer.GetPtr());
CubismRenderer_Vulkan::~CubismRenderer_Vulkan() {
    _depthImage.Destroy(s_device);
}
void CubismRenderer_Vulkan::DoStaticRelease() {}
]=])
bongo_cat_vulkan_shader_io(vulkan)
if(vulkan MATCHES "std::ifstream" OR NOT vulkan MATCHES "GetLoadFileFunction" OR
    NOT vulkan MATCHES "buffer.data")
  message(FATAL_ERROR "Vulkan shaders must use owned, aligned asset-root bytes")
endif()
set(metal [=[#include "CubismShader_Metal.hpp"
[device newLibraryWithURL:libraryURL error:nil]
[NSData dataWithContentsOfURL:vertShaderFileLibURL]
[NSData dataWithContentsOfURL:fragShaderFileLibURL]
id<MTLRenderPipelineState> CubismShader_Metal::MakeRenderPipelineState(id<MTLDevice> device, CubismShader_Metal::ShaderProgram* shaderProgram, int blendMode)
{
}
]=])
bongo_cat_metal_shader_io(metal)
if(metal MATCHES "newLibraryWithURL" OR metal MATCHES "dataWithContentsOfURL" OR
    NOT metal MATCHES "GetReleaseBytesFunction")
  message(FATAL_ERROR "Metal libraries must load through application-owned assets")
endif()
function(check_sources platform processor headers expected)
  set(WIN32 FALSE)
  if(platform STREQUAL "Windows")
    set(WIN32 TRUE)
  endif()
  set(CMAKE_SYSTEM_NAME "${platform}")
  set(CMAKE_SYSTEM_PROCESSOR "${processor}")
  set(BONGO_CAT_VULKAN_INCLUDE_DIR "${headers}")
  include("${CMAKE_CURRENT_LIST_DIR}/RuntimeSources.cmake")
  list(FIND BONGO_CAT_RENDER_SOURCES "src/render/rhi/rhi_vk.c" native)
  list(FIND BONGO_CAT_RENDER_SOURCES "src/render/rhi/rhi_vk_stub.c" stub)
  if(expected)
    if(native EQUAL -1 OR NOT stub EQUAL -1)
      message(FATAL_ERROR "Missing Vulkan source on ${platform}/${processor}")
    endif()
  elseif(NOT native EQUAL -1 OR stub EQUAL -1)
    message(FATAL_ERROR "Unnecessary Vulkan source on ${platform}/${processor}")
  endif()
endfunction()
check_sources(Windows AMD64 include TRUE)
check_sources(Linux x86_64 include TRUE)
check_sources(Linux aarch64 include FALSE)
check_sources(Darwin arm64 include FALSE)
check_sources(Windows AMD64 "" FALSE)
message(STATUS "Native renderer asset I/O and platform source policy passed")
