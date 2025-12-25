/*
 * Flex Engine - Main Header
 *
 * Complete Flex Engine: Compiler + Runtime + Bridge
 *
 * Features:
 * - Declarative scene graph (Group, Shape, Text, Image)
 * - Timeline animations with keyframes and easing
 * - State machine for logic control
 * - DSL parser for .flex files
 * - Arena allocator for zero-allocation performance
 *
 * For modular usage:
 *   #include "flex/compiler.h"  // Only lexer/parser/AST
 *   #include "flex/runtime.h"   // Only scene graph/animation/rendering
 */

#pragma once

// Modular headers
#include "flex/compiler.h"
#include "flex/runtime.h"

// Bridge: AST to Runtime converter and renderer factory
#include "flex/bridge/ast_to_runtime.h"
#include "flex/bridge/renderer.h"

// Standard library
#include <fmtlog.h>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <thread>

namespace flex {

// ============================================================================
// Forward Declarations
// ============================================================================

// Forward declaration for bridge converter
class AstToRuntimeConverter;

// ============================================================================
// Definition - Immutable blueprint loaded from .flex file
// ============================================================================

class Definition {
  friend class Instance;
  friend class AstToRuntimeConverter; // In flex namespace

public:
  using Ptr = std::shared_ptr<Definition>;

  ~Definition() = default;

  // Load from .flex source
  static Ptr load(const char *source);
  static Ptr load_file(const char *path);

  // Load from .flexb binary (fast loading, no parsing)
  // Note: Use BinaryReader directly for more control (see flex/binary/reader.h)
  static Ptr load_binary(const char *path);
  static Ptr load_binary_data(const void *data, size_t size);

  // Load encrypted binary - NOT YET IMPLEMENTED
  // This method exists for future compatibility but currently not supported
  static Ptr load_binary_encrypted(const char *path, const char *password);

  // Check for parse errors
  bool has_error() const { return impl_->has_error; }
  const char *error_message() const { return impl_->error_message.c_str(); }
  int error_line() const { return impl_->error_line; }
  int error_column() const { return impl_->error_column; }

  // Access parsed objects (no more ast::Document)
  Artboard::Ptr artboard() const { return impl_->artboard; }
  const std::vector<Timeline::Ptr> &timelines() const { return impl_->timelines; }

  // Access runtime objects
  const std::vector<std::shared_ptr<class RuntimeStateMachine>> &machines() const {
    return impl_->machines;
  }

private:
  Definition() = default;
  struct Impl {
    // Arena allocator for this definition's objects
    ArenaAllocator object_alloc{64 * 1024}; // 64KB

    Artboard::Ptr artboard;
    std::vector<Timeline::Ptr> timelines;

    // Runtime objects
    std::vector<std::shared_ptr<class RuntimeStateMachine>> machines;

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
  using Ptr = std::shared_ptr<Instance>;

  ~Instance();

  // Create instance from definition
  static Ptr create(Definition::Ptr definition);

  // Create standalone instance (no definition)
  static Ptr create(float width, float height);

  // Create instance with custom arena allocator (optional)
  static Ptr create(float width, float height, ArenaAllocator &frame_alloc,
                    ArenaAllocator &object_alloc);

  // Get raw pointer (for TimelinePlayer)
  Instance *get_ptr() { return this; }

  // -------------------------------------------
  // Scene Access (IInstanceContext interface)
  // -------------------------------------------

  Artboard *artboard() const override { return impl_->artboard.get(); }

  // -------------------------------------------
  // Input Control (IInstanceContext interface)
  // -------------------------------------------

  void set_input(const char *name, float value) override;
  void set_input(const char *name, const char *value) override;
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
  void send_event(const char *name) override;

  // -------------------------------------------
  // Animation Control (IInstanceContext interface)
  // -------------------------------------------

  // Get animation controller
  AnimationController *animation_controller() const override;

  // Add a timeline
  void add_timeline(Timeline::Ptr timeline);

  // Play a timeline on a node
  TimelinePlayer *play(const char *timeline_name, Node *target) override;
  TimelinePlayer *play(const char *timeline_name) override; // Play on artboard root

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
    Definition::Ptr definition;
    Artboard::Ptr artboard;
    float time = 0;

    // Arena allocators (based on memory_pool) - MUST be declared first!
    ArenaAllocator frame_alloc{16 * 1024};  // 16KB
    ArenaAllocator object_alloc{64 * 1024}; // 64KB

    // Input values (Phase 2.3: Single map with variant)
    using InputValue = std::variant<float, std::string>;
    std::unordered_map<std::string, InputValue> inputs;
    std::unordered_map<std::string, std::string> assets;

    // Data bindings
    std::unique_ptr<BindingContext> bindings;

    // Script context for expression evaluation
    std::unique_ptr<ScriptContext> script_ctx;

    // Phase 2: Animation and State Machine
    AnimationController animation_controller{object_alloc};
    std::set<std::string> fired_events;

    // Runtime State Machine
    std::map<std::string, std::string> layer_states;

    // Runtime systems (Phase 3)
    std::vector<std::shared_ptr<class RuntimeStateMachine>> machines;

    // Asset management (Phase 4)
    std::unique_ptr<class AssetManager> asset_manager;

    // Pointer state for event handling
    std::weak_ptr<Node> hover_node;
    std::weak_ptr<Node> pointer_down_node;
    bool is_pointer_down = false;
  };
  std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Parser API
// ============================================================================

namespace parser {

// Parse .flex source and build Runtime objects directly
// Returns nullptr on error - use get_error() for details
Artboard::Ptr parse(const char *source, std::vector<Timeline::Ptr> *out_timelines,
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

// For backend-specific initialization, include the corresponding header:
// ThorVG:  #include "flex/backends/thorvg/init.h"
// NanoVG:  #include "flex/backends/nanovg/init.h" (planned)

} // namespace flex
