# The fetched FreeType provider is linked by the actual gCanvas build.
if(NOT TARGET Freetype::Freetype)
  add_library(Freetype::Freetype INTERFACE IMPORTED GLOBAL)
  set_target_properties(Freetype::Freetype PROPERTIES
    INTERFACE_LINK_LIBRARIES freetype
    INTERFACE_INCLUDE_DIRECTORIES
      "$<TARGET_PROPERTY:freetype-interface,INTERFACE_INCLUDE_DIRECTORIES>")
endif()

set(Freetype_FOUND TRUE)
set(FREETYPE_FOUND TRUE)
