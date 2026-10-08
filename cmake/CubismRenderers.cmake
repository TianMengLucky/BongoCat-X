# Cubism multi-backend renderer support (Cubism 5.1+ ships Rendering/Vulkan
# and Rendering/Metal next to Rendering/OpenGL). Both implementations compile
# into the Framework, but each backend .cpp defines the CubismRenderer::
# Create/StaticRelease factory members; only the OpenGL one may keep the
# canonical definitions. The patched Vulkan copy strips them: the Live2D
# bridge constructs CubismRenderer_Vulkan through a small derived class
# (derived classes reach the protected constructor) and destroys through
# the canonical path (virtual destructor). All Vulkan calls go through
# volk's runtime table (VK_NO_PROTOTYPES), so no Vulkan SDK import library
# is needed. glslangValidator compiles the renderer's GLSL to
# FrameworkShaders/*.spv, including blend variants, during SDK builds.

set(BONGO_CAT_CUBISM_VULKAN_DIR
  "${BONGO_CAT_CUBISM_SDK}/Framework/src/Rendering/Vulkan")
set(BONGO_CAT_CUBISM_VULKAN_SHADERS_DIR
  "${BONGO_CAT_CUBISM_VULKAN_DIR}/Shaders")

if(NOT EXISTS "${BONGO_CAT_CUBISM_VULKAN_DIR}/CubismRenderer_Vulkan.hpp")
  message(FATAL_ERROR "BONGO_CAT_CUBISM_VULKAN requires a Cubism 5.1+ SDK "
    "with Framework/src/Rendering/Vulkan (found ${BONGO_CAT_CUBISM_SDK})")
endif()
if(NOT volk_SOURCE_DIR)
  find_path(volk_SOURCE_DIR NAMES volk.c)
endif()
if(NOT EXISTS "${volk_SOURCE_DIR}/volk.c" OR
    NOT EXISTS "${volk_SOURCE_DIR}/volk.h" OR
    NOT EXISTS "${BONGO_CAT_VULKAN_INCLUDE_DIR}/vulkan/vulkan.h")
  message(FATAL_ERROR "BONGO_CAT_CUBISM_VULKAN requires Vulkan-Headers and "
    "volk.c/volk.h. Enable BONGO_CAT_FETCH_DEPS, supply "
    "BONGO_CAT_VULKAN_INCLUDE_DIR and volk_SOURCE_DIR, or disable "
    "BONGO_CAT_CUBISM_VULKAN.")
endif()

