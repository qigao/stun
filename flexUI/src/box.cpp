/*
 * flexUI - Box Implementation
 */

#include <flexUI/box.h>
#include <flexUI/renderer.h>
#include <flexUI/types.h>
#include <flex/bridge/renderer.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fmtlog.h>

// Profiling macro - set to 1 to enable
#define FLEXUI_PROFILE 1

#if FLEXUI_PROFILE
#define PROFILE_START(name) auto _profile_##name = std::chrono::high_resolution_clock::now()
#define PROFILE_END(name, threshold_ms) do { \
    auto _end = std::chrono::high_resolution_clock::now(); \
    double _ms = std::chrono::duration<double, std::milli>(_end - _profile_##name).count(); \
    if (_ms > threshold_ms) logi("[PERF] {} took {:.2f}ms", #name, _ms); \
} while(0)
#else
#define PROFILE_START(name)
#define PROFILE_END(name, threshold_ms)
#endif

namespace flexUI {

// ============================================================================
// 构造/析构
// ============================================================================

Box::Box(flex::Renderer* renderer) : flex_renderer_(renderer) {
  // 初始化 Renderer wrapper
  renderer_ = std::make_unique<Renderer>(renderer);
}

Box::~Box() {
  // 清理所有元素（unique_ptr 自动删除）
  // Widget 需要手动删除，computed_style 已内嵌无需删除
  for (auto& elem_ptr : elements_) {
    auto* elem = elem_ptr.get();
    if (elem->widget) {
      delete elem->widget;
      elem->widget = nullptr;
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
  elem->set_tag(tag);
  elem->set_element_id(id);
  elem->owner_box_ = this;

  // computed_style 已内嵌，无需 new

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
  // 自动注册需要 update_time 的 Widget
  active_widgets_.push_back(elem);
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

// ============================================================================
// 同步 CSS 属性到 flex 布局属性
// ============================================================================

// Map flexUI::Position to flex::PositionMode
flex::PositionMode to_flex_position(Position p) {
  switch (p) {
    case Position::Static:   return flex::PositionMode::Static;
    case Position::Relative: return flex::PositionMode::Relative;
    case Position::Absolute: return flex::PositionMode::Absolute;
    case Position::Fixed:    return flex::PositionMode::Fixed;
    default:                 return flex::PositionMode::Static;
  }
}

// Map flexUI::BoxSizing to flex::BoxSizing
flex::BoxSizing to_flex_box_sizing(BoxSizing b) {
  switch (b) {
    case BoxSizing::ContentBox: return flex::BoxSizing::ContentBox;
    case BoxSizing::BorderBox:  return flex::BoxSizing::BorderBox;
    default:                    return flex::BoxSizing::ContentBox;
  }
}

void sync_layout_to_flex(Element* elem, float container_w, float container_h) {
  if (!elem) return;

  auto* style = elem->computed_style;
  if (!style) return;

  // Skip display:none
  if (style->display == Display::None) {
    elem->set_visible(false);
    return;
  }
  elem->set_visible(style->visibility == Visibility::Visible);
  elem->set_opacity(style->opacity);

  // ========== 同步容器布局属性到 Group ==========

  // Display mode: Block behaves like flex-direction: column
  if (style->display == Display::Flex) {
    elem->set_layout(flex::LayoutMode::Flex);
  } else if (style->display == Display::Block) {
    // Block = vertical stacking = flex column
    elem->set_layout(flex::LayoutMode::Flex);
    elem->set_flex_direction(flex::FlexDirection::Column);
  } else {
    elem->set_layout(flex::LayoutMode::None);
  }

  // Flex container properties (only apply if actually flex)
  if (style->display == Display::Flex) {
    elem->set_flex_direction(style->flex_direction);
    elem->set_justify_content(style->justify_content);
    elem->set_align_items(style->align_items);
  }
  elem->set_gap(style->gap);

  // Padding
  elem->set_padding(style->padding[0], style->padding[1],
                    style->padding[2], style->padding[3]);

  // ========== 同步子项布局属性到 Node ==========

  // flex-grow, flex-shrink, flex-basis
  float flex_grow = style->get_variable_float(Symbol("flex-grow"), 0.0f);
  float flex_shrink = style->get_variable_float(Symbol("flex-shrink"), 1.0f);
  float flex_basis = style->get_variable_float(Symbol("flex-basis"), 0.0f);
  elem->set_flex(flex_grow, flex_shrink, flex_basis);

  // Position mode (static/relative/absolute/fixed)
  elem->set_position_mode(to_flex_position(style->position));

  // Position offsets (top/right/bottom/left)
  elem->set_position_offsets(style->top, style->right, style->bottom, style->left);

  // Z-index
  elem->set_z_index(style->z_index);

  // Box sizing
  elem->set_box_sizing(to_flex_box_sizing(style->box_sizing));

  // Margin
  elem->set_margin(style->margin[0], style->margin[1], style->margin[2], style->margin[3]);

  // Border width
  elem->set_border_width(style->border_width[0], style->border_width[1],
                         style->border_width[2], style->border_width[3]);

  // ========== 设置尺寸 ==========

  // Width: resolve percentages immediately against container
  float width = 0;
  bool auto_width = false;
  if (style->width_is_percent && style->width > 0) {
    width = container_w * style->width / 100.0f;
    elem->set_layout_width(width);
  } else if (style->width > 0) {
    width = style->width;
    elem->set_layout_width(width);
  } else {
    auto_width = true;
    // Auto width - use container width as preliminary value for children
    // Final width will be computed from children for containers
    width = container_w;  // Use container as preliminary for children
    if (style->display == Display::Block) {
      elem->set_layout_width(width);
    } else {
      elem->set_layout_width(0);  // Will compute from children later
    }
  }

  // Height: resolve percentages immediately against container
  float height = 0;
  bool auto_height = false;
  if (style->height_is_percent && style->height > 0) {
    height = container_h * style->height / 100.0f;
    elem->set_layout_height(height);
  } else if (style->height > 0) {
    height = style->height;
    elem->set_layout_height(height);
  } else {
    auto_height = true;  // Will compute from children
    height = elem->layout_height();  // May be 0 initially
  }

  // ========== 递归处理子元素 (先处理子元素以计算 auto height) ==========

  // Content area for children
  float content_w = width - style->padding[1] - style->padding[3];
  float content_h = height - style->padding[0] - style->padding[2];
  if (content_w < 0) content_w = 0;
  if (content_h < 0) content_h = 0;

  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      sync_layout_to_flex(child, content_w, content_h);
    }
  }

  // ========== Auto height: compute from children ==========

  if (auto_height) {
    float children_height = 0;
    bool is_flex = (style->display == Display::Flex);
    bool is_row = (style->flex_direction == FlexDirection::Row ||
                   style->flex_direction == FlexDirection::RowReverse);

    int in_flow_count = 0;

    for (auto* node : elem->children()) {
      auto* child = static_cast<Element*>(node);
      if (!child || !child->is_visible()) continue;

      // Skip absolute/fixed positioned elements
      auto* cs = child->computed_style;
      if (cs && (cs->position == Position::Absolute || cs->position == Position::Fixed)) {
        continue;
      }

      float h = child->layout_height();
      if (cs) h += cs->margin[0] + cs->margin[2];

      if (is_flex && is_row) {
        // Row flex: height = max child height
        children_height = std::max(children_height, h);
      } else {
        // Block or column flex: height = sum of children
        children_height += h;
      }
      in_flow_count++;
    }

    // Add gap for column layout
    if (!is_row && in_flow_count > 1) {
      children_height += style->gap * (in_flow_count - 1);
    }

    float computed_height = children_height + style->padding[0] + style->padding[2];

    // Minimum height for leaf nodes with text
    if (elem->children().empty() && !elem->text().empty()) {
      float min_text_height = style->font_size > 0 ? style->font_size * 1.5f : 24.0f;
      min_text_height += style->padding[0] + style->padding[2];
      computed_height = std::max(computed_height, min_text_height);
    } else if (computed_height <= 0 && elem->children().empty()) {
      // Fallback for empty leaf nodes
      computed_height = 24.0f;
    }

    elem->set_layout_height(computed_height);
  }

  // ========== Auto width: compute from children for containers ==========
  if (auto_width && !elem->children().empty()) {
    float children_width = 0;
    bool is_flex = (style->display == Display::Flex);
    bool is_row = (style->flex_direction == FlexDirection::Row ||
                   style->flex_direction == FlexDirection::RowReverse);

    int in_flow_count = 0;

    for (auto* node : elem->children()) {
      auto* child = static_cast<Element*>(node);
      if (!child || !child->is_visible()) continue;

      // Skip absolute/fixed positioned elements
      auto* cs = child->computed_style;
      if (cs && (cs->position == Position::Absolute || cs->position == Position::Fixed)) {
        continue;
      }

      float w = child->layout_width();
      if (cs) w += cs->margin[1] + cs->margin[3];

      if (is_flex && is_row) {
        // Row flex: width = sum of children
        children_width += w;
      } else {
        // Block or column flex: width = max child width
        children_width = std::max(children_width, w);
      }
      in_flow_count++;
    }

    // Add gap for row layout
    if (is_row && in_flow_count > 1) {
      children_width += style->gap * (in_flow_count - 1);
    }

    float computed_width = children_width + style->padding[1] + style->padding[3];
    elem->set_layout_width(computed_width);
  }

  // ========== Auto width for leaf nodes with text ==========
  if (auto_width && elem->children().empty() && !elem->text().empty()) {
    // Estimate text width: text_length * font_size * 0.6 (rough approximation)
    float fs = style->font_size > 0 ? style->font_size : 14.0f;
    float text_width = elem->text().length() * fs * 0.6f;
    float computed_width = text_width + style->padding[1] + style->padding[3];

    // Minimum width
    if (computed_width < fs * 2) {
      computed_width = fs * 2;
    }

    elem->set_layout_width(computed_width);
  }
}

// 递归执行布局
void perform_layout_recursive(Element* elem) {
  if (!elem) return;

  // 执行当前元素的布局
  // perform_layout() 现在处理 absolute/fixed 定位和 flexbox
  elem->perform_layout();

  // 递归处理子元素
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      perform_layout_recursive(child);
    }
  }
}

void Box::update() {
  PROFILE_START(update_total);

  if (!root_ || !flex_renderer_) {
    return;
  }

  // 第1步：计算样式（如果需要）- O(1) 检查
  if (subtree_dirty_style_) {
    PROFILE_START(compute_styles);
    compute_styles(root_);
    PROFILE_END(compute_styles, 1.0);
    subtree_dirty_style_ = false;
  }

  // 第2步：计算布局（如果需要）- O(1) 检查
  if (subtree_dirty_layout_) {
    PROFILE_START(layout_total);
    // 2.1 设置根元素位置
    root_->set_x(0);
    root_->set_y(0);

    // 2.2 同步 CSS 属性到 flex 布局属性
    PROFILE_START(sync_layout);
    sync_layout_to_flex(root_, viewport_width_, viewport_height_);
    PROFILE_END(sync_layout, 1.0);

    // 2.3 执行 flex 布局 (递归)
    PROFILE_START(perform_layout);
    perform_layout_recursive(root_);
    PROFILE_END(perform_layout, 1.0);

    PROFILE_END(layout_total, 2.0);
    subtree_dirty_layout_ = false;
  }

  // 第3步：渲染（如果需要）- O(1) 检查
  if (subtree_dirty_paint_) {
    PROFILE_START(render_total);

    // Begin frame
    PROFILE_START(begin_frame);
    flex_renderer_->begin_frame(viewport_width_, viewport_height_, 1.0f);
    PROFILE_END(begin_frame, 1.0);

    // Clear with background color (dark gray)
    flex_renderer_->clear(Color{0.12f, 0.12f, 0.12f, 1.0f});

    // Render tree (normal elements)
    PROFILE_START(render_tree);
    render_tree(root_);
    PROFILE_END(render_tree, 5.0);

    // Render overlays (dropdowns, popups, etc.) - always on top
    PROFILE_START(render_overlays);
    render_overlays(root_);
    PROFILE_END(render_overlays, 1.0);

    // End frame
    PROFILE_START(end_frame);
    flex_renderer_->end_frame();
    PROFILE_END(end_frame, 5.0);

    PROFILE_END(render_total, 10.0);
    subtree_dirty_paint_ = false;
  }

  PROFILE_END(update_total, 16.0);
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

  // 只更新活跃的 Widget - O(k) where k = active widgets count
  for (auto* elem : active_widgets_) {
    if (elem && elem->widget) {
      elem->widget->update(delta_ms, *elem);
    }
    // 如果元素有活跃的 transition，需要重绘
    if (transitions_.has_active(reinterpret_cast<intptr_t>(elem), time_ms_)) {
      elem->mark_paint_dirty();
    }
  }
}

void Box::register_active_widget(Element* elem) {
  // 避免重复添加
  auto it = std::find(active_widgets_.begin(), active_widgets_.end(), elem);
  if (it == active_widgets_.end()) {
    active_widgets_.push_back(elem);
  }
}

void Box::unregister_active_widget(Element* elem) {
  auto it = std::find(active_widgets_.begin(), active_widgets_.end(), elem);
  if (it != active_widgets_.end()) {
    active_widgets_.erase(it);
  }
}

// ============================================================================
// 脏标记检查
// ============================================================================

bool Box::has_dirty_style(Element* elem) {
  if (elem->dirty_style()) return true;
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      if (has_dirty_style(child)) return true;
    }
  }
  return false;
}

