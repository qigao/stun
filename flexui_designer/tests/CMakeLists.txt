# ============================================================================
# flexUI Designer Tests
# ============================================================================

find_package(Catch2 REQUIRED)
enable_testing()

# ----------------------------------------------------------------------------
# History Panel Property Tests
# ----------------------------------------------------------------------------
add_executable(test_history_panel test_history_panel.cpp)
target_link_libraries(test_history_panel PRIVATE 
    flexui_designer
    Catch2::Catch2WithMain
)
target_include_directories(test_history_panel PRIVATE ../include)
set_target_properties(test_history_panel PROPERTIES FOLDER "flexui_designer/tests")
add_test(NAME test_history_panel COMMAND test_history_panel)

# ----------------------------------------------------------------------------
# Search Panel Property Tests
# ----------------------------------------------------------------------------
add_executable(test_search_panel test_search_panel.cpp)
target_link_libraries(test_search_panel PRIVATE 
    flexui_designer
    Catch2::Catch2WithMain
)
target_include_directories(test_search_panel PRIVATE ../include)
set_target_properties(test_search_panel PROPERTIES FOLDER "flexui_designer/tests")
add_test(NAME test_search_panel COMMAND test_search_panel)

# ----------------------------------------------------------------------------
# Batch Edit Property Tests
# ----------------------------------------------------------------------------
add_executable(test_batch_edit test_batch_edit.cpp)
target_link_libraries(test_batch_edit PRIVATE 
    flexui_designer
    Catch2::Catch2WithMain
)
target_include_directories(test_batch_edit PRIVATE ../include)
set_target_properties(test_batch_edit PROPERTIES FOLDER "flexui_designer/tests")
add_test(NAME test_batch_edit COMMAND test_batch_edit)

# ----------------------------------------------------------------------------
# Script Editor Property Tests
# ----------------------------------------------------------------------------
add_executable(test_script_editor test_script_editor.cpp)
target_link_libraries(test_script_editor PRIVATE 
    flexui_designer
    Catch2::Catch2WithMain
)
target_include_directories(test_script_editor PRIVATE ../include)
set_target_properties(test_script_editor PROPERTIES FOLDER "flexui_designer/tests")
add_test(NAME test_script_editor COMMAND test_script_editor)
