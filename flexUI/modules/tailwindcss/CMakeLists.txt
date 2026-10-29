# The native compiler owns no UI tree, CSS parser, JSON catalog or runtime.
# flex/core/types.h supplies the shared, standard-library-only Color value.
include(GNUInstallDirs)
get_filename_component(
  FLEXUI_NATIVE_FLEX_INCLUDE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../flex/include" ABSOLUTE)

add_library(
  flexUI_tailwind_native STATIC
  native_compiler.cpp
  ${FLEXUI_INCLUDE_DIR}/flexUI/tailwind_native.h
  ${FLEXUI_INCLUDE_DIR}/flexUI/style_property_id.h)
add_library(FlexUI::TailwindNative ALIAS flexUI_tailwind_native)
target_include_directories(
  flexUI_tailwind_native
  PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
         $<BUILD_INTERFACE:${FLEXUI_NATIVE_FLEX_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(flexUI_tailwind_native PUBLIC cxx_std_17)
set_target_properties(
  flexUI_tailwind_native PROPERTIES FOLDER "flexUI/tailwindcss"
                                    EXPORT_NAME TailwindNative
                                    POSITION_INDEPENDENT_CODE ON)
install(
  TARGETS flexUI_tailwind_native
  EXPORT FlexUITailwindNativeTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT FlexUITailwindNative
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT FlexUITailwindNative
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT FlexUITailwindNative)

# Existing CSS-emitting compiler for unmigrated consumers. Core owns its tree
# scanning and stylesheet application until consumer migration is complete.

add_library(
  flexUI_tailwindcss STATIC
  compiler.cpp
  utility_jit.cpp
  ${FLEXUI_INCLUDE_DIR}/flexUI/tailwindcss.h
  ${FLEXUI_INCLUDE_DIR}/flexUI/utility_jit.h)
add_library(FlexUI::TailwindCSS ALIAS flexUI_tailwindcss)

target_include_directories(
  flexUI_tailwindcss
  PUBLIC $<BUILD_INTERFACE:${FLEXUI_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:include>)
target_compile_features(flexUI_tailwindcss PUBLIC cxx_std_17)
target_link_libraries(flexUI_tailwindcss PUBLIC nlohmann_json::nlohmann_json)
set_target_properties(
  flexUI_tailwindcss PROPERTIES FOLDER "flexUI/tailwindcss"
                                POSITION_INDEPENDENT_CODE ON)
