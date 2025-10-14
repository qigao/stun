find_package(lunasvg REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(thorvg REQUIRED IMPORTED_TARGET thorvg)

set(WHITEBOARD_SOURCES
    src/export_manager.cpp
    src/layers_panel.cpp
    src/main.cpp
    src/modern_canvas.cpp
    src/modern_whiteboard_app.cpp
    src/properties_panel.cpp
    src/search_bar.cpp
    src/stb_image_write.cpp
    src/template_gallery.cpp
    src/template_library.cpp
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
