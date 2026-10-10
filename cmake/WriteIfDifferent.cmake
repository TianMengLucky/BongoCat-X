include_guard(GLOBAL)

# Keep generated inputs stable across configure runs so incremental builds
# only compile files whose contents actually changed.
function(bongo_cat_write_if_different path content)
  if(EXISTS "${path}")
    file(READ "${path}" previous)
    if(previous STREQUAL content)
      return()
    endif()
  endif()
  file(WRITE "${path}" "${content}")
endfunction()
