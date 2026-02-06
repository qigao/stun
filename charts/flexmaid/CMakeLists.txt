# FlexMaid - Unified Architecture (Clean Implementation)
cmake_minimum_required(VERSION 3.20)

# FlexMaid - Unified Architecture Library
add_library(
  flexmaid
  # 核心统一结构
  src/ir/unified_diagram.cpp
  # 统一解析器
  src/parser/unified_parser.cpp
  # Interpreter Pattern: 表达式和解释器
  src/parser/expression.cpp
  src/parser/diagram_interpreter.cpp
  # Layout engine
  src/layout/dagre_layout.cpp
  src/layout/edge_router.cpp
  # Strategy Pattern: Chart Renderers
  src/render/block_renderer.cpp
  src/render/c4_renderer.cpp
  src/render/chart_renderer.cpp
  src/render/class_renderer.cpp
  src/render/er_renderer.cpp
  src/render/flowchart_renderer.cpp
  src/render/icon_manager.cpp
  src/render/render_types.cpp
  src/render/template_manager.cpp
  # 新的简洁API
  src/flexmaid.cpp
  # Flex 运行时渲染组件
  src/mermaid_component.cpp)

target_link_libraries(flexmaid PUBLIC TurboNet::Mustache flex)

target_include_directories(
  flexmaid
  PUBLIC   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/flexmaid>
  PRIVATE src)

target_compile_features(flexmaid PUBLIC cxx_std_17)

# 设置编译选项
target_compile_options(
  flexmaid PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
                   $<$<CXX_COMPILER_ID:MSVC>:/W4>)

# 统一架构测试
add_executable(flexmaid_tests tests/unified_design_test.cpp)

target_link_libraries(flexmaid_tests PRIVATE flexmaid)

# 添加测试
add_test(NAME FlexMaidTests COMMAND flexmaid_tests)

# 设置工作目录
set_tests_properties(FlexMaidTests PROPERTIES WORKING_DIRECTORY
                                              ${CMAKE_CURRENT_BINARY_DIR})

# BDD Tests
add_executable(flexmaid_bdd_tests tests/parser_bdd_test.cpp)
target_compile_definitions(flexmaid_bdd_tests PRIVATE 
    TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
target_link_libraries(flexmaid_bdd_tests PRIVATE flexmaid bdd)
add_test(NAME FlexMaidBDDTests COMMAND flexmaid_bdd_tests)
set_target_properties(flexmaid_bdd_tests PROPERTIES FOLDER "Modules/FlexMaid/Tests")

# 示例程序
add_executable(flexmaid_example examples/simple_example.cpp)

target_link_libraries(flexmaid_example PRIVATE flexmaid TurboNet::Parser)

find_package(SDL2 CONFIG QUIET)
# Mermaid 渲染示例 (需要 SDL2 + ThorVG)
add_executable(mermaid_render_example examples/mermaid_render_example.cpp)
target_link_libraries(mermaid_render_example PRIVATE flexmaid  flex SDL2::SDL2 SDL2::SDL2main )
set_target_properties(mermaid_render_example
                      PROPERTIES FOLDER "Modules/FlexMaid/Examples")

# Set IDE folders
set_target_properties(flexmaid PROPERTIES FOLDER "Modules/FlexMaid")
set_target_properties(flexmaid_tests PROPERTIES FOLDER "Modules/FlexMaid/Tests")
set_target_properties(flexmaid_example PROPERTIES FOLDER
                                                  "Modules/FlexMaid/Examples")
