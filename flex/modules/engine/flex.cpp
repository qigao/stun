/*
 * Flex Engine - Main Implementation
 *
 * Definition loading and Instance management.
 *
 */

#include "flex.h"
#include "flex/core/debug.h"
#include "flex/bridge/ast_to_runtime.h"
#include "flex/dsl/flex_parser.h"
#include "flex/core/expr_compiled.h"
#include "runtime/asset_resolver.h"
#include "runtime/scene_clone.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace flex {

// ============================================================================
// Definition Implementation (stores Runtime objects)
// ============================================================================

// Internal: Parse and merge imports recursively
static bool load_imports_recursive(
    const std::string &base_dir,
    parser::AstProgram &program,
    std::set<std::string> &loaded_files,
    std::string &error_message,
    int &error_line,
    int &error_column) {

  for (const auto &import : program.imports) {
    std::string import_path = runtime::join_path(base_dir, import.path);
    std::string normalized = runtime::normalize_path(import_path);

    // Check for circular import
    if (loaded_files.count(normalized)) {
      FLEX_LOGD("Skipping already imported file: {}", normalized);
      continue;  // Already loaded, skip (not an error)
    }

    // Mark as loaded to prevent cycles
    loaded_files.insert(normalized);

    // Read imported file
    std::ifstream file(import_path);
    if (!file.is_open()) {
      error_message = "Could not open imported file: " + import.path;
      error_line = import.line;
      error_column = import.column;
      return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    // Parse imported file
    auto imported_program = parser::parse(buffer.str().c_str());
    if (!imported_program) {
      error_message = "Error in imported file '" + import.path + "': " + parser::get_error();
      error_line = import.line;
      error_column = import.column;
      return false;
    }

    FLEX_LOGD("Imported: {} ({} animations, {} machines)",
              import.path,
              imported_program->animations.size(),
              imported_program->machines.size());

    std::string import_dir = runtime::get_directory(import_path);

    // Imported assets are declared relative to the imported file, but once
    // merged into the parent program they need to keep resolving correctly.
    // Rebase only the current file's own assets before nested imports merge in.
    runtime::rebase_program_asset_paths(import_dir, *imported_program);

    // Recursively process imports in the imported file
    if (!load_imports_recursive(import_dir, *imported_program, loaded_files,
                                error_message, error_line, error_column)) {
      return false;
    }

    // Merge imported content into main program
    // Note: Scene is NOT merged (each file should have at most one scene)
    // Components, animations, machines, data, assets are merged

    for (auto &comp : imported_program->components) {
      program.components.push_back(std::move(comp));
    }

    for (auto &anim : imported_program->animations) {
      program.animations.push_back(std::move(anim));
    }

    for (auto &machine : imported_program->machines) {
      program.machines.push_back(std::move(machine));
    }

    for (auto &data : imported_program->data_blocks) {
      program.data_blocks.push_back(std::move(data));
    }

    // Imported constants remain parse-time local to their source file, while
    // runtime variables participate in the Definition-wide input schema.
    for (auto &declaration : imported_program->constants) {
      if (declaration.is_variable) {
        program.constants.push_back(std::move(declaration));
      }
    }

    // Merge assets
    if (imported_program->assets) {
      if (!program.assets) {
        program.assets = std::make_shared<parser::AstAssets>();
      }
      for (auto &asset : imported_program->assets->assets) {
        program.assets->assets.push_back(std::move(asset));
      }
    }
  }

  return true;
}

Definition::SharedPtr Definition::load(const char *source) {
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

  // Note: load() doesn't handle imports (no base directory)
  // Use load_file() for import support

  // Convert AST to Runtime objects using AstToRuntimeConverter
  AstToRuntimeConverter converter(def->impl_.get());
  converter.convert(*program);

  // Default scene if none was created
  if (!def->impl_->scene) {
    def->impl_->scene = Scene::create(800, 600, def->impl_->object_alloc);
  }

  if (!def->impl_->has_error) {
    def->impl_->has_error = false;
  }

  // AST PRUNING: The AST is no longer needed after conversion.
  // Since 'program' is a shared_ptr to AstProgram, it will be deleted when it goes out of scope here.
  // To be explicit and support future optimizations where we might want to discard the compiler module,
  // we ensure no references remain.

  return def;
}

Definition::SharedPtr Definition::load_file(const char *path) {
  auto def = std::shared_ptr<Definition>(new Definition());
  def->impl_ = std::make_unique<Impl>();

  std::ifstream file(path);
  if (!file.is_open()) {
    def->impl_->has_error = true;
    def->impl_->error_message = "Could not open file: " + std::string(path);
    return def;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  // Parse main file
  auto program = parser::parse(buffer.str().c_str());
  if (!program) {
    def->impl_->has_error = true;
    def->impl_->error_message = parser::get_error();
    def->impl_->error_line = parser::get_error_line();
    def->impl_->error_column = parser::get_error_column();
    return def;
  }

  // Process imports recursively
  std::string base_dir = runtime::get_directory(path);
  std::set<std::string> loaded_files;
  loaded_files.insert(runtime::normalize_path(path));  // Mark main file as loaded

  std::string import_error;
  int import_error_line = 0;
  int import_error_column = 0;

  if (!load_imports_recursive(base_dir, *program, loaded_files,
                              import_error, import_error_line, import_error_column)) {
    def->impl_->has_error = true;
    def->impl_->error_message = import_error;
    def->impl_->error_line = import_error_line;
    def->impl_->error_column = import_error_column;
    return def;
  }

  FLEX_LOGD("Loaded {} files total", loaded_files.size());

  // Convert AST to Runtime objects using AstToRuntimeConverter
  AstToRuntimeConverter converter(def->impl_.get());
  converter.convert(*program);

  // Default scene if none was created
  if (!def->impl_->scene) {
    def->impl_->scene = Scene::create(800, 600, def->impl_->object_alloc);
  }

  if (!def->impl_->has_error) {
    def->impl_->has_error = false;
  }

  // AST PRUNING: The AST is discarded as it's no longer needed.
  
  return def;
}

const Definition::InputSchema &Definition::input_schema() const {
  return impl_->parsed_variables;
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
      count += count_nodes(child);  // children() returns vector<Node*>
    }
  }
  return count;
}

