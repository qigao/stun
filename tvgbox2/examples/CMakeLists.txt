# Visual Demo - Real window with ThorVG software rendering + SDL2
find_package(SDL2 CONFIG REQUIRED)
add_executable(visual_demo visual_demo.cpp)
target_link_libraries(visual_demo tvgbox2 thorvg
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
set_target_properties(visual_demo PROPERTIES FOLDER tvgbox2/)
