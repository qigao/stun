/*
 * flexUI - TabsWidget Implementation
 *
 * Tab navigation with integrated page management.
 */

#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

namespace {

constexpr const char* kPageStateAttribute = "data-state";
constexpr const char* kPageAriaHiddenAttribute = "aria-hidden";

void set_page_state(Element* page, bool active) {
  if (!page) {
    return;
  }

  page->set_attribute(kPageStateAttribute, active ? "active" : "inactive");
  page->set_attribute(kPageAriaHiddenAttribute, active ? "false" : "true");
  // Keep event dispatch and painting correct until CSS is recomputed.
  page->set_visible(active);
}

void release_page_state(Element* page) {
  if (!page) {
    return;
  }

  page->remove_attribute(kPageStateAttribute);
  page->remove_attribute(kPageAriaHiddenAttribute);
  page->set_visible(true);
}

std::vector<float> resolve_tab_widths(const std::vector<TabsWidget::Tab>& tabs,
                                      const ComputedStyle* style, float font_size,
                                      float available_width) {
  std::vector<float> widths;
  widths.reserve(tabs.size());
  if (tabs.empty()) {
    return widths;
  }

  float desired_total = 0.0f;
  for (const auto& tab : tabs) {
    ComputedStyle measure_style;
    if (style) {
      measure_style = *style;
    }
    measure_style.font_size = font_size;
    const float desired =
        approximate_segmented_text_width(&measure_style, tab.label) + 32.0f;
    widths.push_back(desired);
    desired_total += desired;
  }

  if (available_width <= 0.0f || desired_total <= available_width) {
    return widths;
  }

  const float even_width = available_width / static_cast<float>(tabs.size());
  for (auto& width : widths) {
    width = even_width;
  }
  return widths;
}

} // namespace

TabsWidget::TabsWidget() {}

bool TabsWidget::measure_intrinsic_size(const Element& elem, float available_width,
                                        float available_height, float& out_width,
                                        float& out_height) const {
  (void)available_height;
  const auto* style = elem.computed_style;
  const float font_size =
      style && style->font_size > 0.0f ? style->font_size : 14.0f;
  const auto widths = resolve_tab_widths(tabs_, style, font_size, available_width);

  float total_width = 8.0f;
  for (float width : widths) {
    total_width += width;
  }
  if (widths.size() > 1) {
    total_width += 4.0f * static_cast<float>(widths.size() - 1);
  }

  out_width = std::max(total_width, 140.0f);
  out_height = 36.0f;
  return true;
}

void TabsWidget::sync_host_semantics() {
  set_host_attribute("role", "tablist");
  set_host_attribute("data-orientation", "horizontal");
  set_host_attribute("aria-orientation", "horizontal");
  const std::string active = active_id();
  if (active.empty()) {
    clear_host_attribute("data-active-id");
    clear_host_attribute("data-active-index");
    clear_host_attribute("aria-activedescendant");
    set_host_attribute("data-state", "empty");
  } else {
    set_host_attribute("data-state", "active");
    set_host_attribute("data-active-id", active);
    set_host_attribute("data-active-index", std::to_string(active_index_));
    set_host_attribute("aria-activedescendant", active);
  }
}

void TabsWidget::add_tab(const std::string& label, const std::string& id, Element* page, bool disabled) {
  tabs_.push_back({label, id, page, disabled});
  if (tabs_.size() == 1) {
    active_index_ = 0;
  }
  update_page_visibility();
  dirty_ = true;
  sync_host_semantics();
}

void TabsWidget::remove_tab(const std::string& id) {
  auto it = std::find_if(tabs_.begin(), tabs_.end(),
    [&id](const Tab& t) { return t.id == id; });
  if (it != tabs_.end()) {
    const int removed_index = static_cast<int>(std::distance(tabs_.begin(), it));
    Element* removed_page = it->page;
    tabs_.erase(it);
    release_page_state(removed_page);
    if (tabs_.empty()) {
      active_index_ = -1;
    } else if (removed_index < active_index_) {
      --active_index_;
    } else if (active_index_ >= static_cast<int>(tabs_.size())) {
      active_index_ = tabs_.empty() ? -1 : static_cast<int>(tabs_.size()) - 1;
    }
    update_page_visibility();
    dirty_ = true;
    sync_host_semantics();
  }
}

