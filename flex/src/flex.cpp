/*
 * Flex Engine - Main Implementation
 *
 * Definition loading and Instance management.
 *
 */

#include "flex.h"
#include "flex/runtime/debug.h"
#include "flex/bridge/ast_to_runtime.h"
#include "flex/compiler/flex_parser.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <thorvg.h>
#include <vector>

// Filesystem for path manipulation
#ifdef _WIN32
#include <direct.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#define PATH_SEP '/'
#endif

namespace flex {

// ============================================================================
// Helper: Path utilities for import resolution
// ============================================================================

static std::string get_directory(const std::string &path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) {
    return ".";
  }
  return path.substr(0, pos);
}

static std::string join_path(const std::string &dir, const std::string &file) {
  if (dir.empty() || dir == ".") {
    return file;
  }
  char last = dir.back();
  if (last == '/' || last == '\\') {
    return dir + file;
  }
  return dir + PATH_SEP + file;
}

static std::string normalize_path(const std::string &path) {
  // Simple normalization: replace backslashes with forward slashes
  std::string result = path;
  for (char &c : result) {
    if (c == '\\') c = '/';
  }
  return result;
}

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
    std::string import_path = join_path(base_dir, import.path);
    std::string normalized = normalize_path(import_path);

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

    // Recursively process imports in the imported file
    std::string import_dir = get_directory(import_path);
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

  // Note: load() doesn't handle imports (no base directory)
  // Use load_file() for import support

  // Convert AST to Runtime objects using AstToRuntimeConverter
  AstToRuntimeConverter converter(def->impl_.get());
  converter.convert(*program);

  // Default scene if none was created
  if (!def->impl_->scene) {
    def->impl_->scene = Scene::create(800, 600, def->impl_->object_alloc);
  }

  def->impl_->has_error = false;

  // AST PRUNING: The AST is no longer needed after conversion.
  // Since 'program' is a shared_ptr to AstProgram, it will be deleted when it goes out of scope here.
  // To be explicit and support future optimizations where we might want to discard the compiler module,
  // we ensure no references remain.

  return def;
}

Definition::Ptr Definition::load_file(const char *path) {
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
  std::string base_dir = get_directory(path);
  std::set<std::string> loaded_files;
  loaded_files.insert(normalize_path(path));  // Mark main file as loaded

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

  def->impl_->has_error = false;

  // AST PRUNING: The AST is discarded as it's no longer needed.
  
  return def;
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

Instance::Ptr Instance::create(Definition::Ptr definition) {
  auto instance = std::shared_ptr<Instance>(new Instance());
  instance->impl_->definition = definition;

  if (definition && definition->scene()) {
    // No Builder - Definition already has Runtime objects
    instance->impl_->scene = definition->scene();

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
          [weak_inst](const std::string &layer, const std::string &from_state,
                     const std::string &to_state, const std::string &animation,
                     const std::string &play_audio, const std::string &stop_audio) {
            auto inst = weak_inst.lock();
            if (!inst) return;

            if (!animation.empty()) {
              FLEX_LOGD("State change triggers animation: {}", animation);
              inst->start_animation(animation);
            }
            if (!stop_audio.empty()) {
              FLEX_LOGD("State change stops audio: {}", stop_audio);
              inst->stop_audio(stop_audio.c_str());
            }
            if (!play_audio.empty()) {
              FLEX_LOGD("State change plays audio: {}", play_audio);
              inst->play_audio(play_audio.c_str());
            }
          });

      // Trigger initial state animations now that callback is set
      cloned_machine->trigger_initial_animations();
    }

    // NOTE: ScriptContext is NOT created by default (saves ~15MB per Instance)
    // It will be created on-demand if expression bindings are actually used
    // in BindingContext::evaluate() or when user calls script APIs

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

Instance::Ptr Instance::create(float width, float height) {
  auto instance = std::shared_ptr<Instance>(new Instance());
  instance->impl_->scene = Scene::create(width, height, instance->impl_->object_alloc);

  // It will be created on-demand if script APIs are used

  return instance;
}

void Instance::set_input(const char *name, float value) {
  Symbol sym(name);
  impl_->inputs[sym] = value;
  if (impl_->bindings) {
    impl_->bindings->set_input(sym, value); // Using Symbol directly avoiding std::string
    impl_->bindings->mark_dirty();
  }
  // Forward to state machines
  for (auto &machine : impl_->machines) {
    machine->set_input(sym, value);
  }
}

void Instance::set_input(const char *name, const char *value) {
  Symbol sym(name);
  impl_->inputs[sym] = std::string(value);
  if (impl_->bindings) {
    impl_->bindings->set_input(sym, std::string(value));
    impl_->bindings->mark_dirty();
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

  // Check if point is within this node's bounds in its own local coordinate space
  // Note: Node::bounds() now returns bounds in local space
  if (!node->bounds().contains(local_pos.x(), local_pos.y())) {
    return nullptr;
  }

  // For groups, check children first (they may be on top)
  if (node->is_group()) {
    auto *group = static_cast<Group *>(node);
    const auto &children = group->children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      // Pass the SAME global coordinates to children, they will do their own to_local
      Node *hit = hit_test_recursive(*it, x, y);  // children() returns vector<Node*>
      if (hit)
        return hit;
    }
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

// Helper: dispatch event through propagation path with bubbling
static void dispatch_with_bubbling(PointerEvent &event, Node **path, size_t count,
                                   void (Node::*fire_method)(PointerEvent &)) {
  if (count == 0)
    return;

  // Target phase: last node in path
  Node *target = path[count - 1];
  event.phase = EventPhase::Target;
  event.current_target = target;
  event.local_x = event.x - target->x();
  event.local_y = event.y - target->y();
  (target->*fire_method)(event);
  if (event.propagation_stopped())
    return;

  // Bubble phase: from parent to root (reverse order, skip target)
  // Parent is at count-2 (if count >= 2)
  event.phase = EventPhase::Bubble;
  for (int i = static_cast<int>(count) - 2; i >= 0; --i) {
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

      impl_->hover_node = hit_node;
    } else {
      impl_->hover_node = nullptr;
    }
  }

  // Handle pointer down (with bubbling)
  if (is_down && !impl_->is_pointer_down) {
    impl_->is_pointer_down = true;
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
  return impl_->animation_controller.play(timeline_name, impl_->scene->root());
}

void Instance::stop(const char *timeline_name) { impl_->animation_controller.stop(timeline_name); }

void Instance::stop_all() { impl_->animation_controller.stop_all(); }

float Instance::get_input(const char *name) const {
  Symbol sym(name);
  auto it = impl_->inputs.find(sym);
  if (it != impl_->inputs.end() && std::holds_alternative<float>(it->second)) {
    return std::get<float>(it->second);
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
