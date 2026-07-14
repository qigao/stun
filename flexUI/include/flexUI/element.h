/*
 * flexUI - Element (DOM Node)
 *
 * Element 继承 flex::Group，获得：
 * - 变换、脏标记、事件回调（来自 Node）
 * - 子节点管理、布局计算（来自 Group）
 *
 * Element 新增：
 * - CSS 类和伪状态
 * - ComputedStyle（内嵌）
 * - Widget 指针（可选）
 */

#ifndef FLEXUI_ELEMENT_H
#define FLEXUI_ELEMENT_H

#include "computed_style.h"
#include "widget.h"
#include <flex/runtime/group.h>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <vector>

namespace flexUI {

// Forward declaration
class Box;
class RenderCommandList;

enum class ElementOwnership {
  Application,
  Widget,
};

/**
 * Element - 继承 flex::Group 的 DOM 节点
 *
 * 继承自 flex::Group：
 * - x_, y_, scale_x_, scale_y_, rotation_, opacity_, visible_ (来自 Node)
 * - dirty_flags_ (来自 Node)
 * - children_, layout 属性 (来自 Group)
 *
 * 新增属性：
 * - tag, id, classes, pseudo_states (CSS 选择器)
 * - computed_style (内嵌样式)
 * - widget (可选组件)
 */
class Element : public flex::Group {
public:
  // Arena 分配工厂
  static Element* create(flex::ArenaAllocator& arena, Symbol tag, const std::string& id = "");
  static Element* create(flex::ArenaAllocator& arena, const std::string& tag, const std::string& id = "");

  // 类型标识
  flex::NodeType type() const override { return flex::NodeType::Group; }
  const char* type_name() const override { return "Element"; }

  // ========== 身份 ==========
  Symbol tag_name() const { return tag_; }
  Symbol element_id() const { return id_; }
  void set_element_id(Symbol id) { id_ = id; }
  void set_element_id(const char* id) { set_element_id(std::string(id ? id : "")); }
  void set_element_id(const std::string& id);

  // 兼容旧代码的公共访问器（返回存储的字符串）
  const std::string& tag() const { return tag_str_; }
  const std::string& id() const { return id_str_; }

  ElementOwnership ownership() const { return ownership_; }
  bool is_widget_owned() const { return ownership_ == ElementOwnership::Widget; }

  // 设置 tag（供旧代码使用）
  void set_tag(const std::string& t) { tag_ = Symbol(t); tag_str_ = t; }

  // ========== CSS 类操作 ==========
  void add_class(Symbol cls);
  void add_class(const char* cls);
  void add_class(const std::string& cls);
  void add_classes(std::string_view class_list);
  void set_classes(std::string_view class_list);
  void toggle_class(const std::string& cls, bool enabled);
  bool replace_class(const std::string& old_class,
                     const std::string& new_class);
  void remove_class(Symbol cls);
  void remove_class(const char* cls);
  void remove_class(const std::string& cls);
  bool has_class(Symbol cls) const { return classes_.count(cls) > 0; }
  const std::unordered_set<Symbol, SymbolHash>& classes() const { return classes_; }
  const std::unordered_set<std::string>& class_names() const {
    return class_names_;
  }

  // ========== Attributes ==========
  void set_attribute(Symbol name, const std::string& value = "");
  void set_attribute(const char* name, const std::string& value = "") {
    set_attribute(Symbol(name), value);
  }
  void set_attribute(const std::string& name, const std::string& value = "") {
    set_attribute(Symbol(name), value);
  }
  void remove_attribute(Symbol name);
  void remove_attribute(const char* name) { remove_attribute(Symbol(name)); }
  void remove_attribute(const std::string& name) { remove_attribute(Symbol(name)); }
  bool has_attribute(Symbol name) const;
  bool has_attribute(const char* name) const { return has_attribute(Symbol(name)); }
  bool has_attribute(const std::string& name) const { return has_attribute(Symbol(name)); }
  const std::string* attribute(Symbol name) const;
  const std::string* attribute(const char* name) const {
    return attribute(Symbol(name));
  }
  const std::string* attribute(const std::string& name) const {
    return attribute(Symbol(name));
  }

  // ========== Inline CSS custom properties ==========
  void set_custom_property(const std::string& name, const std::string& value);
  void remove_custom_property(const std::string& name);
  const std::string* custom_property(const std::string& name) const;
  const std::unordered_map<Symbol, std::string, SymbolHash>&
  custom_properties() const {
    return custom_properties_;
  }

  // ========== 伪状态操作 ==========
  void set_state(Symbol state, bool active = true);
  void set_state(const char* state, bool active = true) { set_state(Symbol(state), active); }
  bool has_state(Symbol state) const { return pseudo_states_.count(state) > 0; }

