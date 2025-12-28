/*
 * flexUI - Element Implementation
 */

#include "flexUI/element.h"
#include "flexUI/box.h"
#include <flex/bridge/renderer.h>

namespace flexUI {

Element* Element::create(flex::ArenaAllocator& arena, Symbol tag, const std::string& id) {
  auto* elem = arena.create<Element>(tag);
  if (!id.empty()) {
    elem->set_element_id(id);
  }
  return elem;
}

Element* Element::create(flex::ArenaAllocator& arena, const std::string& tag, const std::string& id) {
  auto* elem = arena.create<Element>(Symbol(tag));
  elem->tag_str_ = tag;  // Store the tag string
  if (!id.empty()) {
    elem->set_element_id(id);
  }
  return elem;
}

void Element::add_class(Symbol cls) {
  if (classes_.insert(cls).second) {
    mark_style_dirty();
  }
}

void Element::remove_class(Symbol cls) {
  if (classes_.erase(cls) > 0) {
    mark_style_dirty();
  }
}

void Element::set_state(Symbol state, bool active) {
  if (active) {
    if (pseudo_states_.insert(state).second) {
      mark_paint_dirty();
    }
  } else {
    if (pseudo_states_.erase(state) > 0) {
      mark_paint_dirty();
    }
  }
}

void Element::set_text(const std::string& text) {
  if (text_content_ == text) return;
  text_content_ = text;
  mark_layout_dirty();
}

void Element::render(flex::Renderer& renderer) {
  if (!is_visible()) return;

  // Widget 渲染由 Box::paint_element 处理
  // 这里只处理子元素渲染

  renderer.save();
  renderer.set_transform(world_transform());

  if (opacity() < 1.0f) {
    renderer.set_global_alpha(opacity());
  }

  // 如果有裁剪
  if (clip() && layout_width() > 0 && layout_height() > 0) {
    renderer.clip_rect(0, 0, layout_width(), layout_height());
  }

  // 渲染子元素
  for (auto* child : children()) {
    child->render(renderer);
  }

  renderer.restore();
}

} // namespace flexUI
