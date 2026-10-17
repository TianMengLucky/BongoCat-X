if(BUILD_TESTING)
  add_executable(bongo_cat_items_memory_tests tests/items/test_items_memory.c)
  target_include_directories(bongo_cat_items_memory_tests PRIVATE
    src/runtime/items src/runtime tests/support)
  target_link_libraries(bongo_cat_items_memory_tests PRIVATE bongo_cat_runtime bongo_cat_warnings)
  add_test(NAME items-memory COMMAND bongo_cat_items_memory_tests)
endif()
