cmake_minimum_required(VERSION 3.10)
project(FlexPlayer)

# Explicitly list sources from engine to avoid globbing issues
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/engine")
set(ENGINE_SOURCES
    "${ENGINE_DIR}/audio_decoder.cpp"
    "${ENGINE_DIR}/audio_player.cpp"
    "${ENGINE_DIR}/audio_resampler.cpp"
    "${ENGINE_DIR}/audio_track_manager.cpp"
    "${ENGINE_DIR}/demuxer.cpp"
    "${ENGINE_DIR}/ffmpeg.cpp"
    "${ENGINE_DIR}/filter_presets.cpp"
    "${ENGINE_DIR}/format_converter.cpp"
    "${ENGINE_DIR}/frame_capture.cpp"
    "${ENGINE_DIR}/hw_decoder.cpp"
    "${ENGINE_DIR}/player.cpp"
    "${ENGINE_DIR}/stream_handler.cpp"
    "${ENGINE_DIR}/subtitle_renderer.cpp"
    "${ENGINE_DIR}/timer.cpp"
    "${ENGINE_DIR}/video_decoder.cpp"
    "${ENGINE_DIR}/video_filter.cpp"
)

# Current sources
file(GLOB CURRENT_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(FlexPlayer ${CURRENT_SOURCES} ${ENGINE_SOURCES})

# Include paths
target_include_directories(FlexPlayer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${ENGINE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../flex/include"
)

# Find Dependencies
find_package(PkgConfig REQUIRED)
find_package(SDL2 CONFIG QUIET)
find_package(OpenGL REQUIRED)
pkg_check_modules(libass REQUIRED IMPORTED_TARGET libass)
# Find FFmpeg (assuming it's available via same mechanism as MediaPlayer)
set(FFMPEG_DIR ${EXTERNAL_ROOT}/ffmpeg)
target_include_directories(FlexPlayer PRIVATE ${FFMPEG_DIR}/include)
target_link_directories(FlexPlayer PRIVATE ${FFMPEG_DIR}/lib)

# Link Libraries
target_link_libraries(FlexPlayer PRIVATE
    flex
   SDL2::SDL2 SDL2::SDL2main OpenGL::GL
    avcodec
    avformat
    avfilter
    swscale
    avutil
    swresample
    PkgConfig::libass
)

# Define macros
target_compile_definitions(FlexPlayer PRIVATE
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
    __STDC_LIMIT_MACROS
)

# Copy player_gui.flex to the output directory
add_custom_command(TARGET FlexPlayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/player_gui.flex"
        "$<TARGET_FILE_DIR:FlexPlayer>/player_gui.flex"
    COMMENT "Copying player_gui.flex to output directory"
)

set_target_properties(PROPERTIES FOLDER "MediaPlayer"
                                               POSITION_INDEPENDENT_CODE ON)
