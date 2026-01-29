add_executable(tui_demo demo.cpp)
target_link_libraries(tui_demo PRIVATE tui)
set_target_properties(tui_demo PROPERTIES FOLDER tui/exmaples)
add_executable(tui_widgets widgets_demo.cpp)
target_link_libraries(tui_widgets PRIVATE tui)
set_target_properties(tui_widgets PROPERTIES FOLDER tui/exmaples)

# flexUI TUI demo - renders flexUI widgets to terminal
add_executable(flexui_tui_demo flexui_demo.cpp)
target_link_libraries(flexui_tui_demo PRIVATE tui flexUI)
set_target_properties(flexui_tui_demo PROPERTIES FOLDER tui/exmaples)
# ClawdBot TUI demo
add_executable(clawdbot_tui clawdbot_demo.cpp)
target_link_libraries(clawdbot_tui PRIVATE tui flexUI)
if(WIN32)
    target_link_libraries(clawdbot_tui PRIVATE winmm)
endif()
set_target_properties(clawdbot_tui PROPERTIES FOLDER tui/exmaples)
