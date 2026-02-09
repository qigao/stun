project(pdfgen)

add_library(pdfgen STATIC pdfgen.c pdfgen.h)

target_include_directories(pdfgen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(pdfgen PROPERTIES FOLDER "vendor/pdfgen")
enable_testing()
add_subdirectory(tests)
