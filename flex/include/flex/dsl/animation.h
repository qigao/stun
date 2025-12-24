/*
 * Flex Engine - Runtime Animation System
 * Converts AST animations to runtime timeline objects
 */

#pragma once

#include "flex/node.h"
#include "flex/types.h"
#include "parser/flex_ast.h"
#include "shape.h"
#include "text.h"
#include <algorithm>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace flex {

struct EasingFunction {
  EasingType type = EasingType::Linear;
  float bezier_x1 = 0.0f;
  float bezier_y1 = 0.0f;
  float bezier_x2 = 1.0f;
  float bezier_y2 = 1.0f;

  float ease(float t) const {
    switch (type) {
    case EasingType::Linear:
      return t;

    case EasingType::EaseIn:
      return t * t * t;

    case EasingType::EaseOut:
      t = t - 1.0f;
      return t * t * t + 1.0f;

    case EasingType::EaseInOut:
      if (t < 0.5f) {
        return 4.0f * t * t * t;
      } else {
        t = t - 1.0f;
        return (t * t * t * 4.0f) + 1.0f;
      }

    case EasingType::CubicBezier: {
      // Cubic Bezier easing function using control points
      // Control points define the shape of the easing curve
      // Default: (0.25, 0.1, 0.25, 1.0) - ease-in-out
      // This uses the standard cubic-bezier formula with Newton-Raphson iteration

      // Convert to cubic Bezier curve parameters
      // For easing functions, we use a parametric curve where x represents time
      // and y represents the eased value

      // Use Newton-Raphson method to find the parameter that gives us the desired x
      float x = t;
      float x1 = bezier_x1;
      float x2 = bezier_x2;

      // For standard easing functions, x1 and x2 are typically in [0,1]
      // If they're outside [0,1], we need to handle extrapolation

      // Initial guess: use t directly
      float param = t;

      // Newton-Raphson iteration (max 8 iterations for performance)
      for (int i = 0; i < 8; i++) {
        // Calculate the cubic Bezier at current parameter
        float x_t = (1 - param) * (1 - param) * (1 - param) * 0 +
                    3 * (1 - param) * (1 - param) * param * x1 +
                    3 * (1 - param) * param * param * x2 + param * param * param * 1;

        // Calculate derivative (slope)
        float dx_dt = 3 * (1 - param) * (1 - param) * (x1 - 0) +
                      6 * (1 - param) * param * (x2 - x1) + 3 * param * param * (1 - x2);

        // If derivative is too small, break to avoid division by zero
        if (std::abs(dx_dt) < 1e-6f)
          break;

        // Newton-Raphson step
        float delta = (x_t - x) / dx_dt;
        param -= delta;

        // Clamp parameter to valid range
        if (param < 0)
          param = 0;
        if (param > 1)
          param = 1;

        // If converged, break
        if (std::abs(delta) < 1e-6f)
          break;
      }

      // Now calculate y value at the converged parameter
      float y_t = (1 - param) * (1 - param) * (1 - param) * 0 +
                  3 * (1 - param) * (1 - param) * param * bezier_y1 +
                  3 * (1 - param) * param * param * bezier_y2 + param * param * param * 1;

      return y_t;
    }

    default:
      return t;
    }
  }
};

// ============================================================================
// Color Parsing Helper
// ============================================================================

inline uint32_t parse_color_string(const std::string &color_str) {
  if (color_str.empty() || color_str[0] != '#')
    return 0x000000FF;

  const char *hex = color_str.c_str() + 1;
  size_t len = color_str.length() - 1;

  uint32_t r = 0, g = 0, b = 0, a = 255;

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

  return (r << 24) | (g << 16) | (b << 8) | a;
}

// ============================================================================
// Runtime Keyframe
// ============================================================================

class RuntimeKeyframe {
public:
  RuntimeKeyframe(float time, const parser::AstValue &value) : time_(time), value_(value) {}

  float time() const { return time_; }
  const parser::AstValue &value() const { return value_; }

private:
  float time_;
  parser::AstValue value_;
};

// ============================================================================
// Runtime Track
// ============================================================================

