cmake_minimum_required(VERSION 3.10)
project(libdxfrw VERSION 0.6.3)

set(libdxfrw_SOURCES
    src/drw_classes.cpp
    src/drw_entities.cpp
    src/drw_header.cpp
    src/drw_objects.cpp
    src/libdwgr.cpp
    src/libdxfrw.cpp
    src/intern/drw_dbg.cpp
    src/intern/drw_textcodec.cpp
    src/intern/dwgbuffer.cpp
    src/intern/dwgreader.cpp
    src/intern/dwgreader15.cpp
    src/intern/dwgreader18.cpp
    src/intern/dwgreader21.cpp
    src/intern/dwgreader24.cpp
    src/intern/dwgreader27.cpp
    src/intern/dwgreader32.cpp
    src/intern/dwgutil.cpp
    src/intern/dxfreader.cpp
    src/intern/dxfwriter.cpp
    src/intern/rscodec.cpp
)

add_library(dxfrw STATIC ${libdxfrw_SOURCES})

target_include_directories(dxfrw 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/intern
)
set_target_properties(dxfrw PROPERTIES FOLDER "vendor/dxf")

add_subdirectory(tests)
add_subdirectory(dwg2text)
add_subdirectory(dwg2dxf)
 

