# flexUI - UI Framework

cmake_minimum_required(VERSION 3.10)

project(flexUI)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(include)

find_package(nlohmann_json CONFIG REQUIRED)

set(FLEXUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(FLEXUI_INCLUDE_DIR "${FLEXUI_ROOT}/include")
add_subdirectory(modules/tailwindcss)

# Find lexbor for CSS parsing
find_path(LEXBOR_INCLUDE_DIR NAMES lexbor/core/base.h)
find_library(LEXBOR_LIBRARY NAMES lexbor)

# Find tree-sitter for syntax highlighting (core only, languages via plugins)
find_package(unofficial-tree-sitter CONFIG REQUIRED)

if(NOT LEXBOR_INCLUDE_DIR)
    message(FATAL_ERROR
      "flexUI: Lexbor include directory not found. Please install via vcpkg: vcpkg install lexbor"
    )
endif()

if(NOT LEXBOR_LIBRARY)
    message(FATAL_ERROR
      "flexUI: Lexbor library not found. Please install via vcpkg: vcpkg install lexbor"
    )
endif()

message(STATUS "flexUI: Using lexbor from ${LEXBOR_INCLUDE_DIR}")
include_directories(${LEXBOR_INCLUDE_DIR})

set(FLEXUI_UTILITY_WHITELIST
    "${CMAKE_CURRENT_SOURCE_DIR}/../tools/shadcn-ir/schema/utility_whitelist.json")
set(FLEXUI_DEFAULT_THEME
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/default_theme.css")
set_property(
  DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${FLEXUI_UTILITY_WHITELIST}"
  "${FLEXUI_DEFAULT_THEME}")
file(READ "${FLEXUI_UTILITY_WHITELIST}"
     FLEXUI_DEFAULT_UTILITY_WHITELIST_JSON)
file(READ "${FLEXUI_DEFAULT_THEME}" FLEXUI_DEFAULT_THEME_CSS)

# Keep each generated literal below MSVC's per-literal size limit. Adjacent C++
# string literals are concatenated by the compiler without changing the asset.
function(flexui_make_cpp_raw_string input output)
  set(chunk_size 8192)
  string(LENGTH "${input}" input_length)
  set(offset 0)
  set(result "")
  while(offset LESS input_length)
    math(EXPR remaining "${input_length} - ${offset}")
    if(remaining GREATER chunk_size)
      set(length ${chunk_size})
    else()
      set(length ${remaining})
    endif()
    string(SUBSTRING "${input}" ${offset} ${length} chunk)
    string(APPEND result "R\"FLEXUI_ASSET(${chunk})FLEXUI_ASSET\"\n")
    math(EXPR offset "${offset} + ${length}")
  endwhile()
  set(${output} "${result}" PARENT_SCOPE)
endfunction()

flexui_make_cpp_raw_string("${FLEXUI_DEFAULT_UTILITY_WHITELIST_JSON}"
                           FLEXUI_DEFAULT_UTILITY_WHITELIST_CPP)
flexui_make_cpp_raw_string("${FLEXUI_DEFAULT_THEME_CSS}"
                           FLEXUI_DEFAULT_THEME_CPP)
set(FLEXUI_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${FLEXUI_GENERATED_DIR}")
configure_file(
  src/default_style_assets.cpp.in
  "${FLEXUI_GENERATED_DIR}/default_style_assets.cpp"
  @ONLY)


# 源文件
set(FLEXUI_SOURCES
  "${FLEXUI_GENERATED_DIR}/default_style_assets.cpp"
  src/box.cpp
  src/binding_runtime.cpp
  src/ui_document.cpp
  src/keyed_repeater.cpp
  src/view_pipeline.cpp
  src/element.cpp
  src/widget.cpp
  src/computed_style.cpp
  src/style_engine.cpp
  src/clipboard.cpp
  src/textedit.cpp
  src/transition.cpp
  src/text_util.cpp
  src/text_layout.cpp
  src/event_dispatcher.cpp
  src/layout_manager.cpp
  src/render_manager.cpp
  src/render_command.cpp
  src/shadcn_ir.cpp
  src/syntax_highlighter.cpp
  src/highlighters/treesitter_highlighter.cpp
  src/highlighters/ts_plugin_loader.cpp
  src/widgets/input_widget.cpp
  src/widgets/button_widget.cpp
  src/widgets/label_widget.cpp
  src/widgets/checkbox_widget.cpp
  src/widgets/slider_widget.cpp
  src/widgets/progressbar_widget.cpp
  src/widgets/radio_widget.cpp
  src/widgets/textarea_widget.cpp
  src/widgets/select_widget.cpp
  src/widgets/switch_widget.cpp
  src/widgets/tooltip_widget.cpp
  src/widgets/modal_widget.cpp
  src/widgets/tabs_widget.cpp
  src/widgets/image_widget.cpp
  src/widgets/badge_widget.cpp
  src/widgets/spinner_widget.cpp
  src/widgets/divider_widget.cpp
  src/widgets/toast_widget.cpp
  src/widgets/dropdown_widget.cpp
  src/widgets/accordion_widget.cpp
  src/widgets/calendar_widget.cpp
  src/widgets/table_widget.cpp
  src/widgets/tree_widget.cpp
  src/widgets/colorpicker_widget.cpp
  src/widgets/group_button_widget.cpp
  src/widgets/toggle_group_widget.cpp
  src/widgets/avatar_widget.cpp
  src/widgets/card_widget.cpp
  src/widgets/breadcrumb_widget.cpp
  src/widgets/pagination_widget.cpp
  src/widgets/stepper_widget.cpp
  src/widgets/gradient_editor_widget.cpp
  src/widgets/markdown_widget.cpp
)

# 头文件
set(FLEXUI_HEADERS
  include/flexUI/types.h
  include/flexUI/binding_runtime.h
  include/flexUI/ui_document.h
  include/flexUI/keyed_repeater.h
  include/flexUI/text_value_widget.h
  include/flexUI/computed_style.h
  include/flexUI/style_engine.h
  include/flexUI/event.h
  include/flexUI/event_dispatcher.h
  include/flexUI/view_pipeline.h
  include/flexUI/layout_manager.h
  include/flexUI/render_manager.h
  include/flexUI/render_command.h
  include/flexUI/render_frame.h
  include/flexUI/shadcn_ir.h
  include/flexUI/tailwindcss.h
  include/flexUI/utility_jit.h
  include/flexUI/widget.h
  include/flexUI/element.h
  include/flexUI/box.h
  include/flexUI/textedit.h
  include/flexUI/transition.h
  include/flexUI/text_layout.h
  include/flexUI/text_util.h
  include/flexUI/renderer.h
  include/flexUI/group.h
  include/flexUI/shapes.h
  include/flexUI/widgets/input_widget.h
  include/flexUI/widgets/label_widget.h
  include/flexUI/widgets/checkbox_widget.h
  include/flexUI/widgets/slider_widget.h
  include/flexUI/widgets/progressbar_widget.h
  include/flexUI/widgets/button_widget.h
  include/flexUI/widgets/radio_widget.h
  include/flexUI/widgets/textarea_widget.h
  include/flexUI/widgets/select_widget.h
  include/flexUI/widgets/tooltip_widget.h
  include/flexUI/widgets/modal_widget.h
  include/flexUI/widgets/tabs_widget.h
  include/flexUI/widgets/switch_widget.h
  include/flexUI/widgets/image_widget.h
  include/flexUI/widgets/badge_widget.h
  include/flexUI/widgets/spinner_widget.h
  include/flexUI/widgets/divider_widget.h
  include/flexUI/widgets/toast_widget.h
  include/flexUI/widgets/dropdown_widget.h
  include/flexUI/widgets/accordion_widget.h
  include/flexUI/widgets/calendar_widget.h
  include/flexUI/widgets/table_widget.h
  include/flexUI/widgets/tree_widget.h
  include/flexUI/widgets/colorpicker_widget.h
  include/flexUI/widgets/group_button_widget.h
  include/flexUI/widgets/toggle_group_widget.h
  include/flexUI/widgets/avatar_widget.h
  include/flexUI/widgets/card_widget.h
  include/flexUI/widgets/breadcrumb_widget.h
  include/flexUI/widgets/pagination_widget.h
  include/flexUI/widgets/stepper_widget.h
  include/flexUI/widgets/gradient_editor_widget.h
  include/flexUI/widgets/markdown_widget.h
)

# 创建库
add_library(flexUI STATIC ${FLEXUI_SOURCES} ${FLEXUI_HEADERS})
add_library(FlexUI::Core ALIAS flexUI)

# Windows: disable min/max macros that conflict with std::min/std::max
if(WIN32)
    target_compile_definitions(flexUI PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# 链接依赖
target_link_libraries(flexUI 
    PUBLIC ${LEXBOR_LIBRARY} Flex::Runtime Flex::Render Flex::Animation FlexUI::TailwindCSS TurboUtils::Core md_parser nlohmann_json::nlohmann_json
    PRIVATE Flex::Compiler unofficial::tree-sitter::tree-sitter
)
set_target_properties(flexUI PROPERTIES FOLDER flexUI/)
target_include_directories(
  flexUI
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR}/modules/tailwindcss
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/tools/tree-sitter/plugins>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/
         ${CMAKE_SOURCE_DIR}/vendor/md/include
         $<INSTALL_INTERFACE:include>
         ${Stb_INCLUDE_DIR})


# 安装头文件
install(DIRECTORY include/flexUI DESTINATION include)
install(FILES include/flexUI.h DESTINATION include)

# 安装库
install(TARGETS flexUI flexUI_tailwindcss DESTINATION lib)

add_subdirectory(examples)
add_subdirectory(tests)
