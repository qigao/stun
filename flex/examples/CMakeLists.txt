# Flex Examples

# Copy .flex files to build directory
configure_file(hello.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hello.flex COPYONLY)
configure_file(tesla_speedometer.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tesla_speedometer.flex COPYONLY)
configure_file(analog_clock.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/analog_clock.flex COPYONLY)
configure_file(loading_animation.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/loading_animation.flex COPYONLY)
configure_file(music_player.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/music_player.flex COPYONLY)
configure_file(breakout_game.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/breakout_game.flex COPYONLY)
configure_file(space_shooter.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/space_shooter.flex COPYONLY)
configure_file(ui_components.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/ui_components.flex COPYONLY)
configure_file(data_binding.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/data_binding.flex COPYONLY)
configure_file(shopping_cart.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shopping_cart.flex COPYONLY)
configure_file(rocket_launch.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocket_launch.flex COPYONLY)
configure_file(path_showcase.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/path_showcase.flex COPYONLY)
configure_file(flex_layout.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/flex_layout.flex COPYONLY)
configure_file(component_demo.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/component_demo.flex COPYONLY)
configure_file(widget_gallery.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/widget_gallery.flex COPYONLY)
configure_file(nested_components.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/nested_components.flex COPYONLY)
configure_file(component_dsl_demo.flex ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/component_dsl_demo.flex COPYONLY)

# Parser Debug Tool (no GUI, just prints parse results)
add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE flex)
set_target_properties(test_parser PROPERTIES FOLDER "flex/examples")

# SDL2 Examples (require SDL2)
find_package(SDL2 CONFIG QUIET)

if(SDL2_FOUND)

  # Simple ThorVG Animation Demo (simplified with error checking)
  add_executable(flex_thorvg_simple flex_thorvg_simple.cpp)
  target_link_libraries(flex_thorvg_simple PRIVATE flex SDL2::SDL2
                                                   SDL2::SDL2main)
  set_target_properties(flex_thorvg_simple PROPERTIES FOLDER "flex/examples")

  # Editor Demo (Meta-Editor prototype)
  add_executable(flex_editor_demo editor_demo.cpp)
  target_link_libraries(flex_editor_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(flex_editor_demo PROPERTIES FOLDER "flex/examples")

  # DSL Render Demo (loads .flex file and renders with ThorVG)
  add_executable(flex_dsl_render flex_dsl_render.cpp)
  target_link_libraries(flex_dsl_render PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(flex_dsl_render PROPERTIES FOLDER "flex/examples")

  # Tesla Speedometer Demo (interactive speedometer with animations)
  add_executable(tesla_speedometer_demo tesla_speedometer_demo.cpp)
  target_link_libraries(tesla_speedometer_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(tesla_speedometer_demo PROPERTIES FOLDER "flex/examples")

  # Analog Clock Demo (real-time clock with rotating hands)
  add_executable(analog_clock_demo analog_clock_demo.cpp)
  target_link_libraries(analog_clock_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(analog_clock_demo PROPERTIES FOLDER "flex/examples")

  # Loading Animation Demo (multiple animated spinners)
  add_executable(loading_animation_demo loading_animation_demo.cpp)
  target_link_libraries(loading_animation_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(loading_animation_demo PROPERTIES FOLDER "flex/examples")

  # Music Player Demo (modern music player UI with visualizer)
  add_executable(music_player_demo music_player_demo.cpp)
  target_link_libraries(music_player_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(music_player_demo PROPERTIES FOLDER "flex/examples")

  # Breakout Game Demo (classic brick breaker)
  add_executable(breakout_game_demo breakout_game_demo.cpp)
  target_link_libraries(breakout_game_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(breakout_game_demo PROPERTIES FOLDER "flex/examples")

  # Space Shooter Demo (vertical scrolling shooter)
  add_executable(space_shooter_demo space_shooter_demo.cpp)
  target_link_libraries(space_shooter_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(space_shooter_demo PROPERTIES FOLDER "flex/examples")

  # UI Components Demo (interactive UI gallery)
  add_executable(ui_components_demo ui_components_demo.cpp)
  target_link_libraries(ui_components_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(ui_components_demo PROPERTIES FOLDER "flex/examples")

  # Data Binding Demo (reactive data binding)
  add_executable(data_binding_demo data_binding_demo.cpp)
  target_link_libraries(data_binding_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(data_binding_demo PROPERTIES FOLDER "flex/examples")

  # Shopping Cart Demo (e-commerce cart with calculations)
  add_executable(shopping_cart_demo shopping_cart_demo.cpp)
  target_link_libraries(shopping_cart_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(shopping_cart_demo PROPERTIES FOLDER "flex/examples")

  # Rocket Launch Demo (path animation)
  add_executable(rocket_launch_demo rocket_launch_demo.cpp)
  target_link_libraries(rocket_launch_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(rocket_launch_demo PROPERTIES FOLDER "flex/examples")

  # Path Showcase Demo (multiple path animation examples)
  add_executable(path_showcase_demo path_showcase_demo.cpp)
  target_link_libraries(path_showcase_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(path_showcase_demo PROPERTIES FOLDER "flex/examples")

  # Flex Layout Demo (flexbox automatic positioning)
  add_executable(flex_layout_demo flex_layout_demo.cpp)
  target_link_libraries(flex_layout_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(flex_layout_demo PROPERTIES FOLDER "flex/examples")

  # Component Demo (reusable UI components)
  add_executable(component_demo component_demo.cpp)
  target_link_libraries(component_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(component_demo PROPERTIES FOLDER "flex/examples")

  # Widget Gallery (standard UI component library)
  add_executable(widget_gallery widget_gallery.cpp)
  target_link_libraries(widget_gallery PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(widget_gallery PROPERTIES FOLDER "flex/examples")

  # Nested Components (component composition demo)
  add_executable(nested_components nested_components.cpp)
  target_link_libraries(nested_components PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(nested_components PROPERTIES FOLDER "flex/examples")

  # Component DSL Demo (declarative component usage)
  add_executable(component_dsl_demo component_dsl_demo.cpp)
  target_link_libraries(component_dsl_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(component_dsl_demo PROPERTIES FOLDER "flex/examples")

  # State Management Demo (reactive state with observable pattern)
  add_executable(state_demo state_demo.cpp)
  target_link_libraries(state_demo PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(state_demo PROPERTIES FOLDER "flex/examples")

  # Music Player Application (complete app with state management)
  add_executable(music_player_app music_player_app.cpp)
  target_link_libraries(music_player_app PRIVATE flex SDL2::SDL2 SDL2::SDL2main)
  set_target_properties(music_player_app PROPERTIES FOLDER "flex/examples")


  message(
    STATUS
      "Flex: Building SDL2 examples (thorvg_demo, editor_demo, dsl_render, platformer, physics_demo)"
  )
else()
  message(STATUS "Flex: SDL2 not found, skipping SDL2 examples")
endif()
