if(NOT TARGET freetype)
  message(FATAL_ERROR "The Android qualification harness requires the fetched FreeType target")
endif()

if(NOT TARGET Freetype::Freetype)
  add_library(Freetype::Freetype INTERFACE IMPORTED GLOBAL)
  set_target_properties(Freetype::Freetype PROPERTIES
    INTERFACE_LINK_LIBRARIES freetype
    INTERFACE_INCLUDE_DIRECTORIES
      "$<TARGET_PROPERTY:freetype-interface,INTERFACE_INCLUDE_DIRECTORIES>")
endif()

set(Freetype_FOUND TRUE)
set(FREETYPE_FOUND TRUE)
