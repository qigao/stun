/*
 * Flex Engine - Main Implementation
 *
 * Definition loading and Instance management.
 *
 */

#include "flex/flex.h"
#include "ast_to_runtime.cpp" // Inline converter
#include "flex/component.h"
#include "flex/group.h"
#include "flex/script.h"
#include "flex_parser.h"
#include <algorithm>
#include <cstring>
#include <fmtlog.h>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <thorvg.h>
#include <vector>

namespace flex {

// ============================================================================
// Definition Implementation (stores Runtime objects)
// ============================================================================

Definition::Ptr Definition::load(const char *source) {
  auto def = std::shared_ptr<Definition>(new Definition());
  def->impl_ = std::make_unique<Impl>();

  // Parse source into AST
  auto program = parser::parse(source);

  if (!program) {
    def->impl_->has_error = true;
    def->impl_->error_message = parser::get_error();
    def->impl_->error_line = parser::get_error_line();
    def->impl_->error_column = parser::get_error_column();
    return def;
  }

  // Convert AST scene to Artboard
  if (program->scene) {
    def->impl_->artboard = parser::convert_ast_scene(program->scene);
  }

  if (!def->impl_->artboard) {
    def->impl_->artboard = Artboard::create(800, 600);
  }

  def->impl_->has_error = false;

  // Convert animations and state machines using AstToRuntimeConverter
  AstToRuntimeConverter converter(def->impl_.get());
  converter.convert(*program);

  return def;
}

Definition::Ptr Definition::load_file(const char *path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    auto def = std::shared_ptr<Definition>(new Definition());
    def->impl_ = std::make_unique<Impl>();
    def->impl_->has_error = true;
    def->impl_->error_message = "Could not open file: " + std::string(path);
    return def;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return load(buffer.str().c_str());
}

// ============================================================================
// Instance Implementation
// ============================================================================

Instance::Instance() : impl_(std::make_unique<Impl>()) {}

// Helper: Count nodes recursively
static size_t count_nodes(Node *node) {
  if (!node)
    return 0;
  size_t count = 1;
  if (node->is_group()) {
    auto *group = static_cast<Group *>(node);
    for (const auto &child : group->children()) {
      count += count_nodes(child.get());
    }
  }
  return count;
}

// Diagnostic: Print arena allocator statistics
Instance::~Instance() {
  if (impl_) {
    // Count total nodes in scene
    size_t node_count = count_nodes(impl_->artboard ? impl_->artboard->root() : nullptr);

    size_t frame_total = impl_->frame_alloc.size();
    size_t frame_used = impl_->frame_alloc.used();
    size_t frame_peak = impl_->frame_alloc.peak_used();
    float frame_usage = frame_total > 0 ? (float)frame_used / frame_total * 100.0f : 0;
    float frame_peak_pct = frame_total > 0 ? (float)frame_peak / frame_total * 100.0f : 0;

    logi("  Frame:  {:.2f} MB allocated, {:.2f} MB used ({:.1f}%), peak {:.2f} MB ({:.1f}%), {} "
         "allocations",
         frame_total / (1024.0f * 1024.0f), frame_used / (1024.0f * 1024.0f), frame_usage,
         frame_peak / (1024.0f * 1024.0f), frame_peak_pct, impl_->frame_alloc.alloc_count());

    size_t obj_total = impl_->object_alloc.size();
    size_t obj_used = impl_->object_alloc.used();
    size_t obj_peak = impl_->object_alloc.peak_used();
    float obj_usage = obj_total > 0 ? (float)obj_used / obj_total * 100.0f : 0;
    float obj_peak_pct = obj_total > 0 ? (float)obj_peak / obj_total * 100.0f : 0;

    logi("  Object: {:.2f} MB allocated, {:.2f} MB used ({:.1f}%), peak {:.2f} MB ({:.1f}%), {} "
         "allocations",
         obj_total / (1024.0f * 1024.0f), obj_used / (1024.0f * 1024.0f), obj_usage,
         obj_peak / (1024.0f * 1024.0f), obj_peak_pct, impl_->object_alloc.alloc_count());

    logi("  Nodes:  {} total in scene graph", node_count);
  }
}

Instance::Ptr Instance::create(Definition::Ptr definition) {
  auto instance = std::shared_ptr<Instance>(new Instance());
  instance->impl_->definition = definition;

  if (definition && definition->artboard()) {
    // No Builder - Definition already has Runtime objects
    instance->impl_->artboard = definition->artboard();

    // Initialize bindings context
    instance->impl_->bindings = std::make_unique<BindingContext>();

    // Initialize Animations - Timelines are already built by Parser
    for (const auto &tl : definition->timelines()) {
      instance->impl_->animation_controller.add_timeline(tl);
    }

    // Initialize State Machines - copy from definition and set up callbacks
    for (const auto &machine : definition->machines()) {
      instance->impl_->machines.push_back(machine);

      // Set up callback to trigger animations on state changes
      auto inst_ptr = instance.get();
      machine->set_state_change_callback(
          [inst_ptr](const std::string &layer, const std::string &from_state,
                     const std::string &to_state, const std::string &animation) {
            if (!animation.empty()) {
              std::cout << "[Instance] State change triggers animation: " << animation << "\n";
              inst_ptr->start_animation(animation);
            }
          });
    }

    // NOTE: ScriptContext is NOT created by default (saves ~15MB per Instance)
    // It will be created on-demand if expression bindings are actually used
    // in BindingContext::evaluate() or when user calls script APIs

    // Auto-play timelines set to Loop
    // (Optional: logic to auto-play)
  }

  return instance;
}

Instance::Ptr Instance::create(float width, float height) {
  auto instance = std::shared_ptr<Instance>(new Instance());
  instance->impl_->artboard = Artboard::create(width, height);

  // It will be created on-demand if script APIs are used

  return instance;
}

void Instance::set_input(const char *name, float value) {
  impl_->inputs[std::string(name)] = value;
  if (impl_->bindings) {
    impl_->bindings->set_input(std::string(name), value);
    impl_->bindings->mark_dirty(); // Mark dirty when input changes
  }
  // Forward to state machines
  for (auto &machine : impl_->machines) {
    machine->set_input(std::string(name), value);
  }
}

void Instance::set_input(const char *name, const char *value) {
  impl_->inputs[std::string(name)] = std::string(value);
  if (impl_->bindings) {
    impl_->bindings->set_input(std::string(name), std::string(value));
    impl_->bindings->mark_dirty(); // Mark dirty when input changes
  }
}

void Instance::advance(float dt) {
  impl_->time += dt;

  // Update bindings time and evaluate only if dirty
  if (impl_->bindings) {
    impl_->bindings->advance_time(dt);
    if (impl_->bindings->is_dirty()) {
      impl_->bindings->evaluate();
      impl_->bindings->clear_dirty();
    }
  }

  // Update state machines
  for (auto &machine : impl_->machines) {
    machine->update(dt);
  }

  // Update old timeline animations
  impl_->animation_controller.advance(dt);

  // Update runtime animations (new system)
  if (impl_->definition) {
    for (auto &[name, anim] : impl_->definition->impl_->animations) {
      if (anim->is_running()) {
        anim->update(dt);
      }
    }
  }

  // Clear fired events
  impl_->fired_events.clear();
}

void Instance::render(Renderer &renderer) {
  if (impl_->artboard) {
    impl_->artboard->render(renderer);
  }
}

// Helper: recursive hit test on scene graph (back-to-front, returns topmost hit)
// x, y are in the coordinate space of node's parent
static Node *hit_test_recursive(Node *node, float x, float y) {
  if (!node || !node->visible())
    return nullptr;

  // First check if point is within this node's bounds (in parent's coordinate space)
  Bounds b = node->bounds();
  bool inside = b.contains(x, y);

  // For groups, check children first (they may be on top of this node's bounds)
  if (node->is_group()) {
    auto *group = static_cast<Group *>(node);
    // Transform to this node's local coordinate space for children
    float local_x = x - node->x();
    float local_y = y - node->y();

    const auto &children = group->children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      Node *hit = hit_test_recursive(it->get(), local_x, local_y);
      if (hit)
        return hit;
    }
  }

