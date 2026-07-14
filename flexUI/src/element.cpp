/*
 * flexUI - Element Implementation
 */

#include "flexUI/element.h"
#include "flexUI/box.h"
#include "flexUI/render_command.h"
#include "flexUI/renderer.h"
#include <flex/bridge/renderer.h>
#include <cctype>
#include <stdexcept>

namespace flexUI {

namespace {

void mark_selector_scope_dirty(Element* elem) {
  if (!elem) {
    return;
  }
  elem->mark_style_dirty();
  if (auto* parent = elem->parent_elem()) {
    parent->mark_style_dirty();
  }
}

void emit_element_render_commands(Element& elem, RenderCommandList& commands,
                                  const flex::Transform& parent_transform) {
  if (!elem.is_visible()) return;

  using flex::operator*;
  const flex::Transform local_transform =
      parent_transform * elem.local_transform();

  commands.save();
  commands.set_transform(local_transform);

  if (elem.opacity() < 1.0f) {
    commands.set_global_alpha(elem.opacity());
  }

  if (elem.clip() && elem.layout_width() > 0 && elem.layout_height() > 0) {
    commands.clip_rect(0, 0, elem.layout_width(), elem.layout_height());
  }

  for (auto* child : elem.children()) {
    if (child) {
      emit_element_render_commands(*static_cast<Element*>(child), commands,
                                   local_transform);
    }
  }

  commands.restore();
}

} // namespace

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
  const bool source_added = symbol_only_classes_.insert(cls).second;
  const bool selector_added = classes_.insert(cls).second;
  if (source_added || selector_added) {
    mark_selector_scope_dirty(this);
    if (owner_box_) {
      owner_box_->notify_utility_tree_changed();
    }
  }
}

void Element::add_class(const char* cls) {
  if (cls) {
    add_class(std::string(cls));
  }
}

void Element::add_class(const std::string& cls) {
  if (cls.empty() || !class_names_.insert(cls).second) {
    return;
  }
  classes_.insert(Symbol(cls));
  sync_class_attribute();
  mark_selector_scope_dirty(this);
  if (owner_box_) {
    owner_box_->notify_utility_tree_changed();
  }
}

void Element::add_classes(std::string_view class_list) {
  bool changed = false;
  size_t cursor = 0;
  while (cursor < class_list.size()) {
    while (cursor < class_list.size() &&
           std::isspace(static_cast<unsigned char>(class_list[cursor]))) {
      ++cursor;
    }
    const size_t start = cursor;
    while (cursor < class_list.size() &&
           !std::isspace(static_cast<unsigned char>(class_list[cursor]))) {
      ++cursor;
    }
    if (start != cursor) {
      std::string token(class_list.substr(start, cursor - start));
      if (class_names_.insert(token).second) {
        classes_.insert(Symbol(token));
        changed = true;
      }
    }
  }
  if (!changed) {
    return;
  }
  sync_class_attribute();
  mark_selector_scope_dirty(this);
  if (owner_box_) {
    owner_box_->notify_utility_tree_changed();
  }
}

void Element::set_classes(std::string_view class_list) {
  std::unordered_set<std::string> next_names;
  size_t cursor = 0;
  while (cursor < class_list.size()) {
    while (cursor < class_list.size() &&
           std::isspace(static_cast<unsigned char>(class_list[cursor]))) {
      ++cursor;
    }
    const size_t start = cursor;
    while (cursor < class_list.size() &&
           !std::isspace(static_cast<unsigned char>(class_list[cursor]))) {
      ++cursor;
    }
    if (start != cursor) {
      next_names.emplace(class_list.substr(start, cursor - start));
    }
  }
  if (next_names == class_names_) {
    return;
  }

  class_names_ = std::move(next_names);
  classes_ = symbol_only_classes_;
  for (const auto& name : class_names_) {
    classes_.insert(Symbol(name));
  }
  sync_class_attribute();
  mark_selector_scope_dirty(this);
  if (owner_box_) {
    owner_box_->notify_utility_tree_changed();
  }
}

void Element::toggle_class(const std::string& cls, bool enabled) {
  if (enabled) {
    add_class(cls);
  } else {
    remove_class(cls);
  }
}

bool Element::replace_class(const std::string& old_class,
                            const std::string& new_class) {
  if (old_class.empty() || new_class.empty() ||
      class_names_.count(old_class) == 0) {
    return false;
  }
  auto next_names = class_names_;
  next_names.erase(old_class);
  next_names.insert(new_class);
  if (next_names == class_names_) {
    return false;
  }
  class_names_ = std::move(next_names);
  classes_ = symbol_only_classes_;
  for (const auto& name : class_names_) {
    classes_.insert(Symbol(name));
  }
  sync_class_attribute();
  mark_selector_scope_dirty(this);
  if (owner_box_) {
    owner_box_->notify_utility_tree_changed();
  }
  return true;
}

void Element::remove_class(Symbol cls) {
  const bool source_removed = symbol_only_classes_.erase(cls) > 0;
  const auto previous_name_count = class_names_.size();
  for (auto it = class_names_.begin(); it != class_names_.end();) {
    if (Symbol(*it) == cls) {
      it = class_names_.erase(it);
    } else {
      ++it;
    }
  }
  const bool named_source_removed = class_names_.size() != previous_name_count;
  const bool selector_removed = classes_.erase(cls) > 0;
  if (source_removed || named_source_removed || selector_removed) {
    sync_class_attribute();
    mark_selector_scope_dirty(this);
    if (owner_box_) {
      owner_box_->notify_utility_tree_changed();
    }
  }
}

