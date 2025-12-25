# tvgbox2 - 新架构

cmake_minimum_required(VERSION 3.10)

project(tvgbox2)

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
      "tvgbox2: Lexbor include directory not found. Please install via vcpkg: vcpkg install lexbor"
    )
endif()

if(NOT LEXBOR_LIBRARY)
    message(FATAL_ERROR
      "tvgbox2: Lexbor library not found. Please install via vcpkg: vcpkg install lexbor"
    )
endif()

message(STATUS "tvgbox2: Using lexbor from ${LEXBOR_INCLUDE_DIR}")
include_directories(${LEXBOR_INCLUDE_DIR})

# 源文件
set(TVGBOX2_SOURCES
  src/box.cpp
  src/computed_style.cpp
  src/style_engine.cpp
  src/layout_engine.cpp
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
)

# 头文件
set(TVGBOX2_HEADERS
  include/tvgbox2/types.h
  include/tvgbox2/computed_style.h
  include/tvgbox2/style_engine.h
  include/tvgbox2/layout_engine.h
  include/tvgbox2/event.h
  include/tvgbox2/widget.h
  include/tvgbox2/element.h
  include/tvgbox2/box.h
  include/tvgbox2/textedit.h
  include/tvgbox2/transition.h
  include/tvgbox2/text_util.h
  include/tvgbox2/renderer.h
  include/tvgbox2/group.h
  include/tvgbox2/shapes.h
  include/tvgbox2/widgets/input_widget.h
  include/tvgbox2/widgets/label_widget.h
  include/tvgbox2/widgets/checkbox_widget.h
  include/tvgbox2/widgets/slider_widget.h
  include/tvgbox2/widgets/progressbar_widget.h
  include/tvgbox2/widgets/button_widget.h
  include/tvgbox2/widgets/radio_widget.h
  include/tvgbox2/widgets/textarea_widget.h
  include/tvgbox2/widgets/select_widget.h
  include/tvgbox2/widgets/tooltip_widget.h
  include/tvgbox2/widgets/modal_widget.h
  include/tvgbox2/widgets/tabs_widget.h
  include/tvgbox2/widgets/switch_widget.h
  include/tvgbox2/widgets/image_widget.h
  include/tvgbox2/widgets/badge_widget.h
  include/tvgbox2/widgets/spinner_widget.h
  include/tvgbox2/widgets/divider_widget.h
  include/tvgbox2/widgets/toast_widget.h
  include/tvgbox2/widgets/dropdown_widget.h
  include/tvgbox2/widgets/accordion_widget.h
  include/tvgbox2/widgets/calendar_widget.h
  include/tvgbox2/widgets/table_widget.h
  include/tvgbox2/widgets/tree_widget.h
  include/tvgbox2/widgets/colorpicker_widget.h
  include/tvgbox2/widgets/group_button_widget.h
)

# 创建库
add_library(tvgbox2 STATIC ${TVGBOX2_SOURCES} ${TVGBOX2_HEADERS})

# 链接依赖（flex 提供后端无关的渲染抽象，thorvg 由 flex 传递依赖）
target_link_libraries(tvgbox2 PUBLIC ${LEXBOR_LIBRARY} flex)
set_target_properties(tvgbox2 PROPERTIES FOLDER tvgbox2/)

# 安装头文件
install(DIRECTORY include/tvgbox2 DESTINATION include)

# 安装库
install(TARGETS tvgbox2 DESTINATION lib)

add_subdirectory(examples)