  // If point is inside this node, return it
  if (inside) {
    return node;
  }

  return nullptr;
}

// Helper: build path from root to target node
static void build_propagation_path(Node *target, std::vector<Node *> &path) {
  path.clear();
  for (Node *n = target; n != nullptr; n = n->parent()) {
    path.push_back(n);
  }
  // path is now [target, parent, grandparent, ..., root]
  // Reverse to get [root, ..., grandparent, parent, target]
  std::reverse(path.begin(), path.end());
}

// Helper: dispatch event through propagation path with bubbling
static void dispatch_with_bubbling(PointerEvent &event, const std::vector<Node *> &path,
                                   void (Node::*fire_method)(PointerEvent &)) {
  if (path.empty())
    return;

  // Target phase: last node in path
  Node *target = path.back();
  event.phase = EventPhase::Target;
  event.current_target = target;
  event.local_x = event.x - target->x();
  event.local_y = event.y - target->y();
  (target->*fire_method)(event);
  if (event.propagation_stopped())
    return;

  // Bubble phase: from parent to root (reverse order, skip target)
  event.phase = EventPhase::Bubble;
  for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
    Node *node = path[i];
    event.current_target = node;
    event.local_x = event.x - node->x();
    event.local_y = event.y - node->y();
    (node->*fire_method)(event);
    if (event.propagation_stopped())
      return;
  }
}