bool Box::has_dirty_layout(Element* elem) {
  if (elem->dirty_layout()) return true;
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      if (has_dirty_layout(child)) return true;
    }
  }
  return false;
}

bool Box::has_dirty_paint(Element* elem) {
  if (elem->dirty_paint()) return true;
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      if (has_dirty_paint(child)) return true;
    }
  }
  return false;
}

// ============================================================================
// 样式计算（简化版）
// ============================================================================

void Box::compute_styles(Element* elem) {
  // 跳过干净的元素
  if (!elem->dirty_style()) {
    for (auto* node : elem->children()) {
      if (auto* child = static_cast<Element*>(node)) {
        compute_styles(child);
      }
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
  elem->clear_dirty(flex::DirtyFlags::Content);

  // 递归处理子元素
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      compute_styles(child);
    }
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
  r.translate(elem->x() + style->transform_x, elem->y() + style->transform_y);

  if (style->transform_scale != 1.0f) {
    float cx = elem->width() / 2;
    float cy = elem->height() / 2;
    r.translate(cx, cy);
    r.scale(style->transform_scale, style->transform_scale);
    r.translate(-cx, -cy);
  }

  if (style->transform_rotate != 0.0f) {
    float cx = elem->width() / 2;
    float cy = elem->height() / 2;
    r.translate(cx, cy);
    r.rotate(style->transform_rotate * 180.0f / 3.14159265f);  // Convert radians to degrees
    r.translate(-cx, -cy);
  }

  r.set_global_alpha(style->opacity);
  
  // 基础元素渲染 (Background & Shadow)
  float radius = style->border_radius[0];  // 使用第一个值（简化：四角相同）

  // 1. 阴影（渲染在背景之前）
  if (style->has_shadow && !style->shadow.inset) {
    auto& shadow = style->shadow;
    float sx = shadow.offset_x - shadow.spread_radius;
    float sy = shadow.offset_y - shadow.spread_radius;
    float sw = elem->width() + shadow.spread_radius * 2;
    float sh = elem->height() + shadow.spread_radius * 2;

    r.draw_rect(sx, sy, sw, sh, radius,
                Paint::solid(shadow.color), Paint::none(), 0);
  }

  // 2. 背景
  Color bg_color = style->get_variable_color("--bg", style->background_color);
  if (bg_color.a > 0) {
    r.draw_rect(0, 0, elem->width(), elem->height(), radius,
                Paint::solid(bg_color), Paint::none(), 0);
  }

  // 3. 渲染 Widget 内容
  if (elem->widget && renderer_) {
    elem->widget->render(*elem, *renderer_);
  } 

  // 4. 文本 (Always draw text if present)
  if (!elem->text().empty()) {
    Color text_col = style->get_variable_color("--text-color", style->text_color);

    float text_x = style->padding[3];
    float text_y = (elem->height() + style->font_size) / 2;

    r.draw_text(elem->text(), text_x, text_y,
                style->font_family, style->font_size, false, text_col);
  }

  // 递归渲染所有子元素
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      render_element(child);
    }
  }

  // 恢复变换状态
  r.restore();

  elem->clear_dirty(flex::DirtyFlags::Visual);
}