// Diagnostic: Print arena allocator statistics
Instance::~Instance() {
  if (impl_) {
    // Count total nodes in scene
    size_t node_count = count_nodes(impl_->scene ? impl_->scene->root() : nullptr);

    size_t frame_total = impl_->frame_alloc.size();
    size_t frame_used = impl_->frame_alloc.used();
    size_t frame_peak = impl_->frame_alloc.peak_used();
    float frame_usage = frame_total > 0 ? (float)frame_used / frame_total * 100.0f : 0;
    float frame_peak_pct = frame_total > 0 ? (float)frame_peak / frame_total * 100.0f : 0;

    size_t obj_total = impl_->object_alloc.size();
    size_t obj_used = impl_->object_alloc.used();
    size_t obj_peak = impl_->object_alloc.peak_used();
    float obj_usage = obj_total > 0 ? (float)obj_used / obj_total * 100.0f : 0;
    float obj_peak_pct = obj_total > 0 ? (float)obj_peak / obj_total * 100.0f : 0;
  }
}

Instance::SharedPtr Instance::create(Definition::SharedPtr definition) {
  auto instance = std::shared_ptr<Instance>(new Instance());
  instance->impl_->definition = definition;

  if (definition && definition->scene()) {
    std::unordered_map<Node::RawPtr, Node::SharedPtr> cloned_shared_nodes;

    // Each instance needs an isolated scene graph.
    instance->impl_->scene = runtime::clone_scene(definition->scene(),
                                                  instance->impl_->object_alloc,
                                                  &cloned_shared_nodes);

    // Initialize bindings context
    instance->impl_->bindings = std::make_unique<BindingContext>();

    // Initialize Animations - Timelines are already built by Parser
    for (const auto &tl : definition->timelines()) {
      instance->impl_->animation_controller.add_timeline(tl);
    }

    // Initialize State Machines - copy from definition and set up callbacks
    for (const auto &machine : definition->machines()) {
      // Clone the machine to ensure unique state per instance
      auto cloned_machine = machine->clone();
      instance->impl_->machines.push_back(cloned_machine);

      // Set up callback to trigger animations and audio on state changes
      std::weak_ptr<Instance> weak_inst = instance;
      cloned_machine->set_state_change_callback(
          [weak_inst](const StateChangeInfo& info) {
            auto inst = weak_inst.lock();
            if (!inst) return;

            // Handle animation with optional params
            if (!info.animation.empty()) {
              FLEX_LOGD("State change triggers animation: {}", info.animation);
              auto* player = inst->play_animation(info.animation);
              if (player && !info.animation_params.empty()) {
                for (const auto& [name, expr_str] : info.animation_params) {
                  // Evaluate expression with current inputs
                  auto& compiled = inst->impl_->compiled_expressions[expr_str];
                  std::unordered_map<Symbol, float, SymbolHash> float_inputs;
                  for (const auto& [sym, val] : inst->impl_->inputs) {
                    if (auto* fval = std::get_if<float>(&val)) {
                      float_inputs[sym] = *fval;
                    } else if (auto* bval = std::get_if<bool>(&val)) {
                      float_inputs[sym] = *bval ? 1.0f : 0.0f;
                    }
                  }
                  float value = 0.0f;
                  if (!evaluate_mir_expression_inputs(expr_str, compiled,
                                                      float_inputs, value)) {
                    FLEX_LOGE("Could not compile MIR animation parameter expression: {}",
                              expr_str);
                    continue;
                  }
                  if (name == "duration") player->set_duration_override(value);
                  else if (name == "speed") player->set_speed(value);
                }
              }
            }
            if (!info.stop_audio.empty()) {
              FLEX_LOGD("State change stops audio: {}", info.stop_audio);
              inst->stop_audio(info.stop_audio.c_str());
            }
            if (!info.play_audio.empty()) {
              FLEX_LOGD("State change plays audio: {}", info.play_audio);
              inst->play_audio(info.play_audio.c_str());
            }

            // Handle state entry actions: set #node.prop: ${expr}
            if (!info.actions.empty() && inst->impl_->scene) {
              std::unordered_map<Symbol, float, SymbolHash> float_inputs;
              for (const auto& [sym, val] : inst->impl_->inputs) {
                if (auto* fval = std::get_if<float>(&val)) {
                  float_inputs[sym] = *fval;
                } else if (auto* bval = std::get_if<bool>(&val)) {
                  float_inputs[sym] = *bval ? 1.0f : 0.0f;
                }
              }
              for (const auto& action : info.actions) {
                Node* target = inst->impl_->scene->find(action.node_id);
                if (!target) continue;
                auto& compiled = inst->impl_->compiled_expressions[action.expression];
                float result = 0.0f;
                if (!evaluate_mir_expression_inputs(action.expression, compiled,
                                                    float_inputs, result)) {
                  FLEX_LOGE("Could not compile MIR state action expression: {}",
                            action.expression);
                  continue;
                }
                PropertyID pid = get_property_id(action.property.c_str());
                if (pid != PropertyID::Unknown) {
                  target->set_animated_property(pid, AnimValue(result));
                }
              }
            }
          });

    }

    // Register bindings from definition
    if (!definition->impl_->bindings.empty()) {
      for (const auto &binding_def : definition->impl_->bindings) {
        Node *target = instance->impl_->scene
                           ? instance->impl_->scene->find(binding_def.node_id)
                           : nullptr;
        if (!target) {
          FLEX_LOGW("Skipping binding setup for missing cloned target '{}'",
                    binding_def.node_id);
          continue;
        }
        Binding binding = binding_def.binding;
        binding.target = target;
        instance->impl_->bindings->add_binding(target,
                                               binding_def.property.c_str(),
                                               binding);
      }
      instance->impl_->bindings->mark_dirty();
    }

    if (!definition->impl_->component_bindings.empty()) {
      for (const auto &binding_def : definition->impl_->component_bindings) {
        Node *target = instance->impl_->scene
                           ? instance->impl_->scene->find(binding_def.node_id)
                           : nullptr;
        if (!target) {
          FLEX_LOGW("Skipping component binding setup for missing cloned node '{}'",
                    binding_def.node_id);
          continue;
        }
        auto shared_it = cloned_shared_nodes.find(target);
        if (shared_it == cloned_shared_nodes.end()) {
          FLEX_LOGW("Skipping component binding setup for missing shared node '{}'",
                    binding_def.node_id);
          continue;
        }
        ComponentPropBinding def;
        def.component_name = binding_def.component_name;
        def.node = shared_it->second;
        def.component = binding_def.component;
        def.base_props = binding_def.base_props;
        def.prop_bindings = binding_def.prop_bindings;
        instance->impl_->bindings->add_component_binding(def);
      }
      instance->impl_->bindings->mark_dirty();
    }

    // Materialize one independent copy of each DSL `var` default only after
    // bindings and machines exist, so all runtime consumers observe the same
    // initial value.
    for (const auto &[name, default_value] : definition->impl_->parsed_variables) {
      instance->set_input_value(name.c_str(), default_value);
    }

    // Initial state actions and animation parameters may reference `var`
    // declarations, so they must run after defaults have been installed.
    for (const auto &machine : instance->impl_->machines) {
      machine->trigger_initial_animations();
    }

    // Auto-play timelines set to Loop
    // (Optional: logic to auto-play)

    // Initialize assets from definition
    if (!definition->impl_->parsed_assets.empty()) {
      for (const auto &asset : definition->impl_->parsed_assets) {
        if (asset.type == "audio") {
          instance->register_audio(asset.id.c_str(), asset.path.c_str(), asset.loop, asset.volume);
        } else if (asset.type == "image") {
          instance->register_image(asset.id.c_str(), asset.path.c_str());
        } else if (asset.type == "font") {
          instance->register_font(asset.id.c_str(), asset.path.c_str());
        }
        // Note: svg assets are handled as images
      }
      // Preload assets if requested
      instance->preload_assets();
    }
  }

  return instance;
}

