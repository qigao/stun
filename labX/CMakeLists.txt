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