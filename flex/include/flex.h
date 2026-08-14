/*
 * Flex Engine - Main Header
 *
 * Complete Flex Engine: DSL + Core + Lowering + Backends
 *
 * Features:
 * - Declarative scene graph (Group, Shape, Text, Image)
 * - Timeline animations with keyframes and easing
 * - State machine for logic control
 * - DSL parser for .flex files
 * - Arena allocator for zero-allocation performance
 *
 * Preferred modular usage:
 *   #include "flex/dsl.h"       // DSL frontend: lexer/parser/AST
 *   #include "flex/core.h"      // Core scene graph/animation/layout/binding
 *
 * Legacy aliases still work:
 *   #include "flex/compiler.h"
 *   #include "flex/runtime.h"
 */

#pragma once

// Modular headers
#include "flex/dsl.h"
#include "flex/core.h"

// Integration: lowering + backend-neutral renderer factory
#include "flex/lowering.h"
#include "flex/bridge/renderer.h"

// Standard library
#include <tlog.h>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <thread>
#include <variant>

namespace flex {

// ============================================================================
// Forward Declarations
// ============================================================================

// Forward declaration for lowering converter
class AstToRuntimeConverter;
namespace binary {
class BinaryCompiler;
}

// ============================================================================
// Definition - Immutable blueprint loaded from .flex file
// ============================================================================

class Definition {
  friend class Instance;
  friend class AstToRuntimeConverter; // In flex namespace
  friend class binary::BinaryCompiler;

public:
  using SharedPtr = std::shared_ptr<Definition>;
  using Ptr = SharedPtr;
  using InputValue = std::variant<float, std::string, bool>;
  using InputSchema = std::map<std::string, InputValue, std::less<>>;

  ~Definition() = default;

  // Load from .flex source
  static SharedPtr load(const char *source);
  static SharedPtr load_file(const char *path);

  // Load from .flexb binary (fast loading, no parsing)
  // Note: Use BinaryReader directly for more control (see flex/binary/reader.h)
  static SharedPtr load_binary(const char *path);
  static SharedPtr load_binary_data(const void *data, size_t size);

  // Load encrypted binary - NOT YET IMPLEMENTED
  // This method exists for future compatibility but currently not supported
  static SharedPtr load_binary_encrypted(const char *path, const char *password);

  // Check for parse errors
  bool has_error() const { return impl_->has_error; }
  const char *error_message() const { return impl_->error_message.c_str(); }
  int error_line() const { return impl_->error_line; }
  int error_column() const { return impl_->error_column; }

  // Access parsed objects (no more ast::Document)
  Scene::RawPtr scene() const { return impl_->scene; }
  const std::vector<Timeline::SharedPtr> &timelines() const { return impl_->timelines; }

  // Access executable core objects
  const std::vector<RuntimeStateMachine::SharedPtr> &machines() const {
    return impl_->machines;
  }

  // Runtime inputs declared with top-level `var`, including inferred types and defaults.
  const InputSchema &input_schema() const;

private:
  Definition() = default;
  struct Impl {
    // Arena allocator for this definition's objects
    ArenaAllocator object_alloc{1024 * 1024}; // 1MB

    Scene::RawPtr scene;
    std::vector<Timeline::SharedPtr> timelines;

    // Runtime objects
    std::vector<RuntimeStateMachine::SharedPtr> machines;

    // Runtime input declarations from top-level `var` statements. The variant
    // alternative is the schema type; each Instance owns a copy of the value.
    InputSchema parsed_variables;

    // Parsed assets from DSL
    struct ParsedAsset {
      std::string type;    // "audio", "image", "font", "svg"
      std::string id;
      std::string path;
      bool loop = false;
      float volume = 1.0f;
      bool preload = true;
    };
    std::vector<ParsedAsset> parsed_assets;

    struct ParsedNodeRef {
      const Impl *owner = nullptr;
      const std::string *node_id = nullptr;

      ParsedNodeRef() = default;
      ParsedNodeRef(const Impl *impl, const std::string *id)
          : owner(impl), node_id(id) {}

      Node::RawPtr get() const {
        if (!owner || !owner->scene || !node_id || node_id->empty()) {
          return nullptr;
        }
        return owner->scene->find(*node_id);
      }

      Node::RawPtr operator->() const { return get(); }
      operator Node::RawPtr() const { return get(); }
      explicit operator bool() const { return get() != nullptr; }
    };

    struct ParsedBinding {
      const Impl *owner = nullptr;
      std::string node_id;
      ParsedNodeRef target;
      std::string property;
      Binding binding;

      ParsedBinding() = default;
      ParsedBinding(const Impl *impl, std::string id, std::string prop, Binding bind)
          : owner(impl), node_id(std::move(id)), property(std::move(prop)), binding(std::move(bind)) {
        rebind_target();
      }

      ParsedBinding(const ParsedBinding &other)
          : owner(other.owner), node_id(other.node_id), property(other.property), binding(other.binding) {
        rebind_target();
      }