class RuntimeTrack {
public:
  RuntimeTrack(const std::string &property);
  ~RuntimeTrack() = default;

  // Add a keyframe
  void add_keyframe(float time, const parser::AstValue &value);

  // Evaluate track at given time
  void evaluate(float time, Node *target_node, const EasingFunction &easing) const;

  const std::string &property() const { return property_; }
  const std::vector<RuntimeKeyframe> &keyframes() const { return keyframes_; }
  std::vector<RuntimeKeyframe> &keyframes() {
    return keyframes_;
  } // Non-const version for AnimationManager

private:
  std::string property_;
  std::vector<RuntimeKeyframe> keyframes_;

  // Helper methods
  void apply_value(Node *node, const std::string &property, const parser::AstValue &value) const;
  void apply_transform_value(Node *node, const std::string &property, float value) const;
  void apply_visual_value(Node *node, const std::string &property,
                          const parser::AstValue &value) const;
  void apply_text_value(Node *node, const std::string &property,
                        const parser::AstValue &value) const;
};

// ============================================================================
// Runtime Animation
// ============================================================================

class RuntimeAnimation {
public:
  using Ptr = std::shared_ptr<RuntimeAnimation>;

  RuntimeAnimation(const std::string &name);
  ~RuntimeAnimation() = default;

  // Configuration
  void set_duration(float duration) { duration_ = duration; }
  void set_loop_mode(const std::string &mode) { loop_mode_ = mode; }

  // Set root node for finding targets by ID (e.g., #statusText)
  void set_target_root(Node *root) { target_root_ = root; }
  Node *target_root() const { return target_root_; }

  // Add a track
  void add_track(const std::string &property);

  // Get track by property path
  RuntimeTrack *get_track(const std::string &property);

  // Start animation
  void start();

  // Update animation
  void update(float dt);

  // Check if animation is running
  bool is_running() const { return is_running_; }

  // Check if animation is finished
  bool is_finished() const;

  // Stop the animation
  void stop() { is_running_ = false; }

  const std::string &name() const { return name_; }
  float duration() const { return duration_; }
  const std::string &loop_mode() const { return loop_mode_; }

private:
  std::string name_;
  float duration_ = 0.0f;
  std::string loop_mode_ = "once";

  float current_time_ = 0.0f;
  bool is_running_ = false;
  int loop_count_ = 0;

  std::unordered_map<std::string, std::unique_ptr<RuntimeTrack>> tracks_;
  EasingFunction easing_;
  Node *target_root_ = nullptr; // Root node for finding targets

  // Helper methods
  void apply_keyframes(float time);
};

// ============================================================================
// Animation Manager
// ============================================================================

class AnimationManager {
public:
  AnimationManager();
  ~AnimationManager() = default;

  // Create animation from AST
  RuntimeAnimation::Ptr create_animation(const parser::AstAnim &ast_anim);

  // Get animation by name
  RuntimeAnimation *get_animation(const std::string &name);

  // Start animation
  void start_animation(const std::string &name);

  // Stop animation
  void stop_animation(const std::string &name);

  // Update all running animations
  void update(float dt);

  const std::unordered_map<std::string, RuntimeAnimation::Ptr> &animations() const {
    return animations_;
  }

private:
  std::unordered_map<std::string, RuntimeAnimation::Ptr> animations_;
};

// ============================================================================
// Implementation
// ============================================================================

// ---------------------------------------------------------------------------
// RuntimeTrack
// ---------------------------------------------------------------------------

inline RuntimeTrack::RuntimeTrack(const std::string &property) : property_(property) {}

inline void RuntimeTrack::add_keyframe(float time, const parser::AstValue &value) {
  keyframes_.emplace_back(time, value);

  // Sort keyframes by time
  std::sort(keyframes_.begin(), keyframes_.end(),
            [](const RuntimeKeyframe &a, const RuntimeKeyframe &b) { return a.time() < b.time(); });
}

