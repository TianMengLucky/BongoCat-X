if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
    BONGO_CAT_PACKAGE_PLATFORM STREQUAL "linux-x64")
  set(BONGO_CAT_APPIMAGE_DEPENDS bongo_cat)
  if(TARGET bongo-cat-live2d-backend)
    list(APPEND BONGO_CAT_APPIMAGE_DEPENDS bongo-cat-live2d-backend)
  endif()
  add_custom_target(package-appimage
    COMMAND bash "${CMAKE_SOURCE_DIR}/packaging/linux/build-appimage.sh"
      "${CMAKE_BINARY_DIR}"
    DEPENDS ${BONGO_CAT_APPIMAGE_DEPENDS}
    COMMENT "Building the BongoCat Linux AppImage"
    VERBATIM)
endif()
