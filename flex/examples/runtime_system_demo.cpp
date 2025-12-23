/*
 * Runtime State Machine, Animation & Layout Demo
 * Demonstrates the complete runtime system
 */

#include <flex/animation.h>
#include <flex/flex.h>
#include <flex/layout.h>
#include <flex/runtime_machine.h>
#include <iostream>


using namespace flex;
using namespace flex::parser;

// ============================================================================
// Runtime System Demo
// ============================================================================

void demo_state_machine() {
  std::cout << "\n=== State Machine Runtime Demo ===\n\n";

  // Create a state machine
  auto machine = std::make_shared<RuntimeStateMachine>("statusTracker");

  // Add layer
  machine->add_layer("status");

  // Get the layer and add states
  auto layer = machine->get_layer("status");
  layer->add_state("neutral", true, "fadeIn");
  layer->add_state("positive", false, "pulse");
  layer->add_state("negative", false, "shake");

  // Add transitions
  layer->add_transition("neutral", "positive", "counter", ">", 0);
  layer->add_transition("positive", "neutral", "counter", "<", 0.1);
  layer->add_transition("neutral", "negative", "counter", "<", 0);
  layer->add_transition("negative", "neutral", "counter", ">", -0.1);

  std::cout << "Created state machine with 3 states\n";
  std::cout << "Initial state: " << layer->current_state() << "\n\n";

  // Simulate state transitions
  std::cout << "Setting counter to 5...\n";
  layer->set_input("counter", 5.0f);
  machine->update(0.016f);
  std::cout << "Current state: " << layer->current_state() << "\n\n";

  std::cout << "Setting counter to -3...\n";
  layer->set_input("counter", -3.0f);
  machine->update(0.016f);
  std::cout << "Current state: " << layer->current_state() << "\n\n";

  std::cout << "Setting counter to 0...\n";
  layer->set_input("counter", 0.0f);
  machine->update(0.016f);
  std::cout << "Current state: " << layer->current_state() << "\n\n";

  std::cout << "✅ State machine demo complete!\n";
}

void demo_animation() {
  std::cout << "\n=== Animation Runtime Demo ===\n\n";

  // Create animation manager
  AnimationManager manager;

  // Create an animation from AST-like data
  parser::AstAnim ast_anim("fadeIn");
  ast_anim.duration = 1.0f;
  ast_anim.loop_mode = "once";

  // Add track
  parser::AstTrack track("opacity");
  track.keyframes.push_back({0.0f, parser::AstValue(0.0f)});
  track.keyframes.push_back({0.5f, parser::AstValue(0.5f)});
  track.keyframes.push_back({1.0f, parser::AstValue(1.0f)});
  ast_anim.tracks.push_back(track);

  // Convert to runtime animation
  auto runtime_anim = manager.create_animation(ast_anim);

  std::cout << "Created animation: " << runtime_anim->name() << "\n";
  std::cout << "Duration: " << runtime_anim->duration() << "s\n";
  std::cout << "Loop: " << runtime_anim->loop_mode() << "\n";
  std::cout << "Tracks: " << runtime_anim->get_track("opacity")->keyframes().size() << "\n\n";

  // Simulate animation playback
  std::cout << "Playing animation...\n";
  runtime_anim->start();

  for (int i = 0; i < 60; i++) { // 1 second at 60fps
    runtime_anim->update(1.0f / 60.0f);

    if (i % 10 == 0) {
      std::cout << "  Time: " << (i / 60.0f) << "s"
                << ", Running: " << (runtime_anim->is_running() ? "yes" : "no")
                << ", Finished: " << (runtime_anim->is_finished() ? "yes" : "no") << "\n";
    }
  }

  std::cout << "\n✅ Animation demo complete!\n";
}

void demo_layout() {
  std::cout << "\n=== Layout Engine Demo ===\n\n";

  // Create a mock container and items
  auto container = std::make_shared<Group>();
  container->set_layout_size(400, 300);

  auto item1 = std::make_shared<Shape>();
  item1->set_layout_size(100, 50);

  auto item2 = std::make_shared<Shape>();
  item2->set_layout_size(100, 50);

  auto item3 = std::make_shared<Shape>();
  item3->set_layout_size(100, 50);

  std::cout << "Created container (400x300) with 3 items (100x50 each)\n\n";

  // Test 1: Row layout with space-between
  std::cout << "Test 1: Row layout with justifyContent: space-between\n";
  FlexLayoutEngine layout(container.get());
  layout.set_direction(FlexDirection::Row);
  layout.set_justify_content(JustifyContent::SpaceBetween);
  layout.set_gap(20);

  layout.layout(400, 300);

  std::cout << "  Item 1 x: " << item1->x() << "\n";
  std::cout << "  Item 2 x: " << item2->x() << "\n";
  std::cout << "  Item 3 x: " << item3->x() << "\n\n";

  // Test 2: Column layout
  std::cout << "Test 2: Column layout with center alignment\n";
  FlexLayoutEngine layout2(container.get());
  layout2.set_direction(FlexDirection::Column);
  layout2.set_justify_content(JustifyContent::Center);
  layout2.set_gap(20);

  layout2.layout(400, 300);

  std::cout << "  Item 1 y: " << item1->y() << "\n";
  std::cout << "  Item 2 y: " << item2->y() << "\n";
  std::cout << "  Item 3 y: " << item3->y() << "\n\n";

  std::cout << "✅ Layout demo complete!\n";
}

int main() {
#ifdef _WIN32
  system("chcp 65001 >nul"); // UTF-8
#endif
  std::cout << "=================================================\n";
  std::cout << "Flex Runtime System Demo\n";
  std::cout << "State Machine + Animation + Layout\n";
  std::cout << "=================================================\n";

  // Demo each system
  demo_state_machine();
  demo_animation();
  demo_layout();

  std::cout << "\n=================================================\n";
  std::cout << "✅ All runtime system demos complete!\n";
  std::cout << "=================================================\n";
  std::cout << "\nRuntime Systems Implemented:\n";
  std::cout << "  ✓ State Machine (tinyfsm-based)\n";
  std::cout << "  ✓ Animation (keyframes + easing)\n";
  std::cout << "  ✓ Layout (flexbox engine)\n\n";

  return 0;
}