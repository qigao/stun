/*
 * Flex Engine - AST to Runtime Converter
 * Converts parsed AST objects to runtime objects
 */

#include "flex.h"
#include "flex/debug.h"
#include "flex/dsl/timeline.h"
#include "parser/flex_ast.h"
#include "parser/flex_parser.h"

namespace flex {

// ============================================================================
// Helper: Parse color from hex string
// ============================================================================

static Color parse_color_from_string(const std::string& color_str) {
  if (color_str.empty() || color_str[0] != '#') {
    return Color(0.0f, 0.0f, 0.0f, 1.0f);
  }

  const char* hex = color_str.c_str() + 1;
  size_t len = color_str.length() - 1;

  unsigned int r = 0, g = 0, b = 0, a = 255;

  if (len == 6) {
    sscanf(hex, "%02x%02x%02x", &r, &g, &b);
  } else if (len == 8) {
    sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a);
  } else if (len == 3) {
    unsigned int r4, g4, b4;
    sscanf(hex, "%1x%1x%1x", &r4, &g4, &b4);
    r = r4 * 17;
    g = g4 * 17;
    b = b4 * 17;
  }

  // Convert from 0-255 to 0-1 range for flex::Color (which uses floats)
  return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

// ============================================================================
// AST to Runtime Converter
// ============================================================================

class AstToRuntimeConverter {
public:
  AstToRuntimeConverter(Definition::Impl *definition_impl);
  ~AstToRuntimeConverter() = default;

  // Convert AST program to runtime objects
  void convert(const parser::AstProgram &program);

private:
  Definition::Impl *impl_;

  // Helper methods
  void convert_machines(const std::vector<std::shared_ptr<parser::AstMachine>> &machines);
  void convert_animations(const std::vector<std::shared_ptr<parser::AstAnim>> &animations);
  void convert_assets(const parser::AstAssets &assets);

  // Machine conversion
  RuntimeStateMachine::Ptr convert_machine(const parser::AstMachine &machine);
  void convert_layer(RuntimeStateMachine *machine, const parser::AstLayer &layer);

  // Animation conversion - now creates Timeline instead of RuntimeAnimation
  Timeline::Ptr convert_animation(const parser::AstAnim &anim);

