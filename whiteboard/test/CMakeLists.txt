# Test executable for WhiteboardDocument
add_executable(test_whiteboard_document 
    test_whiteboard_document.cpp
    ../src/model/whiteboard_document.cpp
)

target_include_directories(
    test_whiteboard_document
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

target_link_libraries(
    test_whiteboard_document
    PRIVATE
        nanogui
        nanovg::nanovg
)

# Add test to CTest
enable_testing()
add_test(NAME WhiteboardDocumentTest COMMAND test_whiteboard_document)
