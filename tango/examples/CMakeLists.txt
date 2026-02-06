cmake_minimum_required(VERSION 3.16)

# Widgets demo
add_executable(tui_widgets_demo widgets_demo.c)
target_link_libraries(tui_widgets_demo PRIVATE tango)

# Layout demo (Clay-based flexbox)
add_executable(tui_layout_demo layout_demo.c)
target_link_libraries(tui_layout_demo PRIVATE tango)

# Test runner demo
add_executable(tui_test_runner test_runner_demo.c)
target_link_libraries(tui_test_runner PRIVATE tango)

# Advanced features demo
add_executable(tui_advanced_demo advanced_demo.c)
target_link_libraries(tui_advanced_demo PRIVATE tango)

# Chat demo (Clawdbot-style)
add_executable(tui_chat_demo chat_demo.c)
target_link_libraries(tui_chat_demo PRIVATE tango)

set_target_properties(
  tui_widgets_demo 
  tui_layout_demo 
  tui_test_runner 
  tui_advanced_demo
  tui_chat_demo
  PROPERTIES FOLDER "tango/examples")