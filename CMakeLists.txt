cmake_minimum_required(VERSION 3.24)
set(BONGO_CAT_VERSION "2.0.0")
project(BongoCat VERSION ${BONGO_CAT_VERSION} DESCRIPTION "BongoCat" HOMEPAGE_URL "https://github.com/TianMengLucky/BongoCat-X" LANGUAGES C CXX)
include(cmake/Version.cmake)
include(FetchContent)
include(CTest)
option(BONGO_CAT_FETCH_DEPS "Download pinned open-source dependencies" ON)
option(BONGO_CAT_REQUIRE_CUBISM
  "Require the Cubism SDK, so the bongo-cat-live2d-backend library is built; \
set ON to fail the configure step when it is missing, leave OFF (default) to \
build only the executable, which loads a Live2D backend library at runtime" OFF)
option(BONGO_CAT_RUNTIME_CORE
  "Keep the Cubism Core out of the backend library and resolve it at runtime \
(user-supplied Core library, an SDK zip dropped by the user, or the \
settings-window import); OFF links the Core into the backend library. The \
executable always resolves the Core at runtime, with or without the SDK" ON)
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
find_package(OpenGL REQUIRED)
# The executable never compiles the proprietary Cubism SDK: it only links the
# diagnostic stub, the dispatch layer and the runtime backend loader. When the
# SDK is present locally the build additionally produces the
# bongo-cat-live2d-backend shared library, which the executable loads at runtime
# instead. The Cubism Framework is C++ source shipped inside the SDK, so it can
# only be compiled while building that library, never after shipment.
set(BONGO_CAT_LIVE2D_SOURCES
  src/live2d/live2d_stub.c
  src/live2d/live2d_dispatch.c
  src/live2d/live2d_backend_load.c)
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
    include(cmake/Live2DBackend.cmake)
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
    "To build only the executable, without the bongo-cat-live2d-backend shared "
    "library that provides Live2D rendering, reconfigure with\n"
    "-DBONGO_CAT_REQUIRE_CUBISM=OFF.")
else()
  message(WARNING
    "Cubism SDK missing and BONGO_CAT_REQUIRE_CUBISM=OFF; building only the "
    "executable, without the bongo-cat-live2d-backend library that renders "
    "Live2D models.\n"
    "The application still runs: it reports Live2D as unavailable until a "
    "bongo-cat-live2d-backend library is placed next to the executable or "
    "inside a live2d folder beside it or in the data directory.\n"
    "To build that library as well, install the SDK and reconfigure with "
    "-DBONGO_CAT_REQUIRE_CUBISM=ON.")
endif()
if(BONGO_CAT_CUBISM_ENABLED AND BONGO_CAT_RUNTIME_CORE)
  # The backend library resolves the Core at runtime, so tests linking the
  # bridge objects need the Core staged next to them.
  set(BONGO_CAT_RUNTIME_CORE_MODE ON)
endif()
# The dispatch layer owns the public bongo_cat_live2d_* ABI; the stub implements
# it under the stub_ prefix, exactly like the backend bridge does under the
# bridge_ prefix.
set(BONGO_CAT_LIVE2D_ABI
  create destroy load load_ex ready canvas_size frame measure_frame
  set_frame viewport resize reshape try_reuse_texture_quality
  texture_refresh_pending texture_refresh_due texture_refresh_busy
  cancel_texture_refresh refresh_textures update draw set_mirror
  set_vertical_flip set_render_options set_dragging
  set_centered_dragging prepare_viewer_audit prepare_cover_capture
  set_parameter parameter start_motion restore_motion_state
  preview_motion restore_motion_preview commit_motion_preview
  motion_selected motion_persistent motion_visible motion_same_toggle
  set_expression expression visual_state)
set(BONGO_CAT_STUB_DEFS "")
foreach(NAME IN LISTS BONGO_CAT_LIVE2D_ABI)
  list(APPEND BONGO_CAT_STUB_DEFS
    "bongo_cat_live2d_${NAME}=stub_live2d_${NAME}")
endforeach()
set_source_files_properties(src/live2d/live2d_stub.c PROPERTIES
  COMPILE_DEFINITIONS "${BONGO_CAT_STUB_DEFS}")
