set(BONGO_CAT_CUBISM_VULKAN_DEFAULT OFF)
if(WIN32 OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
    CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"))
  set(BONGO_CAT_CUBISM_VULKAN_DEFAULT ON)
endif()
option(BONGO_CAT_CUBISM_VULKAN
  "Build the Cubism Vulkan renderer into the Framework (Cubism 5.1+ SDK)"
  ${BONGO_CAT_CUBISM_VULKAN_DEFAULT})
if(BONGO_CAT_CUBISM_VULKAN AND NOT BONGO_CAT_CUBISM_VULKAN_DEFAULT)
  message(FATAL_ERROR "The Cubism Vulkan RHI supports Windows and x86_64 Linux; "
    "configure this platform with -DBONGO_CAT_CUBISM_VULKAN=OFF")
endif()

option(BONGO_CAT_CUBISM_METAL "Build the Cubism Metal renderer" ${APPLE})
if(BONGO_CAT_CUBISM_METAL AND NOT APPLE)
  message(FATAL_ERROR "The Metal renderer requires macOS")
endif()
if(BONGO_CAT_CUBISM_METAL)
  enable_language(OBJCXX)
  set(CMAKE_OBJCXX_STANDARD 17)
  set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

set(CUBISM_CORE_PATH "${BONGO_CAT_CUBISM_SDK}/Core")
set(CUBISM_FRAMEWORK_PATH "${BONGO_CAT_CUBISM_SDK}/Framework")
set(CUBISM_GLEW_PATH "${BONGO_CAT_CUBISM_SDK}/Samples/OpenGL/thirdParty/glew")

foreach(PATH IN ITEMS CUBISM_FRAMEWORK_PATH CUBISM_GLEW_PATH)
  if(NOT EXISTS "${${PATH}}")
    message(FATAL_ERROR "Incomplete Cubism SDK: ${${PATH}} is missing")
  endif()
endforeach()

# Core is never linked into the plugin. Only its API header is needed here.
if(WIN32)
  if(MSVC_VERSION LESS 1930 OR MSVC_VERSION GREATER_EQUAL 1960)
    message(FATAL_ERROR "Cubism Windows build requires Visual Studio 2022 or newer")
  endif()
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(CUBISM_WINDOWS_ARCH "x86_64")
  else()
    set(CUBISM_WINDOWS_ARCH "x86")
  endif()
  add_compile_definitions(CSM_CORE_WIN32_DLL=1)
endif()

# The generated shim owns the csm* ABI and forwards calls through host
# callbacks to the user-supplied shared Core library on every platform.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(BONGO_CAT_CORE_SHIM "${CMAKE_BINARY_DIR}/generated/cubism_core_shim.c")
add_custom_command(OUTPUT "${BONGO_CAT_CORE_SHIM}"
  COMMAND ${Python3_EXECUTABLE}
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_core_shim.py"
    --header "${CUBISM_CORE_PATH}/include/Live2DCubismCore.h"
    --output "${BONGO_CAT_CORE_SHIM}"
  DEPENDS "${CUBISM_CORE_PATH}/include/Live2DCubismCore.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gen_core_shim.py"
  COMMENT "Generating the Cubism Core runtime shim" VERBATIM)
add_library(bongo_cat_core_shim STATIC "${BONGO_CAT_CORE_SHIM}")
target_compile_definitions(bongo_cat_core_shim PRIVATE GLEW_STATIC GLEW_NO_GLU)
target_include_directories(bongo_cat_core_shim SYSTEM PUBLIC
  "${CUBISM_CORE_PATH}/include")
target_link_libraries(bongo_cat_core_shim PUBLIC bongo_cat_warnings
  ${CMAKE_DL_LIBS})
target_include_directories(bongo_cat_core_shim PRIVATE include src/live2d
  "${CUBISM_GLEW_PATH}/include"
  "${BONGO_CAT_GENERATED_INCLUDE_DIR}"
  $<TARGET_PROPERTY:SDL3::SDL3-static,INTERFACE_INCLUDE_DIRECTORIES>)

add_library(glew_s STATIC "${CUBISM_GLEW_PATH}/src/glew.c")
target_include_directories(glew_s SYSTEM PUBLIC "${CUBISM_GLEW_PATH}/include")
target_compile_definitions(glew_s PUBLIC GLEW_STATIC GLEW_NO_GLU)
target_link_libraries(glew_s PUBLIC OpenGL::GL)
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_link_libraries(glew_s PRIVATE X11::X11 ${CMAKE_DL_LIBS})
endif()
set(FRAMEWORK_SOURCE OpenGL)
add_subdirectory("${CUBISM_FRAMEWORK_PATH}" "${CMAKE_BINARY_DIR}/cubism-framework")
include(cmake/CubismUserModelSafety.cmake)
bongo_cat_harden_cubism_user_model(Framework)
include(cmake/CubismCoreProfile.cmake)
include(cmake/CubismShaderOptimize.cmake)
bongo_cat_optimize_cubism_shaders(Framework)
if(APPLE)
  bongo_cat_core_profile_prepare_shaders()
  bongo_cat_core_profile_patch_renderer(Framework)
endif()

if(WIN32)
  target_compile_definitions(Framework PUBLIC CSM_TARGET_WIN_GL)
elseif(APPLE)
  target_compile_definitions(Framework PUBLIC CSM_TARGET_MAC_GL)
else()
  target_compile_definitions(Framework PUBLIC CSM_TARGET_LINUX_GL)
endif()
# The upstream Framework is not warning-clean; keep project /Werror flags
# from reaching it (release CI builds with warnings-as-errors).
target_compile_options(Framework PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/w>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>)
target_include_directories(Framework SYSTEM PUBLIC
  "${CUBISM_FRAMEWORK_PATH}/src"
  "${CUBISM_CORE_PATH}/include")
if(BONGO_CAT_VULKAN_INCLUDE_DIR)
  target_include_directories(Framework SYSTEM PUBLIC
    ${BONGO_CAT_VULKAN_INCLUDE_DIR})
endif()
if(BONGO_CAT_CUBISM_VULKAN)
  include(cmake/CubismRenderers.cmake)
endif()
if(BONGO_CAT_CUBISM_METAL)
  include(cmake/CubismMetal.cmake)
endif()
target_link_libraries(Framework PUBLIC bongo_cat_core_shim glew_s)
