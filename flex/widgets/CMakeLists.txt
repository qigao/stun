# Flex Widgets Library
# Standard UI controls with Fluent Design theme

set(WIDGETS_SOURCES
    src/widgets.cpp
)

set(WIDGETS_HEADERS
    include/widgets/widgets.h
    include/widgets/widget_base.h
    include/widgets/widget_theme.h
)
add_library(widgets STATIC ${WIDGETS_HEADERS} ${WIDGETS_SOURCES})
target_include_directories(
    widgets
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/widgets>
           $<INSTALL_INTERFACE:include>
)
target_link_libraries(widgets PUBLIC flex)
set_target_properties(widgets PROPERTIES FOLDER "widgets" POSITION_INDEPENDENT_CODE
                                                    ON)

# Examples
add_subdirectory(examples)

# Install
install(TARGETS widgets
    EXPORT widgets-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin)

install(DIRECTORY include/widgets DESTINATION include)
