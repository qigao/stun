/*
 * flexUI - ToggleGroupWidget Implementation
 */

#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

ToggleGroupWidget::ToggleGroupWidget(const std::vector<Option>& options)
    : options_(options) {
    if (!options_.empty()) {
        selected_index_ = 0;
    }
}

void ToggleGroupWidget::set_options(const std::vector<Option>& options) {
    options_ = options;
    selected_index_ = options_.empty() ? -1 : 0;
    selected_indices_.clear();
    backgrounds_.clear();
    labels_.clear();
    dirty_ = true;
}

void ToggleGroupWidget::set_selected_index(int index) {
    if (index >= -1 && index < static_cast<int>(options_.size())) {
        selected_index_ = index;
        dirty_ = true;
        if (change_callback_ && index >= 0) {
            change_callback_(index, options_[index].id);
        }
    }
}

std::string ToggleGroupWidget::selected_id() const {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(options_.size())) {
        return options_[selected_index_].id;
    }
    return "";
}

void ToggleGroupWidget::set_selected_indices(const std::vector<int>& indices) {
    selected_indices_ = indices;
    dirty_ = true;
}

bool ToggleGroupWidget::is_selected(int index) const {
    if (multi_select_) {
        return std::find(selected_indices_.begin(), selected_indices_.end(), index) != selected_indices_.end();
    }
    return index == selected_index_;
}

void ToggleGroupWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    if (elem.width() == cached_width_ &&
        elem.height() == cached_height_ &&
        !backgrounds_.empty()) {
        return;
    }

    root_.clear();
    backgrounds_.clear();
    labels_.clear();

    cached_width_ = elem.width();
    cached_height_ = elem.height();

    if (options_.empty()) return;

    float gap = style->get_variable_float("--toggle-gap", 0.0f);
    float radius = style->get_variable_float("--toggle-radius", 4.0f);

    int count = static_cast<int>(options_.size());
    float total_gap = gap * (count - 1);
    float btn_width = (elem.width() - total_gap) / count;
    float btn_height = elem.height();

    for (int i = 0; i < count; ++i) {
        float x = i * (btn_width + gap);

        // Button background
        auto* bg = root_.add<RectShape>(x, 0, btn_width, btn_height, radius);
        backgrounds_.push_back(bg);

        // Button label
        float char_width = style->font_size * 0.6f;
        float text_width = options_[i].label.size() * char_width;
        float text_x = x + (btn_width - text_width) / 2;
        float text_y = (btn_height - style->font_size) / 2;

        auto* label = root_.add<TextShape>(text_x, text_y, options_[i].label);
        label->set_font_family(style->font_family);
        label->set_font_size(style->font_size);
        labels_.push_back(label);
    }
}

void ToggleGroupWidget::update_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    Color bg_normal = style->get_variable_color("--toggle-bg", color_from_u8(255, 255, 255, 255));
    Color bg_active = style->get_variable_color("--toggle-bg-active", color_from_u8(59, 130, 246, 255));
    Color text_normal = style->get_variable_color("--toggle-text", color_from_u8(100, 100, 100, 255));
    Color text_active = style->get_variable_color("--toggle-text-active", color_from_u8(255, 255, 255, 255));
    Color border = style->get_variable_color("--toggle-border", color_from_u8(200, 200, 200, 255));

    for (size_t i = 0; i < backgrounds_.size(); ++i) {
        bool selected = is_selected(static_cast<int>(i));

        backgrounds_[i]->set_fill(selected ? bg_active : bg_normal);
        backgrounds_[i]->set_stroke(border, 1.0f);

        if (i < labels_.size()) {
            labels_[i]->set_color(selected ? text_active : text_normal);
        }
    }
}

int ToggleGroupWidget::hit_test(float x, float y, const Element& elem) {
    auto* style = elem.computed_style;
    if (!style || options_.empty()) return -1;

    float gap = style->get_variable_float("--toggle-gap", 0.0f);
    int count = static_cast<int>(options_.size());
    float total_gap = gap * (count - 1);
    float btn_width = (elem.width() - total_gap) / count;

    for (int i = 0; i < count; ++i) {
        float btn_x = i * (btn_width + gap);
        if (x >= btn_x && x < btn_x + btn_width && y >= 0 && y < elem.height()) {
            return i;
        }
    }
    return -1;
}

void ToggleGroupWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    rebuild_shapes(elem);
    update_shapes(elem);

    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());
    float opacity = style->opacity;

    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool ToggleGroupWidget::handle_event(const Event& event, Element& elem) {
    if (elem.has_state("disabled")) return false;

    switch (event.type) {
        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                float local_x = event.x - elem.absolute_x();
                float local_y = event.y - elem.absolute_y();
                int index = hit_test(local_x, local_y, elem);

                if (index >= 0) {
                    if (multi_select_) {
                        auto it = std::find(selected_indices_.begin(), selected_indices_.end(), index);
                        if (it != selected_indices_.end()) {
                            selected_indices_.erase(it);
                        } else {
                            selected_indices_.push_back(index);
                        }
                    } else {
                        selected_index_ = index;
                    }

                    if (change_callback_) {
                        change_callback_(index, options_[index].id);
                    }

                    elem.mark_paint_dirty();
                    return true;
                }
            }
            break;

        case EventType::MouseMove: {
            float local_x = event.x - elem.absolute_x();
            float local_y = event.y - elem.absolute_y();
            int index = hit_test(local_x, local_y, elem);
            if (index != hovered_index_) {
                hovered_index_ = index;
                elem.mark_paint_dirty();
            }
            break;
        }

        default:
            break;
    }

    return false;
}

void ToggleGroupWidget::update(float delta_ms, Element& elem) {
    (void)delta_ms;
    (void)elem;
}

} // namespace flexUI
