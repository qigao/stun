# Add test to CTest
enable_testing()

find_package(lunasvg CONFIG REQUIRED)

add_test(NAME WhiteboardDocumentTest COMMAND test_whiteboard_document)

# Test executable for StyleSheetLoader
add_executable(test_stylesheet_loader test_stylesheet_loader.cpp
                                      ../src/ddf/stylesheet_loader.cpp)

target_include_directories(
  test_stylesheet_loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                 ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

find_package(Catch2 CONFIG REQUIRED)

target_link_libraries(test_stylesheet_loader PRIVATE Catch2::Catch2WithMain)

add_test(NAME StyleSheetLoaderTest COMMAND test_stylesheet_loader)

# Test executable for StyleSheet
add_executable(test_stylesheet test_stylesheet.cpp ../src/ddf/stylesheet.cpp
                               ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_stylesheet
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(test_stylesheet PRIVATE Catch2::Catch2WithMain
                                              ${LEXBOR_LIBRARY})

add_test(NAME StyleSheetTest COMMAND test_stylesheet)

# Test executable for StyleLayer
add_executable(
  test_style_layer
  test_style_layer.cpp ../src/ddf/style_layer.cpp ../src/ddf/stylesheet.cpp
  ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_style_layer
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(test_style_layer PRIVATE Catch2::Catch2WithMain
                                               ${LEXBOR_LIBRARY})

add_test(NAME StyleLayerTest COMMAND test_style_layer)

# Test executable for Shape structure
add_executable(test_shape_structure test_shape_structure.cpp)

target_include_directories(
  test_shape_structure PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                               ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_shape_structure PRIVATE Catch2::Catch2WithMain)

add_test(NAME ShapeStructureTest COMMAND test_shape_structure)

# Test executable for ShapeLayer
add_executable(test_shape_layer test_shape_layer.cpp ../src/ddf/shape_layer.cpp
                                ../src/ddf/data_layer.cpp)

target_include_directories(
  test_shape_layer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_shape_layer PRIVATE Catch2::Catch2WithMain)

add_test(NAME ShapeLayerTest COMMAND test_shape_layer)

# Test executable for Data Binding
add_executable(
  test_data_binding test_data_binding.cpp ../src/ddf/shape_layer.cpp
                    ../src/ddf/data_layer.cpp)

target_include_directories(
  test_data_binding PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_data_binding PRIVATE Catch2::Catch2WithMain)

add_test(NAME DataBindingTest COMMAND test_data_binding)

# Test executable for RenderNode
add_executable(test_render_node test_render_node.cpp)

target_include_directories(
  test_render_node PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_render_node PRIVATE Catch2::Catch2WithMain)

add_test(NAME RenderNodeTest COMMAND test_render_node)

# Test executable for RenderNode style computation
add_executable(
  test_render_node_styles
  test_render_node_styles.cpp ../src/ddf/render_node.cpp
  ../src/ddf/stylesheet.cpp ../src/ddf/expression_parser.cpp
  ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_render_node_styles
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(
  test_render_node_styles PRIVATE Catch2::Catch2WithMain nanovg::nanovg
                                  ${LEXBOR_LIBRARY})

add_test(NAME RenderNodeStylesTest COMMAND test_render_node_styles)

# Test executable for RenderNode transform computation
add_executable(
  test_render_node_transforms
  test_render_node_transforms.cpp ../src/ddf/render_node.cpp
  ../src/ddf/stylesheet.cpp ../src/ddf/expression_parser.cpp
  ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_render_node_transforms
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(
  test_render_node_transforms PRIVATE Catch2::Catch2WithMain nanovg::nanovg
                                      ${LEXBOR_LIBRARY})

add_test(NAME RenderNodeTransformsTest COMMAND test_render_node_transforms)

# Test executable for Expression Parser
add_executable(test_expression_parser test_expression_parser.cpp
                                      ../src/ddf/expression_parser.cpp)

target_include_directories(
  test_expression_parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                 ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_expression_parser PRIVATE Catch2::Catch2WithMain)

add_test(NAME ExpressionParserTest COMMAND test_expression_parser)

# Test executable for ComponentLayer
add_executable(
  test_component_layer test_component_layer.cpp ../src/ddf/component_layer.cpp
                       ../src/ddf/shape_layer.cpp ../src/ddf/data_layer.cpp)

target_include_directories(
  test_component_layer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                               ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_component_layer PRIVATE Catch2::Catch2WithMain)

add_test(NAME ComponentLayerTest COMMAND test_component_layer)

# Test executable for ConnectorLayer
add_executable(
  test_connector_layer test_connector_layer.cpp ../src/ddf/connector_layer.cpp
                       ../src/ddf/shape_layer.cpp ../src/ddf/data_layer.cpp)

target_include_directories(
  test_connector_layer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                               ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_connector_layer PRIVATE Catch2::Catch2WithMain
                                                   nanovg::nanovg)

add_test(NAME ConnectorLayerTest COMMAND test_connector_layer)

# Test executable for EventLayer
add_executable(
  test_event_layer
  test_event_layer.cpp ../src/ddf/event_layer.cpp ../src/ddf/data_layer.cpp
  ../src/ddf/shape_layer.cpp ../src/ddf/expression_parser.cpp)

target_include_directories(
  test_event_layer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                           ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_event_layer PRIVATE Catch2::Catch2WithMain)

add_test(NAME EventLayerTest COMMAND test_event_layer)

# Test executable for Layout Algorithms
add_executable(
  test_layout_algorithms
  test_layout_algorithms.cpp ../src/ddf/data_layer.cpp
  ../src/ddf/layout/tree_layout.cpp ../src/ddf/layout/force_directed_layout.cpp
  ../src/ddf/layout/grid_layout.cpp)

target_include_directories(
  test_layout_algorithms PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                 ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_layout_algorithms PRIVATE Catch2::Catch2WithMain)

add_test(NAME LayoutAlgorithmsTest COMMAND test_layout_algorithms)

# Test executable for DDFDocument integration
add_executable(
  test_ddf_document
  test_ddf_document.cpp
  ../src/ddf/ddf_document.cpp
  ../src/ddf/data_layer.cpp
  ../src/ddf/shape_layer.cpp
  ../src/ddf/component_layer.cpp
  ../src/ddf/connector_layer.cpp
  ../src/ddf/event_layer.cpp
  ../src/ddf/style_layer.cpp
  ../src/ddf/stylesheet.cpp
  ../src/ddf/expression_parser.cpp
  ../src/ddf/rendering_pipeline.cpp
  ../src/ddf/render_node.cpp
  ../src/ddf/svg_renderer.cpp
  ../src/ddf/svg_importer.cpp
  ../src/svg/svg_renderer.cpp
  ../src/svg/svg_generator.cpp
  ../src/svg/svg_shape_library.cpp
  ../src/ddf/layout/tree_layout.cpp
  ../src/ddf/layout/force_directed_layout.cpp
  ../src/ddf/layout/grid_layout.cpp
  ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_ddf_document
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(
  test_ddf_document
  PRIVATE Catch2::Catch2WithMain
          nlohmann_json::nlohmann_json
          pugixml::pugixml
          nanovg::nanovg
          fmtlog::fmtlog
          nanogui
          lunasvg::lunasvg
          ${LEXBOR_LIBRARY})

add_test(NAME DDFDocumentTest COMMAND test_ddf_document)

# Test executable for SVG Export
add_executable(
  test_svg_export
  test_svg_export.cpp
  ../src/ddf/ddf_document.cpp
  ../src/ddf/data_layer.cpp
  ../src/ddf/shape_layer.cpp
  ../src/ddf/component_layer.cpp
  ../src/ddf/connector_layer.cpp
  ../src/ddf/event_layer.cpp
  ../src/ddf/style_layer.cpp
  ../src/ddf/stylesheet.cpp
  ../src/ddf/expression_parser.cpp
  ../src/ddf/rendering_pipeline.cpp
  ../src/ddf/render_node.cpp
  ../src/ddf/svg_renderer.cpp
  ../src/ddf/layout/tree_layout.cpp
  ../src/ddf/layout/force_directed_layout.cpp
  ../src/ddf/layout/grid_layout.cpp
  ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_svg_export
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(
  test_svg_export PRIVATE Catch2::Catch2WithMain nlohmann_json::nlohmann_json
                          nanovg::nanovg ${LEXBOR_LIBRARY})

add_test(NAME SVGExportTest COMMAND test_svg_export)

# Test executable for SVG Import
add_executable(
  test_svg_import
  test_svg_import.cpp
  ../src/ddf/ddf_document.cpp
  ../src/ddf/data_layer.cpp
  ../src/ddf/shape_layer.cpp
  ../src/ddf/component_layer.cpp
  ../src/ddf/connector_layer.cpp
  ../src/ddf/event_layer.cpp
  ../src/ddf/style_layer.cpp
  ../src/ddf/stylesheet.cpp
  ../src/ddf/expression_parser.cpp
  ../src/ddf/rendering_pipeline.cpp
  ../src/ddf/render_node.cpp
  ../src/ddf/svg_renderer.cpp
  ../src/ddf/svg_importer.cpp
  ../src/ddf/layout/tree_layout.cpp
  ../src/ddf/layout/force_directed_layout.cpp
  ../src/ddf/layout/grid_layout.cpp
  ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_svg_import
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(
  test_svg_import PRIVATE Catch2::Catch2WithMain nlohmann_json::nlohmann_json
                          pugixml::pugixml nanovg::nanovg ${LEXBOR_LIBRARY})

add_test(NAME SVGImportTest COMMAND test_svg_import)

# Test executable for DataLayer
add_executable(test_data_layer test_data_layer.cpp ../src/ddf/data_layer.cpp)

target_include_directories(
  test_data_layer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
                          ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(test_data_layer PRIVATE Catch2::Catch2WithMain)

add_test(NAME DataLayerTest COMMAND test_data_layer)

# Test executable for Lexbor Integration
add_executable(test_lexbor_integration test_lexbor_integration.cpp)

target_include_directories(
  test_lexbor_integration
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(test_lexbor_integration PRIVATE Catch2::Catch2WithMain
                                                      ${LEXBOR_LIBRARY})

add_test(NAME LexborIntegrationTest COMMAND test_lexbor_integration)

# Test executable for Lexbor CSS Parser
add_executable(test_lexbor_css_parser test_lexbor_css_parser.cpp
                                      ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_lexbor_css_parser
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(test_lexbor_css_parser PRIVATE Catch2::Catch2WithMain
                                                     ${LEXBOR_LIBRARY})

add_test(NAME LexborCSSParserTest COMMAND test_lexbor_css_parser)

# Test executable for Color Functions
add_executable(
  test_color_functions
  test_color_functions.cpp ../src/ddf/render_node.cpp ../src/ddf/stylesheet.cpp
  ../src/ddf/expression_parser.cpp ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_color_functions
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(
  test_color_functions PRIVATE Catch2::Catch2WithMain nanovg::nanovg
                               ${LEXBOR_LIBRARY})

add_test(NAME ColorFunctionsTest COMMAND test_color_functions)

# Test executable for Calc Function
add_executable(test_calc_function test_calc_function.cpp
                                  ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_calc_function
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(test_calc_function PRIVATE Catch2::Catch2WithMain
                                                 ${LEXBOR_LIBRARY})

add_test(NAME CalcFunctionTest COMMAND test_calc_function)

# Test executable for CSS Variables
add_executable(test_css_variables test_css_variables.cpp
                                  ../src/lexbor/lexbor_css_parser.cpp)

target_include_directories(
  test_css_variables
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include
          ${CMAKE_CURRENT_SOURCE_DIR}/../../include ${LEXBOR_INCLUDE_DIR})

target_link_libraries(test_css_variables PRIVATE Catch2::Catch2WithMain
                                                 ${LEXBOR_LIBRARY})

add_test(NAME CSSVariablesTest COMMAND test_css_variables)