Instance::SharedPtr Instance::create(float width, float height) {
  auto instance = std::shared_ptr<Instance>(new Instance());
  instance->impl_->scene = Scene::create(width, height, instance->impl_->object_alloc);

  return instance;
}

void Instance::set_input_value(const char *name, Impl::InputValue value) {
  if (!name || !name[0]) {
    throw std::invalid_argument("Flex input name must not be empty");
  }

  if (impl_->definition) {
    const auto &variables = impl_->definition->impl_->parsed_variables;
    const auto variable = variables.find(name);
    if (variable != variables.end()) {
      if (variable->second.index() != value.index()) {
        static constexpr const char *kTypeNames[] = {"number", "string", "boolean"};
        throw std::invalid_argument(
            "Flex input '" + std::string(name) + "' expects " +
            kTypeNames[variable->second.index()] + ", got " +
            kTypeNames[value.index()]);
      }
    }
  }

  Symbol sym(name);
  impl_->inputs[sym] = value;
  if (impl_->bindings) {
    if (const auto *number = std::get_if<float>(&value)) {
      impl_->bindings->set_input(sym, *number);
    } else if (const auto *text = std::get_if<std::string>(&value)) {
      impl_->bindings->set_input(sym, *text);
    } else {
      impl_->bindings->set_input(sym, std::get<bool>(value));
    }
    impl_->bindings->mark_dirty();
  }

  if (const auto *number = std::get_if<float>(&value)) {
    for (auto &machine : impl_->machines) {
      machine->set_input(sym, *number);
    }
  } else if (const auto *flag = std::get_if<bool>(&value)) {
    for (auto &machine : impl_->machines) {
      machine->set_input(sym, *flag ? 1.0f : 0.0f);
    }
  }
}

