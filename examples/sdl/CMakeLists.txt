file(GLOB SOURCES example*.cpp)
   find_package(SDL3 CONFIG REQUIRED)
   find_package(SDL3_image CONFIG REQUIRED)
   find_package(SDL3_ttf CONFIG REQUIRED)
 
foreach(example_source ${SOURCES})
    get_filename_component(source_name ${example_source} NAME_WE)
    set(EXAMPLE_NAME ${source_name})
  
        add_executable(${EXAMPLE_NAME} ${example_source} ${CMAKE_SOURCE_DIR}/vendor/nanovg/src/nanovg.c )
        target_link_libraries(
            ${EXAMPLE_NAME} PRIVATE fmtlog::fmtlog  SDL3::SDL3 nanovg glad::glad
            SDL3_ttf::SDL3_ttf
            $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-shared>,SDL3_image::SDL3_image-shared,SDL3_image::SDL3_image-static>
        )
        
       target_include_directories(${EXAMPLE_NAME}  PRIVATE   ${CMAKE_SOURCE_DIR}/vendor/nanovg/src )
        set_target_properties(
            ${EXAMPLE_NAME} PROPERTIES FOLDER sdl/example
        )
 
endforeach()
