if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()

file(GLOB_RECURSE SOURCES
  "${ROOT}/src/*.c" "${ROOT}/src/*.cc" "${ROOT}/src/*.cpp"
  "${ROOT}/src/*.cxx" "${ROOT}/src/*.m" "${ROOT}/src/*.mm"
  "${ROOT}/src/*.h" "${ROOT}/src/*.hh" "${ROOT}/src/*.hpp"
  "${ROOT}/src/*.hxx" "${ROOT}/include/*.h" "${ROOT}/include/*.hh"
  "${ROOT}/include/*.hpp" "${ROOT}/include/*.hxx")
set(KEYS "")
foreach(SOURCE IN LISTS SOURCES)
  file(READ "${SOURCE}" CONTENT)
  string(REGEX MATCHALL
    "\"(native|pages|components|composables)\\.[A-Za-z0-9_.]+\""
    FOUND_KEYS "${CONTENT}")
  foreach(FOUND IN LISTS FOUND_KEYS)
    string(LENGTH "${FOUND}" LENGTH)
    math(EXPR LAST "${LENGTH} - 2")
    string(SUBSTRING "${FOUND}" 1 ${LAST} KEY)
    list(APPEND KEYS "${KEY}")
  endforeach()
endforeach()
list(REMOVE_DUPLICATES KEYS)

file(GLOB LOCALES "${ROOT}/resources/assets/locales/*.json")
if(NOT LOCALES)
  message(FATAL_ERROR "No locale files found")
endif()
foreach(LOCALE IN LISTS LOCALES)
  file(READ "${LOCALE}" JSON)
  get_filename_component(LOCALE_NAME "${LOCALE}" NAME)
  set(MISSING "")
  foreach(KEY IN LISTS KEYS)
    string(REPLACE "." ";" PARTS "${KEY}")
    set(VALUE "${JSON}")
    foreach(PART IN LISTS PARTS)
      string(JSON VALUE ERROR_VARIABLE ERROR GET "${VALUE}" "${PART}")
      if(ERROR)
        list(APPEND MISSING "${KEY}")
        break()
      endif()
    endforeach()
  endforeach()
  if(NOT MISSING)
    continue()
  endif()
  # zh-CN and en-US are the maintained reference locales; the app falls
  # back to English per key, so gaps elsewhere only cost polish.
  if(LOCALE_NAME STREQUAL "zh-CN.json" OR LOCALE_NAME STREQUAL "en-US.json")
    message(FATAL_ERROR
      "Missing localization keys ${MISSING} in ${LOCALE_NAME}")
  endif()
  list(LENGTH MISSING MISSING_COUNT)
  message(WARNING "${LOCALE_NAME} misses ${MISSING_COUNT} localization "
    "keys (${MISSING}); the UI falls back to English for them")
endforeach()
list(LENGTH KEYS KEY_COUNT)
message(STATUS "Localization key policy passed (${KEY_COUNT} keys)")
