include("${CMAKE_CURRENT_LIST_DIR}/WriteIfDifferent.cmake")
function(bongo_cat_enable_mver_physics target)
  set(physics_dir "${CUBISM_FRAMEWORK_PATH}/src/Physics")
  set(source_path "${physics_dir}/CubismPhysics.cpp")
  set(header_path "${physics_dir}/CubismPhysics.hpp")
  set(legacy_path "${CMAKE_CURRENT_SOURCE_DIR}/src/live2d/cubism_physics_mver.inc")
  set(output_root "${CMAKE_CURRENT_BINARY_DIR}/generated/cubism-mver")
  set(output_dir "${output_root}/Physics")
  set(output_source "${output_dir}/CubismPhysics.cpp")
  set(output_header "${output_dir}/CubismPhysics.hpp")

  file(READ "${source_path}" source)
  file(READ "${header_path}" header)
  string(REPLACE "\r\n" "\n" source "${source}")
  string(REPLACE "\r\n" "\n" header "${header}")
  set(evaluate_declaration
    "    void Evaluate(CubismModel* model, csmFloat32 deltaTimeSeconds);")
  string(FIND "${header}" "${evaluate_declaration}" declaration_position)
  if(declaration_position EQUAL -1)
    message(FATAL_ERROR "Cubism 5 r.5 physics patch mismatch: Evaluate declaration")
  endif()
  string(REPLACE "${evaluate_declaration}"
    "${evaluate_declaration}\n\n    void EvaluateMver(CubismModel* model, csmFloat32 deltaTimeSeconds);"
    header "${header}")

  file(TO_CMAKE_PATH "${legacy_path}" legacy_include)
  set(interpolate_anchor "void CubismPhysics::Interpolate(CubismModel* model, csmFloat32 weight)")
  string(FIND "${source}" "${interpolate_anchor}" interpolate_position)
  if(interpolate_position EQUAL -1)
    message(FATAL_ERROR "Cubism 5 r.5 physics patch mismatch: Interpolate definition")
  endif()
  string(REPLACE "${interpolate_anchor}"
    "#include \"${legacy_include}\"\n\n${interpolate_anchor}" source "${source}")

  file(MAKE_DIRECTORY "${output_dir}")
  file(GLOB physics_headers "${physics_dir}/*.hpp")
  foreach(physics_header IN LISTS physics_headers)
    get_filename_component(header_name "${physics_header}" NAME)
    if(NOT header_name STREQUAL "CubismPhysics.hpp")
      configure_file("${physics_header}" "${output_dir}/${header_name}" COPYONLY)
    endif()
  endforeach()
  bongo_cat_write_if_different("${output_header}" "${header}")
  bongo_cat_write_if_different("${output_source}" "${source}")

  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${source_path}" "${header_path}" "${legacy_path}" ${physics_headers})
  get_target_property(framework_sources ${target} SOURCES)
  list(REMOVE_ITEM framework_sources "${source_path}")
  set_property(TARGET ${target} PROPERTY SOURCES "${framework_sources}")
  target_sources(${target} PRIVATE "${output_source}")
  target_include_directories(${target} BEFORE PUBLIC "${output_root}")
endfunction()

function(bongo_cat_enable_mver_motion target)
  set(motion_dir "${CUBISM_FRAMEWORK_PATH}/src/Motion")
  set(source_path "${motion_dir}/CubismMotion.cpp")
  set(header_path "${motion_dir}/CubismMotion.hpp")
  set(legacy_path "${CMAKE_CURRENT_SOURCE_DIR}/src/live2d/cubism_motion_mver.inc")
  set(output_root "${CMAKE_CURRENT_BINARY_DIR}/generated/cubism-mver")
  set(output_dir "${output_root}/Motion")
  set(output_source "${output_dir}/CubismMotion.cpp")
  set(output_header "${output_dir}/CubismMotion.hpp")

  file(READ "${source_path}" source)
  file(READ "${header_path}" header)
  string(REPLACE "\r\n" "\n" source "${source}")
  string(REPLACE "\r\n" "\n" header "${header}")
  foreach(sibling IN ITEMS CubismMotionInternal CubismMotionJson
      CubismMotionQueueManager CubismMotionQueueEntry)
    string(REPLACE "#include \"${sibling}.hpp\""
      "#include \"Motion/${sibling}.hpp\"" source "${source}")
  endforeach()
  string(REPLACE "#include \"ACubismMotion.hpp\""
    "#include \"Motion/ACubismMotion.hpp\"" header "${header}")

  set(behavior_declaration
    "    void SetMotionBehavior(MotionBehavior motionBehavior);")
  string(FIND "${header}" "${behavior_declaration}" declaration_position)
  if(declaration_position EQUAL -1)
    message(FATAL_ERROR "Cubism 5 r.5 motion patch mismatch: behavior declaration")
  endif()
  string(REPLACE "${behavior_declaration}"
    "${behavior_declaration}\n\n    void UseMverCurveEvaluation();"
    header "${header}")

  file(TO_CMAKE_PATH "${legacy_path}" legacy_include)
  set(behavior_anchor
    "void CubismMotion::SetMotionBehavior(MotionBehavior motionBehavior)")
  string(FIND "${source}" "${behavior_anchor}" behavior_position)
  if(behavior_position EQUAL -1)
    message(FATAL_ERROR "Cubism 5 r.5 motion patch mismatch: behavior definition")
  endif()
  string(REPLACE "${behavior_anchor}"
    "#include \"${legacy_include}\"\n\n${behavior_anchor}" source "${source}")

  file(REMOVE_RECURSE "${output_dir}")
  file(MAKE_DIRECTORY "${output_dir}")
  bongo_cat_write_if_different("${output_header}" "${header}")
  bongo_cat_write_if_different("${output_source}" "${source}")

  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${source_path}" "${header_path}" "${legacy_path}")
  get_target_property(framework_sources ${target} SOURCES)
  list(REMOVE_ITEM framework_sources "${source_path}")
  set_property(TARGET ${target} PROPERTY SOURCES "${framework_sources}")
  target_sources(${target} PRIVATE "${output_source}")
  target_include_directories(${target} BEFORE PUBLIC "${output_root}")
endfunction()
