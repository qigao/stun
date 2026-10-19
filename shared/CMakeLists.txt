# Repository-owned reusable modules. These are ordinary CMake subdirectories,
# not Git submodules, and remain versioned with the application.
# gCanvas is configured earlier because Flex/FlexUI consume its targets.
if(NOT TARGET gcanvas_core)
  add_subdirectory(gcanvas)
endif()
add_subdirectory(markdown)
add_subdirectory(layout)