void Instance::set_input(const char *name, float value) {
  set_input_value(name, value);
}

void Instance::set_input(const char *name, const char *value) {
  if (!value) {
    throw std::invalid_argument("Flex string input value must not be null");
  }
  set_input_value(name, std::string(value));
}

void Instance::set_input(const char *name, bool value) {
  set_input_value(name, value);
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

  // Update timeline animations
  impl_->animation_controller.advance(dt);

  // Clear fired events
  impl_->fired_events.clear();
}

void Instance::render(Renderer &renderer) {
  if (impl_->scene) {
    impl_->scene->render(renderer);
  }
}

// Helper: recursive hit test on scene graph (back-to-front, returns topmost hit)
// x, y are in the coordinate space of node's parent
static Node *hit_test_recursive(Node *node, float x, float y) {
  if (!node || !node->visible())
    return nullptr;

  // Convert global point to node's local space using Eigen matrices
  Vec2 global_pos(x, y);
  Vec2 local_pos = node->to_local(global_pos);
  bool contains_point = node->bounds().contains(local_pos.x, local_pos.y);

  if (node->is_group()) {
    auto *group = static_cast<Group *>(node);
    const auto &children = group->children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      Node* child = *it;
      if (!child || !child->visible()) {
        continue;
      }

      bool can_reach_child =
          child->position_mode() == PositionMode::Fixed || contains_point || !group->clip();
      if (!can_reach_child) {
        continue;
      }

      // Pass the SAME global coordinates to children, they will do their own to_local
      Node *hit = hit_test_recursive(child, x, y);  // children() returns vector<Node*>
      if (hit)
        return hit;
    }
  }

  // Check if point is within this node's bounds in its own local coordinate space
  // Note: Node::bounds() now returns bounds in local space
  if (!contains_point) {
    return nullptr;
  }

  return node;
}

