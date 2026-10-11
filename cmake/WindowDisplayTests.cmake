if(BUILD_TESTING)
  add_executable(bongo_cat_window_display_tests tests/platform/test_window_display.c)
  target_include_directories(bongo_cat_window_display_tests PRIVATE
    tests/support include "${BONGO_CAT_GENERATED_INCLUDE_DIR}"
    ${BONGO_CAT_RUNTIME_INTERNAL_INCLUDE_DIRS})
  target_link_libraries(bongo_cat_window_display_tests PRIVATE
    SDL3::SDL3-static bongo_cat_warnings)
  if(MSVC)
    target_compile_options(bongo_cat_window_display_tests PRIVATE /experimental:c11atomics)
  endif()
  add_test(NAME window-display-policy COMMAND bongo_cat_window_display_tests)
endif()
