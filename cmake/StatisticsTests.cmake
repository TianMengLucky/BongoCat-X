if(BUILD_TESTING)
  add_executable(bongo_cat_input_metrics_tests tests/core/test_input_metrics.c)
  target_include_directories(bongo_cat_input_metrics_tests PRIVATE tests/support)
  target_link_libraries(bongo_cat_input_metrics_tests PRIVATE bongo_cat_core bongo_cat_warnings)
  add_test(NAME input-metrics COMMAND bongo_cat_input_metrics_tests)
  set_tests_properties(input-metrics PROPERTIES TIMEOUT 30)
  if(BONGO_CAT_BUILD_STATISTICS)
    add_test(NAME statistics-rust
      COMMAND "${BONGO_CAT_CARGO}" test --locked --manifest-path
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rust/bongo-stats/Cargo.toml"
        --target-dir "${CMAKE_BINARY_DIR}/statistics-tests")
    set_tests_properties(statistics-rust PROPERTIES TIMEOUT 180)
  endif()
endif()
