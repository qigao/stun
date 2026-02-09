cmake_minimum_required(VERSION 3.20)
project(cola LANGUAGES CXX C)

# 添加 cmake 文件夹到模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ColaCommon)

# 按依赖顺序添加子目录
add_subdirectory(libvpsc)
add_subdirectory(libcola)
add_subdirectory(libavoid)
add_subdirectory(libtopology)
add_subdirectory(libproject)
add_subdirectory(libdialect)
