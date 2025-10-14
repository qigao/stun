find_package(PkgConfig REQUIRED)
pkg_check_modules(thorvg REQUIRED IMPORTED_TARGET thorvg)

add_executable(labx_demo
    labx_example.cpp
)

target_link_libraries(labx_demo
    PRIVATE
        nanogui
        nanovg::nanovg
        PkgConfig::thorvg
)

target_include_directories(labx_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${THORVG_INCLUDE_DIRS} # Add ThorVG include directories
)

set_target_properties(labx_demo PROPERTIES
    FOLDER nanogui/labX
)
add_subdirectory(racks)