add_executable(flexchart_demo flexchart_demo.cpp)

target_include_directories(flexchart_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/nanovg
)

target_link_libraries(flexchart_demo PRIVATE
    flexchart
    glfw
    glad::glad
)

set_target_properties(flexchart_demo PROPERTIES FOLDER flexchart/examples)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/js DESTINATION ${CMAKE_BINARY_DIR}/bin/flexchart_examples)
