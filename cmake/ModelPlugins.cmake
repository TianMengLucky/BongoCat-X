include("${CMAKE_CURRENT_LIST_DIR}/WriteIfDifferent.cmake")
# Renderer implementations are shared libraries; the host never links Framework.
option(BONGO_CAT_BUILD_INOX2D "Build the Rust Inox2D renderer plugin" ON)
set(BONGO_CAT_MODEL_HOST_SOURCES
  src/live2d/live2d_stub.c src/live2d/live2d_dispatch.c
  src/runtime/model/plugins/model_runtime.c
  src/runtime/model/plugins/model_plugin_archive.c
  src/runtime/model/plugins/model_plugins.c
  src/runtime/model/plugins/model_stub_ops.c
  src/runtime/model/plugins/model_plugin_loader.c
  src/runtime/model/plugins/model_plugin_services.c)
set(BONGO_CAT_MODEL_PLUGIN_METHODS
  create destroy set_rhi_info load load_ex ready canvas_size frame measure_frame set_frame viewport overlay_viewport resize reshape try_reuse_texture_quality texture_refresh_pending texture_refresh_due texture_refresh_busy cancel_texture_refresh refresh_textures update draw draw_checked set_mirror set_vertical_flip set_render_options set_tight_frame set_tight_overlay_rect set_dragging set_centered_dragging prepare_viewer_audit prepare_cover_capture set_parameter parameter start_motion restore_motion_state preview_motion restore_motion_preview commit_motion_preview motion_selected motion_persistent motion_visible motion_same_toggle set_expression expression visual_state)
set(stub_definitions "")
foreach(method IN LISTS BONGO_CAT_MODEL_PLUGIN_METHODS)
  if(NOT method STREQUAL "set_rhi_info")
    list(APPEND stub_definitions
      "bongo_cat_model_runtime_${method}=stub_model_runtime_${method}")
  endif()
endforeach()
set_source_files_properties(src/live2d/live2d_stub.c PROPERTIES
  COMPILE_DEFINITIONS "${stub_definitions}")
if(BONGO_CAT_CUBISM_ENABLED)
  set(BONGO_CAT_PLUGIN_SHADER_SOURCE "${CMAKE_BINARY_DIR}/generated/cubism_plugin_shaders.cpp")
  set(shader_list "${CMAKE_BINARY_DIR}/generated/cubism_plugin_shaders.list")
  file(GLOB_RECURSE gl_shader_sources CONFIGURE_DEPENDS
    "${BONGO_CAT_CUBISM_SHADER_SOURCE_DIR}/*")
  string(REPLACE ";" "\n" shader_paths "${gl_shader_sources};${BONGO_CAT_VULKAN_SHADER_BINARIES};${BONGO_CAT_METAL_SHADER_BINARIES}")
  bongo_cat_write_if_different("${shader_list}" "${shader_paths}")
  add_custom_command(OUTPUT "${BONGO_CAT_PLUGIN_SHADER_SOURCE}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_plugin_shaders.py"
      --output "${BONGO_CAT_PLUGIN_SHADER_SOURCE}" --list "${shader_list}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_plugin_shaders.py" "${shader_list}"
      ${gl_shader_sources} ${BONGO_CAT_VULKAN_SHADER_BINARIES} ${BONGO_CAT_METAL_SHADER_BINARIES}
    VERBATIM)
  add_library(bongo_cubism_plugin_shaders OBJECT "${BONGO_CAT_PLUGIN_SHADER_SOURCE}")
  # Complete shader compilation before the embedding rule examines binary
  # dependencies. MSBuild otherwise emits the same output rules in both
  # projects and may execute them twice in one parallel build.
  foreach(backend IN ITEMS vulkan metal)
    if(TARGET bongo_cat_${backend}_shaders)
      add_dependencies(bongo_cubism_plugin_shaders bongo_cat_${backend}_shaders)
    endif()
  endforeach()
  target_link_libraries(bongo_cubism_plugin_shaders PRIVATE bongo_cat_warnings)
  add_library(bongo_live2d SHARED ${BONGO_CAT_LIVE2D_SOURCES} src/live2d/cubism_plugin.cpp)
  target_link_libraries(bongo_live2d PRIVATE bongo_cubism_plugin_shaders)
  target_include_directories(bongo_live2d PRIVATE include
    "${BONGO_CAT_GENERATED_INCLUDE_DIR}")
  target_link_libraries(bongo_live2d PRIVATE Framework bongo_cat_warnings)
  # Headers are needed for the callback table, but SDL is owned by the host.
  target_include_directories(bongo_live2d SYSTEM PRIVATE
    $<TARGET_PROPERTY:SDL3::SDL3-static,INTERFACE_INCLUDE_DIRECTORIES>)
  foreach(method IN LISTS BONGO_CAT_MODEL_PLUGIN_METHODS)
    target_compile_definitions(bongo_live2d PRIVATE
      "bongo_cat_model_runtime_${method}=cubism_runtime_${method}")
  endforeach()
  target_compile_definitions(bongo_live2d PRIVATE BONGO_CAT_LIVE2D_CORE_RUNTIME=1)
  foreach(backend IN ITEMS VULKAN METAL)
    if(BONGO_CAT_CUBISM_${backend})
      target_compile_definitions(bongo_live2d PRIVATE BONGO_CAT_HAS_CUBISM_${backend}=1)
    endif()
  endforeach()
  set_target_properties(bongo_live2d PROPERTIES CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
endif()
if(BONGO_CAT_BUILD_INOX2D)
  include("${CMAKE_CURRENT_LIST_DIR}/Inox2D.cmake")
endif()
function(bongo_cat_stage_model_plugins target)
  if(APPLE)
    set(destination "$<TARGET_BUNDLE_DIR:${target}>/Contents/PlugIns")
  else()
    set(destination "$<TARGET_FILE_DIR:${target}>/plugins")
  endif()
  foreach(plugin IN ITEMS bongo_live2d bongo_inox2d-shared)
    if(TARGET ${plugin})
      # Restage changed DLLs even when the executable needs no relink.
      add_custom_target(${target}_stage_${plugin}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${destination}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:${plugin}>" "${destination}"
        DEPENDS ${plugin} VERBATIM)
      add_dependencies(${target} ${target}_stage_${plugin})
      if(APPLE)
        # Re-run bundle signing whenever a plugin binary changes.
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "$<TARGET_FILE:${plugin}>")
      endif()
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${destination}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "$<TARGET_FILE:${plugin}>" "${destination}"
        VERBATIM)
      if(APPLE)
        # Sign nested binaries before the final bundle signature is sealed.
        add_custom_command(TARGET ${target} POST_BUILD
          COMMAND /usr/bin/codesign --force --sign -
            "${destination}/$<TARGET_FILE_NAME:${plugin}>"
          VERBATIM)
        install(FILES "$<TARGET_FILE:${plugin}>"
          DESTINATION "BongoCat.app/Contents/PlugIns" COMPONENT Runtime)
      else()
        install(FILES "$<TARGET_FILE:${plugin}>" DESTINATION plugins COMPONENT Runtime)
      endif()
    endif()
  endforeach()
endfunction()
