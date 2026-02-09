cmake_minimum_required(VERSION 3.10)
project(linmath)

add_library(linmath INTERFACE)
target_include_directories(linmath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(linmath_test linmath_test.c)
target_link_libraries(linmath_test PRIVATE linmath TurboNet::TinyTest)
target_compile_definitions(linmath_test PRIVATE _USE_MATH_DEFINES) # For M_PI_4
                                                                   # on Windows

add_test(NAME linmath_test COMMAND linmath_test)
set_target_properties( linmath_test PROPERTIES FOLDER vendor/linmath)