      ParsedBinding(ParsedBinding &&other) noexcept
          : owner(other.owner), node_id(std::move(other.node_id)),
            property(std::move(other.property)), binding(std::move(other.binding)) {
        rebind_target();
      }

      ParsedBinding &operator=(const ParsedBinding &other) {
        if (this == &other) {
          return *this;
        }
        owner = other.owner;
        node_id = other.node_id;
        property = other.property;
        binding = other.binding;
        rebind_target();
        return *this;
      }

      ParsedBinding &operator=(ParsedBinding &&other) noexcept {
        if (this == &other) {
          return *this;
        }
        owner = other.owner;
        node_id = std::move(other.node_id);
        property = std::move(other.property);
        binding = std::move(other.binding);
        rebind_target();
        return *this;
      }

    private:
      void rebind_target() { target = ParsedNodeRef(owner, &node_id); }
    };
    std::vector<ParsedBinding> bindings;

    struct ParsedComponentBinding {
      const Impl *owner = nullptr;
      std::string component_name;
      std::string node_id;
      ParsedNodeRef node;
      Component::SharedPtr component;
      Props base_props;
      std::map<std::string, Binding> prop_bindings;

      ParsedComponentBinding() = default;
      ParsedComponentBinding(const Impl *impl, std::string name, std::string id,
                             Component::SharedPtr comp, Props props,
                             std::map<std::string, Binding> bindings)
          : owner(impl), component_name(std::move(name)), node_id(std::move(id)),
            component(std::move(comp)), base_props(std::move(props)),
            prop_bindings(std::move(bindings)) {
        rebind_node();
      }

      ParsedComponentBinding(const ParsedComponentBinding &other)
          : owner(other.owner), component_name(other.component_name), node_id(other.node_id),
            component(other.component), base_props(other.base_props),
            prop_bindings(other.prop_bindings) {
        rebind_node();
      }

      ParsedComponentBinding(ParsedComponentBinding &&other) noexcept
          : owner(other.owner), component_name(std::move(other.component_name)),
            node_id(std::move(other.node_id)), component(std::move(other.component)),
            base_props(std::move(other.base_props)),
            prop_bindings(std::move(other.prop_bindings)) {
        rebind_node();
      }

      ParsedComponentBinding &operator=(const ParsedComponentBinding &other) {
        if (this == &other) {
          return *this;
        }
        owner = other.owner;
        component_name = other.component_name;
        node_id = other.node_id;
        component = other.component;
        base_props = other.base_props;
        prop_bindings = other.prop_bindings;
        rebind_node();
        return *this;
      }

      ParsedComponentBinding &operator=(ParsedComponentBinding &&other) noexcept {
        if (this == &other) {
          return *this;
        }
        owner = other.owner;
        component_name = std::move(other.component_name);
        node_id = std::move(other.node_id);
        component = std::move(other.component);
        base_props = std::move(other.base_props);
        prop_bindings = std::move(other.prop_bindings);
        rebind_node();
        return *this;
      }

    private:
      void rebind_node() { node = ParsedNodeRef(owner, &node_id); }
    };
    std::vector<ParsedComponentBinding> component_bindings;
    std::vector<ComponentNodePtr> component_instances;
    std::map<std::string, Component::SharedPtr> dsl_components;

    std::string error_message;
    int error_line = 0;
    int error_column = 0;
    bool has_error = false;
  };
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Instance - Mutable runtime state
// ============================================================================

class Instance : public IInstanceContext {
public:
  using SharedPtr = std::shared_ptr<Instance>;
  using Ptr = SharedPtr;

  ~Instance();

  // Create instance from definition
  static SharedPtr create(Definition::SharedPtr definition);

  // Create standalone instance (no definition)
  static SharedPtr create(float width, float height);

  // Create instance with custom arena allocator (optional)
  static SharedPtr create(float width, float height, ArenaAllocator &frame_alloc,
                    ArenaAllocator &object_alloc);

  // Get raw pointer (for TimelinePlayer)
  Instance *get_ptr() { return this; }

  // -------------------------------------------
  // Scene Access (IInstanceContext interface)
  // -------------------------------------------

  Scene::RawPtr scene() const override { return impl_->scene ; }

  // -------------------------------------------
  // Input Control (IInstanceContext interface)
  // -------------------------------------------

  void set_input(const char *name, float value) override;
  void set_input(const char *name, const char *value) override;
  void set_input(const char *name, bool value);
  float get_input(const char *name) const override;

  // -------------------------------------------
  // Frame Update
  // -------------------------------------------

  // Advance by delta time (seconds) - resets frame allocator
  void advance(float dt);

  // -------------------------------------------
  // Rendering
  // -------------------------------------------

  void render(Renderer &renderer);

  // -------------------------------------------
  // Event Handling (IInstanceContext interface)
  // -------------------------------------------