# The compat shim must precede the real Vulkan-Headers so <vulkan/vulkan.h>
# resolves to the volk-routing header.
# Generated shim: routes <vulkan/vulkan.h> to VK_NO_PROTOTYPES + volk, with
# the real header included by absolute path so directory order cannot break
# the chain (types first, then volk's function pointers).
set(BONGO_CAT_VULKAN_SHIM_DIR "${CMAKE_BINARY_DIR}/vulkan-compat")
file(MAKE_DIRECTORY "${BONGO_CAT_VULKAN_SHIM_DIR}/vulkan")
file(WRITE "${BONGO_CAT_VULKAN_SHIM_DIR}/vulkan/vulkan.h" "#ifndef BONGO_CAT_COMPAT_VULKAN_H
#define BONGO_CAT_COMPAT_VULKAN_H
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include \"${BONGO_CAT_VULKAN_INCLUDE_DIR}/vulkan/vulkan.h\"
#include <volk.h>
#endif
")
target_include_directories(Framework SYSTEM BEFORE PUBLIC
  "${BONGO_CAT_VULKAN_SHIM_DIR}"
  "${volk_SOURCE_DIR}"
  "${BONGO_CAT_VULKAN_INCLUDE_DIR}"
  "${BONGO_CAT_CUBISM_VULKAN_DIR}"
  "${CUBISM_FRAMEWORK_PATH}/src/Model"
  "${CUBISM_GLEW_PATH}/include")
target_compile_definitions(Framework PUBLIC VK_NO_PROTOTYPES=1)

# Compile the Vulkan renderer's support classes (device info, command
# and image classes, offscreen managers) from the SDK's own
# subdirectory; the colliding factory definition inside
# CubismRenderer_Vulkan.cpp is handled by the patched copy below.
set(LIB_NAME Framework)
add_subdirectory("${BONGO_CAT_CUBISM_VULKAN_DIR}"
  "${CMAKE_BINARY_DIR}/cubism-renderer-vulkan-sdk")

# Patched Vulkan renderer copy: strips the colliding factory definitions
# (exact strings from the vendored SDK; a mismatch fails the configure so
# an SDK update can never silently reintroduce the collision).
set(BONGO_CAT_FACTORY_VK_SOURCE
  "${BONGO_CAT_CUBISM_VULKAN_DIR}/CubismRenderer_Vulkan.cpp")
set(BONGO_CAT_FACTORY_VK_OUTPUT
  "${CMAKE_BINARY_DIR}/cubism-renderer-vulkan/CubismRenderer_factory_VK.cpp")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cubism-renderer-vulkan")
file(READ "${BONGO_CAT_FACTORY_VK_SOURCE}" FACTORY_TEXT)
string(REPLACE "\r\n" "\n" FACTORY_TEXT "${FACTORY_TEXT}")
foreach(definition IN ITEMS
    "CubismRenderer* CubismRenderer::Create(csmUint32 width, csmUint32 height)\n{\n    return CSM_NEW CubismRenderer_Vulkan(width, height);\n}\n"
    "void CubismRenderer::StaticRelease()\n{\n    CubismRenderer_Vulkan::DoStaticRelease();\n}\n")
  string(FIND "${FACTORY_TEXT}" "${definition}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "The Cubism Vulkan renderer factory text changed; "
      "update cmake/CubismRenderers.cmake")
  endif()
  string(LENGTH "${definition}" length)
  math(EXPR after "${position} + ${length}")
  string(SUBSTRING "${FACTORY_TEXT}" 0 "${position}" head)
  string(SUBSTRING "${FACTORY_TEXT}" "${after}" -1 tail)
  set(FACTORY_TEXT "${head}${tail}")
endforeach()
# Dynamic cull state is core in the required Vulkan 1.3 API. Resolving the
# EXT alias without enabling that extension can return a null function.
set(cull_lookup [=[vkGetDeviceProcAddr(s_device, "vkCmdSetCullModeEXT")]=])
string(FIND "${FACTORY_TEXT}" "${cull_lookup}" cull_position)
if(cull_position EQUAL -1)
  message(FATAL_ERROR "The Cubism Vulkan cull-state lookup changed; "
    "update cmake/CubismRenderers.cmake")
endif()
string(REPLACE "${cull_lookup}"
  [=[vkGetDeviceProcAddr(s_device, "vkCmdSetCullMode")]=]
  FACTORY_TEXT "${FACTORY_TEXT}")
include(cmake/CubismNativeShaderIO.cmake)
bongo_cat_vulkan_shader_io(FACTORY_TEXT)
include(cmake/CubismVulkanSafety.cmake)
bongo_cat_harden_vulkan_pipelines(FACTORY_TEXT)
bongo_cat_vulkan_depth_aspects(FACTORY_TEXT)
file(WRITE "${BONGO_CAT_FACTORY_VK_OUTPUT}" "${FACTORY_TEXT}")
# Remove the original from the Framework target (source properties set
# from a parent scope do not reach files added in a subdirectory; the
# core-profile patcher uses the same target-SOURCES rewrite.
get_target_property(framework_sources Framework SOURCES)
list(REMOVE_ITEM framework_sources "${BONGO_CAT_FACTORY_VK_SOURCE}")
set_property(TARGET Framework PROPERTY SOURCES "${framework_sources}")
set_source_files_properties(
  "${BONGO_CAT_FACTORY_VK_OUTPUT}"
  "${volk_SOURCE_DIR}/volk.c"
  PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_sources(Framework PRIVATE
  "${BONGO_CAT_FACTORY_VK_OUTPUT}"
  "${volk_SOURCE_DIR}/volk.c")

# CubismUserModel creation is routed in CubismUserModelSafety.cmake, in the
# same generated copy that applies null checks. Never replace the SDK's
# original source after the hardened copy has been added to Framework.

include(cmake/CubismNativeShaders.cmake)
set(BONGO_CAT_VULKAN_SHADER_OUTPUT "${CMAKE_BINARY_DIR}/FrameworkShaders")
bongo_cat_compile_native_shaders(VULKAN "${BONGO_CAT_CUBISM_VULKAN_SHADERS_DIR}/src"
  "${BONGO_CAT_VULKAN_SHADER_OUTPUT}")
bongo_cat_harden_vulkan_resources(Framework)
bongo_cat_harden_vulkan_render_targets(Framework)