void Instance::send_pointer_event(float x, float y, bool is_down) {
  if (!impl_->artboard)
    return;

  // Find node at pointer position
  Node *hit_node = hit_test_recursive(impl_->artboard->root(), x, y);

  // Build propagation path for hit node
  std::vector<Node *> path;
  if (hit_node) {
    build_propagation_path(hit_node, path);
  }

  // Handle hover enter/leave (no bubbling for enter/leave)
  auto prev_hover = impl_->hover_node.lock();
  if (hit_node != prev_hover.get()) {
    // Leave old node
    if (prev_hover) {
      PointerEvent leave_event;
      leave_event.type = PointerEventType::Leave;
      leave_event.phase = EventPhase::Target;
      leave_event.x = x;
      leave_event.y = y;
      leave_event.target = prev_hover.get();
      leave_event.current_target = prev_hover.get();
      leave_event.local_x = x - prev_hover->x();
      leave_event.local_y = y - prev_hover->y();
      prev_hover->fire_hover_leave(leave_event);
    }

    // Enter new node
    if (hit_node) {
      PointerEvent enter_event;
      enter_event.type = PointerEventType::Enter;
      enter_event.phase = EventPhase::Target;
      enter_event.x = x;
      enter_event.y = y;
      enter_event.target = hit_node;
      enter_event.current_target = hit_node;
      enter_event.local_x = x - hit_node->x();
      enter_event.local_y = y - hit_node->y();
      hit_node->fire_hover_enter(enter_event);

      impl_->hover_node = hit_node->shared_from_this();
    } else {
      impl_->hover_node.reset();
    }
  }

  // Handle pointer down (with bubbling)
  if (is_down && !impl_->is_pointer_down) {
    impl_->is_pointer_down = true;
    if (hit_node) {
      impl_->pointer_down_node = hit_node->shared_from_this();
      PointerEvent event;
      event.type = PointerEventType::Down;
      event.x = x;
      event.y = y;
      event.target = hit_node;
      dispatch_with_bubbling(event, path, &Node::fire_pointer_down);

    } else {
      impl_->pointer_down_node.reset();
    }
  }
  // Handle pointer up (with bubbling)
  else if (!is_down && impl_->is_pointer_down) {
    impl_->is_pointer_down = false;

    auto down_node = impl_->pointer_down_node.lock();
    if (down_node) {
      // Build path for the original down node (not current hit)
      std::vector<Node *> down_path;
      build_propagation_path(down_node.get(), down_path);

      PointerEvent up_event;
      up_event.type = PointerEventType::Up;
      up_event.x = x;
      up_event.y = y;
      up_event.target = down_node.get();
      dispatch_with_bubbling(up_event, down_path, &Node::fire_pointer_up);

      // Fire click if up on same node as down (with bubbling)
      if (down_node.get() == hit_node) {
        // Click events bubble from target to root
        for (auto it = down_path.rbegin(); it != down_path.rend(); ++it) {
          (*it)->fire_click();
        }
      }
    }

    impl_->pointer_down_node.reset();
  }
  // Handle pointer move (with bubbling)
  else if (hit_node) {
    PointerEvent event;
    event.type = PointerEventType::Move;
    event.x = x;
    event.y = y;
    event.target = hit_node;
    dispatch_with_bubbling(event, path, &Node::fire_pointer_move);
  }
}

