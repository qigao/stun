find_package(lunasvg REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(thorvg REQUIRED IMPORTED_TARGET thorvg)

set(WHITEBOARD_SOURCES
    # Model layer
    src/model/whiteboard_document.cpp
    
    # Canvas MVC
    src/canvas/canvas_view.cpp
    src/canvas/canvas_controller.cpp
    
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
    
    # Existing sources
    src/auto_save_manager.cpp
    src/export_manager.cpp
    src/main.cpp
    src/menu_toolbar_module.cpp
    src/modern_whiteboard_app.cpp
    src/properties_panel_module.cpp
    src/search_bar.cpp
    src/stb_image_write.cpp
    src/template_gallery.cpp
    src/template_library.cpp
    src/text_panel_module.cpp
    src/toolbar_panel_module.cpp
    src/zoom_panel_module.cpp
)

add_executable(modern_whiteboard ${WHITEBOARD_SOURCES})

target_include_directories(
    modern_whiteboard
    PRIVATE
        ../include
        include
        ${THORVG_INCLUDE_DIRS}
)

target_link_libraries(
    modern_whiteboard
    PRIVATE
        fmtlog::fmtlog
        nanogui
        nanovg::nanovg
        lunasvg::lunasvg
        NanoGUI::M3Theme
        NanoGUI::AppleTheme
        NanoGUI::FluentIOSTheme
        NanoGUI::FluentWebTheme
        nlohmann_json::nlohmann_json
        nfd
        PkgConfig::thorvg
)

set_target_properties(modern_whiteboard PROPERTIES FOLDER whiteboard/)

# Add tests subdirectory
add_subdirectory(test)