  void add_state(Symbol state) { set_state(state, true); }
  void remove_state(Symbol state) { set_state(state, false); }

  void set_hover(bool hover) { set_state("hover", hover); }
  void set_active(bool active) { set_state("active", active); }
  void set_focus(bool focus) { set_state("focus", focus); }
  void set_focus_visible(bool focus_visible) {
    set_state("focus-visible", focus_visible);
  }

  bool is_hover() const { return has_state("hover"); }
  bool is_active() const { return has_state("active"); }
  bool is_focus() const { return has_state("focus"); }
  bool is_focus_visible() const { return has_state("focus-visible"); }

  // ========== 内容操作 ==========
  void set_text(const std::string& text);
  const std::string& text() const { return text_content_; }

  // ========== 树操作 (委托给 Group) ==========
  void add_child(flex::Node::RawPtr child) {
    auto* elem = dynamic_cast<Element*>(child);
    if (is_widget_owned() || !elem || elem->is_widget_owned()) {
      return;
    }
    flex::Group::add_child(elem);
    elem->owner_box_ = owner_box_;
    mark_tree_structure_dirty();
  }

  template<typename T>
  void add_child(const std::shared_ptr<T>& child) {
    auto* elem = child ? dynamic_cast<Element*>(child.get()) : nullptr;
    if (is_widget_owned() || !elem || elem->is_widget_owned()) {
      return;
    }
    flex::Group::add_child(child);
    elem->owner_box_ = owner_box_;
    mark_tree_structure_dirty();
  }

  void insert_child(flex::Node::RawPtr child, size_t index) {
    auto* elem = dynamic_cast<Element*>(child);
    if (is_widget_owned() || !elem || elem->is_widget_owned()) {
      return;
    }
    flex::Group::insert_child(elem, index);
    elem->owner_box_ = owner_box_;
    mark_tree_structure_dirty();
  }

  template<typename T>
  void insert_child(const std::shared_ptr<T>& child, size_t index) {
    auto* elem = child ? dynamic_cast<Element*>(child.get()) : nullptr;
    if (is_widget_owned() || !elem || elem->is_widget_owned()) {
      return;
    }
    flex::Group::insert_child(child, index);
    elem->owner_box_ = owner_box_;
    mark_tree_structure_dirty();
  }

  void remove_child(flex::Node::RawPtr child) {
    auto* elem = dynamic_cast<Element*>(child);
    if (is_widget_owned() || !elem || elem->is_widget_owned()) {
      return;
    }
    flex::Group::remove_child(elem);
    mark_tree_structure_dirty();
  }

  void remove_child_at(size_t index) {
    auto* elem = index < children().size()
                     ? dynamic_cast<Element*>(children()[index])
                     : nullptr;
    if (is_widget_owned() || !elem || elem->is_widget_owned()) {
      return;
    }
    flex::Group::remove_child_at(index);
    mark_tree_structure_dirty();
  }

  void clear_children() {
    if (is_widget_owned()) {
      return;
    }
    const auto snapshot = children();
    for (auto* child : snapshot) {
      remove_child(child);
    }
  }

  void clear() { clear_children(); }

  /**
   * Atomically validates and replaces application-owned children.
   *
   * @complexity Expected O(n) time and O(n) temporary space.
   */
  void replace_children(const std::vector<Element*>& children) {
    if (is_widget_owned()) {
      throw std::invalid_argument(
          "widget-owned elements cannot replace their children");
    }
    std::unordered_set<Element*> unique;
    unique.reserve(children.size());
    for (auto* child : children) {
      if (!child || child->is_widget_owned() ||
          (child->parent_elem() && child->parent_elem() != this) ||
          !unique.insert(child).second) {
        throw std::invalid_argument(
            "replacement children must be unique, detached or existing "
            "application elements");
      }
    }

    flex::Group::clear_children();
    for (auto* child : children) {
      flex::Group::add_child(child);
      child->owner_box_ = owner_box_;
    }
    mark_tree_structure_dirty();
  }

  Element* append(Element* child) {
    if (is_widget_owned() || !child) {
      return nullptr;
    }
    if (child->is_widget_owned()) {
      return child->parent_elem() == this ? child : nullptr;
    }
    add_child(child);
    child->owner_box_ = owner_box_;
    return child;
  }

  bool remove(Element* child) {
    if (is_widget_owned() || !child || child->is_widget_owned()) {
      return false;
    }
    const bool attached = child->parent_elem() == this;
    remove_child(child);
    return attached;
  }

  Element* parent_elem() const {
    return static_cast<Element*>(parent());
  }

  // 获取子元素数量
  size_t child_count() const { return children().size(); }

  // 获取第 i 个子元素（直接 static_cast，因为 append 只接受 Element*）
  Element* child_at(size_t i) const {
    return static_cast<Element*>(children()[i]);
  }

