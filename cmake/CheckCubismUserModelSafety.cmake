cmake_minimum_required(VERSION 3.24)

include("${CMAKE_CURRENT_LIST_DIR}/CubismUserModelSafety.cmake")

function(assert_patched variable label)
  set(source "${${variable}}")
  bongo_cat_patch_cubism_renderer_creation(source)
  set(expected [=[if (_renderer)
    {
        _renderer->Initialize(_model, maskBufferCount);
    }]=])
  string(FIND "${source}" "${expected}" guard_position)
  if(guard_position EQUAL -1)
    message(FATAL_ERROR "Cubism user-model safety test failed: ${label}")
  endif()
endfunction()

set(official_unsafe [=[void CubismUserModel::CreateRenderer(
    csmUint32 width, csmUint32 height, csmInt32 maskBufferCount)
{
    _renderer = Rendering::CubismRenderer::Create(width, height);

    _renderer->Initialize(_model, maskBufferCount);
}

void CubismUserModel::DeleteRenderer()
{
}]=])
assert_patched(official_unsafe "official r.5 layout")

set(alternate_unsafe [=[void CubismUserModel::CreateRenderer(csmInt32 maskBufferCount)
{
    _renderer=Rendering::CubismRenderer::Create();
    _renderer -> Initialize ( _model , maskBufferCount ) ;
}
void CubismUserModel::DeleteRenderer()
{
}]=])
assert_patched(alternate_unsafe "alternate renderer signature")

set(already_safe [=[void CubismUserModel::CreateRenderer(csmInt32 maskBufferCount)
{
    _renderer = Rendering::CubismRenderer::Create();
    if (_renderer != nullptr)
    {
        _renderer->Initialize(_model, maskBufferCount);
    }
}
void CubismUserModel::DeleteRenderer()
{
}]=])
set(expected_safe "${already_safe}")
bongo_cat_patch_cubism_renderer_creation(already_safe)
if(NOT already_safe STREQUAL expected_safe)
  message(FATAL_ERROR
    "Cubism user-model safety test failed: safe source was modified")
endif()

message(STATUS "Cubism user-model safety patch policy passed")

# Routing must preserve the null guard rather than generating an unsafe
# second copy, and the call must reference the bridge's own namespace.
set(routed "#include \"CubismUserModel.hpp\"\n${official_unsafe}")
bongo_cat_patch_cubism_renderer_creation(routed)
bongo_cat_route_cubism_renderer(routed)
foreach(required IN ITEMS
    "namespace bongo_cat {"
    "_renderer = ::bongo_cat::bongo_cat_cubism_create_renderer(width, height);"
    "if (_renderer)")
  string(FIND "${routed}" "${required}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "Cubism renderer routing test failed: ${required}")
  endif()
endforeach()
message(STATUS "Cubism renderer routing preserves safety checks")
