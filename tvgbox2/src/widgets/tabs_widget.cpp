/*
 * tvgbox2 - TabsWidget Implementation
 */

#include <tvgbox2/widgets/tabs_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>

namespace tvgbox2 {

TabsWidget::TabsWidget() {}

void TabsWidget::add_tab(const std::string& label, const std::string& id, bool disabled) {
  tabs_.push_back({label, id, disabled});
  if (tabs_.size() == 1) {
    active_index_ = 0;
  }
  dirty_ = true;
}

void TabsWidget::remove_tab(const std::string& id) {
  auto it = std::find_if(tabs_.begin(), tabs_.end(),
    [&id](const Tab& t) { return t.id == id; });
  if (it != tabs_.end()) {
    int idx = std::distance(tabs_.begin(), it);
    tabs_.erase(it);
    if (active_index_ >= static_cast<int>(tabs_.size())) {
      active_index_ = tabs_.empty() ? -1 : tabs_.size() - 1;
    }
    dirty_ = true;
  }
}

void TabsWidget::clear_tabs() {
  tabs_.clear();
  active_index_ = -1;
  dirty_ = true;
}

void TabsWidget::set_active_index(int index) {
  if (index >= 0 && index < static_cast<int>(tabs_.size()) && !tabs_[index].disabled) {
    active_index_ = index;
    dirty_ = true;
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
      active_index_ = i;
      dirty_ = true;
      return;
    }
  }
}

float TabsWidget::get_tab_width(const Tab& tab, float font_size) const {
  return tab.label.size() * font_size * 0.6f + 32.0f;
}

void TabsWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  render_tabs(scene, elem);
  render_indicator(scene, elem);
}

void TabsWidget::render_tabs(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {245, 245, 245, 255};
  Color text_color = {100, 100, 100, 255};
  Color active_text = {0, 0, 0, 255};
  float font_size = 14.0f;
  std::string font_family = "Arial";

  if (style) {
    bg_color = style->get_variable_color("--tabs-bg", bg_color);
    text_color = style->get_variable_color("--tabs-text", text_color);
    active_text = style->get_variable_color("--tabs-active-text", active_text);
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, elem.width(), elem.height());
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(bg));

  float x = 0;
  for (size_t i = 0; i < tabs_.size(); i++) {
    const auto& tab = tabs_[i];
    float tab_width = get_tab_width(tab, font_size);

    if (static_cast<int>(i) == hover_index_ && static_cast<int>(i) != active_index_) {
      auto hover_bg = tvg::Shape::gen();
      hover_bg->appendRect(x, 0, tab_width, elem.height());
      hover_bg->fill(230, 230, 230, 255);
      scene->push(std::move(hover_bg));
    }

    auto text_shape = tvg::Text::gen();
    text_shape->font(font_family.c_str());
    text_shape->size(font_size);
    text_shape->text(tab.label.c_str());

    if (tab.disabled) {
      text_shape->fill(180, 180, 180);
    } else if (static_cast<int>(i) == active_index_) {
      text_shape->fill(active_text.r, active_text.g, active_text.b);
    } else {
      text_shape->fill(text_color.r, text_color.g, text_color.b);
    }

    float text_x = x + (tab_width - tab.label.size() * font_size * 0.6f) / 2;
    float text_y = elem.height() / 2 + font_size / 3;
    text_shape->translate(text_x, text_y);
    scene->push(std::move(text_shape));

    if (static_cast<int>(i) == active_index_) {
      target_indicator_x_ = x;
      target_indicator_width_ = tab_width;
    }

    x += tab_width;
  }
}

void TabsWidget::render_indicator(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color indicator_color = {59, 130, 246, 255};
  if (style) {
    indicator_color = style->get_variable_color("--tabs-indicator", indicator_color);
  }

  if (indicator_width_ > 0) {
    auto indicator = tvg::Shape::gen();
    indicator->appendRect(indicator_x_, elem.height() - 3, indicator_width_, 3, 1.5f, 1.5f);
    indicator->fill(indicator_color.r, indicator_color.g, indicator_color.b, indicator_color.a);
    scene->push(std::move(indicator));
  }
}

bool TabsWidget::handle_event(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style ? (style->font_size > 0 ? style->font_size : 14.0f) : 14.0f;

  switch (event.type) {
    case EventType::MouseDown: {
      float local_x = event.x - elem.absolute_x();
      float x = 0;
      for (size_t i = 0; i < tabs_.size(); i++) {
        float tab_width = get_tab_width(tabs_[i], font_size);
        if (local_x >= x && local_x < x + tab_width) {
          if (!tabs_[i].disabled && static_cast<int>(i) != active_index_) {
            set_active_index(i);
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
      float local_x = event.x - elem.absolute_x();
      float x = 0;
      int new_hover = -1;
      for (size_t i = 0; i < tabs_.size(); i++) {
        float tab_width = get_tab_width(tabs_[i], font_size);
        if (local_x >= x && local_x < x + tab_width) {
          new_hover = i;
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

} // namespace tvgbox2
