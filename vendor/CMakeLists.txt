set(BUILD_SHARED_LIBS OFF)

if(STUN_BUILD_APPLICATIONS)
  # Native file dialogs are used by the bundled designer.
  set(NFD_INSTALL OFF CACHE BOOL " " FORCE)
  set(NFD_PORTAL ON CACHE BOOL " " FORCE)
  set(NFD_APPEND_EXTENSION ON CACHE BOOL " " FORCE)
  set(NFD_CASE_SENSITIVE_FILTER ON CACHE BOOL " " FORCE)
  add_subdirectory(nfd)

  mark_as_advanced(
    APPKIT_LIBRARY
    UNIFORMTYPEIDENTIFIERS_LIBRARY
    NFD_APPEND_EXTENSION
    NFD_BUILD_SDL2_TESTS
    NFD_BUILD_TESTS
    NFD_CASE_SENSITIVE_FILTER
    NFD_INSTALL
    NFD_OVERRIDE_RECENT_WITH_DEFAULT
    NFD_PORTAL
    NFD_USE_ALLOWEDCONTENTTYPES_IF_AVAILABLE
  )
endif()

if(STUN_BUILD_APPLICATIONS OR FLEX_BUILD_RENDER_ENGINE_NANOVG OR
   FLEX_OPENGL_RENDER_ENGINE STREQUAL "nanovg" OR
   (APPLE AND NANOGUI_BACKEND STREQUAL "Metal"))
  add_subdirectory(nanovg)
endif()
add_subdirectory(gCanvas)
if(STUN_BUILD_APPLICATIONS)
  add_subdirectory(pdfgen)
  add_subdirectory(dxf)
  add_subdirectory(OpenXLSX)
endif()
if(STUN_BUILD_CHARTS)
  add_subdirectory(cola)
endif()
add_subdirectory(mir)
add_subdirectory(miniblas)
