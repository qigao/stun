# Widgets Examples

find_package(SDL2 CONFIG REQUIRED)

file(GLOB WIDGET_EXAMPLES *.cpp)

foreach(EXAMPLE_FILE ${WIDGET_EXAMPLES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_FILE})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE widgets flex thorvg SDL2::SDL2 SDL2::SDL2main)
    set_target_properties(${EXAMPLE_NAME} PROPERTIES FOLDER "widgets/examples")
endforeach()
