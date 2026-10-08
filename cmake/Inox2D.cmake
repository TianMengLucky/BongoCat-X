# Reuse the pinned upstream renderer, patched only in the generated build tree.
set(BONGO_CAT_INOX2D_SOURCE "" CACHE PATH "Existing pinned Inox2D checkout (offline builds)")
if(NOT BONGO_CAT_INOX2D_SOURCE)
  include(FetchContent)
  FetchContent_Declare(bongo_inox2d_source
    GIT_REPOSITORY https://github.com/Inochi2D/inox2d.git
    GIT_TAG d4dd9dd7f16b775042cbda44370570abf9a8cf81 GIT_SHALLOW FALSE)
  FetchContent_GetProperties(bongo_inox2d_source)
  if(NOT bongo_inox2d_source_POPULATED)
    FetchContent_Populate(bongo_inox2d_source)
  endif()
  set(BONGO_CAT_INOX2D_SOURCE "${bongo_inox2d_source_SOURCE_DIR}")
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(inox_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/inox2d")
execute_process(COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/prepare_inox2d.py"
  --upstream "${BONGO_CAT_INOX2D_SOURCE}" --source "${CMAKE_CURRENT_SOURCE_DIR}"
  --output "${inox_generated}" RESULT_VARIABLE inox_patch_result)
if(NOT inox_patch_result EQUAL 0)
  message(FATAL_ERROR "Cannot prepare the pinned Inox2D renderer")
endif()
file(GLOB inox_plugin_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rust/bongo-inox2d/src/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${inox_plugin_sources} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/prepare_inox2d.py"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/inox2d_resource_owner.rs"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rust/bongo-inox2d/build.rs"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rust/bongo-inox2d/Cargo.toml"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/rust/bongo-inox2d/Cargo.lock")
corrosion_import_crate(MANIFEST_PATH "${inox_generated}/bongo-inox2d/Cargo.toml"
  CRATE_TYPES cdylib LOCKED)

# BSD redistribution terms accompany the dynamically installed renderer.
if(APPLE)
  set(inox_license_destination "BongoCat.app/Contents/Resources/licenses")
else()
  set(inox_license_destination "licenses")
endif()
install(FILES "${BONGO_CAT_INOX2D_SOURCE}/LICENSE"
  DESTINATION "${inox_license_destination}" RENAME Inox2D-LICENSE COMPONENT Runtime)
