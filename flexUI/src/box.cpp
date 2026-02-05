/*
 * flexUI - Box Implementation
 */

#include <flexUI/box.h>
#include <flexUI/renderer.h>
#include <flexUI/layout_manager.h>
#include <flexUI/render_manager.h>
#include <flex/bridge/renderer.h>
#include <algorithm>

namespace flexUI {

Box::Box(flex::Renderer* renderer) : flex_renderer_(renderer) {
  renderer_ = std::make_unique<Renderer>(renderer);
  render_mgr_ = std::make_unique<RenderManager>(renderer_.get());
}

Box::~Box() {
  for (auto& elem_ptr : elements_) {
    if (elem_ptr->widget) {
      delete elem_ptr->widget;
      elem_ptr->widget = nullptr;
    }
  }
}

void Box::load_css(const std::string& css) {
  style_engine_.parse_css(css);
  if (root_) root_->mark_style_dirty();
}

void Box::set_variable(const std::string& name, const std::string& value) {}

Element* Box::create(const std::string& tag, const std::string& id) {
  auto elem = std::make_unique<Element>();
  elem->set_tag(tag);
  elem->set_element_id(id);
  elem->owner_box_ = this;
  Element* ptr = elem.get();
  elements_.push_back(std::move(elem));
  if (!id.empty()) elements_by_id_[id] = ptr;
  return ptr;
}

Element* Box::create_with_widget(const std::string& tag, Widget* widget, const std::string& id) {
  Element* elem = create(tag, id);
  elem->widget = widget;
  elem->focusable = true;
  active_widgets_.push_back(elem);
  return elem;
}

Element* Box::get_by_id(const std::string& id) {
  auto it = elements_by_id_.find(id);
  return it != elements_by_id_.end() ? it->second : nullptr;
}

void Box::set_root(Element* elem) {
  root_ = elem;
  if (root_) root_->mark_style_dirty();
}

void Box::set_viewport(float width, float height) {
  if (viewport_width_ == width && viewport_height_ == height) return;
  viewport_width_ = width;
  viewport_height_ = height;
  if (root_) root_->mark_layout_dirty();
}

void Box::update() {
  if (!root_ || !flex_renderer_) return;

  if (dirty_style_) {
    compute_styles(root_);
    dirty_style_ = false;
  }

  if (dirty_layout_) {
    root_->set_x(0);
    root_->set_y(0);
    LayoutManager::sync_to_flex(root_, viewport_width_, viewport_height_);
    LayoutManager::perform_layout(root_);
    dirty_layout_ = false;
  }

  // 只在脏时渲染
  if (!dirty_paint_) return;

  flex_renderer_->begin_frame(viewport_width_, viewport_height_, 1.0f);
  flex_renderer_->clear(Color{0.12f, 0.12f, 0.12f, 1.0f});
  render_mgr_->render_tree(root_);
  render_mgr_->render_overlays(root_);
  flex_renderer_->end_frame();
  dirty_paint_ = false;
}

void Box::invalidate() {
  if (root_) root_->mark_paint_dirty();
}

void Box::dispatch_event(Event& event) {
  events_.dispatch(event, root_);
}

void Box::update_time(float delta_ms) {
  time_ms_ += delta_ms;
  transitions_.update(time_ms_);
  for (auto* elem : active_widgets_) {
    if (elem && elem->widget) elem->widget->update(delta_ms, *elem);
    if (transitions_.has_active(reinterpret_cast<intptr_t>(elem), time_ms_)) elem->mark_paint_dirty();
  }
}

void Box::register_active_widget(Element* elem) {
  if (std::find(active_widgets_.begin(), active_widgets_.end(), elem) == active_widgets_.end()) {
    active_widgets_.push_back(elem);
  }
}

void Box::unregister_active_widget(Element* elem) {
  auto it = std::find(active_widgets_.begin(), active_widgets_.end(), elem);
  if (it != active_widgets_.end()) active_widgets_.erase(it);
}

void Box::compute_styles(Element* elem) {
  if (!elem->dirty_style()) {
    for (auto* node : elem->children()) {
      if (auto* child = static_cast<Element*>(node)) compute_styles(child);
    }
    return;
  }
  if (!elem->computed_style) elem->computed_style = new ComputedStyle();
  style_engine_.apply_styles(elem);
  elem->clear_dirty(flex::DirtyFlags::Content);
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) compute_styles(child);
  }
}

} // namespace flexUI