void Box::render_tree(Element* elem) {
  if (!flex_renderer_ || !elem) return;

  // 渲染整个树
  render_element(elem);
}

void Box::render_overlays(Element* elem) {
  if (!elem || !renderer_) return;

  // Skip invisible elements
  if (!elem->is_visible()) return;

  // 渲染当前元素的 overlay
  if (elem->widget && elem->widget->has_overlay()) {
    auto& r = renderer_->flex();
    r.save();
    r.translate(elem->absolute_x(), elem->absolute_y());
    elem->widget->render_overlay(*elem, *renderer_);
    r.restore();
  }

  // 递归处理子元素
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      render_overlays(child);
    }
  }
}


// ============================================================================
// 事件处理
// ============================================================================

void Box::dispatch_event(Event& event) {
  PROFILE_START(dispatch_event);

  // 第1步：路由事件（找到目标元素）
  Element* target = nullptr;

  if (event.type == EventType::MouseMove ||
      event.type == EventType::MouseDown ||
      event.type == EventType::MouseUp ||
      event.type == EventType::MouseWheel) {
    // Check cached capturing element - O(1) instead of O(n) tree traversal
    if (capturing_element_) {
      // Route mouse event to capturing widget first
      event.target = capturing_element_;
      bool consumed = false;
      if (capturing_element_->widget) {
        consumed = capturing_element_->widget->handle_event(event, *capturing_element_);
      }
      if (consumed) {
        event.handled = true;
        return;
      }
    }

    // 鼠标事件 - 通过坐标查找
    PROFILE_START(hit_test);
    target = hit_test(root_, event.x, event.y);
    PROFILE_END(hit_test, 1.0);
  } else if (event.type == EventType::KeyDown ||
             event.type == EventType::KeyUp ||
             event.type == EventType::TextInput) {
    // 键盘事件 - 发送给焦点元素
    target = focused_element_;
  }

  event.target = target;

  // 第2步：特殊处理（状态更新）
  PROFILE_START(special_events);
  handle_special_events(event, target);
  PROFILE_END(special_events, 1.0);

  // 第3步：事件传播（冒泡）
  if (target) {
    PROFILE_START(propagate);
    propagate_event(event, target);
    PROFILE_END(propagate, 1.0);
  }

  // 第4步：更新鼠标捕获缓存
  // 检查目标元素的 widget 是否改变了捕获状态
  if (target && target->widget) {
    if (target->widget->wants_mouse_capture()) {
      capturing_element_ = target;
    } else if (capturing_element_ == target) {
      capturing_element_ = nullptr;
    }
  }
  // 检查之前的捕获元素是否释放了捕获
  if (capturing_element_ && capturing_element_->widget &&
      !capturing_element_->widget->wants_mouse_capture()) {
    capturing_element_ = nullptr;
  }

  PROFILE_END(dispatch_event, 5.0);
}

