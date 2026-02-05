/*
 * flexUI - RenderManager Implementation
 */

#include <flexUI/render_manager.h>
#include <flexUI/element.h>
#include <flexUI/renderer.h>
#include <flexUI/computed_style.h>
#include <flexUI/widget.h>
 #include <flex/runtime/group.h>

namespace flexUI {

void RenderManager::render_tree(Element* root) {
  if (!root || !renderer_) return;
  render_element(root);
}

void RenderManager::render_element(Element* elem) {
  auto* style = elem->computed_style;
  if (!style || !elem->is_visible()) return;

  // Skip elements outside viewport (simple culling)
  float abs_x = elem->absolute_x();
  float abs_y = elem->absolute_y();
  Bounds vp = renderer_->flex().viewport();
  
  // If element is completely outside viewport, skip it and its children
  if (abs_x + elem->width() < vp.x || abs_x > vp.x + vp.width ||
      abs_y + elem->height() < vp.y || abs_y > vp.y + vp.height) {
    return;
  }

  auto& r = renderer_->flex();

  r.save();

  // Transform
  r.translate(elem->x() + style->transform_x, elem->y() + style->transform_y);

  if (style->transform_scale != 1.0f) {
    float cx = elem->width() / 2;
    float cy = elem->height() / 2;
    r.translate(cx, cy);
    r.scale(style->transform_scale, style->transform_scale);
    r.translate(-cx, -cy);
  }

  if (style->transform_rotate != 0.0f) {
    float cx = elem->width() / 2;
    float cy = elem->height() / 2;
    r.translate(cx, cy);
    r.rotate(style->transform_rotate * 180.0f / 3.14159265f);
    r.translate(-cx, -cy);
  }

  r.set_global_alpha(style->opacity);

  float radius = style->border_radius[0];

  // Shadow
  if (style->has_shadow && !style->shadow.inset) {
    auto& shadow = style->shadow;
    float sx = shadow.offset_x - shadow.spread_radius;
    float sy = shadow.offset_y - shadow.spread_radius;
    float sw = elem->width() + shadow.spread_radius * 2;
    float sh = elem->height() + shadow.spread_radius * 2;
    r.draw_rect(sx, sy, sw, sh, radius, Paint::solid(shadow.color), Paint::none(), 0);
  }

  // Background
  Color bg_color = style->get_variable_color("--bg", style->background_color);
  if (bg_color.a > 0) {
    r.draw_rect(0, 0, elem->width(), elem->height(), radius,
                Paint::solid(bg_color), Paint::none(), 0);
  }

  // Widget content
  if (elem->widget && renderer_) {
    elem->widget->render(*elem, *renderer_);
  }

  // Text
  if (!elem->text().empty()) {
    Color text_col = style->get_variable_color("--text-color", style->text_color);
    float text_x = style->padding[3];
    float text_y = (elem->height() + style->font_size) / 2;
    r.draw_text(elem->text(), text_x, text_y,
                style->font_family, style->font_size,
                style->font_weight >= FontWeight::Bold, text_col);
  }

  // Children
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      render_element(child);
    }
  }

  r.restore();

  elem->clear_dirty(flex::DirtyFlags::Visual);
}

void RenderManager::render_overlays(Element* elem) {
  if (!elem || !renderer_ || !elem->is_visible()) return;

  if (elem->widget && elem->widget->has_overlay()) {
    auto& r = renderer_->flex();
    r.save();
    r.translate(elem->absolute_x(), elem->absolute_y());
    elem->widget->render_overlay(*elem, *renderer_);
    r.restore();
  }

  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      render_overlays(child);
    }
  }
}

} // namespace flexUI
