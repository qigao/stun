/*
 * tvgbox2 - Element (DOM Node)
 *
 * 轻量级 DOM 节点
 */

#ifndef TVGBOX2_ELEMENT_H
#define TVGBOX2_ELEMENT_H

#include "computed_style.h"
#include "widget.h"
#include <string>
#include <vector>
#include <set>
#include <algorithm>

namespace tvgbox2 {

// Forward declaration
class Box;

/**
 * Element - DOM 节点
 *
 * 设计原则：
 * 1. 只包含 DOM 相关数据（身份、树结构、布局）
 * 2. Widget 状态存储在 Widget 对象中
 * 3. 样式存储在 ComputedStyle 对象中
 * 4. 清晰的脏标记传播
 */
struct Element {
  // ========== 身份 ==========
  std::string tag;              // 标签名（div, button, input, etc.）
  std::string id;               // CSS ID
  std::vector<std::string> classes;  // CSS classes
  std::set<std::string> pseudo_states;  // :hover, :active, :focus, etc.

  // ========== 树结构 ==========
  Element* parent = nullptr;
  std::vector<Element*> children;
  Box* owner_box = nullptr;     // Owning Box (for overlay access)

  // ========== 内容 ==========
  std::string text_content;     // 文本内容

  // ========== 布局（由布局引擎计算） ==========
  float x_ = 0;
  float y_ = 0;
  float width_ = 0;
  float height_ = 0;

  // ========== 样式和渲染 ==========
  ComputedStyle* computed_style = nullptr;  // 计算后的样式
  Widget* widget = nullptr;                 // Widget（如果需要）

  // ========== 脏标记 ==========
  bool dirty_style_ = true;   // 需要重新计算样式
  bool dirty_layout_ = true;  // 需要重新布局
  bool dirty_paint_ = true;   // 需要重新渲染

  // ========== 焦点 ==========
  bool focusable = false;     // 是否可聚焦
  int tab_index = 0;          // Tab 顺序

  // ========================================================================
  // CSS 类操作
  // ========================================================================

  void add_class(const std::string& cls) {
    if (std::find(classes.begin(), classes.end(), cls) == classes.end()) {
      classes.push_back(cls);
      mark_style_dirty();
    }
  }

  void remove_class(const std::string& cls) {
    auto it = std::remove(classes.begin(), classes.end(), cls);
    if (it != classes.end()) {
      classes.erase(it, classes.end());
      mark_style_dirty();
    }
  }

  bool has_class(const std::string& cls) const {
    return std::find(classes.begin(), classes.end(), cls) != classes.end();
  }

  // ========================================================================
  // 伪状态操作
  // ========================================================================

  void set_state(const std::string& state, bool active = true) {
    if (active) {
      if (pseudo_states.insert(state).second) {
        // 伪状态变化通常只影响外观，不影响布局
        // 只需要重绘，不需要重新计算布局
        mark_paint_dirty();
      }
    } else {
      if (pseudo_states.erase(state) > 0) {
        mark_paint_dirty();
      }
    }
  }

  bool has_state(const std::string& state) const {
    return pseudo_states.count(state) > 0;
  }

  // 便捷方法
  void add_state(const std::string& state) { set_state(state, true); }
  void remove_state(const std::string& state) { set_state(state, false); }

  void set_hover(bool hover) { set_state("hover", hover); }
  void set_active(bool active) { set_state("active", active); }
  void set_focus(bool focus) { set_state("focus", focus); }

  bool is_hover() const { return has_state("hover"); }
  bool is_active() const { return has_state("active"); }
  bool is_focus() const { return has_state("focus"); }

  // ========================================================================
  // 内容操作
  // ========================================================================

  void set_text(const std::string& text) {
    if (text_content == text) return;
    text_content = text;
    mark_layout_dirty();  // 文本改变可能影响尺寸
  }

  const std::string& text() const { return text_content; }

  // ========================================================================
  // 树操作
  // ========================================================================

  void append(Element* child) {
    if (!child || child->parent == this) return;

    // 从旧父节点移除
    if (child->parent) {
      child->parent->remove(child);
    }

    children.push_back(child);
    child->parent = this;
    mark_layout_dirty();  // 子元素添加需要重新布局
  }

  void remove(Element* child) {
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
      (*it)->parent = nullptr;
      children.erase(it);
      mark_layout_dirty();
    }
  }

  Element* parent_elem() const { return parent; }

  const std::vector<Element*>& child_elements() const { return children; }

  // ========================================================================
  // 布局访问（只读）
  // ========================================================================

  float x() const { return x_; }
  float y() const { return y_; }
  float width() const { return width_; }
  float height() const { return height_; }

  // Absolute position (sum of all parent positions)
  float absolute_x() const {
    float ax = x_;
    for (Element* p = parent; p; p = p->parent) ax += p->x_;
    return ax;
  }
  float absolute_y() const {
    float ay = y_;
    for (Element* p = parent; p; p = p->parent) ay += p->y_;
    return ay;
  }

  // ========================================================================
  // Widget 访问
  // ========================================================================

  template<typename T>
  T* widget_as() {
    return dynamic_cast<T*>(widget);
  }

  template<typename T>
  const T* widget_as() const {
    return dynamic_cast<const T*>(widget);
  }

  // ========================================================================
  // 脏标记（核心！）
  // ========================================================================

  /**
   * 标记样式脏（会传播到布局和渲染）
   *
   * 何时调用：
   * - add_class() / remove_class()
   * - set_state() (伪状态改变)
   */
  void mark_style_dirty() {
    dirty_style_ = true;
    dirty_layout_ = true;  // 样式改变可能影响布局
    dirty_paint_ = true;   // 肯定需要重新渲染

    // 递归标记子元素（样式继承）
    for (auto* child : children) {
      child->mark_style_dirty();
    }
  }

  /**
   * 标记布局脏（会传播到渲染）
   *
   * 何时调用：
   * - 窗口调整大小
   * - 父元素尺寸改变
   * - 添加/删除子元素
   * - 文本内容改变
   */
  void mark_layout_dirty() {
    dirty_layout_ = true;
    dirty_paint_ = true;

    // 递归标记子元素（布局级联）
    for (auto* child : children) {
      child->mark_layout_dirty();
    }
  }

  /**
   * 标记渲染脏（不影响布局）
   *
   * 何时调用：
   * - Widget 内部状态改变
   * - 动画更新
   * - 滚动位置改变
   */
  void mark_paint_dirty() {
    dirty_paint_ = true;
    // 不传播到子元素
  }

  // ========================================================================
  // 可见性检查
  // ========================================================================

  bool is_visible() const {
    if (!computed_style) return true;
    return computed_style->display != Display::None &&
           computed_style->visibility != Visibility::Hidden;
  }
};

} // namespace tvgbox2

#endif // TVGBOX2_ELEMENT_H
