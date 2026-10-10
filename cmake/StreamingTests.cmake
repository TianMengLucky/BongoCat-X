add_test(NAME cubism-renderer-streaming COMMAND ${CMAKE_COMMAND}
  "-DSDK=${BONGO_CAT_CUBISM_SDK}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCubismStreaming.cmake")
if(TARGET Framework AND BONGO_CAT_CUBISM_VULKAN)
  add_executable(bongo_cat_vulkan_submission_tests tests/live2d/test_vulkan_submission.cpp)
  target_include_directories(bongo_cat_vulkan_submission_tests PRIVATE src/live2d)
  target_link_libraries(bongo_cat_vulkan_submission_tests PRIVATE Framework bongo_cat_warnings)
  add_test(NAME live2d-vulkan-submission COMMAND bongo_cat_vulkan_submission_tests)
endif()
