  add_test(NAME cubism-vulkan-bindless COMMAND ${CMAKE_COMMAND}
    "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DSDK=${BONGO_CAT_CUBISM_SDK}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCubismVulkanBindless.cmake")
  add_test(NAME cubism-vulkan-recording COMMAND ${CMAKE_COMMAND} "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DSDK=${BONGO_CAT_CUBISM_SDK}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCubismVulkanRecording.cmake")
  add_test(NAME generated-input-stability COMMAND ${CMAKE_COMMAND}
    "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/generated-input-test"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckGeneratedInputStability.cmake")