  void send_pointer_event(float x, float y, bool is_down);
  void send_key_event(KeyCode key, bool is_down, const KeyModifiers& modifiers = {},
                      bool repeat = false);
  void send_text_input(const char* utf8, bool from_ime = false);
  void send_composition_event(CompositionEventType type, const char* utf8,
                              int selection_start = -1, int selection_end = -1);
  bool request_focus(Node::RawPtr node,
                     FocusChangeReason reason = FocusChangeReason::Programmatic);
  void clear_focus(FocusChangeReason reason = FocusChangeReason::Clear);
  Node::RawPtr focused_node() const;
  void send_event(const char *name) override;

  // -------------------------------------------
  // Animation Control (IInstanceContext interface)
  // -------------------------------------------

  // Get animation controller
  AnimationController *animation_controller() const override;

  // Add a timeline
  void add_timeline(Timeline::SharedPtr timeline);

  // Play a timeline on a node
  TimelinePlayer *play(const char *timeline_name, Node *target) override;
  TimelinePlayer *play(const char *timeline_name) override; // Play on scene root

  // Stop animations
  void stop(const char *timeline_name) override;
  void stop_all() override;

  // -------------------------------------------
  // Runtime Systems Access (IInstanceContext interface)
  // -------------------------------------------

  // Get state machine by name
  RuntimeStateMachine *get_machine(const std::string &name) override;

  // Play animation by name (using Timeline system)
  TimelinePlayer* play_animation(const std::string &name);

  // Start animation - alias for play_animation
  void start_animation(const std::string &name);

  // Stop animation by name
  void stop_animation(const std::string &name);

  // Asset resolution (IInstanceContext interface)
  void register_asset(const char *name, const char *path);
  const char *resolve_asset(const char *name) const override;

  // -------------------------------------------
  // Asset Management (IInstanceContext interface)
  // -------------------------------------------

  AssetManager* asset_manager() const override;

  // Register assets
  void register_audio(const char* id, const char* path, bool loop = false, float volume = 1.0f);
  void register_image(const char* id, const char* path);
  void register_font(const char* id, const char* path);

  // Audio control
  int play_audio(const char* id);
  int play_audio(const char* id, bool loop, float volume);
  void stop_audio(const char* id);
  void stop_audio_channel(int channel);
  void set_audio_volume(int channel, float volume);
  bool is_audio_playing(int channel);

  // Preload all registered assets
  void preload_assets();

  // -------------------------------------------
  // Memory Pool Access
  // -------------------------------------------

  // Get frame allocator (for temporary allocations)
  ArenaAllocator *frame_allocator() const;

  // Get object allocator (for persistent allocations)
  ArenaAllocator *object_allocator() const;

  // Reset frame allocator (call at start of each frame)
  void reset_frame();

private:
  Instance();
  struct Impl {
    Definition::SharedPtr definition;
    Scene::RawPtr scene;
    float time = 0;

    // Arena allocators (based on memory_pool) - MUST be declared first!
    ArenaAllocator frame_alloc{256 * 1024};  // 256KB
    ArenaAllocator object_alloc{1024 * 1024}; // 1MB

    // Input values (Phase 2.3: Single map with variant)
    using InputValue = Definition::InputValue;
    std::unordered_map<Symbol, InputValue, SymbolHash> inputs;
    std::unordered_map<Symbol, std::string, SymbolHash> assets;

    // Data bindings
    std::unique_ptr<BindingContext> bindings;

    // Phase 2: Animation and State Machine
    AnimationController animation_controller{object_alloc};
    std::set<std::string> fired_events;

    // Runtime State Machine
    std::map<std::string, std::string> layer_states;

    // Runtime systems (Phase 3)
    std::vector<RuntimeStateMachine::SharedPtr> machines;

    // MIR programs keep mutable input slots and therefore belong to exactly
    // one runtime instance rather than the shared Definition.
    std::unordered_map<std::string, std::shared_ptr<void>> compiled_expressions;

    // Asset management (Phase 4)
    std::unique_ptr<class AssetManager> asset_manager;

    // Pointer state for event handling
    Node::RawPtr hover_node = nullptr;
    Node::RawPtr pointer_down_node = nullptr;
    Node::RawPtr focused_node = nullptr;
    bool is_pointer_down = false;
  };

  void set_input_value(const char *name, Impl::InputValue value);
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Parser API
// ============================================================================

namespace parser {

// Parse .flex source and build Runtime objects directly
// Returns nullptr on error - use get_error() for details
Scene::RawPtr parse(const char *source, std::vector<Timeline::SharedPtr> *out_timelines,
                    void *out_machine, // REMOVED: Machine::Ptr* out_machine (old system)
                    ArenaAllocator &alloc);

// Get last parse error
const char *get_error();
int get_error_line();
int get_error_column();

} // namespace parser

// ============================================================================
// Backend Initialization
// ============================================================================

// Backend modules are considered integration details. End-user code should
// prefer the backend-agnostic create_renderer(handle) entry point after the
// hosting application has registered and selected a default renderer factory.
// Backend-specific init/register headers remain available for in-repo or app
// integration code, but are not part of the intended public user surface.

} // namespace flex
