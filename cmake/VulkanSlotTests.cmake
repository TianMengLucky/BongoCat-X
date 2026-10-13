# Delayed/failing dispatch exercises production slot ownership without a GPU.
if(BONGO_CAT_PLATFORM_VULKAN AND BONGO_CAT_VULKAN_INCLUDE_DIR)
  add_executable(bongo_cat_vk_slot_tests tests/platform/test_vk_slots.c
    src/render/rhi/rhi_vk_sync.c src/render/rhi/rhi_vk_upload.c)
  target_compile_definitions(bongo_cat_vk_slot_tests PRIVATE BONGO_CAT_HAS_VULKAN_RHI=1)
  target_include_directories(bongo_cat_vk_slot_tests PRIVATE
    include src/render/rhi tests/support "${BONGO_CAT_GENERATED_INCLUDE_DIR}")
  target_include_directories(bongo_cat_vk_slot_tests SYSTEM PRIVATE "${BONGO_CAT_VULKAN_INCLUDE_DIR}")
  target_link_libraries(bongo_cat_vk_slot_tests PRIVATE SDL3::SDL3-static bongo_cat_warnings)
  add_test(NAME vulkan-fixed-slots COMMAND bongo_cat_vk_slot_tests)
  add_executable(bongo_cat_vk_submission_tests tests/live2d/test_vulkan_submission.cpp
    src/live2d/cubism_vulkan_submission.cpp)
  target_include_directories(bongo_cat_vk_submission_tests PRIVATE src/live2d tests/support)
  target_include_directories(bongo_cat_vk_submission_tests SYSTEM PRIVATE "${BONGO_CAT_VULKAN_INCLUDE_DIR}")
  target_link_libraries(bongo_cat_vk_submission_tests PRIVATE bongo_cat_warnings)
  add_test(NAME vulkan-async-submission COMMAND bongo_cat_vk_submission_tests)
  # Test the production lazy-creation critical section without creating a GPU.
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CubismVulkanLazyPipelines.cmake" lazy_patch)
  string(FIND "${lazy_patch}" "namespace {" lazy_begin)
  string(SUBSTRING "${lazy_patch}" ${lazy_begin} -1 lazy_tail)
  string(FIND "${lazy_tail}" "CubismPipeline_Vulkan::CubismPipeline_Vulkan()" lazy_end)
  if(lazy_begin LESS 0 OR lazy_end LESS 0)
    message(FATAL_ERROR "Lazy pipeline test extraction failed")
  endif()
  string(SUBSTRING "${lazy_tail}" 0 ${lazy_end} lazy_body)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/vulkan_lazy_pipeline_test.inc" "${lazy_body}")
  add_executable(bongo_cat_vk_lazy_pipeline_tests tests/live2d/test_vulkan_lazy_pipelines.cpp)
  target_include_directories(bongo_cat_vk_lazy_pipeline_tests PRIVATE tests/support "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_libraries(bongo_cat_vk_lazy_pipeline_tests PRIVATE bongo_cat_warnings)
  add_test(NAME vulkan-lazy-pipelines COMMAND bongo_cat_vk_lazy_pipeline_tests)

endif()

add_test(NAME cubism-opengl-lazy-base COMMAND ${CMAKE_COMMAND} "-DSDK=${BONGO_CAT_CUBISM_SDK}"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckCubismOpenGLLazyBase.cmake")

add_executable(bongo_cat_ui_software_tests tests/ui/test_ui_software.c)
target_include_directories(bongo_cat_ui_software_tests PRIVATE src/ui/backend src/ui/rendering tests/support)
target_include_directories(bongo_cat_ui_software_tests SYSTEM PRIVATE ${BONGO_CAT_NUKLEAR_INCLUDE_DIR})
target_link_libraries(bongo_cat_ui_software_tests PRIVATE bongo_cat_runtime bongo_cat_warnings)
add_test(NAME ui-software-memory COMMAND bongo_cat_ui_software_tests)

add_executable(bongo_cat_gl_program_tests tests/platform/test_gl_program.c)
target_include_directories(bongo_cat_gl_program_tests PRIVATE tests/support)
target_link_libraries(bongo_cat_gl_program_tests PRIVATE bongo_cat_runtime bongo_cat_warnings)
add_test(NAME opengl-program-ownership COMMAND bongo_cat_gl_program_tests)
