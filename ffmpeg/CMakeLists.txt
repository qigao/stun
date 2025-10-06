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
        # FFmpeg libraries
        avcodec
        avformat
        swscale
        avutil
        swresample
) 

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER ${folder_name})
 