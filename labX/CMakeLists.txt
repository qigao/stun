find_package(rlottie REQUIRED)

add_executable(labx_demo
    labx_example.cpp
)

target_link_libraries(labx_demo
    PRIVATE
        nanogui
        rlottie::rlottie
)

target_include_directories(labx_demo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set_target_properties(labx_demo PROPERTIES
    FOLDER nanogui/labX
)
add_subdirectory(racks)

find_package(lunasvg REQUIRED)
find_package(SDL3 REQUIRED)

add_executable( svg_example
    svg_example.cpp
)

target_link_libraries( svg_example
    PRIVATE
        SDL3::SDL3
        lunasvg::lunasvg
)

target_include_directories( svg_example
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set_target_properties( svg_example PROPERTIES
    FOLDER nanogui/labX
)

add_executable( svg_nanogui_example
    svg_nanogui_example.cpp
)

target_link_libraries( svg_nanogui_example
    PRIVATE
        nanogui
        lunasvg::lunasvg
)

target_include_directories( svg_nanogui_example
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

set_target_properties( svg_nanogui_example PROPERTIES
    FOLDER nanogui/labX
)