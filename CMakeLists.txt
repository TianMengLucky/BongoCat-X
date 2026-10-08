cmake_minimum_required(VERSION 3.24)
set(BONGO_CAT_VERSION "2.0.4")
project(BongoCat VERSION ${BONGO_CAT_VERSION} DESCRIPTION "BongoCat" HOMEPAGE_URL "https://github.com/TianMengLucky/BongoCat-X" LANGUAGES C CXX)
include(cmake/Version.cmake)
include(FetchContent)
include(CTest)
option(BONGO_CAT_FETCH_DEPS "Download pinned open-source dependencies" ON)
option(BONGO_CAT_REQUIRE_CUBISM
  "Require the Cubism SDK; set ON to fail the configure step when it is \
missing, leave OFF (default) to build the diagnostic backend instead" OFF)
option(BONGO_CAT_RUNTIME_CORE
  "Load user-supplied Cubism Core externally; static Core linking is unsupported" ON)
if(NOT BONGO_CAT_RUNTIME_CORE)
  message(FATAL_ERROR "BONGO_CAT_RUNTIME_CORE=OFF is unsupported: renderer plugins "
    "must not embed Cubism Core. Supply Core separately at runtime.")
endif()
option(BONGO_CAT_WARNINGS_AS_ERRORS "Treat native source warnings as errors" OFF)
option(BONGO_CAT_STATIC_ANALYZER "Run GCC's static analyzer on project C sources" OFF)
set(BONGO_CAT_CUBISM_SDK "${CMAKE_CURRENT_SOURCE_DIR}/vendor/CubismSdkForNative"
  CACHE PATH "Path to Cubism SDK for Native")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(cmake/ReleaseOptimize.cmake)
add_library(bongo_cat_warnings INTERFACE)
if(MSVC)
  target_compile_options(bongo_cat_warnings INTERFACE /W4 /permissive- /utf-8)
  target_compile_options(bongo_cat_warnings INTERFACE
    $<$<COMPILE_LANGUAGE:C>:/experimental:c11atomics>)
  if(BONGO_CAT_WARNINGS_AS_ERRORS)
    target_compile_options(bongo_cat_warnings INTERFACE /WX)
  endif()
else()
  target_compile_options(bongo_cat_warnings INTERFACE -Wall -Wextra)
  if(BONGO_CAT_WARNINGS_AS_ERRORS)
    target_compile_options(bongo_cat_warnings INTERFACE -Werror)
  endif()
endif()
if(BONGO_CAT_STATIC_ANALYZER)
  if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_VERSION VERSION_LESS 10)
    message(FATAL_ERROR "BONGO_CAT_STATIC_ANALYZER requires GCC 10 or newer")
  endif()
  target_compile_options(bongo_cat_warnings INTERFACE
    $<$<COMPILE_LANGUAGE:C>:-fanalyzer>)
