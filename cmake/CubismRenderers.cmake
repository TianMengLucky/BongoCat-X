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
# FrameworkShaders/*.spv when available; otherwise the .spv files must be
# supplied manually.

set(BONGO_CAT_CUBISM_VULKAN_DIR
  "${BONGO_CAT_CUBISM_SDK}/Framework/src/Rendering/Vulkan")
set(BONGO_CAT_CUBISM_VULKAN_SHADERS_DIR
  "${BONGO_CAT_CUBISM_VULKAN_DIR}/Shaders")

if(NOT EXISTS "${BONGO_CAT_CUBISM_VULKAN_DIR}/CubismRenderer_Vulkan.hpp")
  message(FATAL_ERROR "BONGO_CAT_CUBISM_VULKAN requires a Cubism 5.1+ SDK "
    "with Framework/src/Rendering/Vulkan (found ${BONGO_CAT_CUBISM_SDK})")
endif()
if(NOT volk_SOURCE_DIR)
  message(FATAL_ERROR "BONGO_CAT_CUBISM_VULKAN requires the volk dependency "
    "(fetched in cmake/Dependencies.cmake)")
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

# CubismUserModel::CreateRenderer must construct the renderer the Live2D
# bridge selected (the OpenGL factory stays canonical): route the
# installation through a bridge-provided creator.
set(BONGO_CAT_USERMODEL_SOURCE
  "${BONGO_CAT_CUBISM_SDK}/Framework/src/Model/CubismUserModel.cpp")
set(BONGO_CAT_USERMODEL_OUTPUT
  "${CMAKE_BINARY_DIR}/cubism-renderer-vulkan/CubismUserModel_routed.cpp")
file(READ "${BONGO_CAT_USERMODEL_SOURCE}" USERMODEL_TEXT)
string(REPLACE "
" "
" USERMODEL_TEXT "${USERMODEL_TEXT}")
set(from_text
  "_renderer = Rendering::CubismRenderer::Create(width, height);")
set(to_text
  "_renderer = bongo_cat_cubism_create_renderer(width, height);")
string(FIND "${USERMODEL_TEXT}" "${from_text}" position)
if(position EQUAL -1)
  message(FATAL_ERROR "The CubismUserModel renderer-installation text "
    "changed; update cmake/CubismRenderers.cmake")
endif()
string(REPLACE "${from_text}" "${to_text}" USERMODEL_TEXT "${USERMODEL_TEXT}")
file(WRITE "${BONGO_CAT_USERMODEL_OUTPUT}" "${USERMODEL_TEXT}")
set_source_files_properties("${BONGO_CAT_USERMODEL_SOURCE}" PROPERTIES
  HEADER_FILE_ONLY ON)

# Optional shader compilation: the renderer loads FrameworkShaders/*.spv from
# the working directory at runtime. Without glslangValidator the build still
# works; the .spv files must then be supplied another way.
find_program(BONGO_CAT_GLSLANG_VALIDATOR NAMES glslangValidator glslang)
set(BONGO_CAT_VULKAN_SHADER_OUTPUT "${CMAKE_BINARY_DIR}/FrameworkShaders")
if(BONGO_CAT_GLSLANG_VALIDATOR)
  file(GLOB BONGO_CAT_VULKAN_SHADER_SOURCES
    "${BONGO_CAT_CUBISM_VULKAN_SHADERS_DIR}/src/*.vert"
    "${BONGO_CAT_CUBISM_VULKAN_SHADERS_DIR}/src/*.frag")
  if(NOT BONGO_CAT_VULKAN_SHADER_SOURCES)
    message(FATAL_ERROR "The Cubism Vulkan shader directory is empty: "
      "${BONGO_CAT_CUBISM_VULKAN_SHADERS_DIR}/src")
  endif()
  file(MAKE_DIRECTORY "${BONGO_CAT_VULKAN_SHADER_OUTPUT}")
  foreach(SHADER ${BONGO_CAT_VULKAN_SHADER_SOURCES})
    # The renderer loads "VertShaderSrc.spv": strip the stage suffix.
    get_filename_component(SHADER_STEM "${SHADER}" NAME_WE)
    set(SHADER_OUTPUT "${BONGO_CAT_VULKAN_SHADER_OUTPUT}/${SHADER_STEM}.spv")
    add_custom_command(OUTPUT "${SHADER_OUTPUT}"
      COMMAND "${BONGO_CAT_GLSLANG_VALIDATOR}" -V "${SHADER}"
        -o "${SHADER_OUTPUT}"
      DEPENDS "${SHADER}"
      COMMENT "glslangValidator ${SHADER_STEM}")
    list(APPEND BONGO_CAT_VULKAN_SHADER_SPVS "${SHADER_OUTPUT}")
  endforeach()
  add_custom_target(bongo_cat_vulkan_shaders ALL
    DEPENDS ${BONGO_CAT_VULKAN_SHADER_SPVS})
  add_dependencies(Framework bongo_cat_vulkan_shaders)
else()
  message(WARNING "glslangValidator not found: the Cubism Vulkan renderer's "
    "FrameworkShaders/*.spv must be supplied manually next to the executable")
endif()