  // 遍历子元素（避免分配 vector）
  template<typename Fn>
  void for_each_child(Fn&& fn) const {
    for (auto* child : children()) {
      fn(static_cast<Element*>(child));
    }
  }

  // ========== 布局访问 ==========
  // 使用 Group 的 x(), y() 和 layout_width(), layout_height()
  float width() const { return layout_width(); }
  float height() const { return layout_height(); }

  void set_layout_bounds(float x, float y, float w, float h) {
    set_x(x);
    set_y(y);
    set_layout_size(w, h);
  }

  // Absolute position (sum of all parent positions)
  float absolute_x() const {
    float ax = x();
    for (flex::Node* p = parent(); p; p = p->parent()) {
      ax += p->x();
      if (auto* elem = dynamic_cast<Element*>(p)) {
        ax -= elem->scroll_x_;
      }
    }
    return ax;
  }

  float absolute_y() const {
    float ay = y();
    for (flex::Node* p = parent(); p; p = p->parent()) {
      ay += p->y();
      if (auto* elem = dynamic_cast<Element*>(p)) {
        ay -= elem->scroll_y_;
      }
    }
    return ay;
  }

  // ========== 滚动状态 ==========
  float scroll_x() const { return scroll_x_; }
  float scroll_y() const { return scroll_y_; }
  float max_scroll_x() const { return max_scroll_x_; }
  float max_scroll_y() const { return max_scroll_y_; }
  float scroll_content_width() const { return scroll_content_width_; }
  float scroll_content_height() const { return scroll_content_height_; }
  bool is_scroll_container() const { return scroll_container_; }
  bool can_scroll_x() const { return scroll_container_ && max_scroll_x_ > 0.0f; }
  bool can_scroll_y() const { return scroll_container_ && max_scroll_y_ > 0.0f; }

  void set_scroll_metrics(bool enabled, float content_width, float content_height,
                          float max_scroll_x, float max_scroll_y) {
    scroll_container_ = enabled;
    scroll_content_width_ = std::max(content_width, 0.0f);
    scroll_content_height_ = std::max(content_height, 0.0f);
    max_scroll_x_ = std::max(max_scroll_x, 0.0f);
    max_scroll_y_ = std::max(max_scroll_y, 0.0f);
    scroll_x_ = std::clamp(scroll_x_, 0.0f, max_scroll_x_);
    scroll_y_ = std::clamp(scroll_y_, 0.0f, max_scroll_y_);
  }

  bool set_scroll_offset(float x, float y) {
    const float next_x = std::clamp(x, 0.0f, max_scroll_x_);
    const float next_y = std::clamp(y, 0.0f, max_scroll_y_);
    if (std::fabs(next_x - scroll_x_) <= 0.001f &&
        std::fabs(next_y - scroll_y_) <= 0.001f) {
      return false;
    }
    scroll_x_ = next_x;
    scroll_y_ = next_y;
    mark_layout_dirty();
    return true;
  }

  bool scroll_by(float dx, float dy) {
    return set_scroll_offset(scroll_x_ + dx, scroll_y_ + dy);
  }

  bool bounds_in_ancestor_content(const Element* ancestor, float& x, float& y,
                                  float& w, float& h) const {
    x = 0.0f;
    y = 0.0f;
    w = width();
    h = height();

    const Element* current = this;
    while (current && current != ancestor) {
      x += current->x();
      y += current->y();
      current = current->parent_elem();
    }
    return current == ancestor;
  }

  bool scroll_descendant_into_view(const Element* descendant,
                                   const float scroll_padding[4],
                                   const float scroll_margin[4]) {
    if (!descendant || descendant == this || !scroll_container_) {
      return false;
    }

    float target_x = 0.0f;
    float target_y = 0.0f;
    float target_w = 0.0f;
    float target_h = 0.0f;
    if (!descendant->bounds_in_ancestor_content(this, target_x, target_y, target_w,
                                                target_h)) {
      return false;
    }

    float next_scroll_x = scroll_x_;
    float next_scroll_y = scroll_y_;

    if (can_scroll_x()) {
      const float visible_left = scroll_x_ + std::max(scroll_padding[3], 0.0f);
      const float visible_right =
          scroll_x_ + width() - std::max(scroll_padding[1], 0.0f);
      const float target_left = target_x - std::max(scroll_margin[3], 0.0f);
      const float target_right = target_x + target_w + std::max(scroll_margin[1], 0.0f);
      const float visible_width = std::max(visible_right - visible_left, 0.0f);
      const float target_width = std::max(target_right - target_left, 0.0f);
      if (target_left < visible_left) {
        next_scroll_x = target_left - std::max(scroll_padding[3], 0.0f);
      } else if (target_width <= visible_width && target_right > visible_right) {
        next_scroll_x = target_right - width() + std::max(scroll_padding[1], 0.0f);
      }
    }

    if (can_scroll_y()) {
      const float visible_top = scroll_y_ + std::max(scroll_padding[0], 0.0f);
      const float visible_bottom =
          scroll_y_ + height() - std::max(scroll_padding[2], 0.0f);
      const float target_top = target_y - std::max(scroll_margin[0], 0.0f);
      const float target_bottom = target_y + target_h + std::max(scroll_margin[2], 0.0f);
      const float visible_height = std::max(visible_bottom - visible_top, 0.0f);
      const float target_height = std::max(target_bottom - target_top, 0.0f);
      if (target_top < visible_top) {
        next_scroll_y = target_top - std::max(scroll_padding[0], 0.0f);
      } else if (target_height <= visible_height && target_bottom > visible_bottom) {
        next_scroll_y = target_bottom - height() + std::max(scroll_padding[2], 0.0f);
      }
    }

    return set_scroll_offset(next_scroll_x, next_scroll_y);
  }