// Helper: build path from root to target node
static size_t build_propagation_path(Node *target, Node **path_buffer, size_t max_items) {
  size_t count = 0;
  for (Node *n = target; n != nullptr && count < max_items; n = n->parent()) {
    path_buffer[count++] = n;
  }
  // path is now [target, parent, grandparent, ..., root]
  // Reverse to get [root, ..., grandparent, parent, target]
  if (count > 0) {
    for (size_t i = 0; i < count / 2; ++i) {
      std::swap(path_buffer[i], path_buffer[count - 1 - i]);
    }
  }
  return count;
}

static void set_event_local_coordinates(PointerEvent &event, Node *node) {
  if (!node) {
    event.local_x = 0.0f;
    event.local_y = 0.0f;
    return;
  }

  Vec2 local = node->to_local(Vec2(event.x, event.y));
  event.local_x = local.x;
  event.local_y = local.y;
}

static Node* find_focus_candidate(Node* node) {
  for (Node* current = node; current; current = current->parent()) {
    if (current->focusable()) {
      return current;
    }
  }
  return nullptr;
}

// Helper: dispatch event through propagation path with bubbling
static void dispatch_with_bubbling(PointerEvent &event, Node **path, size_t count,
                                   void (Node::*fire_method)(PointerEvent &)) {
  if (count == 0)
    return;

  // Target phase: last node in path
  Node *target = path[count - 1];
  event.phase = EventPhase::Target;
  event.current_target = target;
  set_event_local_coordinates(event, target);
  (target->*fire_method)(event);
  if (event.propagation_stopped())
    return;

  // Bubble phase: from parent to root (reverse order, skip target)
  // Parent is at count-2 (if count >= 2)
  event.phase = EventPhase::Bubble;
  for (int i = static_cast<int>(count) - 2; i >= 0; --i) {
    Node *node = path[i];
    event.current_target = node;
    set_event_local_coordinates(event, node);
    (node->*fire_method)(event);
    if (event.propagation_stopped())
      return;
  }
}

static void dispatch_key_with_bubbling(KeyEvent &event, Node **path, size_t count,
                                       void (Node::*fire_method)(KeyEvent &)) {
  if (count == 0)
    return;

  Node* target = path[count - 1];
  event.phase = EventPhase::Target;
  event.current_target = target;
  (target->*fire_method)(event);
  if (event.propagation_stopped())
    return;

  event.phase = EventPhase::Bubble;
  for (int i = static_cast<int>(count) - 2; i >= 0; --i) {
    event.current_target = path[i];
    (path[i]->*fire_method)(event);
    if (event.propagation_stopped())
      return;
  }
}

