# Native file dialog library
set(BUILD_SHARED_LIBS OFF)
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

add_subdirectory(nanovg)
add_subdirectory(gCanvas)
add_subdirectory(pdfgen)
add_subdirectory(dxf)
add_subdirectory(OpenXLSX)
add_subdirectory(cola)
add_subdirectory(mir)
add_subdirectory(miniblas)
