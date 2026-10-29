/*
 * flexUI - Box (Main API)
 *
 * 主入口类 - 协调 EventDispatcher、LayoutManager、RenderManager
 */

#ifndef FLEXUI_BOX_H
#define FLEXUI_BOX_H

#include "binding_runtime.h"
#include "element.h"
#include "event.h"
#include "event_dispatcher.h"
#include "style_engine.h"
#include "tailwindcss.h"
#include "transition.h"
#include "ui_handle.h"
#include "view_pipeline.h"
#include <cstdint>
#include <flex/runtime/renderer.h>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace flex {
class Renderer;
}

namespace flexUI {

/// Read-only inspection of the live Box's OWN element/Widget/interaction
/// borrowers for one validated mounted subtree. This is not proof of worker
/// join, compiled-handler deregistration, or permission to destroy Elements.
/// Call on Box's owner thread only; no observer callback is made.
enum class BoxSubtreeBorrowErrorCode {
  None,
  WrongThread,
  InvalidRoot,
  InconsistentTree,
  AllocationFailed,
};

struct BoxSubtreeBorrowSnapshot {
  BoxSubtreeBorrowErrorCode error = BoxSubtreeBorrowErrorCode::None;
  std::size_t mounted_elements = 0;
  std::size_t widget_hosts = 0;
  std::size_t binding_targets = 0;
  std::size_t native_click_callbacks = 0;
  std::size_t active_widget_hosts = 0;
  std::size_t keyed_repeater_hosts = 0;
  bool native_event_dispatching = false;
  bool layout_or_widget_traversal_active = false;
  bool focused_descendant = false;
  bool hovered_descendant = false;
  bool active_pointer_descendant = false;
  bool captured_descendant = false;
  bool local_interaction_state = false;

  explicit operator bool() const noexcept {
    return error == BoxSubtreeBorrowErrorCode::None;
  }
};

class Renderer;
class RenderManager;
struct RenderFrame;
class ViewPipeline;
class UiKeyedRepeater;
class UiDocumentInstantiator;
class BoxPreparedMutation;

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

enum class UtilityJitMode {
  BuiltIn,
  Disabled,
};

enum class ThemeMode {
  System,
  Light,
  Dark,
};

struct BoxOptions {
  UtilityJitMode utility_jit = UtilityJitMode::BuiltIn;
  ThemeMode theme = ThemeMode::System;
  tailwind::UtilityJitOptions utility_limits{};

  static BoxOptions legacy_without_jit();
};

/**
 * Box - flexUI 主入口
 *
 * 职责：管理 Element 树，协调样式/布局/渲染流程
 * 默认 cascade 依次为内建 widget CSS、内嵌 theme、Utility JIT slot，
 * 调用方之后加载的 stylesheet 可覆盖默认 utility。内嵌 asset 解析失败时
 * 构造函数抛出异常，不返回半初始化实例。
 */
class Box : private ViewPipelineHost {
public:
  explicit Box(flex::Renderer *renderer, BoxOptions options = {});
  ~Box();

  Box(const Box &) = delete;
  Box &operator=(const Box &) = delete;

  // CSS
  void load_css(const std::string &css);
  CssLoadResult load_stylesheet(const std::string &css, const CssLoadOptions &options = {});
  CssLoadResult replace_stylesheet(StylesheetId stylesheet_id, const std::string &css,
                                   const CssLoadOptions &options = {});
  bool remove_stylesheet(StylesheetId stylesheet_id);
  void enable_utility_jit(nlohmann::json utility_whitelist,
                          tailwind::UtilityJitOptions options = {});
  void enable_utility_jit(std::shared_ptr<const tailwind::UtilityCatalog> utility_catalog,
                          tailwind::UtilityJitOptions options = {});
  void disable_utility_jit();
  bool utility_jit_enabled() const { return utility_jit_ != nullptr; }
  bool is_known_utility(std::string_view token) const {
    return utility_jit_ && utility_jit_->contains(token);
  }
  std::uint64_t utility_jit_revision() const { return utility_jit_ ? utility_jit_->revision() : 0; }
  std::size_t active_utility_count() const {
    return utility_jit_ ? utility_jit_->active_token_count() : 0;
  }
  std::size_t utility_stylesheet_size() const {
    return utility_jit_ ? utility_jit_->stylesheet().size() : 0;
  }
  const std::vector<std::string> &missing_utility_tokens() const { return missing_utility_tokens_; }
  void set_variable(const std::string &name, const std::string &value);
  bool register_font(const std::string &family, const std::string &path);
  void unregister_font(const std::string &family);

  // 元素创建
  Element *create(const std::string &tag, const std::string &id = "");
  // Takes ownership of a prevalidated detached Element forest. The caller
  // stages elements without involving the live tree, then publishes its
  // detached roots using Element::replace_children(). On admission failure,
  // the caller retains and destroys the staged nodes.
  void adopt_detached_elements(std::vector<std::unique_ptr<Element>> &&elements);