void Instance::send_pointer_event(float x, float y, bool is_down) {
  if (!impl_->scene)
    return;

  // Find node at pointer position
  Node *hit_node = hit_test_recursive(impl_->scene->root(), x, y);

  // Stack buffer for path (avoid heap allocation)
  constexpr size_t kMaxPathDepth = 64;
  Node *path[kMaxPathDepth];
  size_t path_count = 0;

  if (hit_node) {
    path_count = build_propagation_path(hit_node, path, kMaxPathDepth);
  }

  // Handle hover enter/leave (no bubbling for enter/leave)
  Node* prev_hover = impl_->hover_node;
  if (hit_node != prev_hover) {
    // Leave old node
    if (prev_hover) {
      PointerEvent leave_event;
      leave_event.type = PointerEventType::Leave;
      leave_event.phase = EventPhase::Target;
      leave_event.x = x;
      leave_event.y = y;
      leave_event.target = prev_hover;
      leave_event.current_target = prev_hover;
      set_event_local_coordinates(leave_event, prev_hover);
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
      set_event_local_coordinates(enter_event, hit_node);
      hit_node->fire_hover_enter(enter_event);

      impl_->hover_node = hit_node;
    } else {
      impl_->hover_node = nullptr;
    }
  }

  // Handle pointer down (with bubbling)
  if (is_down && !impl_->is_pointer_down) {
    impl_->is_pointer_down = true;
    Node* focus_candidate = find_focus_candidate(hit_node);
    if (focus_candidate) {
      request_focus(focus_candidate, FocusChangeReason::Pointer);
    } else {
      clear_focus(FocusChangeReason::Pointer);
    }

    if (hit_node) {
      impl_->pointer_down_node = hit_node;
      PointerEvent event;
      event.type = PointerEventType::Down;
      event.x = x;
      event.y = y;
      event.target = hit_node;
      dispatch_with_bubbling(event, path, path_count, &Node::fire_pointer_down);

    } else {
      impl_->pointer_down_node = nullptr;
    }
  }
  // Handle pointer up (with bubbling)
  else if (!is_down && impl_->is_pointer_down) {
    impl_->is_pointer_down = false;

    Node* down_node = impl_->pointer_down_node;
    if (down_node) {
      // Build path for the original down node (not current hit)
      Node *down_path[kMaxPathDepth];
      size_t down_path_count = build_propagation_path(down_node, down_path, kMaxPathDepth);

      PointerEvent up_event;
      up_event.type = PointerEventType::Up;
      up_event.x = x;
      up_event.y = y;
      up_event.target = down_node;
      dispatch_with_bubbling(up_event, down_path, down_path_count, &Node::fire_pointer_up);

      // Fire click if up on same node as down (with bubbling)
      if (down_node == hit_node) {
        // Click events bubble from target to root
        for (int i = static_cast<int>(down_path_count) - 1; i >= 0; --i) {
          down_path[i]->fire_click();
        }
      }
    }

    impl_->pointer_down_node = nullptr;
  }
  // Handle pointer move (with bubbling)
  else if (hit_node) {
    PointerEvent event;
    event.type = PointerEventType::Move;
    event.x = x;
    event.y = y;
    event.target = hit_node;
    dispatch_with_bubbling(event, path, path_count, &Node::fire_pointer_move);
  }
}

bool Instance::request_focus(Node::RawPtr node, FocusChangeReason reason) {
  if (node && !node->focusable()) {
    return false;
  }

  Node* previous = impl_->focused_node;
  if (previous == node) {
    return true;
  }

  if (previous) {
    impl_->focused_node = nullptr;
    previous->set_focused(false);

    FocusEvent event;
    event.gained = false;
    event.reason = reason;
    event.target = previous;
    event.related_target = node;
    event.current_target = previous;
    previous->fire_focus_event(event);
  }

  if (!node) {
    return true;
  }

  impl_->focused_node = node;
  node->set_focused(true);

  FocusEvent event;
  event.gained = true;
  event.reason = reason;
  event.target = node;
  event.related_target = previous;
  event.current_target = node;
  node->fire_focus_event(event);
  return true;
}

void Instance::clear_focus(FocusChangeReason reason) {
  request_focus(nullptr, reason);
}

Node::RawPtr Instance::focused_node() const {
  return impl_->focused_node;
}

void Instance::send_key_event(KeyCode key, bool is_down, const KeyModifiers& modifiers,
                              bool repeat) {
  Node* target = impl_->focused_node;
  if (!target || !target->visible()) {
    return;
  }

  constexpr size_t kMaxPathDepth = 64;
  Node* path[kMaxPathDepth];
  size_t path_count = build_propagation_path(target, path, kMaxPathDepth);
  if (path_count == 0) {
    return;
  }

  KeyEvent event;
  event.type = is_down ? KeyEventType::Down : KeyEventType::Up;
  event.key = key;
  event.modifiers = modifiers;
  event.repeat = repeat;
  event.target = target;
  dispatch_key_with_bubbling(
      event,
      path,
      path_count,
      is_down ? &Node::fire_key_down : &Node::fire_key_up);
}

void Instance::send_text_input(const char* utf8, bool from_ime) {
  Node* target = impl_->focused_node;
  if (!target || !target->visible() || !utf8) {
    return;
  }

  TextInputEvent event;
  event.text = utf8;
  event.from_ime = from_ime;
  event.target = target;
  event.current_target = target;
  target->fire_text_input(event);
}

void Instance::send_composition_event(CompositionEventType type, const char* utf8,
                                      int selection_start, int selection_end) {
  Node* target = impl_->focused_node;
  if (!target || !target->visible()) {
    return;
  }

  CompositionEvent event;
  event.type = type;
  event.text = utf8 ? utf8 : "";
  event.selection_start = selection_start;
  event.selection_end = selection_end;
  event.target = target;
  event.current_target = target;
  target->fire_composition(event);
}

