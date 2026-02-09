# Visual Demo - Real window with ThorVG OpenGL rendering + GLFW
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(visual_demo visual_demo.cpp)
target_link_libraries(visual_demo flexUI thorvg glfw glad::glad OpenGL::GL)
target_include_directories(visual_demo PRIVATE ${CMAKE_SOURCE_DIR}/flex/include)
set_target_properties(visual_demo PROPERTIES FOLDER flexUI/)

# Markdown Demo - Showcases Markdown renderer
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(markdown_demo markdown_demo.cpp)
target_link_libraries(markdown_demo flexUI thorvg glfw glad::glad OpenGL::GL)
target_include_directories(markdown_demo PRIVATE ${CMAKE_SOURCE_DIR}/flex/include)
set_target_properties(markdown_demo PROPERTIES FOLDER flexUI/)

# TUI Demo - Terminal UI using tango backend
add_executable(flexui_tui_demo tui_demo.cpp)
target_link_libraries(flexui_tui_demo flexUI flex tango)
target_include_directories(flexui_tui_demo PRIVATE ${CMAKE_SOURCE_DIR}/flex/include)
set_target_properties(flexui_tui_demo PROPERTIES FOLDER flexUI/)
 