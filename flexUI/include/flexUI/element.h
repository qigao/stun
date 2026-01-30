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
#include <unordered_set>
#include <string>
#include <vector>

namespace flexUI {

// Forward declaration
class Box;

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
  void set_element_id(const std::string& id) { id_ = Symbol(id); id_str_ = id; set_id(id); }

  // 兼容旧代码的公共访问器（返回存储的字符串）
  const std::string& tag() const { return tag_str_; }
  const std::string& id() const { return id_str_; }

  // 设置 tag（供旧代码使用）
  void set_tag(const std::string& t) { tag_ = Symbol(t); tag_str_ = t; }

  // ========== CSS 类操作 ==========
  void add_class(Symbol cls);
  void add_class(const char* cls) { add_class(Symbol(cls)); }
  void remove_class(Symbol cls);
  void remove_class(const char* cls) { remove_class(Symbol(cls)); }
  bool has_class(Symbol cls) const { return classes_.count(cls) > 0; }
  const std::unordered_set<Symbol, SymbolHash>& classes() const { return classes_; }

  // ========== 伪状态操作 ==========
  void set_state(Symbol state, bool active = true);
  void set_state(const char* state, bool active = true) { set_state(Symbol(state), active); }
  bool has_state(Symbol state) const { return pseudo_states_.count(state) > 0; }

  void add_state(Symbol state) { set_state(state, true); }
  void remove_state(Symbol state) { set_state(state, false); }

  void set_hover(bool hover) { set_state("hover", hover); }
  void set_active(bool active) { set_state("active", active); }
  void set_focus(bool focus) { set_state("focus", focus); }

  bool is_hover() const { return has_state("hover"); }
  bool is_active() const { return has_state("active"); }
  bool is_focus() const { return has_state("focus"); }

  // ========== 内容操作 ==========
  void set_text(const std::string& text);
  const std::string& text() const { return text_content_; }

  // ========== 树操作 (委托给 Group) ==========
  Element* append(Element* child) {
    add_child(child);
    child->owner_box_ = owner_box_;
    return child;
  }

  void remove(Element* child) {
    remove_child(child);
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
    for (flex::Node* p = parent(); p; p = p->parent())
      ax += p->x();
    return ax;
  }

  float absolute_y() const {
    float ay = y();
    for (flex::Node* p = parent(); p; p = p->parent())
      ay += p->y();
    return ay;
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
           computed_style->visibility != Visibility::Hidden;
  }

  // ========== 渲染 ==========
  void render(flex::Renderer& renderer) override;

public:
  // 公共默认构造函数（供旧代码使用 std::make_unique<Element>()）
  Element() : tag_(Symbol("div")) {}

  // Arena 分配使用的构造函数
  explicit Element(Symbol tag) : tag_(tag) {}

private:
  Symbol tag_;
  Symbol id_;
  std::string tag_str_;  // 存储 tag 字符串供旧代码使用
  std::string id_str_;   // 存储 id 字符串供旧代码使用
  std::unordered_set<Symbol, SymbolHash> classes_;
  std::unordered_set<Symbol, SymbolHash> pseudo_states_;
  std::string text_content_;
  ClickCallback onclick_;
};

} // namespace flexUI

#endif // FLEXUI_ELEMENT_H
