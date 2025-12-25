/*
 * Flex Engine - Component State Machine Implementation
 */

#include "flex/runtime/fsm.h"
#include "flex/runtime/group.h"
#include "flex/runtime/shape.h"
#include "flex/runtime/text.h"
#include "flex/runtime/node.h"
#include "tinyfsm.hpp"
#include <iostream>

namespace flex {

// ============================================================================
// PseudoClassStyle Implementation
// ============================================================================

void PseudoClassStyle::apply_to(Node *node) const {
  if (!node)
    return;

  for (const auto &prop : properties_) {
    const std::string &path = prop.path;
    const PropertyValue &value = prop.value;

    // Parse property path: "bg.fill" or "scale" or "label.content"
    size_t dot_pos = path.find('.');

    Node *target_node = node;
    std::string prop_name = path;

    // If path has dot, find child node
    if (dot_pos != std::string::npos) {
      std::string child_id = path.substr(0, dot_pos);
      prop_name = path.substr(dot_pos + 1);
      target_node = node->find(child_id);
      if (!target_node) {
        // Silently skip missing nodes (allows partial component definitions)
        continue;
      }
    }

    // Apply property based on name and type
    // Common node properties
    if (prop_name == "x" && value.is<float>()) {
      target_node->set_x(value.get<float>());
    } else if (prop_name == "y" && value.is<float>()) {
      target_node->set_y(value.get<float>());
    } else if (prop_name == "scale" && value.is<float>()) {
      float s = value.get<float>();
      target_node->set_scale(s, s);
    } else if (prop_name == "scaleX" && value.is<float>()) {
      target_node->set_scale(value.get<float>(), target_node->scale_y());
    } else if (prop_name == "scaleY" && value.is<float>()) {
      target_node->set_scale(target_node->scale_x(), value.get<float>());
    } else if (prop_name == "opacity" && value.is<float>()) {
      target_node->set_opacity(value.get<float>());
    } else if (prop_name == "rotation" && value.is<float>()) {
      target_node->set_rotation(value.get<float>());
    } else if (prop_name == "visible" && value.is<bool>()) {
      target_node->set_visible(value.get<bool>());
    }
    // Shape-specific properties
    else if (prop_name == "fill" && value.is<Color>()) {
      if (auto *shape = dynamic_cast<Shape *>(target_node)) {
        shape->set_fill(value.get<Color>());
      }
    } else if (prop_name == "stroke" && value.is<Color>()) {
      if (auto *shape = dynamic_cast<Shape *>(target_node)) {
        shape->set_stroke(value.get<Color>());
      }
    }
    // Note: strokeWidth and radius are not separately settable in Shape API
    // - strokeWidth: use stroke color + width together
    // - radius (for circles): use set_circle(radius)
    // - corner_radius (for rects): use set_rect(width, height, corner_radius)
    // For now, these properties are not supported in pseudo-class styles
    // Text-specific properties
    else if (prop_name == "content" && value.is<std::string>()) {
      if (auto *text = dynamic_cast<Text *>(target_node)) {
        text->set_content(value.get<std::string>());
      }
    } else if (prop_name == "color" && value.is<Color>()) {
      if (auto *text = dynamic_cast<Text *>(target_node)) {
        text->set_color(value.get<Color>());
      }
    } else if (prop_name == "fontSize" && value.is<float>()) {
      if (auto *text = dynamic_cast<Text *>(target_node)) {
        text->set_font_size(value.get<float>());
      }
    }
  }
}

// ============================================================================
// ComponentFsm Template Implementation
// ============================================================================

template <typename Derived> void ComponentFsm<Derived>::apply_dsl_style(const char *pseudo_class) {
  if (!owner)
    return;

  // Look up pseudo-class style in owner node's style map
  auto *styles = owner->pseudo_class_styles();
  if (!styles)
    return;

  auto it = styles->find(pseudo_class);
  if (it != styles->end()) {
    it->second.apply_to(owner);
  }
}

template <typename Derived> Node *ComponentFsm<Derived>::find(const std::string &id) {
  return owner ? owner->find(id) : nullptr;
}

// Explicit template instantiation will be added as needed

} // namespace flex
