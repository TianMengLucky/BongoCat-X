function(bongo_cat_patch_cubism_renderer_creation variable)
  set(source "${${variable}}")
  set(create_anchor "void CubismUserModel::CreateRenderer")
  set(delete_anchor "void CubismUserModel::DeleteRenderer")
  string(FIND "${source}" "${create_anchor}" create_position)
  string(FIND "${source}" "${delete_anchor}" delete_position)
  if(create_position EQUAL -1 OR delete_position LESS create_position)
    message(FATAL_ERROR
      "Cubism 5 r.5 user-model patch mismatch: renderer functions")
  endif()

  math(EXPR function_length "${delete_position} - ${create_position}")
  string(SUBSTRING "${source}" ${create_position} ${function_length}
    create_function)
  set(create_pattern
    [=[_renderer[ \t]*=[ \t]*Rendering::CubismRenderer::Create[ \t]*\([^;]*\)[ \t]*;]=])
  string(REGEX MATCH "${create_pattern}" create_statement "${create_function}")
  if(NOT create_statement)
    message(FATAL_ERROR
      "Cubism 5 r.5 user-model patch mismatch: renderer creation")
  endif()
  string(FIND "${create_function}" "${create_statement}" statement_position)
  string(LENGTH "${create_statement}" statement_length)
  math(EXPR statement_end "${statement_position} + ${statement_length}")
  string(SUBSTRING "${create_function}" ${statement_end} -1 create_tail)
  string(REGEX MATCH "${create_pattern}" duplicate_create "${create_tail}")
  if(duplicate_create)
    message(FATAL_ERROR
      "Cubism 5 r.5 user-model patch mismatch: multiple renderer creators")
  endif()

  set(initialize_pattern
    [=[_renderer[ \t]*->[ \t]*Initialize[ \t]*\([ \t]*_model[ \t]*,[ \t]*maskBufferCount[ \t]*\)[ \t]*;]=])
  string(REGEX MATCH "${initialize_pattern}" initialize_statement
    "${create_function}")
  if(NOT initialize_statement)
    message(FATAL_ERROR
      "Cubism 5 r.5 user-model patch mismatch: renderer initialization")
  endif()
  string(FIND "${create_function}" "${initialize_statement}"
    initialize_position)
  string(LENGTH "${initialize_statement}" initialize_length)
  math(EXPR initialize_end "${initialize_position} + ${initialize_length}")
  string(SUBSTRING "${create_function}" ${initialize_end} -1 initialize_tail)
  string(REGEX MATCH "${initialize_pattern}" duplicate_initialize
    "${initialize_tail}")
  if(duplicate_initialize)
    message(FATAL_ERROR
      "Cubism 5 r.5 user-model patch mismatch: multiple renderer initializers")
  endif()

  set(renderer_condition
    [=[if[ \t]*\([ \t]*_renderer([ \t]*!=[ \t]*(NULL|nullptr))?[ \t]*\)]=])
  set(guard_pattern
    "${renderer_condition}[ \t\n]*(\\{[ \t\n]*)?${initialize_pattern}")
  string(REGEX MATCH "${guard_pattern}" guarded_initialize
    "${create_function}")
  if(NOT guarded_initialize)
    set(safe_initialize [=[if (_renderer)
    {
        _renderer->Initialize(_model, maskBufferCount);
    }]=])
    string(REPLACE "${initialize_statement}" "${safe_initialize}"
      create_function "${create_function}")
    string(SUBSTRING "${source}" 0 ${create_position} prefix)
    string(SUBSTRING "${source}" ${delete_position} -1 suffix)
    set(source "${prefix}${create_function}${suffix}")
  endif()
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

# Apply routing after the safety patch, so the same generated translation
# unit keeps the null guard and reaches the bridge's selected renderer.
function(bongo_cat_route_cubism_renderer variable)
  set(source "${${variable}}")
  set(anchor [=[#include "CubismUserModel.hpp"]=])
  set(creator [=[_renderer = Rendering::CubismRenderer::Create(width, height);]=])
  string(FIND "${source}" "${anchor}" include_position)
  string(FIND "${source}" "${creator}" creator_position)
  if(include_position EQUAL -1 OR creator_position EQUAL -1)
    message(FATAL_ERROR "Cubism user-model renderer routing patch mismatch")
  endif()
  set(declaration [=[
namespace bongo_cat {
Live2D::Cubism::Framework::Rendering::CubismRenderer *
bongo_cat_cubism_create_renderer(Live2D::Cubism::Framework::csmUint32 width,
    Live2D::Cubism::Framework::csmUint32 height);
}
]=])
  string(REPLACE "${anchor}" "${anchor}\n${declaration}" source "${source}")
  string(REPLACE "${creator}"
    "_renderer = ::bongo_cat::bongo_cat_cubism_create_renderer(width, height);"
    source "${source}")
  set(${variable} "${source}" PARENT_SCOPE)
endfunction()

function(bongo_cat_harden_cubism_user_model target)
  set(model_dir "${CUBISM_FRAMEWORK_PATH}/src/Model")
  set(source_path "${model_dir}/CubismUserModel.cpp")
  set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/cubism-model")
  set(output_source "${output_dir}/CubismUserModel.cpp")

  file(READ "${source_path}" source)
  string(REPLACE "\r\n" "\n" source "${source}")

  set(resize_unsafe [=[    _renderer->SetRenderTargetSize(width, height);]=])
  set(resize_safe [=[    if (_renderer)
    {
        _renderer->SetRenderTargetSize(width, height);
    }]=])
  if(source MATCHES "void CubismUserModel::SetRenderTargetSize")
    string(FIND "${source}" "${resize_safe}" resize_safe_position)
    if(resize_safe_position EQUAL -1)
      string(FIND "${source}" "${resize_unsafe}" resize_unsafe_position)
      if(resize_unsafe_position EQUAL -1)
        message(FATAL_ERROR
          "Cubism 5 r.5 user-model patch mismatch: render target resize")
      endif()
      string(REPLACE "${resize_unsafe}" "${resize_safe}" source "${source}")
    endif()
  else()
    message(FATAL_ERROR
      "Cubism 5 r.5 user-model patch mismatch: resize function")
  endif()

  bongo_cat_patch_cubism_renderer_creation(source)
  if(BONGO_CAT_CUBISM_VULKAN OR BONGO_CAT_CUBISM_METAL)
    bongo_cat_route_cubism_renderer(source)
  endif()

  file(MAKE_DIRECTORY "${output_dir}")
  file(WRITE "${output_source}" "${source}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${source_path}")
  get_target_property(framework_sources ${target} SOURCES)
  list(REMOVE_ITEM framework_sources "${source_path}")
  set_property(TARGET ${target} PROPERTY SOURCES "${framework_sources}")
  target_sources(${target} PRIVATE "${output_source}")
  target_include_directories(${target} PRIVATE "${model_dir}")
endfunction()
