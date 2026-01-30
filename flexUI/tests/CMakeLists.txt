find_package(Catch2 REQUIRED)
enable_testing()

add_executable(test_markdown_widget test_markdown_widget.cpp)
target_link_libraries(test_markdown_widget PRIVATE flexUI Catch2::Catch2WithMain  )
target_include_directories(test_markdown_widget PRIVATE ../include ${CMAKE_SOURCE_DIR}/vendor/md4c/src)
set_target_properties(test_markdown_widget PROPERTIES FOLDER "flexUI/tests")

add_test(NAME test_markdown_widget COMMAND test_markdown_widget)
