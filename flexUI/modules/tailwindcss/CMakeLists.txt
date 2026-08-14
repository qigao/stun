# Tailwind-style utility compiler. This target is independent of FlexUI Core;
# Core owns tree scanning and stylesheet application.

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
