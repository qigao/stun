# FlexUI Examples

# ============================================================================
# Comprehensive Widget Showcase
# ============================================================================

add_executable(all_widgets_showcase all_widgets_showcase.cpp)
target_link_libraries(all_widgets_showcase PRIVATE flexui_core)
set_target_properties(all_widgets_showcase PROPERTIES FOLDER flexui/examples)

# ============================================================================
# Basic Examples
# ============================================================================

add_executable(basic_widgets_demo basic_widgets_demo.cpp)
target_link_libraries(basic_widgets_demo PRIVATE flexui_core)
set_target_properties(basic_widgets_demo PROPERTIES FOLDER flexui/examples)

add_executable(all_widgets_xml_demo all_widgets_xml_demo.cpp)
target_link_libraries(all_widgets_xml_demo PRIVATE flexui_core)
set_target_properties(all_widgets_xml_demo PROPERTIES FOLDER flexui/examples)

# ============================================================================
# Form Examples
# ============================================================================

add_executable(login_form_demo login_form_demo.cpp)
target_link_libraries(login_form_demo PRIVATE flexui_core)
set_target_properties(login_form_demo PROPERTIES FOLDER flexui/examples)

add_executable(login_form_xml_demo login_form_xml_demo.cpp)
target_link_libraries(login_form_xml_demo PRIVATE flexui_core)
set_target_properties(login_form_xml_demo PROPERTIES FOLDER flexui/examples)

add_executable(survey_form_demo survey_form_demo.cpp)
target_link_libraries(survey_form_demo PRIVATE flexui_core)
set_target_properties(survey_form_demo PROPERTIES FOLDER flexui/examples)

add_executable(upload_demo upload_demo.cpp)
target_link_libraries(upload_demo PRIVATE flexui_core)
set_target_properties(upload_demo PROPERTIES FOLDER flexui/examples)

add_executable(settings_demo settings_demo.cpp)
target_link_libraries(settings_demo PRIVATE flexui_core)
set_target_properties(settings_demo PROPERTIES FOLDER flexui/examples)

# ============================================================================
# Advanced Features
# ============================================================================

add_executable(javascript_demo javascript_demo.cpp)
target_link_libraries(javascript_demo PRIVATE flexui_core)
set_target_properties(javascript_demo PROPERTIES FOLDER flexui/examples)

add_executable(svg_demo svg_demo.cpp)
target_link_libraries(svg_demo PRIVATE flexui_core)
set_target_properties(svg_demo PROPERTIES FOLDER flexui/examples)
