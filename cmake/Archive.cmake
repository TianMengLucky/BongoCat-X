include("${CMAKE_CURRENT_LIST_DIR}/WriteIfDifferent.cmake")
if(BONGO_CAT_FETCH_DEPS)
  FetchContent_Declare(miniz
    URL https://codeload.github.com/richgel999/miniz/tar.gz/refs/tags/3.1.0
    URL_HASH SHA256=09569fc19d060ac9f5999ba9356728c2494ebe6a24ac0eb0a6b6ae3d396cfea6
    SOURCE_SUBDIR bongocat-unused)
  FetchContent_MakeAvailable(miniz)
  # Build only the library, without changing the parent's build options.
  bongo_cat_write_if_different("${miniz_BINARY_DIR}/miniz_export.h"
    "#pragma once\n#define MINIZ_EXPORT\n")
  add_library(bongo_cat_archive STATIC
    "${miniz_SOURCE_DIR}/miniz.c" "${miniz_SOURCE_DIR}/miniz_zip.c"
    "${miniz_SOURCE_DIR}/miniz_tinfl.c" "${miniz_SOURCE_DIR}/miniz_tdef.c")
  target_include_directories(bongo_cat_archive SYSTEM PUBLIC
    "${miniz_SOURCE_DIR}" "${miniz_BINARY_DIR}")
  # Archive callers supply SDL stream callbacks or in-memory data. Removing
  # stdio also prunes file branches retained by the shared ZIP cleanup code.
  # Keep this PUBLIC so every header consumer sees the same available APIs;
  # system packages below retain their original compile-time configuration.
  target_compile_definitions(bongo_cat_archive PUBLIC MINIZ_NO_STDIO)
else()
  find_package(miniz CONFIG REQUIRED)
  add_library(bongo_cat_archive INTERFACE)
  target_link_libraries(bongo_cat_archive INTERFACE miniz::miniz)
endif()
