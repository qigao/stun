set(TARGET_NAME AVPlayer)

# Collect source files
file(GLOB AVPLAYER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

set(FFMPEG_DIR ${EXTERNAL_ROOT}/ffmpeg)

set(folder_name ${TARGET_NAME})   
set(STB_IMG ${EXTERNAL_ROOT}/stb)

add_executable(${TARGET_NAME} ${AVPLAYER_SOURCES}) 

target_include_directories(
    ${TARGET_NAME} 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${STB_IMG}
        ${FFMPEG_DIR}/include
)
find_package(PkgConfig REQUIRED)
pkg_check_modules(libass REQUIRED IMPORTED_TARGET libass)

# Define required macros for FFmpeg on Windows
target_compile_definitions(
    ${TARGET_NAME}
    PRIVATE
        __STDC_CONSTANT_MACROS
        __STDC_FORMAT_MACROS
        __STDC_LIMIT_MACROS
)

target_link_directories(
    ${TARGET_NAME} 
    PRIVATE 
        ${FFMPEG_DIR}/lib
)

# Link dependencies
target_link_libraries(
    ${TARGET_NAME}
    PRIVATE
        nanogui    
        nfd
        # FFmpeg libraries
        avcodec
        avformat
        avfilter
        swscale
        avutil
        swresample
        # Subtitle rendering
        PkgConfig::libass
) 

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER ${folder_name})
 