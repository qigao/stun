find_package(lunasvg REQUIRED)
# Modular VCV rack demo with window adsorption
add_executable(vcv_rack_modular
    main.cpp
    vcv_module_panel.cpp
    speedometer_rack.cpp 
    wasp_filter_module.cpp
    oscilloscope_module.cpp
)

target_link_libraries(vcv_rack_modular
    PRIVATE
        nanogui
        lunasvg::lunasvg
)

target_include_directories(vcv_rack_modular
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(vcv_rack_modular PROPERTIES
    FOLDER nanogui/labX
)