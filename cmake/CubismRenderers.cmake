# Cubism multi-backend renderer support (Cubism 5.1+ ships Rendering/Vulkan
# and Rendering/Metal next to Rendering/OpenGL). Probes the vendored SDK and
# wires the optional renderer builds: the Vulkan renderer sources compile
# into the Framework target and its GLSL shaders build to .spv through
# glslangValidator; the Metal renderer sources (ObjC++) compile on Apple.
# The bridge gates its backend paths on BONGO_CAT_HAS_CUBISM_VULKAN /
# BONGO_CAT_HAS_CUBISM_METAL, so a probe miss degrades to the GL-only
# behaviour instead of failing the build.

set(BONGO_CAT_CUBISM_VULKAN_DIR
  "${BONGO_CAT_CUBISM_SDK}/Framework/src/Rendering/Vulkan")
set(BONGO_CAT_CUBISM_VULKAN_SHADERS_DIR
  "${BONGO_CAT_CUBISM_SDK}/Framework/src/Rendering/Vulkan/Shaders")
set(BONGO_CAT_CUBISM_METAL_DIR
  "${BONGO_CAT_CUBISM_SDK}/Framework/src/Rendering/Metal")

if(EXISTS "${BONGO_CAT_CUBISM_VULKAN_DIR}/CubismRenderer_Vulkan.hpp")
  set(BONGO_CAT_CUBISM_VULKAN_DEFAULT ON)
else()
  set(BONGO_CAT_CUBISM_VULKAN_DEFAULT OFF)
endif()
# The Metal sample ships iOS-only and the renderer needs a macOS device to
# verify against; probe it but keep the default conservative.
if(APPLE AND EXISTS "${BONGO_CAT_CUBISM_METAL_DIR}/CubismRenderer_Metal.hpp")
  set(BONGO_CAT_CUBISM_METAL_DEFAULT ON)
else()
  set(BONGO_CAT_CUBISM_METAL_DEFAULT OFF)
endif()

option(BONGO_CAT_CUBISM_VULKAN
  "Build the Cubism Vulkan renderer into the Framework (needs glslangValidator)"
  ${BONGO_CAT_CUBISM_VULKAN_DEFAULT})
option(BONGO_CAT_CUBISM_METAL
  "Build the Cubism Metal renderer into the Framework (Apple only)"
  ${BONGO_CAT_CUBISM_METAL_DEFAULT})

if(BONGO_CAT_CUBISM_VULKAN)
  if(NOT EXISTS "${BONGO_CAT_CUBISM_VULKAN_DIR}/CubismRenderer_Vulkan.hpp")
    message(FATAL_ERROR "BONGO_CAT_CUBISM_VULKAN requires a Cubism 5.1+ SDK "
      "with Framework/src/Rendering/Vulkan (found ${BONGO_CAT_CUBISM_SDK})")
  endif()
  find_program(BONGO_CAT_GLSLANG_VALIDATOR
    NAMES glslangValidator glslang)
  if(NOT BONGO_CAT_GLSLANG_VALIDATOR)
    message(FATAL_ERROR "BONGO_CAT_CUBISM_VULKAN requires glslangValidator "
      "(the Vulkan SDK's shader compiler) to build the renderer's .spv shaders")
  endif()
  # The renderer sources call Vulkan directly: link the loader.
  find_package(Vulkan QUIET)
  if(TARGET Vulkan::Vulkan)
    target_link_libraries(Framework PUBLIC Vulkan::Vulkan)
  else()
    find_library(BONGO_CAT_VULKAN_LOADER NAMES vulkan-1 vulkan)
    if(NOT BONGO_CAT_VULKAN_LOADER)
      message(FATAL_ERROR "BONGO_CAT_CUBISM_VULKAN requires the Vulkan loader "
        "(vulkan-1 on Windows, libvulkan on Linux)")
    endif()
    target_link_libraries(Framework PUBLIC ${BONGO_CAT_VULKAN_LOADER})
  endif()
  # The framework's own subdirectory adds its sources to the Framework
  # target (it addresses it through LIB_NAME, defined only in its parent
  # scope); the include dir carries Vulkan-Headers via the caller.
  set(LIB_NAME Framework)
  add_subdirectory("${BONGO_CAT_CUBISM_VULKAN_DIR}"
    "${CMAKE_BINARY_DIR}/cubism-renderer-vulkan")
  # The renderer loads "FrameworkShaders/<name>.spv" relative to the working
  # directory: compile the framework's GLSL next to the executable.
  set(BONGO_CAT_VULKAN_SHADER_OUTPUT
    "${CMAKE_BINARY_DIR}/FrameworkShaders")
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
      COMMAND "${BONGO_CAT_GLSLANG_VALIDATOR}" -V "${SHADER}" -o "${SHADER_OUTPUT}"
      DEPENDS "${SHADER}"
      COMMENT "glslangValidator ${SHADER_STEM}")
    list(APPEND BONGO_CAT_VULKAN_SHADER_SPVS "${SHADER_OUTPUT}")
  endforeach()
  add_custom_target(bongo_cat_vulkan_shaders ALL
    DEPENDS ${BONGO_CAT_VULKAN_SHADER_SPVS})
  add_dependencies(Framework bongo_cat_vulkan_shaders)
  target_compile_definitions(bongo_cat_runtime PUBLIC
    BONGO_CAT_HAS_CUBISM_VULKAN=1)
endif()

if(BONGO_CAT_CUBISM_METAL)
  if(NOT APPLE)
    message(FATAL_ERROR "BONGO_CAT_CUBISM_METAL requires Apple platforms")
  endif()
  if(NOT EXISTS "${BONGO_CAT_CUBISM_METAL_DIR}/CubismRenderer_Metal.hpp")
    message(FATAL_ERROR "BONGO_CAT_CUBISM_METAL requires a Cubism 5.1+ SDK "
      "with Framework/src/Rendering/Metal (found ${BONGO_CAT_CUBISM_SDK})")
  endif()
  add_subdirectory("${BONGO_CAT_CUBISM_METAL_DIR}"
    "${CMAKE_BINARY_DIR}/cubism-renderer-metal")
  target_compile_definitions(bongo_cat_runtime PUBLIC
    BONGO_CAT_HAS_CUBISM_METAL=1)
endif()
