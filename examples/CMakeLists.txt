file(GLOB SOURCES example*.cpp)
find_package(lunasvg)
find_package(rlottie)
foreach(example_source ${SOURCES})
    get_filename_component(source_name ${example_source} NAME_WE)
    set(EXAMPLE_NAME ${source_name})
  
        add_executable(${EXAMPLE_NAME} ${example_source})
        target_link_libraries(
            ${EXAMPLE_NAME} PRIVATE fmtlog::fmtlog nanogui 
            nanogui_apple_theme nanogui_m3_theme nanogui_fluent_theme  lunasvg::lunasvg rlottie::rlottie
        )
        
        
        target_include_directories(${EXAMPLE_NAME} PRIVATE 
            ../include ../theme/apple ../theme/fluent ../theme/me ../theme/material)

        set_target_properties(
            ${EXAMPLE_NAME} PROPERTIES FOLDER nanogui/example
        )
 
endforeach()
add_subdirectory(sdl)