void TabsWidget::clear_tabs() {
  for (const auto& tab : tabs_) {
    release_page_state(tab.page);
  }
  tabs_.clear();
  active_index_ = -1;
  dirty_ = true;
  sync_host_semantics();
}

void TabsWidget::set_tab_page(int index, Element* page) {
  if (index >= 0 && index < static_cast<int>(tabs_.size())) {
    if (tabs_[index].page != page) {
      release_page_state(tabs_[index].page);
    }
    tabs_[index].page = page;
    update_page_visibility();
  }
}

void TabsWidget::set_tab_page(const std::string& id, Element* page) {
  for (auto& tab : tabs_) {
    if (tab.id == id) {
      if (tab.page != page) {
        release_page_state(tab.page);
      }
      tab.page = page;
      update_page_visibility();
      return;
    }
  }
}

Element* TabsWidget::get_tab_page(int index) const {
  if (index >= 0 && index < static_cast<int>(tabs_.size())) {
    return tabs_[index].page;
  }
  return nullptr;
}

Element* TabsWidget::get_tab_page(const std::string& id) const {
  for (const auto& tab : tabs_) {
    if (tab.id == id) {
      return tab.page;
    }
  }
  return nullptr;
}

void TabsWidget::set_active_index(int index) {
  if (index >= 0 && index < static_cast<int>(tabs_.size()) && !tabs_[index].disabled) {
    if (index != active_index_) {
      active_index_ = index;
      update_page_visibility();
      dirty_ = true;
      sync_host_semantics();
      // Note: Element dirty marking should be done by the caller in handle_event
      // but we add it here for safety if called from elsewhere.
    }
  }
}

const std::string& TabsWidget::active_id() const {
  static std::string empty;
  if (active_index_ >= 0 && active_index_ < static_cast<int>(tabs_.size())) {
    return tabs_[active_index_].id;
  }
  return empty;
}

void TabsWidget::set_active_id(const std::string& id) {
  for (size_t i = 0; i < tabs_.size(); i++) {
    if (tabs_[i].id == id && !tabs_[i].disabled) {
      if (static_cast<int>(i) != active_index_) {
        active_index_ = static_cast<int>(i);
        update_page_visibility();
        dirty_ = true;
        sync_host_semantics();
      }
      return;
    }
  }
}

void TabsWidget::update_page_visibility() {
  for (size_t i = 0; i < tabs_.size(); i++) {
    set_page_state(tabs_[i].page, static_cast<int>(i) == active_index_);
  }
}

float TabsWidget::get_tab_width(const Tab& tab, float font_size) const {
  ComputedStyle measure_style;
  measure_style.font_size = font_size;
  return approximate_segmented_text_width(&measure_style, tab.label) + 32.0f;
}

void TabsWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  render_tabs(commands, elem);
  render_indicator(commands, elem);
}