  /// Retire exactly one currently mounted, application-owned structural
  /// subtree in place. This M0 lifetime primitive is ONLY for unbound,
  /// widget-free, compiled-event-free nodes. All other cases fail closed:
  /// a mounted ScriptController / binding / Widget cannot be retired through
  /// this API. It also refuses during event dispatch or if any node holds
  /// focus, hover, active input or mouse capture.
  ///
  /// After success, borrowed Element pointers are invalid and any previous
  /// UiHandles (including descendant IDs) fail resolution. Sibling pointers
  /// and their handle generations remain unchanged. This operation does
  /// not update a CompiledUiProgram or component configuration and MUST NOT
  /// be used as dynamic script-component unregister.
  /// Preflight allocates before the non-allocating detach/index/cache commit;
  /// failure does not remove elements from the tree.
  bool retire_unbound_structural_subtree(const std::string &root_id) noexcept;

  /// M1: retire a manually managed structural subtree AND all Binding targets
  /// owned by its nodes in one owner-thread commit. Bound values are deliberately
  /// NOT restored: every target is destroyed immediately after its Binding
  /// record has been retired. Unaffected bindings and typed input values survive.
  /// Refuses a Box with any installed CompiledUiProgram, Widget, native click
  /// or compiled event handler, active repeater lease, active event/tree traversal
  /// or in-progress Binding mutation. No ScriptController scope unregister or
  /// service completion cancellation is implied. Failure leaves the original
  /// tree, binding count and input schema untouched.
  bool retire_bound_structural_subtree(const std::string &root_id) noexcept;

  /// Inspects CURRENT mounted subtree borrowers, including Widget-owned parts,
  /// using an ephemeral bounded pointer-identity set. No resource detaches and
  /// no compiled-program guard is bypassed. A report with zero flags still
  /// does NOT establish safe unmount (endpoint join and Binding observer
  /// retirement are independent owner contracts).
  BoxSubtreeBorrowSnapshot
  inspect_subtree_borrowers(const Element *subtree_root) const noexcept;

  // Diagnostics only; includes the Box-owned widget projections.
  std::size_t owned_element_count() const noexcept { return elements_.size(); }
  // Diagnostic count of pointer-keyed CSS baseline objects retained in Box.
  std::size_t cached_style_element_count() const noexcept {
    return style_engine_.tracked_element_count();
  }
  Element *create_with_widget(const std::string &tag, std::unique_ptr<Widget> widget,
                              const std::string &id = "");
  // Takes ownership of widget. Prefer the unique_ptr overload in new code.
  Element *create_with_widget(const std::string &tag, Widget *widget, const std::string &id = "");

  template <typename WidgetT, typename... Args>
  Element *create_widget(const std::string &tag, const std::string &id, Args &&...args) {
    return create_with_widget(tag, std::make_unique<WidgetT>(std::forward<Args>(args)...), id);
  }

  Element *get_by_id(const std::string &id);
  /// Returns the current Box-index incarnation for an element.
  /// @param element Element expected to be the current owner of its non-empty
  ///        id in this Box.
  /// @return A value handle, or an invalid handle for an id-less, shadowed, or
  ///         foreign element. Copying the id may throw std::bad_alloc.
  UiHandle handle_for(const Element &element) const;
  /// Resolves a handle only when both id and generation match the current
  /// index incarnation. The returned pointer is borrowed from this Box.
  Element *resolve_handle(const UiHandle &handle) noexcept;
  const Element *resolve_handle(const UiHandle &handle) const noexcept;
  Element *query_selector(const std::string &selector);
  std::vector<Element *> query_selector_all(const std::string &selector);
  void set_root(Element *elem);
  Element *root() { return root_; }
  const Element *root() const { return root_; }

  // 视口
  void set_viewport(float width, float height);
  float viewport_width() const { return viewport_width_; }
  float viewport_height() const { return viewport_height_; }
  void set_media_environment(const MediaEnvironment &env);
  const MediaEnvironment &media_environment() const { return media_environment_; }
  void set_theme_mode(ThemeMode mode);
  ThemeMode theme_mode() const { return theme_mode_; }
  flex::RendererCapabilities renderer_capabilities() const;

  // 更新
  void update();
  void invalidate();
  bool is_dirty() const { return dirty_style_ || dirty_layout_ || dirty_paint_; }
  ViewLifecycleState lifecycle_state() const;

  UiBindingRuntime &bindings() { return bindings_; }
  const UiBindingRuntime &bindings() const { return bindings_; }

  // 事件（委托给 EventDispatcher）
  void dispatch_event(Event &event);

  using EventCallback = std::function<void(Element &, const Event &)>;
  void set_event_callback(EventCallback cb) { events_.set_event_callback(cb); }