void Box::handle_special_events(Event& event, Element* target) {
  if (event.type == EventType::MouseMove) {
    // 更新 hover 状态
    if (target != hovered_element_) {
      // 移除旧元素的 hover
      if (hovered_element_) {
        hovered_element_->set_hover(false);
        Event e = Event::mouse_move(event.x, event.y);
        e.type = EventType::MouseLeave;
        e.target = hovered_element_;
        if (hovered_element_->widget) {
          hovered_element_->widget->handle_event(e, *hovered_element_);
        }
      }

      // 添加新元素的 hover
      hovered_element_ = target;
      if (hovered_element_) {
        hovered_element_->set_hover(true);
        Event e = Event::mouse_move(event.x, event.y);
        e.type = EventType::MouseEnter;
        e.target = hovered_element_;
        if (hovered_element_->widget) {
          hovered_element_->widget->handle_event(e, *hovered_element_);
        }
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
      // If we released over the same element we pressed, trigger click
      if (active_element_ == target && active_element_->click_callback()) {
        active_element_->click_callback()();
      }
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
  const auto& ch = elem->children();
  for (auto it = ch.rbegin(); it != ch.rend(); ++it) {
    if (auto* child = static_cast<Element*>(*it)) {
      Element* hit = hit_test(child, x, y);
      if (hit) return hit;
    }
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
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      Element* capturing = find_capturing_element(child);
      if (capturing) return capturing;
    }
  }

  return nullptr;
}

std::vector<Element*> Box::get_ancestors(Element* elem) {
  std::vector<Element*> ancestors;
  Element* current = elem;
  while (current) {
    ancestors.push_back(current);
    current = current->parent_elem();
  }
  return ancestors;
}

} // namespace flexUI
