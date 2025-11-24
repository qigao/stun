find_package(lunasvg REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

file(GLOB_RECURSE  WHITE_BOARD_HEADERS 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)
set(WHITEBOARD_SOURCES
    # Model layer
    src/model/whiteboard_document.cpp
    src/serialization.cpp
    
    # DDF (Diagram Definition Format) layer
    src/ddf/ddf_document.cpp
    src/ddf/data_layer.cpp
    src/ddf/shape_layer.cpp
    src/ddf/component_layer.cpp
    src/ddf/connector_layer.cpp
    src/ddf/event_layer.cpp
    src/ddf/style_layer.cpp
    src/ddf/stylesheet_loader.cpp
    src/ddf/stylesheet.cpp
    src/ddf/render_node.cpp
    src/ddf/rendering_pipeline.cpp
    src/ddf/expression_parser.cpp
    
    # DDF Layout algorithms
    src/ddf/layout/tree_layout.cpp
    src/ddf/layout/force_directed_layout.cpp
    src/ddf/layout/grid_layout.cpp
    
    # DDF SVG export/import
    src/ddf/svg_renderer.cpp
    src/ddf/svg_importer.cpp
    
    # DDF Whiteboard integration
    src/ddf/whiteboard_ddf_integration.cpp
    
    # SVG rendering
    src/svg/svg_renderer.cpp
    src/svg/svg_generator.cpp
    src/svg/svg_shape_library.cpp
    src/svg/svg_parameter_editor.cpp
    
    # Canvas MVC
    src/canvas/canvas_view.cpp
    src/canvas/canvas_controller.cpp
    src/canvas/inline_text_editor.cpp
    
    # Toolbar MVC
    src/toolbar/toolbar_view.cpp
    src/toolbar/toolbar_controller.cpp
    
    # Panels MVC
    src/panels/layers_view.cpp
    src/panels/layers_controller.cpp
    src/panels/properties_view.cpp
    src/panels/properties_controller.cpp
    src/panels/text_view.cpp
    src/panels/text_controller.cpp
    src/panels/shape_library_view.cpp
    src/panels/shape_library_controller.cpp
    
    # UI components
    src/ui/toast_notification.cpp
    src/ui/help_panel_module.cpp
    src/ui/export_dialog.cpp
    src/ui/context_menu_module.cpp
    
    # Existing sources
    src/auto_save_manager.cpp
    src/export_manager.cpp
    src/main.cpp
    src/menu_toolbar_module.cpp
    src/modern_whiteboard_app.cpp
    src/properties_panel_module.cpp
    src/search_bar.cpp
    src/shape_panel_module.cpp
    src/stb_image_write.cpp
    src/template_gallery.cpp
    src/template_library.cpp 
    src/toolbar_panel_module.cpp
    src/zoom_panel_module.cpp
)

add_executable(modern_whiteboard ${WHITEBOARD_SOURCES} ${WHITE_BOARD_HEADERS})

target_include_directories(
    modern_whiteboard
    PRIVATE 
       ${CMAKE_CURRENT_SOURCE_DIR}/include 
)

find_package(unofficial-libharu CONFIG REQUIRED)

target_link_libraries(
    modern_whiteboard
    PRIVATE
        fmtlog::fmtlog
        nanogui
        nanovg::nanovg
        lunasvg::lunasvg
        nlohmann_json::nlohmann_json
        pugixml::pugixml
        nfd
        unofficial::libharu::hpdf
)

set_target_properties(modern_whiteboard PROPERTIES FOLDER whiteboard/)

 