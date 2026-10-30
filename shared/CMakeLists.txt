# Repository-owned reusable modules. These are ordinary CMake subdirectories,
# not Git submodules, and remain versioned with the application.
# gCanvas is configured earlier because Flex/FlexUI consume its targets.
# The parent defines gCanvas before the dependent Flex/FlexUI modules.
# No target-availability fallback or duplicate provider build.
add_subdirectory(markdown)
add_subdirectory(layout)
