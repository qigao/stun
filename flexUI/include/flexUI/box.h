/*
 * flexUI - Box (Main API)
 *
 * 主入口类
 */

#ifndef FLEXUI_BOX_H
#define FLEXUI_BOX_H

#include "element.h"
#include "event.h"
#include "style_engine.h"
#include "transition.h"
#include <functional>
#include <memory>

namespace flex {
  class Renderer;
}

namespace flexUI {

// 前向声明
class Renderer;

/**
 * Box - flexUI 主入口
 *
 * 职责：
 * 1. 管理 Element 树
 * 2. 加载和管理 CSS
 * 3. 事件分发
 * 4. 协调更新流程（样式 → 布局 → 渲染）
 */
class Box {
public:
  /**
   * 构造函数
   *
   * @param renderer flex::Renderer 指针（由调用方管理生命周期）
   */
  explicit Box(flex::Renderer* renderer);
  ~Box();

  // 不可复制
  Box(const Box&) = delete;
  Box& operator=(const Box&) = delete;

  // ========================================================================
  // CSS
  // ========================================================================

  /**
   * 加载 CSS 样式表
   *
   * @param css CSS 文本
   */
  void load_css(const std::string& css);

  /**
   * 设置 CSS 变量
   *
   * @param name 变量名（如 "--primary-color"）
   * @param value 变量值
   */
  void set_variable(const std::string& name, const std::string& value);

  // ========================================================================
  // 元素创建
  // ========================================================================

  /**
   * 创建元素
   *
   * @param tag 标签名（div, span, button, etc.）
   * @param id CSS ID（可选）
   * @return Element 指针（Box 拥有所有权）
   */
  Element* create(const std::string& tag, const std::string& id = "");

  /**
   * 创建带 Widget 的元素
   *
   * @param tag 标签名
   * @param widget Widget 对象（Box 获得所有权）
   * @param id CSS ID（可选）
   * @return Element 指针
   */
  Element* create_with_widget(const std::string& tag, Widget* widget,
                              const std::string& id = "");

  /**
   * 创建 Widget 元素（模板便捷方法）
   *
   * @tparam WidgetT Widget 类型
   * @tparam Args Widget 构造函数参数
   */
  template<typename WidgetT, typename... Args>
  Element* create_widget(const std::string& tag, const std::string& id, Args&&... args) {
    return create_with_widget(tag, new WidgetT(std::forward<Args>(args)...), id);
  }

  /**
   * 根据 ID 查找元素
   */
  Element* get_by_id(const std::string& id);

  /**
   * 设置根元素
   */
  void set_root(Element* elem);

  Element* root() { return root_; }

  // ========================================================================
  // 视口
  // ========================================================================

  /**
   * 设置视口大小
   */
  void set_viewport(float width, float height);

  float viewport_width() const { return viewport_width_; }
  float viewport_height() const { return viewport_height_; }

  // ========================================================================
  // 更新和渲染
  // ========================================================================

  /**
   * 更新（每帧调用）
   *
   * 自动处理：
   * 1. 样式计算（如果需要）
   * 2. 布局计算（如果需要）
   * 3. 渲染（如果需要）
   */
  void update();

  /**
   * 强制重新渲染所有元素
   */
  void invalidate();

  /**
   * 检查是否有脏标记需要更新
   */
  bool is_dirty() const { return subtree_dirty_style_ || subtree_dirty_layout_ || subtree_dirty_paint_; }

  // ========================================================================
  // 事件处理
  // ========================================================================

  /**
   * 分发事件
   *
   * @param event 事件对象
   */
  void dispatch_event(Event& event);

  /**
   * 设置全局事件回调（可选）
   */
  using EventCallback = std::function<void(Element&, const Event&)>;
  void set_event_callback(EventCallback callback) {
    event_callback_ = callback;
  }

  /**
   * 设置焦点
   */
  void set_focus(Element* elem);

  Element* focused_element() { return focused_element_; }

  // ========================================================================
  // 时间（用于动画）
  // ========================================================================

  /**
   * 更新时间（用于 Widget::update）
   */
  void update_time(float delta_ms);

  float time() const { return time_ms_; }

  /**
   * 获取 TransitionManager（用于动画）
   */
  TransitionManager& transitions() { return transitions_; }

  // ========================================================================
  // 脏标记通知（从 Element 调用）
  // ========================================================================

  void notify_dirty_style() { subtree_dirty_style_ = true; }
  void notify_dirty_layout() { subtree_dirty_layout_ = true; }
  void notify_dirty_paint() { subtree_dirty_paint_ = true; }

  // ========================================================================
  // 鼠标捕获（Widget 注册/注销）
  // ========================================================================

  void set_mouse_capture(Element* elem) { capturing_element_ = elem; }
  void release_mouse_capture(Element* elem) {
    if (capturing_element_ == elem) capturing_element_ = nullptr;
  }
  Element* capturing_element() const { return capturing_element_; }

  // ========================================================================
  // 活跃 Widget 注册（用于 update_time 优化）
  // ========================================================================

  void register_active_widget(Element* elem);
  void unregister_active_widget(Element* elem);

private:
  // ========== 内部方法 ==========

  // 样式计算
  void compute_styles(Element* elem);
  bool has_dirty_style(Element* elem);

  // 布局计算
  bool has_dirty_layout(Element* elem);

  // 渲染
  void render_element(Element* elem);
  void render_tree(Element* elem);
  bool has_dirty_paint(Element* elem);

  // Overlay system for dropdowns, tooltips, etc.
  void render_overlays(Element* elem);

  // 事件处理辅助
  void handle_special_events(Event& event, Element* target);
  void propagate_event(Event& event, Element* target);

  // Hit testing
  Element* hit_test(Element* elem, float x, float y);
  Element* find_capturing_element(Element* elem);  // Find widget that wants mouse capture
  std::vector<Element*> get_ancestors(Element* elem);

  // ========== 状态 ==========

  flex::Renderer* flex_renderer_;  // 外部渲染器
  Element* root_ = nullptr;

  float viewport_width_ = 800;
  float viewport_height_ = 600;

  float time_ms_ = 0;

  // 事件状态
  Element* hovered_element_ = nullptr;
  Element* active_element_ = nullptr;
  Element* focused_element_ = nullptr;
  Element* capturing_element_ = nullptr;  // 缓存的鼠标捕获元素

  EventCallback event_callback_;

  // 脏标记缓存（O(1) 检查，由 Element 冒泡）
  bool subtree_dirty_style_ = true;
  bool subtree_dirty_layout_ = true;
  bool subtree_dirty_paint_ = true;

  // 活跃 Widget 集合（需要 update_time 的元素）
  std::vector<Element*> active_widgets_;

  // 元素存储（Box 拥有所有元素）
  std::vector<std::unique_ptr<Element>> elements_;

  // ID 索引
  std::map<std::string, Element*> elements_by_id_;

  // 子系统
  std::unique_ptr<Renderer> renderer_;
  StyleEngine style_engine_;
  TransitionManager transitions_;

public:
  // Viewport API for widgets
  float get_viewport_width() const { return viewport_width_; }
  float get_viewport_height() const { return viewport_height_; }
};

} // namespace flexUI

#endif // FLEXUI_BOX_H