  /// Framework integration hook kept separate from the application callback.
  /// Application code should use set_event_callback().
  void set_framework_event_callback(EventCallback cb) {
    events_.set_framework_event_callback(std::move(cb));
  }

  void set_focus(Element *elem) { events_.set_focus(elem); }
  Element *focused_element() { return events_.focused_element(); }

  // 鼠标捕获（委托给 EventDispatcher）
  void set_mouse_capture(Element *elem) { events_.set_capture(elem); }
  void release_mouse_capture(Element *elem) { events_.release_capture(elem); }
  Element *capturing_element() const { return events_.capturing_element(); }
  Element *hovered_element() const { return events_.hovered_element(); }

  // 时间/动画
  void update_time(float delta_ms);
  float time() const { return time_ms_; }
  TransitionManager &transitions() { return transitions_; }
  AnimationManager &animations() { return animations_; }

  // 脏标记通知
  // Keep notifications emitted while a stage is executing. These revisions
  // count every request, including repeated notifications of an already-dirty
  // stage; they are owner-thread scheduling state, not cross-thread atomics.
  void notify_dirty_style() {
    dirty_style_ = true;
    ++style_request_revision_;
  }
  void notify_dirty_layout() {
    dirty_layout_ = true;
    ++layout_request_revision_;
  }
  // Paint requests raised during a synchronous render must survive its commit.
  // The revision increments even if the pending flag was already set.
  void notify_dirty_paint() {
    dirty_paint_ = true;
    ++paint_request_revision_;
  }
  bool style_state_affects_selectors(Symbol state) const;

  // Widget 注册
  void register_active_widget(Element *elem);
  void unregister_active_widget(Element *elem);

  // Viewport API
  float get_viewport_width() const { return viewport_width_; }
  float get_viewport_height() const { return viewport_height_; }

private:
  struct IndexedElement {
    Element *element = nullptr;
    std::uint64_t generation = 0;
  };

  friend class UiKeyedRepeater;
  friend class UiDocumentInstantiator;
  friend class BoxPreparedMutation;
  friend class Element;
  friend class Widget;
  friend class MarkdownWidget;

  void reindex_element_id(Element *elem, const std::string &old_id, const std::string &new_id);
  void notify_utility_tree_changed();
  void sync_utility_stylesheet();
  void validate_utility_token(std::string_view token) const;
  void apply_theme_to_root();
  bool owns_element(const Element *element) const;
  void deactivate_subtree(Element *root);
  bool retire_structural_subtree_impl(
      const std::string &root_id, bool discard_owned_bindings) noexcept;
  // Retire precisely the id-less Box-owned projection nodes that have
  // already been detached by a successful MarkdownWidget tree replacement.
  // Failed validation leaves all elements allocated.
  bool retire_detached_elements(const std::vector<Element*> &elements) noexcept;
  Element *create_widget_part(Element &host, const std::string &tag, const std::string &part_name);
  void compute_styles(Element *elem, bool parent_recomputed = false);
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

  // Box-owned structural mutations are thread-affine. This immutable token
  // allows the retirement preflight to reject cross-thread use before it
  // reads mutable tree/index/widget state.
  const std::thread::id owning_thread_ = std::this_thread::get_id();
  // Publishing any CompiledUiProgram makes the app immutable program table
  // authoritative. Retiring arbitrary Element IDs without rewriting that
  // table would leave stale script/binding/type definitions. M0 refuses it.
  bool compiled_program_installed_ = false;
  // Layout/render and widget-frame traversal borrow Element* across
  // callback boundaries; retirement must wait until those walks finish.
  std::size_t borrowed_tree_depth_ = 0;

  // 元素树
  Element *root_ = nullptr;
  std::vector<std::unique_ptr<Element>> elements_;
  std::vector<std::unique_ptr<Widget>> widgets_;
  // The index entry is the single source of truth for handle generations.
  std::map<std::string, IndexedElement> elements_by_id_;
  std::uint64_t next_element_generation_ = 1;

  // 视口
  float viewport_width_ = 800;
  float viewport_height_ = 600;
  MediaEnvironment media_environment_;
  ThemeMode theme_mode_ = ThemeMode::System;

  // 时间
  float time_ms_ = 0;

  // 脏标记
  bool dirty_style_ = true;
  bool dirty_layout_ = true;
  bool dirty_paint_ = true;
  std::uint64_t style_request_revision_ = 0;
  std::uint64_t layout_request_revision_ = 0;
  std::uint64_t paint_request_revision_ = 0;
  std::unordered_set<std::uintptr_t> styled_elements_;
  std::unordered_map<const Element *, float> container_widths_;

  // 活跃 Widget
  std::vector<Element *> active_widgets_;
  // Keyed repeaters borrow their container for their entire lifetime,
  // including when empty. A structural retire must refuse a leased host
  // or its descendants, not just nodes with a Widget or Binding target.
  std::unordered_set<Element *> keyed_repeater_hosts_;

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