add_library(bongo_cat_core STATIC
  src/core/config_defaults.c src/core/config_model_state.c src/core/config_session_defaults.c src/core/config_names.c src/core/config_fields.c src/core/config_io.c src/core/config_settings_save.c
  src/core/config_file.c src/core/config_session_io.c
  src/core/error.c
  src/core/file.c src/core/i18n.c
  src/core/input_state.c src/core/mver_pointer.c
  src/core/mouse_tracking.c src/core/sha256.c src/core/utf8.c
  src/core/model_catalog.c
  src/core/model_behavior.c src/core/model_json.c
  src/core/path.c src/core/path_io.c src/core/json.c src/core/shortcut.c
  src/core/sound_shortcut.c
  src/core/update_release.c src/core/update_version.c)
target_include_directories(bongo_cat_core PUBLIC "${BONGO_CAT_GENERATED_INCLUDE_DIR}" include)
target_link_libraries(bongo_cat_core PUBLIC yyjson PRIVATE bongo_cat_warnings)
include(cmake/CubismAssets.cmake)
if(WIN32)
  target_sources(bongo_cat_core PRIVATE
    src/platform/windows/windows_utf8.c)
  target_include_directories(bongo_cat_core PRIVATE
    src/platform/windows)
endif()

include(cmake/RuntimeSources.cmake)
add_library(bongo_cat_runtime STATIC
  ${BONGO_CAT_RUNTIME_SOURCES} ${BONGO_CAT_LIVE2D_SOURCES})
target_include_directories(bongo_cat_runtime PUBLIC "${BONGO_CAT_GENERATED_INCLUDE_DIR}" include)
target_include_directories(bongo_cat_runtime PRIVATE
  ${BONGO_CAT_RUNTIME_INTERNAL_INCLUDE_DIRS})
target_include_directories(bongo_cat_runtime SYSTEM PRIVATE
  ${BONGO_CAT_STB_INCLUDE_DIR} ${BONGO_CAT_NUKLEAR_INCLUDE_DIR})
target_link_libraries(bongo_cat_runtime PRIVATE bongo_cat_warnings bongo_cat_archive)
target_link_libraries(bongo_cat_runtime PUBLIC bongo_cat_core SDL3::SDL3-static OpenGL::GL ${BONGO_CAT_MINIAUDIO_TARGET})
if(MSVC)
  set_source_files_properties(src/media/stb_image_impl.c src/media/image_resize.c src/ui/backend/nuklear_impl.c src/ui/preferences/about/preferences_about_svg.c
    PROPERTIES COMPILE_OPTIONS "/W0")
else()
  set_source_files_properties(src/media/stb_image_impl.c src/media/image_resize.c src/ui/backend/nuklear_impl.c src/ui/preferences/about/preferences_about_svg.c
    PROPERTIES COMPILE_OPTIONS "-w")
endif()
target_compile_definitions(bongo_cat_runtime PRIVATE
  BONGO_CAT_DEV_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/assets"
  BONGO_CAT_DEV_LOCALE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/resources/assets/locales")

include(cmake/PlatformRuntime.cmake)
include(cmake/AboutWebP.cmake)

add_executable(bongo_cat src/main.c)
target_link_libraries(bongo_cat PRIVATE bongo_cat_runtime bongo_cat_warnings)
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

bongo_cat_stage_cubism_assets(bongo_cat)
if(TARGET bongo-cat-live2d-backend)
  bongo_cat_stage_live2d_backend(bongo_cat)
endif()

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

add_custom_target(check-lines ALL
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckLines.cmake
  VERBATIM)
add_custom_target(check-localization ALL
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckLocalization.cmake
  VERBATIM)
add_custom_target(check-platform-runtime-safety ALL
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckPlatformRuntimeSafety.cmake
  VERBATIM)
add_custom_target(check-cubism-user-model-safety ALL
  COMMAND ${CMAKE_COMMAND}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCubismUserModelSafety.cmake
  VERBATIM)
include(cmake/Tests.cmake)
include(cmake/Packaging.cmake)