void Element::remove_class(const char* cls) {
  if (cls) {
    remove_class(std::string(cls));
  }
}

void Element::remove_class(const std::string& cls) {
  if (cls.empty() || class_names_.erase(cls) == 0) {
    return;
  }

  const Symbol symbol(cls);
  const bool has_named_source = std::any_of(
      class_names_.begin(), class_names_.end(),
      [symbol](const std::string& name) { return Symbol(name) == symbol; });
  if (!has_named_source && symbol_only_classes_.count(symbol) == 0) {
    classes_.erase(symbol);
  }
  sync_class_attribute();
  mark_selector_scope_dirty(this);
  if (owner_box_) {
    owner_box_->notify_utility_tree_changed();
  }
}

void Element::sync_class_attribute() {
  std::vector<std::string> ordered(class_names_.begin(), class_names_.end());
  std::sort(ordered.begin(), ordered.end());
  class_attribute_.clear();
  for (const auto& name : ordered) {
    if (!class_attribute_.empty()) {
      class_attribute_.push_back(' ');
    }
    class_attribute_ += name;
  }
}

void Element::mark_tree_structure_dirty() {
  mark_style_dirty();
  if (owner_box_) {
    owner_box_->notify_utility_tree_changed();
  }
}

void Element::set_element_id(const std::string& id) {
  if (id_str_ == id) {
    return;
  }

  const std::string old_id = id_str_;
  id_ = Symbol(id);
  id_str_ = id;
  set_id(id);
  if (owner_box_) {
    owner_box_->reindex_element_id(this, old_id, id);
  }
}

void Element::set_attribute(Symbol name, const std::string& value) {
  static const Symbol class_name("class");
  if (name == class_name) {
    set_classes(value);
    return;
  }
  auto it = attributes_.find(name);
  if (it != attributes_.end() && it->second == value) {
    return;
  }
  attributes_[name] = value;
  mark_selector_scope_dirty(this);
}

void Element::remove_attribute(Symbol name) {
  static const Symbol class_name("class");
  if (name == class_name) {
    set_classes("");
    return;
  }
  if (attributes_.erase(name) > 0) {
    mark_selector_scope_dirty(this);
  }
}

bool Element::has_attribute(Symbol name) const {
  static const Symbol class_name("class");
  return name == class_name ? !class_names_.empty()
                            : attributes_.count(name) > 0;
}

const std::string* Element::attribute(Symbol name) const {
  static const Symbol class_name("class");
  if (name == class_name) {
    return class_names_.empty() ? nullptr : &class_attribute_;
  }
  auto it = attributes_.find(name);
  return it != attributes_.end() ? &it->second : nullptr;
}

void Element::set_custom_property(const std::string& name,
                                  const std::string& value) {
  if (name.size() < 3 || name[0] != '-' || name[1] != '-') {
    throw std::invalid_argument("CSS custom property names must start with --");
  }
  const Symbol symbol(name);
  auto it = custom_properties_.find(symbol);
  if (it != custom_properties_.end() && it->second == value) {
    return;
  }
  custom_properties_[symbol] = value;
  mark_style_dirty();
}

void Element::remove_custom_property(const std::string& name) {
  if (custom_properties_.erase(Symbol(name)) > 0) {
    mark_style_dirty();
  }
}

const std::string* Element::custom_property(const std::string& name) const {
  auto it = custom_properties_.find(Symbol(name));
  return it != custom_properties_.end() ? &it->second : nullptr;
}

void Element::set_state(Symbol state, bool active) {
  bool changed = false;
  if (active) {
    changed = pseudo_states_.insert(state).second;
  } else {
    changed = pseudo_states_.erase(state) > 0;
  }
  if (!changed) {
    return;
  }

  if (owner_box_ && owner_box_->style_state_affects_selectors(state)) {
      mark_selector_scope_dirty(this);
    return;
  }

  if (widget && widget->state_affects_paint(state)) {
    mark_paint_dirty();
  }
}

void Element::set_text(const std::string& text) {
  if (text_content_ == text) return;
  text_content_ = text;
  mark_layout_dirty();
}

// ============================================================================
// 脏标记（冒泡到 Box）
// ============================================================================

void Element::mark_style_dirty() {
  mark_dirty(flex::DirtyFlags::Content | flex::DirtyFlags::Layout | flex::DirtyFlags::Visual);
  if (owner_box_) {
    owner_box_->notify_dirty_style();
    owner_box_->notify_dirty_layout();
    owner_box_->notify_dirty_paint();
  }
}

void Element::mark_layout_dirty() {
  mark_dirty(flex::DirtyFlags::Layout | flex::DirtyFlags::Bounds);
  if (owner_box_) {
    owner_box_->notify_dirty_layout();
    owner_box_->notify_dirty_paint();  // Layout changes require repaint
  }
}

void Element::mark_paint_dirty() {
  mark_dirty(flex::DirtyFlags::Visual);
  if (owner_box_) {
    owner_box_->notify_dirty_paint();
  }
}

void Element::emit_render_commands(RenderCommandList& commands) {
  emit_element_render_commands(*this, commands, flex::make_identity());
}

void Element::render(flex::Renderer& renderer) {
  Renderer wrapped_renderer(&renderer);
  RenderCommandList commands(wrapped_renderer.capabilities());
  emit_render_commands(commands);
  commands.replay(wrapped_renderer);
}

} // namespace flexUI
