# Both SDK renderers require all 79 authored color/alpha blend combinations.
function(bongo_cat_compile_native_shaders backend input output)
  file(MAKE_DIRECTORY "${output}" "${CMAKE_BINARY_DIR}/native-shader-intermediates")
  if(backend STREQUAL "VULKAN")
    find_program(BONGO_CAT_GLSLANG_VALIDATOR NAMES glslangValidator glslang
      HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VK_SDK_PATH}/Bin" REQUIRED)
    # --preamble-text only exists in newer glslang releases; Ubuntu 22.04's
    # glslang-tools 11.8 rejects it. When unavailable, the include directive is
    # injected into a generated intermediate source instead (see below).
    execute_process(
      COMMAND "${BONGO_CAT_GLSLANG_VALIDATOR}" --help
      OUTPUT_VARIABLE _bongo_glslang_help
      ERROR_VARIABLE _bongo_glslang_help
      RESULT_VARIABLE _bongo_glslang_help_rc)
    if(_bongo_glslang_help_rc EQUAL 0 AND _bongo_glslang_help MATCHES "preamble-text")
      set(_bongo_glslang_preamble_flag TRUE)
    else()
      set(_bongo_glslang_preamble_flag FALSE)
    endif()
    file(GLOB inputs "${input}/*.vert" "${input}/*.frag")
    file(GLOB includes "${input}/*.glsl")
    set(extension spv)
  else()
    find_program(xcrun NAMES xcrun REQUIRED)
    file(GLOB inputs "${input}/*.metal")
    list(FILTER inputs EXCLUDE REGEX "/(FragShaderSrcColorBlend|FragShaderSrcAlphaBlend)\\.metal$")
    file(GLOB includes "${input}/*.h" "${input}/*ColorBlend.metal" "${input}/*AlphaBlend.metal")
    set(extension metallib)
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
      set(metal_target "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    endif()
  endif()
  set(colors Normal Add AddGlow Darken Multiply ColorBurn LinearBurn Lighten
    Screen ColorDodge Overlay SoftLight HardLight LinearLight Hue Color)
  set(alphas Over Atop Out ConjointOver DisjointOver)
  foreach(shader IN LISTS inputs)
    get_filename_component(stem "${shader}" NAME_WE)
    unset(_bongo_shader_preamble_args)
    unset(_bongo_preamble_dep)
    set(_bongo_compile_src "${shader}")
    if(backend STREQUAL "VULKAN")
      if(_bongo_glslang_preamble_flag)
        list(APPEND _bongo_shader_preamble_args
          "--preamble-text" "#extension GL_GOOGLE_include_directive : enable")
      else()
        # Insert the directive immediately after #version (it is illegal
        # before it), matching --preamble-text, into a generated source so
        # older glslang builds compile the same shader unchanged.
        get_filename_component(_bongo_shader_name "${shader}" NAME)
        set(_bongo_preamble_src
          "${CMAKE_BINARY_DIR}/native-shader-intermediates/preamble-${_bongo_shader_name}")
        file(READ "${shader}" _bongo_shader_text)
        if(_bongo_shader_text MATCHES "^[ \t]*#version[^\n]*\n")
          set(_bongo_preambled
            "${CMAKE_MATCH_0}#extension GL_GOOGLE_include_directive : enable\n")
          string(REGEX REPLACE "^[ \t]*#version[^\n]*\n" ""
            _bongo_shader_rest "${_bongo_shader_text}")
          string(APPEND _bongo_preambled "${_bongo_shader_rest}")
        else()
          set(_bongo_preambled
            "#extension GL_GOOGLE_include_directive : enable\n${_bongo_shader_text}")
        endif()
        file(WRITE "${_bongo_preamble_src}" "${_bongo_preambled}")
        set(_bongo_compile_src "${_bongo_preamble_src}")
        set(_bongo_preamble_dep "${_bongo_preamble_src}")
      endif()
    endif()
    set(variants "base")
    if(stem MATCHES "^Frag.*Blend$")
      set(variants "")
      foreach(color IN LISTS colors)
        foreach(alpha IN LISTS alphas)
          if(NOT (color STREQUAL "Normal" AND alpha STREQUAL "Over"))
            list(APPEND variants "${color}:${alpha}")
          endif()
        endforeach()
      endforeach()
    endif()
    foreach(variant IN LISTS variants)
      set(name "${stem}")
      set(defines "")
      if(NOT variant STREQUAL "base")
        string(REPLACE ":" ";" pair "${variant}")
        list(GET pair 0 color)
        list(GET pair 1 alpha)
        list(FIND colors "${color}" color_index)
        list(FIND alphas "${alpha}" alpha_index)
        set(name "${stem}${color}${alpha}")
        set(defines "-DCSM_COLOR_BLEND_MODE=${color_index}" "-DCSM_ALPHA_BLEND_MODE=${alpha_index}")
      endif()
      set(binary "${output}/${name}.${extension}")
      if(backend STREQUAL "VULKAN")
        add_custom_command(OUTPUT "${binary}"
          COMMAND "${BONGO_CAT_GLSLANG_VALIDATOR}" -V ${_bongo_shader_preamble_args} ${defines} "${_bongo_compile_src}" -o "${binary}"
          DEPENDS "${shader}" ${includes} ${_bongo_preamble_dep} VERBATIM)
      else()
        set(air "${CMAKE_BINARY_DIR}/native-shader-intermediates/${name}.air")
        add_custom_command(OUTPUT "${binary}"
          COMMAND "${xcrun}" -sdk macosx metal ${metal_target} ${defines} -c "${shader}" -o "${air}"
          COMMAND "${xcrun}" -sdk macosx metallib "${air}" -o "${binary}"
          DEPENDS "${shader}" ${includes} VERBATIM)
      endif()
      list(APPEND binaries "${binary}")
    endforeach()
  endforeach()
  string(TOLOWER "${backend}" suffix)
  add_custom_target(bongo_cat_${suffix}_shaders ALL DEPENDS ${binaries})
  add_dependencies(Framework bongo_cat_${suffix}_shaders)
  set(BONGO_CAT_${backend}_SHADER_BINARIES "${binaries}" PARENT_SCOPE)
endfunction()