  // ========== 样式 ==========
  ComputedStyle style_;  // 内嵌样式（避免额外堆分配）
  ComputedStyle* computed_style = &style_;  // 兼容指针访问

  // ========== Widget ==========
  Widget* widget = nullptr;

  template <typename T> T* widget_as() { return dynamic_cast<T*>(widget); }
  template <typename T> const T* widget_as() const { return dynamic_cast<const T*>(widget); }

  // ========== 所有者 ==========
  Box* owner_box_ = nullptr;

  // ========== 焦点 ==========
  bool focusable = false;
  int tab_index = 0;

  using ClickCallback = std::function<void()>;
  void on_click(ClickCallback cb) { onclick_ = std::move(cb); }
  const ClickCallback& click_callback() const { return onclick_; }

  // ========== 脏标记（使用 Node 的 DirtyFlags，冒泡到 Box） ==========

  /**
   * 标记样式脏（会传播到布局和渲染）
   */
  void mark_style_dirty();

  /**
   * 标记布局脏
   */
  void mark_layout_dirty();

  /**
   * 标记渲染脏（不影响布局）
   */
  void mark_paint_dirty();

  // 兼容性方法
  bool dirty_style() const { return is_dirty(flex::DirtyFlags::Content); }
  bool dirty_layout() const { return is_dirty(flex::DirtyFlags::Layout); }
  bool dirty_paint() const { return is_dirty(flex::DirtyFlags::Visual); }

  // ========== 可见性检查 ==========
  bool is_visible() const {
    if (!visible()) return false;
    if (!computed_style) return true;
    return computed_style->display != Display::None &&
           computed_style->visibility == Visibility::Visible;
  }

  // ========== 渲染 ==========
  void emit_render_commands(RenderCommandList& commands);
  void render(flex::Renderer& renderer) override;

public:
  // 公共默认构造函数（供旧代码使用 std::make_unique<Element>()）
  Element() : tag_(Symbol("div")) {}

  // Arena 分配使用的构造函数
  explicit Element(Symbol tag) : tag_(tag) {}

private:
  friend class Box;
  friend class Widget;

  using flex::Group::add;

  void mark_tree_structure_dirty();
  void sync_class_attribute();

  Element* append_widget_part(Element* child) {
    if (!child || !child->is_widget_owned()) {
      return nullptr;
    }
    flex::Group::add_child(child);
    child->owner_box_ = owner_box_;
    mark_tree_structure_dirty();
    return child;
  }

  ElementOwnership ownership_ = ElementOwnership::Application;
  Symbol tag_;
  Symbol id_;
  std::string tag_str_;  // 存储 tag 字符串供旧代码使用
  std::string id_str_;   // 存储 id 字符串供旧代码使用
  std::unordered_set<Symbol, SymbolHash> classes_;
  std::unordered_set<Symbol, SymbolHash> symbol_only_classes_;
  std::unordered_set<std::string> class_names_;
  std::string class_attribute_;
  std::unordered_map<Symbol, std::string, SymbolHash> attributes_;
  std::unordered_map<Symbol, std::string, SymbolHash> custom_properties_;
  std::unordered_set<Symbol, SymbolHash> pseudo_states_;
  std::string text_content_;
  ClickCallback onclick_;
  bool scroll_container_ = false;
  float scroll_x_ = 0.0f;
  float scroll_y_ = 0.0f;
  float max_scroll_x_ = 0.0f;
  float max_scroll_y_ = 0.0f;
  float scroll_content_width_ = 0.0f;
  float scroll_content_height_ = 0.0f;
};

} // namespace flexUI

#endif // FLEXUI_ELEMENT_H
