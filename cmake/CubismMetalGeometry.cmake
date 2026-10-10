include("${CMAKE_CURRENT_LIST_DIR}/CubismShaderOptimize.cmake")
function(bongo_cat_metal_geometry_renderer variable)
  set(source "${${variable}}")
  string(ASCII 239 187 191 utf8_bom)
  string(REPLACE "${utf8_bom}" "" source "${source}")
  bongo_cat_replace_cubism_text(source
    "_drawableDrawCommandBuffer[i]->CreateVertexBuffer(_device, vertexSize, drawableVertexCount);"
    "if (!bongo_cat::metal_geometry_available()) _drawableDrawCommandBuffer[i]->CreateVertexBuffer(_device, vertexSize, drawableVertexCount);"
    "Metal shared drawable vertex allocation")
  set(${variable} "#include \"cubism_metal_geometry.hpp\"\n${source}" PARENT_SCOPE)
endfunction()
function(bongo_cat_metal_geometry_commands variable)
  set(source "${${variable}}")
  string(ASCII 239 187 191 utf8_bom)
  string(REPLACE "${utf8_bom}" "" source "${source}")
  bongo_cat_replace_cubism_text(source "    csmSizeInt length = count * _vbStride;"
    "    if (count <= 0 || bongo_cat::metal_geometry_update(this, data)) return;\n    csmSizeInt length = count * _vbStride;"
    "Metal shared vertex upload")
  # Immutable indices may be requested by several mask/main passes each frame.
  bongo_cat_replace_cubism_text(source "    csmInt16* sourceIndices = reinterpret_cast<csmInt16*>(data);"
    "    csmInt16* sourceIndices = reinterpret_cast<csmInt16*>(data);\n    if (memcmp(dest, sourceIndices, length) == 0) return;"
    "Metal redundant index upload")
  bongo_cat_replace_cubism_text(source "    csmSizeInt length = count * sizeof(csmInt16);"
    "    if (count <= 0) return;\n    csmSizeInt length = count * sizeof(csmInt16);" "Metal empty index upload")
  bongo_cat_replace_cubism_text(source
    "options:MTLResourceStorageModeShared];\n}\n\nvoid CubismCommandBuffer_Metal::DrawCommandBuffer::UpdateVertexBuffer"
    "options:MTLResourceStorageModeShared];\n    if (_indices && _ibCount) memset([_indices contents], 0, sizeof(csmInt16) * _ibCount);\n}\n\nvoid CubismCommandBuffer_Metal::DrawCommandBuffer::UpdateVertexBuffer"
    "Metal initial index contents")
  set(${variable} "#include \"cubism_metal_geometry.hpp\"\n#include <cstring>\n${source}" PARENT_SCOPE)
endfunction()
function(bongo_cat_metal_geometry_shader variable)
  set(source "${${variable}}")
  string(ASCII 239 187 191 utf8_bom)
  string(REPLACE "${utf8_bom}" "" source "${source}")
  foreach(pair IN ITEMS "Vertices;GetVertexBuffer;false" "UVs;GetUvBuffer;true")
    list(GET pair 0 suffix)
    list(GET pair 1 getter)
    list(GET pair 2 uv)
    set(input "MetalVertexInputIndex${suffix}")
    if(uv STREQUAL "true")
      set(input "MetalVertexInputUVs")
    endif()
    bongo_cat_replace_cubism_text(source
      "[renderEncoder setVertexBuffer:(drawCommandBuffer->${getter}()) offset:0 atIndex:${input}];"
      "[renderEncoder setVertexBuffer:bongo_cat::metal_geometry_buffer(drawCommandBuffer, drawCommandBuffer->${getter}()) offset:bongo_cat::metal_geometry_offset(drawCommandBuffer, ${uv}) atIndex:${input}];"
      "Metal shared shader binding")
  endforeach()
  set(${variable} "#include \"cubism_metal_geometry.hpp\"\n${source}" PARENT_SCOPE)
endfunction()
function(bongo_cat_metal_cache_state variable)
  set(source "${${variable}}")
  string(ASCII 239 187 191 utf8_bom)
  string(REPLACE "${utf8_bom}" "" source "${source}")
  string(REGEX REPLACE "\\[([A-Za-z_][A-Za-z_0-9]*) setRenderPipelineState:([A-Za-z_][A-Za-z_0-9]*)\\];"
    "bongo_cat::metal_set_pipeline(\\1, \\2);" source "${source}")
  string(REGEX REPLACE "\\[([A-Za-z_][A-Za-z_0-9]*) setCullMode:([A-Za-z_][A-Za-z_0-9]*)\\];"
    "bongo_cat::metal_set_cull(\\1, \\2);" source "${source}")
  string(REGEX REPLACE "\\[([A-Za-z_][A-Za-z_0-9]*) setFrontFacingWinding:([A-Za-z_][A-Za-z_0-9]*)\\];"
    "bongo_cat::metal_set_winding(\\1, \\2);" source "${source}")
  string(REGEX REPLACE "\\[([A-Za-z_][A-Za-z_0-9]*) setFragmentTexture:([A-Za-z_][A-Za-z_0-9]*) atIndex:([A-Za-z_0-9]+)\\];"
    "bongo_cat::metal_set_texture(\\1, \\2, \\3);" source "${source}")
  string(REGEX REPLACE "\\[([A-Za-z_][A-Za-z_0-9]*) setFragmentTexture:([^;\n]+) atIndex:([^;\n]+)\\];"
    "bongo_cat::metal_set_texture(\\1, \\2, \\3);" source "${source}")
  # Invalidate when encoders are created or obtained for another render pass.
  string(REGEX REPLACE "(_mtlCommandEncoder = [^;\n]+;)"
    "\\1\n    bongo_cat::metal_encoder_begin(_mtlCommandEncoder);" source "${source}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()
