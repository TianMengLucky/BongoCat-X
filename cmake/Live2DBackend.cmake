# Builds the optional bongo-cat-live2d-backend shared library.
#
# The Cubism Framework is C++ source that ships inside the proprietary SDK, so it
# can only be compiled here, while the SDK is present. The executable itself is
# always built without the SDK and dlopen()s the library this file produces,
# which is what lets an SDK-free installation gain Live2D rendering later on.
#
# Layout:
#   bongo_cat_live2d_bridge       OBJECT library: the SDK bridge sources and the
#                                 ABI table. Tests link the same objects and
#                                 install the table directly.
#   bongo_cat_live2d_backend_glue OBJECT library: the plugin entry points and the
#                                 host forwarders. Only the shared library gets
#                                 these, because the forwarder names collide with
#                                 the executable's own service functions.
#   bongo-cat-live2d-backend      MODULE library: the shipped shared library.

set(BONGO_CAT_LIVE2D_BRIDGE_SOURCES
  src/live2d/cubism_bridge.cpp src/live2d/cubism_bridge_load.cpp
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

# The table names the public ABI, so its members must resolve to the bridge
# implementations, not to the dispatch layer's copies.
set(BONGO_CAT_BRIDGE_DEFS "")
foreach(NAME IN LISTS BONGO_CAT_LIVE2D_ABI)
  list(APPEND BONGO_CAT_BRIDGE_DEFS
    "bongo_cat_live2d_${NAME}=bridge_live2d_${NAME}")
endforeach()

# Framework is PUBLIC so the MODULE target below inherits its include paths and
# the Core linkage setup cmake/Cubism.cmake chose for this platform.
function(bongo_cat_add_live2d_bridge_target NAME)
  add_library(${NAME} OBJECT ${ARGN})
  target_include_directories(${NAME} PUBLIC
    "${BONGO_CAT_GENERATED_INCLUDE_DIR}" include src/live2d)
  target_link_libraries(${NAME} PUBLIC Framework bongo_cat_core
    bongo_cat_warnings)
endfunction()

bongo_cat_add_live2d_bridge_target(bongo_cat_live2d_bridge
  ${BONGO_CAT_LIVE2D_BRIDGE_SOURCES} src/live2d/live2d_backend_api.cpp)
bongo_cat_add_live2d_bridge_target(bongo_cat_live2d_backend_glue
  src/live2d/live2d_backend_plugin.cpp)

# The table initializer names the bridge symbols, so it is built with the
# renames. The glue does not reference them and stays unrenamed.
set_source_files_properties(src/live2d/live2d_backend_api.cpp
  PROPERTIES COMPILE_DEFINITIONS "${BONGO_CAT_BRIDGE_DEFS}")

add_library(bongo-cat-live2d-backend MODULE
  $<TARGET_OBJECTS:bongo_cat_live2d_bridge>
  $<TARGET_OBJECTS:bongo_cat_live2d_backend_glue>)
if(APPLE)
  # A Mach-O bundle; dlopen() still resolves it from the app's search paths.
  set_target_properties(bongo-cat-live2d-backend PROPERTIES
    BUNDLE TRUE SUFFIX ".dylib" OUTPUT_NAME "bongo-cat-live2d-backend")
else()
  set_target_properties(bongo-cat-live2d-backend PROPERTIES
    PREFIX "lib" OUTPUT_NAME "bongo-cat-live2d-backend")
endif()
if(WIN32 AND NOT MSVC)
  message(FATAL_ERROR "bongo-cat-live2d-backend requires Visual Studio 2022")
endif()
if(WIN32)
  # The Core is a user-supplied DLL: route its imports through delay-load so the
  # plugin neither embeds nor requires it at load time. POSIX reaches the Core
  # through the generated dlopen shim instead.
  target_link_options(bongo-cat-live2d-backend PRIVATE
    "/DELAYLOAD:Live2DCubismCore.dll")
  target_link_libraries(bongo-cat-live2d-backend PRIVATE delayimp)
  target_compile_definitions(bongo-cat-live2d-backend PRIVATE
    WIN32_LEAN_AND_MEAN)
endif()
# Framework carries the include paths and the Core linkage. The plugin's own
# glue answers every project service call, so nothing from the executable leaks
# into the link and the library loads without special export flags.
target_link_libraries(bongo-cat-live2d-backend PRIVATE Framework glew_s)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(bongo-cat-live2d-backend PRIVATE
    ${CMAKE_DL_LIBS} Threads::Threads)
  if(NOT APPLE)
    target_link_libraries(bongo-cat-live2d-backend PRIVATE m)
  endif()
endif()
if(BONGO_CAT_RUNTIME_CORE AND NOT WIN32)
  # The generated shim owns the csm* ABI and forwards to the user-supplied Core
  # library at runtime; the glue hands it the platform's Core handle.
  target_link_libraries(bongo-cat-live2d-backend PRIVATE bongo_cat_core_shim)
endif()
bongo_cat_enable_release_ipo(bongo-cat-live2d-backend)

# Installed beside the executable, which is the first place the runtime loader
# in src/live2d/live2d_backend_load.c looks for it.
if(APPLE)
  # The executable lives inside the bundle, so the library has to as well.
  install(FILES "$<TARGET_FILE:bongo-cat-live2d-backend>"
    DESTINATION "BongoCat.app/Contents/MacOS" COMPONENT Runtime)
else()
  install(TARGETS bongo-cat-live2d-backend
    LIBRARY DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION . COMPONENT Runtime
    ARCHIVE DESTINATION . COMPONENT Runtime)
endif()

# Stage the library next to the freshly linked executable, mirroring
# bongo_cat_stage_cubism_assets: packaging scripts that copy a built bundle or
# executable straight from the build tree must not miss the library.
function(bongo_cat_stage_live2d_backend target)
  get_target_property(is_bundle ${target} MACOSX_BUNDLE)
  if(APPLE AND is_bundle)
    set(destination "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/MacOS")
  else()
    set(destination "$<TARGET_FILE_DIR:${target}>")
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:bongo-cat-live2d-backend>" "${destination}"
    COMMENT "Staging ${target} beside bongo-cat-live2d-backend"
    VERBOSE)
  add_dependencies(${target} bongo-cat-live2d-backend)
endfunction()
