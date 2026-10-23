cmake_minimum_required(VERSION 3.20...3.31)
project(StunXmlRuntimeDependencies LANGUAGES C CXX)
include(FetchContent)
include(GNUInstallDirs)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static qualification dependencies" FORCE)
set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BOX2D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
# Exact upstream release commits; no window/GPU libraries or vcpkg bootstrap.
FetchContent_Declare(lexbor GIT_REPOSITORY https://github.com/lexbor/lexbor.git
  GIT_TAG v3.0.0 GIT_SHALLOW TRUE)
FetchContent_Declare(box2d GIT_REPOSITORY https://github.com/erincatto/box2d.git
  GIT_TAG v3.1.1 GIT_SHALLOW TRUE)
FetchContent_Declare(tree_sitter GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
  GIT_TAG v0.25.10 GIT_SHALLOW TRUE SOURCE_SUBDIR lib)
FetchContent_MakeAvailable(lexbor box2d tree_sitter)
function(verify_revision dependency expected)
  execute_process(COMMAND git -C "${${dependency}_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE actual OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "${dependency} revision mismatch: ${actual}, expected ${expected}")
  endif()
  message(STATUS "XML dependency ${dependency}: ${actual}")
endfunction()
verify_revision(lexbor 2ae88a1c6b5261830eff73ee12bb3cdf805f3cfe)
verify_revision(box2d 8c661469c9507d3ad6fbd2fea3f1aa71669c2fe3)
verify_revision(tree_sitter da6fe9beb4f7f67beb75914ca8e0d48ae48d6406)

# FlexUI consumes vcpkg's package spelling. Export the actual upstream static
# library under that spelling, with its installed public headers.
install(EXPORT StunTreeSitterTargets NAMESPACE unofficial::tree-sitter::
  DESTINATION lib/cmake/unofficial-tree-sitter)
install(TARGETS tree-sitter EXPORT StunTreeSitterTargets
  ARCHIVE DESTINATION lib INCLUDES DESTINATION include)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/unofficial-tree-sitter-config.cmake"
  "include(\"\${CMAKE_CURRENT_LIST_DIR}/StunTreeSitterTargets.cmake\")\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/unofficial-tree-sitter-config.cmake"
  DESTINATION lib/cmake/unofficial-tree-sitter)
