cmake_minimum_required(VERSION 3.24)
# Run with -DSDL_SOURCE_ROOT=<pinned SDL source> -DOUTPUT_DIR=<scratch directory>.
if(NOT DEFINED SDL_SOURCE_ROOT OR NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR "SDL_SOURCE_ROOT and OUTPUT_DIR are required")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/SDLWindowsRuntime.cmake")

# Exercise the production source transformations without configuring or building SDL.
function(target_include_directories)
endfunction()
function(bongo_cat_sdl_generated_source target original output source)
  if(original MATCHES "SDL_windowswindow[.]c$")
    string(FIND "${source}" "WIN_UnclipCursorForWindow(window);" release)
    string(FIND "${source}" "current.left != videodevice->desktop_bounds.x" shortcut)
    string(FIND "${source}"
      "GetClipCursor(&rect) && SDL_memcmp(&rect, &data->cursor_clipped_rect, sizeof(rect)) == 0"
      ownership)
    string(FIND "${source}"
      "if (!win_have_focus || !cursor_confine) {
        WIN_UnclipCursorForWindow(window);
        return;"
      guarded_release)
    if(release LESS 0 OR NOT shortcut EQUAL -1 OR ownership LESS 0 OR guarded_release LESS 0)
      message(FATAL_ERROR "SDL must release only its own recorded cursor clip")
    endif()
    set_property(GLOBAL PROPERTY BONGO_CURSOR_CHECKED TRUE)
  endif()
endfunction()
set(CMAKE_CURRENT_BINARY_DIR "${OUTPUT_DIR}")
bongo_cat_trim_sdl_windows(unused "${SDL_SOURCE_ROOT}")
get_property(checked GLOBAL PROPERTY BONGO_CURSOR_CHECKED)
if(NOT checked)
  message(FATAL_ERROR "SDL window source was not checked")
endif()
message(STATUS "SDL Windows cursor ownership regression check passed")
