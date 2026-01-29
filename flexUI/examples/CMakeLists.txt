# Visual Demo - Real window with ThorVG software rendering + SDL2
find_package(SDL2 CONFIG REQUIRED)
add_executable(visual_demo visual_demo.cpp)
target_link_libraries(visual_demo flexUI thorvg
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
set_target_properties(visual_demo PROPERTIES FOLDER flexUI/)

# Settings Demo - Showcases new widgets
add_executable(settings_demo settings_demo.cpp)
target_link_libraries(settings_demo flexUI thorvg
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
set_target_properties(settings_demo PROPERTIES FOLDER flexUI/)

# Markdown Demo - Showcases Markdown renderer
add_executable(markdown_demo markdown_demo.cpp)
target_link_libraries(markdown_demo flexUI thorvg
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
set_target_properties(markdown_demo PROPERTIES FOLDER flexUI/)