void Instance::send_event(const char *name) { impl_->fired_events.insert(std::string(name)); }

// ============================================================================
// Animation Control (Phase 2)
// ============================================================================

AnimationController *Instance::animation_controller() const { return &impl_->animation_controller; }

void Instance::add_timeline(Timeline::SharedPtr timeline) {
  impl_->animation_controller.add_timeline(timeline);
}

TimelinePlayer *Instance::play(const char *timeline_name, Node *target) {
  return impl_->animation_controller.play(timeline_name, target);
}

TimelinePlayer *Instance::play(const char *timeline_name) {
  return impl_->animation_controller.play(timeline_name, impl_->scene->root());
}

void Instance::stop(const char *timeline_name) { impl_->animation_controller.stop(timeline_name); }

void Instance::stop_all() { impl_->animation_controller.stop_all(); }

float Instance::get_input(const char *name) const {
  Symbol sym(name);
  auto it = impl_->inputs.find(sym);
  if (it != impl_->inputs.end()) {
    if (std::holds_alternative<float>(it->second)) {
      return std::get<float>(it->second);
    }
    if (std::holds_alternative<bool>(it->second)) {
      return std::get<bool>(it->second) ? 1.0f : 0.0f;
    }
  }
  return 0.0f;
}

void Instance::register_asset(const char *name, const char *path) {
  Symbol sym(name);
  impl_->assets[sym] = std::string(path);
}

const char *Instance::resolve_asset(const char *name) const {
  auto it = impl_->assets.find(Symbol(name));
  if (it != impl_->assets.end()) {
    return it->second.c_str();
  }
  return "";
}

// ============================================================================
// Asset Management (New System)
// ============================================================================

AssetManager* Instance::asset_manager() const {
  return impl_->asset_manager.get();
}

void Instance::register_audio(const char* id, const char* path, bool loop, float volume) {
  if (!impl_->asset_manager) {
    impl_->asset_manager = std::make_unique<AssetManager>();
  }
  AudioOptions opts;
  opts.loop = loop;
  opts.volume = volume;
  impl_->asset_manager->register_audio(id, path, opts);
}

void Instance::register_image(const char* id, const char* path) {
  if (!impl_->asset_manager) {
    impl_->asset_manager = std::make_unique<AssetManager>();
  }
  impl_->asset_manager->register_image(id, path);
}

void Instance::register_font(const char* id, const char* path) {
  if (!impl_->asset_manager) {
    impl_->asset_manager = std::make_unique<AssetManager>();
  }
  impl_->asset_manager->register_font(id, path);
}

int Instance::play_audio(const char* id) {
  if (!impl_->asset_manager) {
    return -1;
  }
  return impl_->asset_manager->play_audio(id);
}

int Instance::play_audio(const char* id, bool loop, float volume) {
  if (!impl_->asset_manager) {
    return -1;
  }
  return impl_->asset_manager->play_audio(id, loop, volume);
}

void Instance::stop_audio(const char* id) {
  if (impl_->asset_manager) {
    impl_->asset_manager->stop_audio(id);
  }
}

void Instance::stop_audio_channel(int channel) {
  if (impl_->asset_manager) {
    impl_->asset_manager->stop_audio_channel(channel);
  }
}

void Instance::set_audio_volume(int channel, float volume) {
  if (impl_->asset_manager) {
    impl_->asset_manager->set_audio_volume(channel, volume);
  }
}

bool Instance::is_audio_playing(int channel) {
  if (!impl_->asset_manager) {
    return false;
  }
  return impl_->asset_manager->is_audio_playing(channel);
}

void Instance::preload_assets() {
  if (impl_->asset_manager) {
    impl_->asset_manager->preload_all();
  }
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

TimelinePlayer* Instance::play_animation(const std::string &name) {
  if (!impl_->scene) {
    return nullptr;
  }
  return impl_->animation_controller.play(name.c_str(), impl_->scene->root());
}

void Instance::start_animation(const std::string &name) {
  play_animation(name);
}

void Instance::stop_animation(const std::string &name) {
  impl_->animation_controller.stop(name.c_str());
}

// ============================================================================
// Memory Pool Access (Arena Allocator)
// ============================================================================

ArenaAllocator *Instance::frame_allocator() const { return &impl_->frame_alloc; }

ArenaAllocator *Instance::object_allocator() const { return &impl_->object_alloc; }

void Instance::reset_frame() { impl_->frame_alloc.reset(); }

} // namespace flex
