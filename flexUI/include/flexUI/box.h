/*
 * flexUI - Box (Main API)
 *
 * 主入口类 - 协调 EventDispatcher、LayoutManager、RenderManager
 */

#ifndef FLEXUI_BOX_H
#define FLEXUI_BOX_H

#include "element.h"
#include "event.h"
#include "style_engine.h"
#include "transition.h"
#include "event_dispatcher.h"
#include <functional>
#include <memory>

namespace flex {
  class Renderer;
}

namespace flexUI {

class Renderer;
class RenderManager;

/**
 * Box - flexUI 主入口
 *
 * 职责：管理 Element 树，协调样式/布局/渲染流程
 */
class Box {
public:
  explicit Box(flex::Renderer* renderer);
  ~Box();

  Box(const Box&) = delete;
  Box& operator=(const Box&) = delete;

  // CSS
  void load_css(const std::string& css);
  void set_variable(const std::string& name, const std::string& value);

  // 元素创建
  Element* create(const std::string& tag, const std::string& id = "");
  Element* create_with_widget(const std::string& tag, Widget* widget, const std::string& id = "");

  template<typename WidgetT, typename... Args>
  Element* create_widget(const std::string& tag, const std::string& id, Args&&... args) {
    return create_with_widget(tag, new WidgetT(std::forward<Args>(args)...), id);
  }

  Element* get_by_id(const std::string& id);
  void set_root(Element* elem);
  Element* root() { return root_; }

  // 视口
  void set_viewport(float width, float height);
  float viewport_width() const { return viewport_width_; }
  float viewport_height() const { return viewport_height_; }

  // 更新
  void update();
  void invalidate();
  bool is_dirty() const { return dirty_style_ || dirty_layout_ || dirty_paint_; }

  // 事件（委托给 EventDispatcher）
  void dispatch_event(Event& event);

  using EventCallback = std::function<void(Element&, const Event&)>;
  void set_event_callback(EventCallback cb) { events_.set_event_callback(cb); }

  void set_focus(Element* elem) { events_.set_focus(elem); }
  Element* focused_element() { return events_.focused_element(); }

  // 鼠标捕获（委托给 EventDispatcher）
  void set_mouse_capture(Element* elem) { events_.set_capture(elem); }
  void release_mouse_capture(Element* elem) { events_.release_capture(elem); }
  Element* capturing_element() const { return events_.capturing_element(); }

  // 时间/动画
  void update_time(float delta_ms);
  float time() const { return time_ms_; }
  TransitionManager& transitions() { return transitions_; }

  // 脏标记通知
  void notify_dirty_style() { dirty_style_ = true; }
  void notify_dirty_layout() { dirty_layout_ = true; }
  void notify_dirty_paint() { dirty_paint_ = true; }

  // Widget 注册
  void register_active_widget(Element* elem);
  void unregister_active_widget(Element* elem);

  // Viewport API
  float get_viewport_width() const { return viewport_width_; }
  float get_viewport_height() const { return viewport_height_; }

private:
  void compute_styles(Element* elem);

  // 渲染器
  flex::Renderer* flex_renderer_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<RenderManager> render_mgr_;

  // 元素树
  Element* root_ = nullptr;
  std::vector<std::unique_ptr<Element>> elements_;
  std::map<std::string, Element*> elements_by_id_;

  // 视口
  float viewport_width_ = 800;
  float viewport_height_ = 600;

  // 时间
  float time_ms_ = 0;

  // 脏标记
  bool dirty_style_ = true;
  bool dirty_layout_ = true;
  bool dirty_paint_ = true;

  // 活跃 Widget
  std::vector<Element*> active_widgets_;

  // 子系统
  EventDispatcher events_{this};
  StyleEngine style_engine_;
  TransitionManager transitions_;
};

} // namespace flexUI

#endif // FLEXUI_BOX_H