inline void RuntimeTrack::evaluate(float time, Node *target_node,
                                   const EasingFunction &easing) const {
  if (keyframes_.empty() || !target_node)
    return;

  // Clamp time to duration
  float max_time = keyframes_.back().time();
  float clamped_time = std::min(time, max_time);

  // Find surrounding keyframes
  const RuntimeKeyframe *prev = nullptr;
  const RuntimeKeyframe *next = nullptr;

  for (size_t i = 0; i < keyframes_.size(); ++i) {
    if (keyframes_[i].time() <= clamped_time) {
      prev = &keyframes_[i];
    }
    if (keyframes_[i].time() >= clamped_time && !next) {
      next = &keyframes_[i];
      break;
    }
  }

  // Interpolate value
  if (prev && next && prev->time() == next->time()) {
    // Same time - use that value
    apply_value(target_node, property_, prev->value());
  } else if (prev && next) {
    // Interpolate
    float t = (clamped_time - prev->time()) / (next->time() - prev->time());
    t = easing.ease(t);

    // Simple linear interpolation for numeric values
    if (std::holds_alternative<float>(prev->value()) &&
        std::holds_alternative<float>(next->value())) {
      float prev_val = std::get<float>(prev->value());
      float next_val = std::get<float>(next->value());
      float interpolated = prev_val + (next_val - prev_val) * t;

      parser::AstValue result(interpolated);
      apply_value(target_node, property_, result);
    } else if (std::holds_alternative<std::string>(prev->value()) &&
               std::holds_alternative<std::string>(next->value())) {
      // For strings, interpolate only at end
      if (t >= 0.99f) {
        apply_value(target_node, property_, next->value());
      } else {
        apply_value(target_node, property_, prev->value());
      }
    }
  } else if (prev) {
    // Use prev value
    apply_value(target_node, property_, prev->value());
  }
}

inline void RuntimeTrack::apply_value(Node *node, const std::string &property,
                                      const parser::AstValue &value) const {
  // Handle property paths like "x", "opacity", "fill", "#nodeId/x"
  if (property[0] == '#') {
    // Path-based property - parse and find target node
    size_t slash_pos = property.find('/');
    if (slash_pos != std::string::npos) {
      std::string node_id = property.substr(1, slash_pos - 1);
      std::string prop_name = property.substr(slash_pos + 1);

      Node *target = node->find(node_id);
      if (target) {
        apply_value(target, prop_name, value);
      }
    }
  } else {
    // Direct property
    if (property == "x" || property == "y" || property == "scale" || property == "scaleX" ||
        property == "scaleY" || property == "rotation") {
      if (std::holds_alternative<float>(value)) {
        apply_transform_value(node, property, std::get<float>(value));
      }
    } else if (property == "opacity") {
      if (std::holds_alternative<float>(value)) {
        apply_visual_value(node, property, value);
      }
    } else if (property == "fill" || property == "stroke") {
      apply_visual_value(node, property, value);
    } else if (property == "content" || property == "color" || property == "text.color" ||
               property == "fontSize") {
      apply_text_value(node, property, value);
    }
  }
}

inline void RuntimeTrack::apply_transform_value(Node *node, const std::string &property,
                                                float value) const {
  if (property == "x") {
    node->set_x(value);
  } else if (property == "y") {
    node->set_y(value);
  } else if (property == "scale") {
    node->set_scale(value, value);
  } else if (property == "scaleX") {
    node->set_scale(value, node->scale_y());
  } else if (property == "scaleY") {
    node->set_scale(node->scale_x(), value);
  } else if (property == "rotation") {
    node->set_rotation(value);
  }
}

inline void RuntimeTrack::apply_visual_value(Node *node, const std::string &property,
                                             const parser::AstValue &value) const {
  if (auto *shape = dynamic_cast<Shape *>(node)) {
    if (property == "opacity") {
      if (std::holds_alternative<float>(value)) {
        node->set_opacity(std::get<float>(value));
      }
    } else if (property == "fill") {
      if (std::holds_alternative<std::string>(value)) {
        // Parse color string (simplified)
        // TODO: Implement proper color parsing
      }
    }
  }
}

