# flexUI - UI Framework

cmake_minimum_required(VERSION 3.10)

project(flexUI)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(include)

# Find lexbor for CSS parsing
find_path(LEXBOR_INCLUDE_DIR NAMES lexbor/core/base.h)
find_library(LEXBOR_LIBRARY NAMES lexbor)

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

# 源文件
set(FLEXUI_SOURCES
  src/box.cpp
  src/element.cpp
  src/computed_style.cpp
  src/style_engine.cpp
  src/textedit.cpp
  src/transition.cpp
  src/text_util.cpp
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
  include/flexUI/computed_style.h
  include/flexUI/style_engine.h
  include/flexUI/event.h
  include/flexUI/widget.h
  include/flexUI/element.h
  include/flexUI/box.h
  include/flexUI/textedit.h
  include/flexUI/transition.h
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

# 链接依赖（flex 提供后端无关的渲染抽象，thorvg 由 flex 传递依赖）
target_link_libraries(flexUI PUBLIC ${LEXBOR_LIBRARY} flex TurboNet::Utils md_parser)
set_target_properties(flexUI PROPERTIES FOLDER flexUI/)
target_include_directories(
  flexUI
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         ${CMAKE_SOURCE_DIR}/vendor/tinyfsm/
         ${CMAKE_SOURCE_DIR}/vendor/md/include
         $<INSTALL_INTERFACE:include>
         ${Stb_INCLUDE_DIR})


# 安装头文件
install(DIRECTORY include/flexUI DESTINATION include)

# 安装库
install(TARGETS flexUI DESTINATION lib)

add_subdirectory(examples)
add_subdirectory(tests)
