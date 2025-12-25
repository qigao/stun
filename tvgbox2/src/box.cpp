/*
 * tvgbox2 - Box Implementation
 */

#include <tvgbox2/box.h>
#include <tvgbox2/renderer.h>
#include <tvgbox2/types.h>
#include <flex/bridge/renderer.h>
#include <algorithm>

namespace tvgbox2 {

// ============================================================================
// 构造/析构
// ============================================================================

Box::Box(flex::Renderer* renderer) : flex_renderer_(renderer) {
  // 初始化 Renderer wrapper
  renderer_ = std::make_unique<Renderer>(renderer);
}

Box::~Box() {
  // 清理所有元素（unique_ptr 自动删除）
  // 但需要手动删除 Widget 和 ComputedStyle
  for (auto& elem_ptr : elements_) {
    auto* elem = elem_ptr.get();
    if (elem->widget) {
      delete elem->widget;
      elem->widget = nullptr;
    }
    if (elem->computed_style) {
      delete elem->computed_style;
      elem->computed_style = nullptr;
    }
  }
}

// ============================================================================
// CSS
// ============================================================================

void Box::load_css(const std::string& css) {
  style_engine_.parse_css(css);
  // 标记所有元素需要重新计算样式
  if (root_) {
    root_->mark_style_dirty();
  }
}

void Box::set_variable(const std::string& name, const std::string& value) {
  // TODO: 设置全局 CSS 变量
}

// ============================================================================
// 元素创建
// ============================================================================

Element* Box::create(const std::string& tag, const std::string& id) {
  auto elem = std::make_unique<Element>();
  elem->tag = tag;
  elem->id = id;
  elem->owner_box = this;  // Set owner for overlay access

  // 创建默认样式
  elem->computed_style = new ComputedStyle();

  Element* ptr = elem.get();
  elements_.push_back(std::move(elem));

  // 注册 ID
  if (!id.empty()) {
    elements_by_id_[id] = ptr;
  }

  return ptr;
}

Element* Box::create_with_widget(const std::string& tag, Widget* widget,
                                 const std::string& id) {
  Element* elem = create(tag, id);
  elem->widget = widget;
  elem->focusable = true;  // 带 Widget 的元素默认可聚焦
  return elem;
}

Element* Box::get_by_id(const std::string& id) {
  auto it = elements_by_id_.find(id);
  return it != elements_by_id_.end() ? it->second : nullptr;
}

void Box::set_root(Element* elem) {
  root_ = elem;
  if (root_) {
    // 标记需要完整的样式/布局/渲染流程
    root_->mark_style_dirty();
  }
}



// ============================================================================
// 视口
// ============================================================================

void Box::set_viewport(float width, float height) {
  if (viewport_width_ == width && viewport_height_ == height) {
    return;
  }

  viewport_width_ = width;
  viewport_height_ = height;

  // 视口改变 → 重新布局
  if (root_) {
    root_->mark_layout_dirty();
  }
}

// ============================================================================
// 更新和渲染
// ============================================================================

void Box::update() {
  if (!root_ || !flex_renderer_) {
    return;
  }

  // 第1步：计算样式（如果需要）
  if (has_dirty_style(root_)) {
    compute_styles(root_);
  }

  // 第2步：计算布局（如果需要）
  if (has_dirty_layout(root_)) {
    layout_engine_.layout(root_, viewport_width_, viewport_height_);
  }

  // 第3步：渲染（如果需要）
  if (has_dirty_paint(root_)) {
    // Begin frame
    flex_renderer_->begin_frame(viewport_width_, viewport_height_, 1.0f);

    // Clear with background color (dark gray)
    flex_renderer_->clear(Color{0.12f, 0.12f, 0.12f, 1.0f});

    // Render tree
    render_tree(root_);

    // End frame
    flex_renderer_->end_frame();
  }
}



void Box::invalidate() {
  if (root_) {
    root_->mark_paint_dirty();
  }
}

void Box::update_time(float delta_ms) {
  time_ms_ += delta_ms;

  // 更新 transitions
  transitions_.update(time_ms_);

  // 更新所有 Widget 和检查 transitions
  std::function<void(Element*)> update_widgets = [&](Element* elem) {
    if (elem->widget) {
      elem->widget->update(delta_ms, *elem);
    }
    // 如果元素有活跃的 transition，需要重绘
    if (transitions_.has_active(reinterpret_cast<intptr_t>(elem), time_ms_)) {
      elem->mark_paint_dirty();
    }
    for (auto* child : elem->children) {
      update_widgets(child);
    }
  };

  if (root_) {
    update_widgets(root_);
  }
}

// ============================================================================
// 脏标记检查
// ============================================================================

bool Box::has_dirty_style(Element* elem) {
  if (elem->dirty_style_) return true;
  for (auto* child : elem->children) {
    if (has_dirty_style(child)) return true;
  }
  return false;
}

bool Box::has_dirty_layout(Element* elem) {
  if (elem->dirty_layout_) return true;
  for (auto* child : elem->children) {
    if (has_dirty_layout(child)) return true;
  }
  return false;
}

bool Box::has_dirty_paint(Element* elem) {
  if (elem->dirty_paint_) return true;
  for (auto* child : elem->children) {
    if (has_dirty_paint(child)) return true;
  }
  return false;
}

// ============================================================================
// 样式计算（简化版）
// ============================================================================

void Box::compute_styles(Element* elem) {
  // 跳过干净的元素
  if (!elem->dirty_style_) {
    for (auto* child : elem->children) {
      compute_styles(child);
    }
    return;
  }

  // 确保有 ComputedStyle
  if (!elem->computed_style) {
    elem->computed_style = new ComputedStyle();
  }

  // 使用 StyleEngine 应用匹配的 CSS 规则
  style_engine_.apply_styles(elem);

  // 清除脏标记
  elem->dirty_style_ = false;

  // 递归处理子元素
  for (auto* child : elem->children) {
    compute_styles(child);
  }
}

// ============================================================================
// 渲染
// ============================================================================

void Box::render_element(Element* elem) {
  auto* style = elem->computed_style;
  if (!style) return;

  // 不可见元素跳过
  if (!elem->is_visible()) {
    return;
  }

  auto& r = renderer_->flex();

  // 保存变换状态
  r.save();

  // 累加变换（相对于父元素）
  r.translate(elem->x_ + style->transform_x, elem->y_ + style->transform_y);

  if (style->transform_scale != 1.0f) {
    float cx = elem->width_ / 2;
    float cy = elem->height_ / 2;
    r.translate(cx, cy);
    r.scale(style->transform_scale, style->transform_scale);
    r.translate(-cx, -cy);
  }

  if (style->transform_rotate != 0.0f) {
    float cx = elem->width_ / 2;
    float cy = elem->height_ / 2;
    r.translate(cx, cy);
    r.rotate(style->transform_rotate * 180.0f / 3.14159265f);  // Convert radians to degrees
    r.translate(-cx, -cy);
  }

  r.set_global_alpha(style->opacity);

  // 渲染元素自身内容
  if (elem->widget && renderer_) {
    elem->widget->render(*elem, *renderer_);
  } else {
    // 基础元素渲染
    float radius = style->border_radius[0];  // 使用第一个值（简化：四角相同）

    // 1. 阴影（渲染在背景之前）
    if (style->has_shadow && !style->shadow.inset) {
      auto& shadow = style->shadow;
      float sx = shadow.offset_x - shadow.spread_radius;
      float sy = shadow.offset_y - shadow.spread_radius;
      float sw = elem->width_ + shadow.spread_radius * 2;
      float sh = elem->height_ + shadow.spread_radius * 2;

      r.draw_rect(sx, sy, sw, sh, radius,
                  Paint::solid(shadow.color), Paint::none(), 0);
    }

    // 2. 背景
    Color bg_color = style->get_variable_color("--bg", style->background_color);
    if (bg_color.a > 0) {
      r.draw_rect(0, 0, elem->width_, elem->height_, radius,
                  Paint::solid(bg_color), Paint::none(), 0);
    }

    // 3. 文本
    if (!elem->text_content.empty()) {
      Color text_col = style->get_variable_color("--text-color", style->text_color);

      float text_x = style->padding[3];
      float text_y = (elem->height_ + style->font_size) / 2;

      r.draw_text(elem->text_content, text_x, text_y,
                  style->font_family, style->font_size, false, text_col);
    }
  }

  // 递归渲染所有子元素
  for (auto* child : elem->children) {
    render_element(child);
  }

  // 恢复变换状态
  r.restore();

  elem->dirty_paint_ = false;
}

void Box::render_tree(Element* elem) {
  if (!flex_renderer_ || !elem) return;

  // 渲染整个树
  render_element(elem);
}

void Box::render_overlays() {
  // Overlay rendering is now handled by widgets directly
  // using flex::Renderer's immediate mode API
}


// ============================================================================
// 事件处理
// ============================================================================

void Box::dispatch_event(Event& event) {
  // 第1步：路由事件（找到目标元素）
  Element* target = nullptr;

  if (event.type == EventType::MouseMove ||
      event.type == EventType::MouseDown ||
      event.type == EventType::MouseUp ||
      event.type == EventType::MouseWheel) {
    // Check if any widget wants to capture mouse events (e.g., open dropdown)
    Element* capturing = find_capturing_element(root_);
    if (capturing) {
      // Route mouse event to capturing widget first
      event.target = capturing;
      bool consumed = false;
      if (capturing->widget) {
        consumed = capturing->widget->handle_event(event, *capturing);
      }
      if (consumed) {
        event.handled = true;
        return;
      }
    }

    // 鼠标事件 - 通过坐标查找
    target = hit_test(root_, event.x, event.y);
  } else if (event.type == EventType::KeyDown ||
             event.type == EventType::KeyUp ||
             event.type == EventType::TextInput) {
    // 键盘事件 - 发送给焦点元素
    target = focused_element_;
  }

  event.target = target;

  // 第2步：特殊处理（状态更新）
  handle_special_events(event, target);

  // 第3步：事件传播（冒泡）
  if (target) {
    propagate_event(event, target);
  }
}

void Box::handle_special_events(Event& event, Element* target) {
  if (event.type == EventType::MouseMove) {
    // 更新 hover 状态
    if (target != hovered_element_) {
      // 移除旧元素的 hover
      if (hovered_element_) {
        hovered_element_->set_hover(false);
      }

      // 添加新元素的 hover
      hovered_element_ = target;
      if (hovered_element_) {
        hovered_element_->set_hover(true);
      }
    }
  }
  else if (event.type == EventType::MouseDown) {
    // 设置 active 状态
    if (target) {
      active_element_ = target;
      target->set_active(true);
    }

    // 更新焦点（如果元素可聚焦）
    if (target && target->focusable) {
      set_focus(target);
    }
  }
  else if (event.type == EventType::MouseUp) {
    // 移除 active 状态
    if (active_element_) {
      active_element_->set_active(false);
      active_element_ = nullptr;
    }
  }
}

void Box::propagate_event(Event& event, Element* target) {
  // 获取祖先链
  auto ancestors = get_ancestors(target);

  // 冒泡阶段：从目标到根
  for (Element* elem : ancestors) {
    // 如果已被处理且不传播，停止
    if (event.handled && !event.propagate) {
      break;
    }

    // 先给 Widget 处理
    if (elem->widget) {
      bool consumed = elem->widget->handle_event(event, *elem);
      if (consumed) {
        event.handled = true;
        event.propagate = false;
        break;
      }
    }

    // 如果 Widget 没消费，触发全局回调
    if (event_callback_) {
      event_callback_(*elem, event);
    }
  }
}

void Box::set_focus(Element* elem) {
  if (focused_element_ == elem) return;

  // 移除旧焦点
  if (focused_element_) {
    focused_element_->set_focus(false);

    Event focus_out = Event::focus_out();
    focus_out.target = focused_element_;
    if (focused_element_->widget) {
      focused_element_->widget->handle_event(focus_out, *focused_element_);
    }
  }

  // 设置新焦点
  focused_element_ = elem;
  if (focused_element_) {
    focused_element_->set_focus(true);

    Event focus_in = Event::focus_in();
    focus_in.target = focused_element_;
    if (focused_element_->widget) {
      focused_element_->widget->handle_event(focus_in, *focused_element_);
    }
  }
}

// ============================================================================
// Hit Testing
// ============================================================================

Element* Box::hit_test(Element* elem, float x, float y) {
  if (!elem) return nullptr;

  auto* style = elem->computed_style;
  if (!style) return nullptr;

  // 不可见元素跳过
  if (!elem->is_visible()) {
    return nullptr;
  }

  // 先测试子元素（从后往前，后渲染的在上层）
  for (auto it = elem->children.rbegin(); it != elem->children.rend(); ++it) {
    Element* hit = hit_test(*it, x, y);
    if (hit) return hit;
  }

  // 检查当前元素的边界 (使用绝对坐标)
  float ex = elem->absolute_x();
  float ey = elem->absolute_y();
  float ew = elem->width();
  float eh = elem->height();

  // 简单矩形测试
  if (x >= ex && x <= ex + ew && y >= ey && y <= ey + eh) {
    return elem;
  }

  return nullptr;
}

Element* Box::find_capturing_element(Element* elem) {
  if (!elem) return nullptr;

  // Check if this element's widget wants mouse capture
  if (elem->widget && elem->widget->wants_mouse_capture()) {
    return elem;
  }

  // Check children
  for (auto* child : elem->children) {
    Element* capturing = find_capturing_element(child);
    if (capturing) return capturing;
  }

  return nullptr;
}

std::vector<Element*> Box::get_ancestors(Element* elem) {
  std::vector<Element*> ancestors;
  Element* current = elem;
  while (current) {
    ancestors.push_back(current);
    current = current->parent;
  }
  return ancestors;
}

} // namespace tvgbox2
