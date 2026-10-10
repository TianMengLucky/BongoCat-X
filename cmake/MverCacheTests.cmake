# Included by Tests.cmake only when BUILD_TESTING is enabled.
add_library(bongo_cat_mver_cache_test_subject OBJECT
  src/render/mver_pointer_overlay_geometry.c)
target_compile_definitions(bongo_cat_mver_cache_test_subject PRIVATE
  bongo_cat_mver_pointer_geometry=mver_cache_counted_geometry)
target_link_libraries(bongo_cat_mver_cache_test_subject PRIVATE
  bongo_cat_core SDL3::SDL3-static bongo_cat_warnings)
add_executable(bongo_cat_mver_cache_tests tests/render/test_mver_cache.c
  $<TARGET_OBJECTS:bongo_cat_mver_cache_test_subject>)
target_include_directories(bongo_cat_mver_cache_tests PRIVATE src/render tests/support)
target_link_libraries(bongo_cat_mver_cache_tests PRIVATE
  bongo_cat_core SDL3::SDL3-static bongo_cat_warnings)
add_test(NAME mver-geometry-cache COMMAND bongo_cat_mver_cache_tests)