endif()
include(cmake/Dependencies.cmake)
include(cmake/AudioDecoder.cmake)
include(cmake/Rust.cmake)
find_package(OpenGL REQUIRED)
set(BONGO_CAT_LIVE2D_SOURCES "")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(BONGO_CAT_RUNTIME_CORE_MODE ON)
set(BONGO_CAT_CUBISM_ENABLED OFF)
set(CUBISM_CORE_HEADER "${BONGO_CAT_CUBISM_SDK}/Core/include/Live2DCubismCore.h")
if(EXISTS "${CUBISM_CORE_HEADER}")
  if(WIN32 AND NOT MSVC)
    if(BONGO_CAT_REQUIRE_CUBISM)
      message(FATAL_ERROR "Cubism Core for Windows requires Visual Studio 2022")
    endif()
    message(WARNING "Cubism SDK found but MinGW is unsupported; using diagnostic backend")
  else()
    include(cmake/Cubism.cmake)
    set(BONGO_CAT_LIVE2D_SOURCES
      src/live2d/cubism_bridge.cpp src/live2d/cubism_bridge_load.cpp
      src/live2d/cubism_native_textures.cpp
      src/live2d/cubism_renderer_factory.cpp
      src/live2d/cubism_render_resources.cpp
      src/live2d/cubism_bridge_visual.cpp
      src/live2d/cubism_expression.cpp
      src/live2d/cubism_model_load.cpp
      src/live2d/cubism_texture_resolution.cpp
      src/live2d/cubism_texture_refresh.cpp
      src/live2d/cubism_texture_refresh_memory.cpp
      src/live2d/cubism_model_renderer.cpp
      src/live2d/cubism_model_masks.cpp
      src/live2d/cubism_model_projection.cpp
      src/live2d/cubism_model_textures.cpp
      src/live2d/cubism_model_idle.cpp
      src/live2d/cubism_model_update.cpp
      src/live2d/cubism_parameter_overrides.cpp
      src/live2d/cubism_motion_playback.cpp
      src/live2d/cubism_motion_preview.cpp
      src/live2d/cubism_motion_selection.cpp
      src/live2d/cubism_motion_state.cpp src/live2d/cubism_viewer_look.cpp
      src/live2d/cubism_model_bounds.cpp
      src/live2d/cubism_model_frame.cpp
      src/live2d/cubism_model_viewport.cpp
      src/live2d/cubism_model_validate.cpp)
    if(BONGO_CAT_CUBISM_VULKAN)
      list(APPEND BONGO_CAT_LIVE2D_SOURCES src/live2d/cubism_model_renderer_vulkan.cpp)
    endif()
    if(BONGO_CAT_CUBISM_METAL)
      list(APPEND BONGO_CAT_LIVE2D_SOURCES src/live2d/cubism_model_renderer_metal.mm)
      set_source_files_properties(src/live2d/cubism_model_renderer_metal.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    endif()
    set(BONGO_CAT_CUBISM_ENABLED ON)
  endif()
elseif(BONGO_CAT_REQUIRE_CUBISM)
  message(FATAL_ERROR
    "Cubism SDK for Native not found at ${BONGO_CAT_CUBISM_SDK}.\n"
    "The Live2D Cubism SDK is proprietary and is not distributed with this repository;\n"
    "each user must download it manually from the official Live2D website:\n"
    "  https://www.live2d.com/en/sdk/download/native/\n"
    "Accept the Live2D Proprietary Software License Agreement, download \"Cubism SDK for Native\",\n"
    "and extract the archive to ${BONGO_CAT_CUBISM_SDK} (rename the versioned folder,\n"
    "e.g. CubismSdkForNative-5-r.5, to CubismSdkForNative) so that <sdk>/Core and <sdk>/Framework\n"
    "exist, or pass its location explicitly via -DBONGO_CAT_CUBISM_SDK=<path>.\n"
    "Newer SDK archives do not bundle GLEW: also download GLEW 2.2.0 from\n"
    "  https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip\n"
    "and extract it to <sdk>/Samples/OpenGL/thirdParty/glew (the directory containing\n"
    "include/GL/glew.h and src/glew.c).\n"
    "Windows Cubism builds additionally require Visual Studio 2022 (MSVC).\n"
    "To build the diagnostic backend without Live2D rendering instead, reconfigure with\n"
    "-DBONGO_CAT_REQUIRE_CUBISM=OFF.")
else()
  message(WARNING
    "Cubism SDK missing and BONGO_CAT_REQUIRE_CUBISM=OFF; building the diagnostic backend "
    "without Live2D model rendering.\n"
    "Live2D requires a compatible plugin built with the SDK "
    "(-DBONGO_CAT_REQUIRE_CUBISM=ON) to produce a binary that loads the Core "
    "at runtime.")
endif()
include(cmake/ModelPlugins.cmake)
add_library(bongo_cat_core STATIC
  src/core/config_defaults.c src/core/config_model_state.c src/core/config_session_defaults.c src/core/config_names.c src/core/config_io.c
  src/core/config_file.c
  src/core/error.c
  src/core/file.c src/core/i18n.c src/core/json_dom_io.c
  src/core/input_state.c src/core/mver_pointer.c
  src/core/mouse_tracking.c src/core/sha256.c src/core/utf8.c
  src/core/model_catalog.c
  src/core/model_behavior.c src/core/model_json.c
  src/core/path.c src/core/path_io.c src/core/json.c src/core/shortcut.c
  src/core/sound_shortcut.c
  src/core/update_release.c src/core/update_version.c)
target_include_directories(bongo_cat_core PUBLIC "${BONGO_CAT_GENERATED_INCLUDE_DIR}" include)
# The Rust crate's CMake target name uses underscores (Corrosion normalizes
# the crate name's dashes for library targets).
target_link_libraries(bongo_cat_core PUBLIC bongo_safe PRIVATE bongo_cat_warnings)
if(WIN32)
  target_sources(bongo_cat_core PRIVATE
    src/platform/windows/windows_utf8.c)
  target_include_directories(bongo_cat_core PRIVATE
    src/platform/windows)
endif()

include(cmake/RuntimeSources.cmake)
add_library(bongo_cat_runtime STATIC
  ${BONGO_CAT_RUNTIME_SOURCES} ${BONGO_CAT_MODEL_HOST_SOURCES})
target_include_directories(bongo_cat_runtime PUBLIC "${BONGO_CAT_GENERATED_INCLUDE_DIR}" include)
target_include_directories(bongo_cat_runtime PRIVATE
  ${BONGO_CAT_RUNTIME_INTERNAL_INCLUDE_DIRS})
target_include_directories(bongo_cat_runtime SYSTEM PRIVATE
  ${BONGO_CAT_STB_INCLUDE_DIR} ${BONGO_CAT_NUKLEAR_INCLUDE_DIR})
if(BONGO_CAT_VULKAN_INCLUDE_DIR)
  target_include_directories(bongo_cat_runtime SYSTEM PRIVATE
    ${BONGO_CAT_VULKAN_INCLUDE_DIR})
  target_compile_definitions(bongo_cat_runtime PRIVATE BONGO_CAT_HAS_VULKAN_RHI=1)
endif()
if(APPLE)
  target_compile_definitions(bongo_cat_runtime PUBLIC BONGO_CAT_HAS_CUBISM_METAL=1)
endif()
if(BONGO_CAT_PLATFORM_VULKAN AND BONGO_CAT_VULKAN_INCLUDE_DIR AND volk_SOURCE_DIR)
  add_library(bongo_cat_host_volk STATIC "${volk_SOURCE_DIR}/volk.c")
  target_include_directories(bongo_cat_host_volk SYSTEM PUBLIC
    "${volk_SOURCE_DIR}" "${BONGO_CAT_VULKAN_INCLUDE_DIR}")
  target_link_libraries(bongo_cat_host_volk PRIVATE ${CMAKE_DL_LIBS})
  target_link_libraries(bongo_cat_runtime PRIVATE bongo_cat_host_volk)
  target_compile_definitions(bongo_cat_runtime PUBLIC BONGO_CAT_HAS_CUBISM_VULKAN=1)
  target_compile_definitions(bongo_cat_runtime PRIVATE VK_NO_PROTOTYPES=1)
endif()
target_link_libraries(bongo_cat_runtime PRIVATE bongo_cat_warnings bongo_cat_archive)
target_link_libraries(bongo_cat_runtime PUBLIC bongo_cat_core SDL3::SDL3-static OpenGL::GL ${BONGO_CAT_MINIAUDIO_TARGET})
if(MSVC)
  set_source_files_properties(src/media/stb_image_impl.c src/media/image_resize.c src/ui/backend/nuklear_impl.c
    PROPERTIES COMPILE_OPTIONS "/W0")
else()
  set_source_files_properties(src/media/stb_image_impl.c src/media/image_resize.c src/ui/backend/nuklear_impl.c
    PROPERTIES COMPILE_OPTIONS "-w")
endif()
target_compile_definitions(bongo_cat_runtime PUBLIC BONGO_CAT_HAS_MODEL_PLUGINS=1)
if(BONGO_CAT_RUNTIME_CORE_MODE)
  target_compile_definitions(bongo_cat_runtime PUBLIC
    BONGO_CAT_LIVE2D_CORE_RUNTIME=1)
endif()
target_compile_definitions(bongo_cat_runtime PRIVATE
  BONGO_CAT_DEV_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/assets"
  BONGO_CAT_DEV_LOCALE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/assets/locales")

option(BONGO_CAT_PCH
  "Use precompiled headers for the core and runtime targets (faster builds)" ON)
# The Visual Studio generator drops $<COMPILE_LANGUAGE:C> options from the PCH
# compilation, so C sources using the PCH then fail with C2855 against the
# /experimental:c11atomics flag that bongo_cat_warnings adds. PCH stays enabled
# for Ninja/MSVC and other generators where the option propagates correctly.
if(MSVC AND CMAKE_GENERATOR MATCHES "^Visual Studio")
  set(BONGO_CAT_PCH OFF)
endif()
if(BONGO_CAT_PCH)
  target_precompile_headers(bongo_cat_core PRIVATE "bongo_cat/json_dom.h")
  target_precompile_headers(bongo_cat_runtime PRIVATE
    <SDL3/SDL.h>
    <SDL3/SDL_opengl.h>)
  # The Cubism bridge includes <GL/glew.h>, which must come before any GL
  # headers; the runtime PCH pre-includes SDL_opengl.h, so those files opt out.
  set_source_files_properties(${BONGO_CAT_LIVE2D_SOURCES} PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON)
  # Files that define feature-test macros (_GNU_SOURCE / _POSIX_C_SOURCE /
  # _DEFAULT_SOURCE) at the top of the file: the PCH pulls in libc headers
  # before those definitions, hiding functions like fileno(3)/openat(2) under
  # strict C11. Skip the PCH for them so the macros take effect first.
  set_source_files_properties(
    src/platform/linux/linux.c
    src/platform/linux/linux_evdev_devices.c
    src/platform/posix/posix_live2d_sdk.c
    src/ui/rendering/ui_font_atlas_sources.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

include(cmake/PlatformRuntime.cmake)

add_executable(bongo_cat src/main.c)
target_link_libraries(bongo_cat PRIVATE bongo_cat_runtime bongo_cat_warnings)
if(NOT APPLE)
  # The Metal implementation is rhi_metal.m (Apple only); the dispatcher
  # still references its symbols, so non-Apple builds compile the stubs.
  target_sources(bongo_cat_runtime PRIVATE
    src/render/rhi/rhi_metal_stub.c)
endif()
set_target_properties(bongo_cat PROPERTIES OUTPUT_NAME "BongoCat")
bongo_cat_enable_release_ipo(Framework bongo_cat_core bongo_cat_runtime bongo_cat)

include(cmake/ValidationTools.cmake)
include(cmake/ApplicationResources.cmake)
if(MSVC AND BONGO_CAT_SDL_WINDOWS_TRIMMED)
  get_filename_component(BONGO_CAT_MSVC_TOOL_DIR "${CMAKE_LINKER}" DIRECTORY)
  find_program(BONGO_CAT_DUMPBIN dumpbin PATHS "${BONGO_CAT_MSVC_TOOL_DIR}" REQUIRED)
  add_custom_command(TARGET bongo_cat POST_BUILD
    COMMAND ${CMAKE_COMMAND} "-DEXECUTABLE=$<TARGET_FILE:bongo_cat>"
      "-DDUMPBIN=${BONGO_CAT_DUMPBIN}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckWindowsImports.cmake"
    COMMENT "Checking the final Windows executable's input capabilities"
    VERBATIM)
endif()
if(MSVC)
  set_property(SOURCE src/main.c APPEND PROPERTY COMPILE_OPTIONS "/experimental:c11atomics")
  get_target_property(BONGO_CAT_RUNTIME_SOURCES bongo_cat_runtime SOURCES)
  foreach(BONGO_CAT_SOURCE IN LISTS BONGO_CAT_RUNTIME_SOURCES)
    if(BONGO_CAT_SOURCE MATCHES "\\.c$")
      set_property(SOURCE "${BONGO_CAT_SOURCE}" APPEND PROPERTY
        COMPILE_OPTIONS "/experimental:c11atomics")
    endif()
  endforeach()
endif()

bongo_cat_stage_model_plugins(bongo_cat)

if(APPLE)
  # The linker signs the executable on its own before the bundle's resources are
  # copied in, which leaves strict verification failing with "code has no resources
  # but signature indicates they must be present". Re-sign once every resource and
  # executable change has landed, so this must stay the last POST_BUILD step.
  add_custom_command(TARGET bongo_cat POST_BUILD
    COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:bongo_cat>"
    COMMENT "Ad-hoc signing the BongoCat bundle"
    VERBATIM)
endif()

# The repo-wide validation scripts run on every build in CI (GitHub Actions
# sets CI); locally they are opt-in targets so routine rebuilds skip them.
if(DEFINED ENV{CI})
  set(BONGO_CAT_CHECK_ALL ALL)
else()
  set(BONGO_CAT_CHECK_ALL "")
endif()
add_custom_target(check-lines ${BONGO_CAT_CHECK_ALL}
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckLines.cmake
  VERBATIM)
add_custom_target(check-localization ${BONGO_CAT_CHECK_ALL}
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckLocalization.cmake
  VERBATIM)
add_custom_target(check-platform-runtime-safety ${BONGO_CAT_CHECK_ALL}
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckPlatformRuntimeSafety.cmake
  VERBATIM)
add_custom_target(check-cubism-user-model-safety ${BONGO_CAT_CHECK_ALL}
  COMMAND ${CMAKE_COMMAND}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCubismUserModelSafety.cmake
  VERBATIM)
include(cmake/Tests.cmake)
include(cmake/Packaging.cmake)