inline void RuntimeTrack::apply_text_value(Node *node, const std::string &property,
                                           const parser::AstValue &value) const {
  if (auto *text = dynamic_cast<Text *>(node)) {
    if (property == "content" && std::holds_alternative<std::string>(value)) {
      text->set_content(std::get<std::string>(value));
      std::cout << "[Animation] Set text content: " << std::get<std::string>(value) << "\n";
    } else if (property == "color" || property == "text.color") {
      if (std::holds_alternative<std::string>(value)) {
        // Parse color from hex string like "#ff0000"
        const std::string &color_str = std::get<std::string>(value);
        if (!color_str.empty() && color_str[0] == '#') {
          uint32_t color = parse_color_string(color_str);
          text->set_color(color);
          std::cout << "[Animation] Set text color: " << color_str << "\n";
        }
      }
    } else if (property == "fontSize" && std::holds_alternative<float>(value)) {
      text->set_font_size(std::get<float>(value));
    }
  }
}

// ---------------------------------------------------------------------------
// RuntimeAnimation
// ---------------------------------------------------------------------------

inline RuntimeAnimation::RuntimeAnimation(const std::string &name) : name_(name) {}

inline void RuntimeAnimation::start() {
  is_running_ = true;
  current_time_ = 0.0f;
  loop_count_ = 0;
  std::cout << "[Animation] Started: " << name_ << "\n";
}

inline void RuntimeAnimation::update(float dt) {
  if (!is_running_)
    return;

  current_time_ += dt;

  if (current_time_ >= duration_) {
    if (loop_mode_ == "once") {
      is_running_ = false;
      std::cout << "[Animation] Finished: " << name_ << "\n";
    } else if (loop_mode_ == "loop") {
      loop_count_++;
      current_time_ = 0.0f;
    } else if (loop_mode_ == "pingpong") {
      // TODO: Implement ping-pong
      current_time_ = 0.0f;
    }
  }

  // Apply keyframes at current time
  apply_keyframes(current_time_);
}

inline bool RuntimeAnimation::is_finished() const {
  return !is_running_ && current_time_ >= duration_;
}

inline RuntimeTrack *RuntimeAnimation::get_track(const std::string &property) {
  auto it = tracks_.find(property);
  return (it != tracks_.end()) ? it->second.get() : nullptr;
}

inline void RuntimeAnimation::add_track(const std::string &property) {
  tracks_[property] = std::make_unique<RuntimeTrack>(property);
}

inline void RuntimeAnimation::apply_keyframes(float time) {
  if (!target_root_)
    return;

  // Apply all tracks - pass root node, let track handle path resolution
  for (const auto &[prop_name, track] : tracks_) {
    track->evaluate(time, target_root_, easing_);
  }
}

// ---------------------------------------------------------------------------
// AnimationManager
// ---------------------------------------------------------------------------

inline AnimationManager::AnimationManager() {}

inline RuntimeAnimation::Ptr AnimationManager::create_animation(const parser::AstAnim &ast_anim) {
  auto animation = std::make_shared<RuntimeAnimation>(ast_anim.name);
  animation->set_duration(ast_anim.duration);
  animation->set_loop_mode(ast_anim.loop_mode);

  // Create tracks
  for (const auto &ast_track : ast_anim.tracks) {
    auto track = std::make_unique<RuntimeTrack>(ast_track.property);

    // Add keyframes
    for (const auto &ast_keyframe : ast_track.keyframes) {
      track->add_keyframe(ast_keyframe.time, ast_keyframe.value);
    }

    animation->add_track(ast_track.property);
    animation->get_track(ast_track.property)->keyframes() = std::move(track->keyframes());
  }

  animations_[ast_anim.name] = animation;
  return animation;
}

inline RuntimeAnimation *AnimationManager::get_animation(const std::string &name) {
  auto it = animations_.find(name);
  return (it != animations_.end()) ? it->second.get() : nullptr;
}

inline void AnimationManager::start_animation(const std::string &name) {
  auto anim = get_animation(name);
  if (anim) {
    anim->start();
  }
}

inline void AnimationManager::stop_animation(const std::string &name) {
  auto anim = get_animation(name);
  if (anim) {
    anim->stop();
  }
}

inline void AnimationManager::update(float dt) {
  for (const auto &[name, animation] : animations_) {
    if (animation->is_running()) {
      animation->update(dt);
    }
  }
}

} // namespace flex