void Instance::send_event(const char *name) { impl_->fired_events.insert(std::string(name)); }

// ============================================================================
// Animation Control (Phase 2)
// ============================================================================

AnimationController *Instance::animation_controller() const { return &impl_->animation_controller; }

void Instance::add_timeline(Timeline::Ptr timeline) {
  impl_->animation_controller.add_timeline(timeline);
}

TimelinePlayer *Instance::play(const char *timeline_name, Node *target) {
  return impl_->animation_controller.play(timeline_name, target);
}

TimelinePlayer *Instance::play(const char *timeline_name) {
  return impl_->animation_controller.play(timeline_name, impl_->artboard->root());
}

void Instance::stop(const char *timeline_name) { impl_->animation_controller.stop(timeline_name); }

void Instance::stop_all() { impl_->animation_controller.stop_all(); }

float Instance::get_input(const char *name) const {
  auto it = impl_->inputs.find(std::string(name));
  if (it != impl_->inputs.end() && std::holds_alternative<float>(it->second)) {
    return std::get<float>(it->second);
  }
  return 0.0f;
}

void Instance::register_asset(const char *name, const char *path) {
  impl_->assets[std::string(name)] = std::string(path);
}

const char *Instance::resolve_asset(const char *name) const {
  auto it = impl_->assets.find(std::string(name));
  if (it != impl_->assets.end()) {
    return it->second.c_str();
  }
  return "";
}

// ============================================================================
// Runtime Systems Access
// ============================================================================

RuntimeStateMachine *Instance::get_machine(const std::string &name) {
  for (auto &machine : impl_->machines) {
    if (machine->name() == name) {
      return machine.get();
    }
  }
  return nullptr;
}

RuntimeAnimation *Instance::get_animation(const std::string &name) {
  if (impl_->definition && impl_->definition->impl_->animations.count(name)) {
    return impl_->definition->impl_->animations.at(name).get();
  }
  return nullptr;
}

void Instance::start_animation(const std::string &name) {
  auto *anim = get_animation(name);
  if (anim && impl_->artboard) {
    // Set target root so animation can find nodes by ID (e.g., #statusText)
    // Note: Artboard is not a Node, use its root Group for node lookup
    anim->set_target_root(impl_->artboard->root());
    anim->start();
  }
}

void Instance::stop_animation(const std::string &name) {
  auto *anim = get_animation(name);
  if (anim) {
    anim->stop();
  }
}

// ============================================================================
// Memory Pool Access (Arena Allocator)
// ============================================================================

ArenaAllocator *Instance::frame_allocator() const { return &impl_->frame_alloc; }

ArenaAllocator *Instance::object_allocator() const { return &impl_->object_alloc; }

void Instance::reset_frame() { impl_->frame_alloc.reset(); }

} // namespace flex
