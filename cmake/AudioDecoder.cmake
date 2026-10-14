# Audio decoding lives in bongo-safe; miniaudio only mixes decoded PCM
# and drives the playback device. Keep matching definitions in every consumer.
if(BONGO_CAT_FETCH_DEPS)
  set_property(TARGET miniaudio PROPERTY SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media/audio/miniaudio_impl.c")
else()
  add_library(bongo_cat_audio_decoder STATIC src/media/audio/miniaudio_impl.c)
  if(BONGO_CAT_MINIAUDIO_INCLUDE_DIR)
    target_include_directories(bongo_cat_audio_decoder SYSTEM PUBLIC
      "${BONGO_CAT_MINIAUDIO_INCLUDE_DIR}")
  else()
    target_include_directories(bongo_cat_audio_decoder SYSTEM PUBLIC
      "$<TARGET_PROPERTY:${BONGO_CAT_MINIAUDIO_TARGET},INTERFACE_INCLUDE_DIRECTORIES>")
  endif()
  find_package(Threads REQUIRED)
  target_link_libraries(bongo_cat_audio_decoder PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
  if(UNIX)
    target_link_libraries(bongo_cat_audio_decoder PRIVATE m)
  endif()
  set(BONGO_CAT_MINIAUDIO_TARGET bongo_cat_audio_decoder)
endif()
target_compile_definitions(${BONGO_CAT_MINIAUDIO_TARGET} PUBLIC
  MA_NO_ENCODING MA_NO_GENERATION MA_NO_CUSTOM
  MA_NO_DECODING MA_NO_RESOURCE_MANAGER)
