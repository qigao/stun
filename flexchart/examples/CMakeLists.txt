add_executable(flexchart_demo flexchart_demo.cpp)

target_include_directories(flexchart_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/nanovg
)

target_link_libraries(flexchart_demo PRIVATE
    flexchart
    SDL3::SDL3
    glad::glad
)

set_target_properties(flexchart_demo PROPERTIES FOLDER flexchart/examples)

# Add JavaScript bindings demo executable
add_executable(bindingsflexchart_demo bindingsflexchart_demo.cpp)

target_include_directories(bindingsflexchart_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/nanovg
)

target_link_libraries(bindingsflexchart_demo PRIVATE
    flexchart
    cssbox
    qjs
    SDL3::SDL3
    glad::glad
)

set_target_properties(bindingsflexchart_demo PROPERTIES FOLDER flexchart/examples)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/js DESTINATION ${CMAKE_BINARY_DIR}/bin/flexchart_examples)
