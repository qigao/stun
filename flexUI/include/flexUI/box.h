/*
 * flexUI - Box (Main API)
 *
 * 主入口类 - 协调 EventDispatcher、LayoutManager、RenderManager
 */

#ifndef FLEXUI_BOX_H
#define FLEXUI_BOX_H

#include "element.h"
#include "binding_runtime.h"
#include "event.h"
#include "style_engine.h"
#include "utility_jit.h"
#include "transition.h"
#include "event_dispatcher.h"
#include "view_pipeline.h"
#include <flex/runtime/renderer.h>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace flex {
  class Renderer;
}

namespace flexUI {

class Renderer;
class RenderManager;
struct RenderFrame;
class ViewPipeline;
class UiKeyedRepeater;

enum class PointerPrecision {
  None,
  Coarse,
  Fine,
};

enum class ContrastPreference {
  NoPreference,
  More,
  Less,
};

struct MediaEnvironment {
  bool prefers_reduced_motion = false;
  bool prefers_dark_scheme = false;
  bool hover_available = true;
  bool any_hover_available = true;
  bool forced_colors_active = false;
  PointerPrecision pointer_precision = PointerPrecision::Fine;
  PointerPrecision any_pointer_precision = PointerPrecision::Fine;
  ContrastPreference contrast_preference = ContrastPreference::NoPreference;
};

/**
 * Box - flexUI 主入口
 *
 * 职责：管理 Element 树，协调样式/布局/渲染流程
 */
class Box : private ViewPipelineHost {
public:
  explicit Box(flex::Renderer* renderer);
  ~Box();

  Box(const Box&) = delete;
  Box& operator=(const Box&) = delete;

  // CSS
  void load_css(const std::string& css);
  CssLoadResult load_stylesheet(const std::string& css,
                                const CssLoadOptions& options = {});
  CssLoadResult replace_stylesheet(StylesheetId stylesheet_id,
                                   const std::string& css,
                                   const CssLoadOptions& options = {});
  bool remove_stylesheet(StylesheetId stylesheet_id);
  void enable_utility_jit(
      nlohmann::json utility_whitelist,
      tailwind::UtilityJitOptions options = {});
  void disable_utility_jit();
  bool utility_jit_enabled() const { return utility_jit_ != nullptr; }
  const std::vector<std::string>& missing_utility_tokens() const {
    return missing_utility_tokens_;
  }
  void set_variable(const std::string& name, const std::string& value);
  bool register_font(const std::string& family, const std::string& path);
  void unregister_font(const std::string& family);

  // 元素创建
  Element* create(const std::string& tag, const std::string& id = "");
  Element* create_with_widget(const std::string& tag, std::unique_ptr<Widget> widget, const std::string& id = "");
  // Takes ownership of widget. Prefer the unique_ptr overload in new code.
  Element* create_with_widget(const std::string& tag, Widget* widget, const std::string& id = "");

  template<typename WidgetT, typename... Args>
  Element* create_widget(const std::string& tag, const std::string& id, Args&&... args) {
    return create_with_widget(
        tag, std::make_unique<WidgetT>(std::forward<Args>(args)...), id);
  }

  Element* get_by_id(const std::string& id);
  Element* query_selector(const std::string& selector);
  std::vector<Element*> query_selector_all(const std::string& selector);
  void set_root(Element* elem);
  Element* root() { return root_; }

  // 视口
  void set_viewport(float width, float height);
  float viewport_width() const { return viewport_width_; }
  float viewport_height() const { return viewport_height_; }
  void set_media_environment(const MediaEnvironment& env);
  const MediaEnvironment& media_environment() const { return media_environment_; }
  flex::RendererCapabilities renderer_capabilities() const;

  // 更新
  void update();
  void invalidate();
  bool is_dirty() const { return dirty_style_ || dirty_layout_ || dirty_paint_; }
  ViewLifecycleState lifecycle_state() const;

  UiBindingRuntime& bindings() { return bindings_; }
  const UiBindingRuntime& bindings() const { return bindings_; }

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
  Element* hovered_element() const { return events_.hovered_element(); }

  // 时间/动画
  void update_time(float delta_ms);
  float time() const { return time_ms_; }
  TransitionManager& transitions() { return transitions_; }
  AnimationManager& animations() { return animations_; }

  // 脏标记通知
  void notify_dirty_style() { dirty_style_ = true; }
  void notify_dirty_layout() { dirty_layout_ = true; }
  void notify_dirty_paint() { dirty_paint_ = true; }
  bool style_state_affects_selectors(Symbol state) const;

  // Widget 注册
  void register_active_widget(Element* elem);
  void unregister_active_widget(Element* elem);

  // Viewport API
  float get_viewport_width() const { return viewport_width_; }
  float get_viewport_height() const { return viewport_height_; }

private:
  friend class UiKeyedRepeater;
  friend class Element;
  friend class Widget;

  void reindex_element_id(Element* elem, const std::string& old_id, const std::string& new_id);
  void notify_utility_tree_changed();
  void sync_utility_stylesheet();
  bool owns_element(const Element* element) const;
  void deactivate_subtree(Element* root);
  Element* create_widget_part(Element& host, const std::string& tag,
                              const std::string& part_name);
  void compute_styles(Element* elem, bool parent_recomputed = false);
  bool has_view_root() const override;
  bool needs_style_stage() const override;
  bool needs_layout_stage() const override;
  bool needs_render_stage() const override;
  void run_style_stage() override;
  void run_layout_stage() override;
  void run_layout_semantics_stage() override;
  bool run_container_query_stage() override;
  void run_positioning_stage() override;
  void run_render_stage() override;
  RenderFrame make_render_frame() const;

  // 渲染器
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<RenderManager> render_mgr_;
  std::unique_ptr<ViewPipeline> pipeline_;

  // 元素树
  Element* root_ = nullptr;
  std::vector<std::unique_ptr<Element>> elements_;
  std::vector<std::unique_ptr<Widget>> widgets_;
  std::map<std::string, Element*> elements_by_id_;

  // 视口
  float viewport_width_ = 800;
  float viewport_height_ = 600;
  MediaEnvironment media_environment_;

  // 时间
  float time_ms_ = 0;

  // 脏标记
  bool dirty_style_ = true;
  bool dirty_layout_ = true;
  bool dirty_paint_ = true;
  std::unordered_set<std::uintptr_t> styled_elements_;
  std::unordered_map<const Element*, float> container_widths_;

  // 活跃 Widget
  std::vector<Element*> active_widgets_;

  // 子系统
  EventDispatcher events_{this};
  StyleEngine style_engine_;
  std::unique_ptr<tailwind::UtilityJit> utility_jit_;
  StylesheetId utility_stylesheet_id_ = 0;
  std::uint64_t utility_applied_revision_ = 0;
  bool utility_tree_dirty_ = false;
  std::vector<std::string> missing_utility_tokens_;
  TransitionManager transitions_;
  AnimationManager animations_;
  UiBindingRuntime bindings_;
};

} // namespace flexUI

#endif // FLEXUI_BOX_H
