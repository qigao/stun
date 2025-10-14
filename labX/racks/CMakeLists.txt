find_package(PkgConfig REQUIRED)
pkg_check_modules(thorvg REQUIRED IMPORTED_TARGET thorvg)

# Modular VCV rack demo with window adsorption
add_executable(vcv_rack_modular
    main.cpp
    vcv_module_panel.cpp
    speedometer_module.cpp
    speedometer_rack.cpp
    wasp_filter_module.cpp
    oscilloscope_module.cpp
)

target_link_libraries(vcv_rack_modular
    PRIVATE
        nanogui
        PkgConfig::thorvg
)

target_include_directories(vcv_rack_modular
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${THORVG_INCLUDE_DIRS}
)

set_target_properties(vcv_rack_modular PROPERTIES
    FOLDER nanogui/labX
)