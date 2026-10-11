cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(StunUICore LANGUAGES C)
    include(CTest)
    find_package(Salts CONFIG REQUIRED)
endif()

add_library(stun_ui_core STATIC src/ui_core.c src/markdown_c11.c)
add_library(Stun::UICore ALIAS stun_ui_core)

target_compile_features(stun_ui_core PUBLIC c_std_11)
target_include_directories(stun_ui_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(stun_ui_core PUBLIC Salts::CMeta)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
