cmake_minimum_required(VERSION 3.20)
project(gCanvasInstallConsumer LANGUAGES CXX)

find_package(gCanvas 0.4 CONFIG REQUIRED)

add_executable(gcanvas_install_consumer main.cpp)
target_link_libraries(gcanvas_install_consumer PRIVATE gCanvas::gCanvas)
target_compile_features(gcanvas_install_consumer PRIVATE cxx_std_17)