void TabsWidget::render_tabs(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {0.96f, 0.96f, 0.96f, 1.0f};
  Color text_color = {0.39f, 0.39f, 0.39f, 1.0f};
  Color active_text = {0.0f, 0.0f, 0.0f, 1.0f};
  float font_size = 14.0f;
  if (style) {
    bg_color = style->get_variable_color("--tabs-bg", bg_color);
    text_color = style->get_variable_color("--tabs-text", text_color);
    active_text = style->get_variable_color("--tabs-active-text", active_text);
    font_size = style->font_size > 0 ? style->font_size : font_size;
  }

  commands.draw_rect(0, 0, elem.width(), elem.height(), 0,
                     Paint::solid(bg_color), Paint::none(), 0);

  const auto tab_widths = resolve_tab_widths(tabs_, style, font_size, elem.width());
  float x = 0;
  for (size_t i = 0; i < tabs_.size(); i++) {
    const auto& tab = tabs_[i];
    const float tab_width = tab_widths[i];

    if (static_cast<int>(i) == hover_index_ && static_cast<int>(i) != active_index_) {
      commands.draw_rect(x, 0, tab_width, elem.height(), 0,
                         Paint::solid(Color{0.90f, 0.90f, 0.90f, 1.0f}),
                         Paint::none(), 0);
    }

    Color tab_text_color;
    if (tab.disabled) {
      tab_text_color = {0.71f, 0.71f, 0.71f, 1.0f};
    } else if (static_cast<int>(i) == active_index_) {
      tab_text_color = active_text;
    } else {
      tab_text_color = text_color;
    }

    if (style) {
      const auto text_block = layout_text_block(
          style, tab.label, x, 0.0f, tab_width, elem.height(), tab_text_color,
          TextVerticalAlign::Middle);
      emit_text_block(commands, text_block);
    }

    if (static_cast<int>(i) == active_index_) {
      target_indicator_x_ = x;
      target_indicator_width_ = tab_width;
    }

    x += tab_width;
  }
}

void TabsWidget::render_indicator(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;

  Color indicator_color = {0.23f, 0.51f, 0.96f, 1.0f};
  if (style) {
    indicator_color = style->get_variable_color("--tabs-indicator", indicator_color);
  }

  if (indicator_width_ > 0) {
    commands.draw_rect(indicator_x_, elem.height() - 3, indicator_width_, 3, 1.5f,
                       Paint::solid(indicator_color), Paint::none(), 0);
  }
}

bool TabsWidget::handle_event(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style ? (style->font_size > 0 ? style->font_size : 14.0f) : 14.0f;
  const auto tab_widths = resolve_tab_widths(tabs_, style, font_size, elem.width());

  switch (event.type) {
    case EventType::MouseDown: {
      const flex::Vec2 local_pos =
          detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
      float local_x = local_pos.x;
      float x = 0;
      for (size_t i = 0; i < tabs_.size(); i++) {
        const float tab_width = tab_widths[i];
        if (local_x >= x && local_x < x + tab_width) {
          if (!tabs_[i].disabled && static_cast<int>(i) != active_index_) {
            set_active_index(static_cast<int>(i));
            elem.mark_layout_dirty();
            elem.mark_paint_dirty();
            if (on_change_) on_change_(active_index_, tabs_[active_index_].id);
          }
          return true;
        }
        x += tab_width;
      }
      break;
    }

    case EventType::MouseMove: {
      const flex::Vec2 local_pos =
          detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
      float local_x = local_pos.x;
      float x = 0;
      int new_hover = -1;
      for (size_t i = 0; i < tabs_.size(); i++) {
        const float tab_width = tab_widths[i];
        if (local_x >= x && local_x < x + tab_width) {
          new_hover = static_cast<int>(i);
          break;
        }
        x += tab_width;
      }
      if (new_hover != hover_index_) {
        hover_index_ = new_hover;
        elem.mark_paint_dirty();
      }
      break;
    }

    default:
      break;
  }

  return false;
}

bool TabsWidget::needs_frame_update(const Element& elem) const {
  (void)elem;
  return std::abs(indicator_x_ - target_indicator_x_) > 0.5f ||
         std::abs(indicator_width_ - target_indicator_width_) > 0.5f;
}

void TabsWidget::update(float delta_ms, Element& elem) {
  float speed = 8.0f * delta_ms / 1000.0f;

  bool changed = false;
  if (std::abs(indicator_x_ - target_indicator_x_) > 0.5f) {
    indicator_x_ += (target_indicator_x_ - indicator_x_) * speed;
    changed = true;
  } else {
    indicator_x_ = target_indicator_x_;
  }

  if (std::abs(indicator_width_ - target_indicator_width_) > 0.5f) {
    indicator_width_ += (target_indicator_width_ - indicator_width_) * speed;
    changed = true;
  } else {
    indicator_width_ = target_indicator_width_;
  }

  if (changed) {
    elem.mark_paint_dirty();
  }
}

} // namespace flexUI
