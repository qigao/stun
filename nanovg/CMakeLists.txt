add_subdirectory(nanovg)
if(APPLE AND NANOGUI_BACKEND STREQUAL "Metal")
  add_subdirectory(metal)
endif()