  // Utility
  LoopMode parse_loop_mode(const std::string &mode_str);
};

inline AstToRuntimeConverter::AstToRuntimeConverter(Definition::Impl *definition_impl)
    : impl_(definition_impl) {}

inline void AstToRuntimeConverter::convert(const parser::AstProgram &program) {
  FLEX_LOGD("Converting AST to runtime objects...");

  // Convert machines
  if (!program.machines.empty()) {
    convert_machines(program.machines);
  }

  // Convert animations to Timelines
  if (!program.animations.empty()) {
    convert_animations(program.animations);
  }

  // Convert assets
  if (program.assets) {
    convert_assets(*program.assets);
  }

  FLEX_LOGD("Conversion complete!");
}

inline void AstToRuntimeConverter::convert_machines(
    const std::vector<std::shared_ptr<parser::AstMachine>> &machines) {
  FLEX_LOGD("Converting {} state machine(s)...", machines.size());

  for (const auto &ast_machine : machines) {
    auto runtime_machine = convert_machine(*ast_machine);
    impl_->machines.push_back(runtime_machine);

    FLEX_LOGD("Converted machine: {}", runtime_machine->name());

    // Convert layers
    for (const auto &ast_layer : ast_machine->layers) {
      convert_layer(runtime_machine.get(), ast_layer);
    }
  }
}

inline void AstToRuntimeConverter::convert_animations(
    const std::vector<std::shared_ptr<parser::AstAnim>> &animations) {
  FLEX_LOGD("Converting {} animation(s) to Timeline...", animations.size());

  impl_->timelines.clear();

  for (const auto &ast_anim : animations) {
    auto timeline = convert_animation(*ast_anim);
    impl_->timelines.push_back(timeline);

    FLEX_LOGD("Converted animation to Timeline: {}", timeline->name());
  }
}

inline RuntimeStateMachine::Ptr
AstToRuntimeConverter::convert_machine(const parser::AstMachine &machine) {
  auto runtime_machine = std::make_shared<RuntimeStateMachine>(machine.name);

  // Layers will be added in convert_layer

  return runtime_machine;
}

inline void AstToRuntimeConverter::convert_layer(RuntimeStateMachine *machine,
                                                 const parser::AstLayer &layer) {
  FLEX_LOGD("  Converting layer: {}", layer.name);

  machine->add_layer(layer.name);
  auto runtime_layer = machine->get_layer(layer.name);

  // Convert states
  for (const auto &ast_state : layer.states) {
    runtime_layer->add_state(ast_state.name, ast_state.initial, ast_state.animation,
                             ast_state.play_audio, ast_state.stop_audio);
    FLEX_LOGD("    Added state: {}{}{}{}{}", ast_state.name,
              ast_state.initial ? " (initial)" : "",
              ast_state.animation.empty() ? "" : " animation=\"" + ast_state.animation + "\"",
              ast_state.play_audio.empty() ? "" : " play=\"" + ast_state.play_audio + "\"",
              ast_state.stop_audio.empty() ? "" : " stop=\"" + ast_state.stop_audio + "\"");
  }

  // Convert transitions
  for (const auto &ast_trans : layer.transitions) {
    runtime_layer->add_transition(ast_trans.from_state, ast_trans.to_state, ast_trans.condition_var,
                                  ast_trans.condition_op, ast_trans.condition_val);

    if (!ast_trans.condition_var.empty()) {
      FLEX_LOGD("    Added transition: {} -> {} when {} {} {}",
                ast_trans.from_state, ast_trans.to_state,
                ast_trans.condition_var, ast_trans.condition_op, ast_trans.condition_val);
    } else {
      FLEX_LOGD("    Added transition: {} -> {}", ast_trans.from_state, ast_trans.to_state);
    }
  }
}

inline Timeline::Ptr AstToRuntimeConverter::convert_animation(const parser::AstAnim &anim) {
  // Create Timeline using Definition's arena allocator
  auto timeline = Timeline::create(anim.name.c_str(), impl_->object_alloc);

  // Set properties
  timeline->set_duration(anim.duration);
  timeline->set_loop_mode(parse_loop_mode(anim.loop_mode));

  // Convert tracks
  for (const auto &ast_track : anim.tracks) {
    auto track = timeline->add_track(ast_track.property.c_str());

    // Add keyframes
    for (const auto &ast_kf : ast_track.keyframes) {
      if (auto *fval = std::get_if<float>(&ast_kf.value)) {
        track->add_keyframe(ast_kf.time, *fval);
      } else if (auto *sval = std::get_if<std::string>(&ast_kf.value)) {
        // Check if it's a color string
        if (!sval->empty() && (*sval)[0] == '#') {
          Color color = parse_color_from_string(*sval);
          track->add_keyframe(ast_kf.time, color);
        } else {
          track->add_keyframe(ast_kf.time, sval->c_str());
        }
      } else if (auto *bval = std::get_if<bool>(&ast_kf.value)) {
        track->add_keyframe(ast_kf.time, *bval ? 1.0f : 0.0f);
      }
    }
  }

  return timeline;
}

inline LoopMode AstToRuntimeConverter::parse_loop_mode(const std::string &mode_str) {
  if (mode_str == "loop") {
    return LoopMode::Loop;
  } else if (mode_str == "pingpong") {
    return LoopMode::PingPong;
  }
  return LoopMode::Once;
}

inline void AstToRuntimeConverter::convert_assets(const parser::AstAssets &assets) {
  FLEX_LOGD("Converting {} asset(s)...", assets.assets.size());

  impl_->parsed_assets.clear();

  for (const auto &ast_asset : assets.assets) {
    Definition::Impl::ParsedAsset parsed;
    parsed.type = ast_asset.type;
    parsed.id = ast_asset.id;
    parsed.path = ast_asset.path;

    // Extract options
    for (const auto &[key, value] : ast_asset.options) {
      if (key == "loop") {
        if (auto *bval = std::get_if<bool>(&value)) {
          parsed.loop = *bval;
        }
      } else if (key == "volume") {
        if (auto *fval = std::get_if<float>(&value)) {
          parsed.volume = *fval;
        }
      } else if (key == "preload") {
        if (auto *bval = std::get_if<bool>(&value)) {
          parsed.preload = *bval;
        }
      }
    }

    impl_->parsed_assets.push_back(parsed);
    if (parsed.type == "audio") {
      FLEX_LOGD("  Asset: {} {} = \"{}\" (loop={}, volume={})",
                parsed.type, parsed.id, parsed.path,
                parsed.loop ? "true" : "false", parsed.volume);
    } else {
      FLEX_LOGD("  Asset: {} {} = \"{}\"", parsed.type, parsed.id, parsed.path);
    }
  }
}

} // namespace flex
