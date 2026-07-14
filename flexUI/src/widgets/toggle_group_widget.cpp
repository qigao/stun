/*
 * flexUI - ToggleGroupWidget Implementation
 */

#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/text_layout.h>
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

    const bool has_single_selection =
        !multi_select_ && selected_index_ >= 0 &&
        selected_index_ < static_cast<int>(options_.size());
    set_host_attribute("role", multi_select_ ? "group" : "radiogroup");
    set_host_attribute("data-orientation", "horizontal");
    set_host_attribute("aria-orientation", "horizontal");
    if (options_.empty()) {
        set_host_attribute("data-state", "empty");
    } else {
        set_host_attribute("data-state", has_single_selection ? "selected" : "unselected");
    }
    set_host_state("selected", has_single_selection);
    set_host_attribute("data-selected-count", has_single_selection ? "1" : "0");
    if (has_single_selection) {
        set_host_attribute("data-value", options_[selected_index_].id);
        set_host_attribute("data-selected-index", std::to_string(selected_index_));
    } else {
        clear_host_attribute("data-value");
        clear_host_attribute("data-selected-index");
    }
}

void ToggleGroupWidget::set_selected_index(int index) {
    if (index >= -1 && index < static_cast<int>(options_.size())) {
        selected_index_ = index;
        dirty_ = true;
        const bool has_selection =
            selected_index_ >= 0 && selected_index_ < static_cast<int>(options_.size());
        set_host_attribute("role", multi_select_ ? "group" : "radiogroup");
        set_host_attribute("data-orientation", "horizontal");
        set_host_attribute("aria-orientation", "horizontal");
        if (options_.empty()) {
            set_host_attribute("data-state", "empty");
        } else {
            set_host_attribute("data-state", has_selection ? "selected" : "unselected");
        }
        set_host_state("selected", has_selection);
        set_host_attribute("data-selected-count", has_selection ? "1" : "0");
        if (has_selection) {
            set_host_attribute("data-value", options_[selected_index_].id);
            set_host_attribute("data-selected-index", std::to_string(selected_index_));
        } else {
            clear_host_attribute("data-value");
            clear_host_attribute("data-selected-index");
        }
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

    size_t selected_count = 0;
    for (int index : selected_indices_) {
        if (index >= 0 && index < static_cast<int>(options_.size())) {
            ++selected_count;
        }
    }
    set_host_attribute("role", multi_select_ ? "group" : "radiogroup");
    set_host_attribute("data-orientation", "horizontal");
    set_host_attribute("aria-orientation", "horizontal");
    if (options_.empty()) {
        set_host_attribute("data-state", "empty");
    } else {
        set_host_attribute("data-state", selected_count > 0 ? "selected" : "unselected");
    }
    set_host_state("selected", selected_count > 0);
    set_host_attribute("data-selected-count", std::to_string(selected_count));
    clear_host_attribute("data-value");
    clear_host_attribute("data-selected-index");
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
        auto* bg =
            root_.add<RectShape>(x, 0.0f, btn_width, btn_height, radius);
        backgrounds_.push_back(bg);

        // Button label
        float text_width = approximate_segmented_text_width(style, options_[i].label);
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

void ToggleGroupWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    auto* style = elem.computed_style;
    if (!style) return;

    size_t selected_count = 0;
    if (multi_select_) {
        for (int index : selected_indices_) {
            if (index >= 0 && index < static_cast<int>(options_.size())) {
                ++selected_count;
            }
        }
    } else if (selected_index_ >= 0 && selected_index_ < static_cast<int>(options_.size())) {
        selected_count = 1;
    }
    set_host_attribute("role", multi_select_ ? "group" : "radiogroup");
    set_host_attribute("data-orientation", "horizontal");
    set_host_attribute("aria-orientation", "horizontal");
    if (options_.empty()) {
        set_host_attribute("data-state", "empty");
    } else {
        set_host_attribute("data-state", selected_count > 0 ? "selected" : "unselected");
    }
    set_host_state("selected", selected_count > 0);
    set_host_attribute("data-selected-count", std::to_string(selected_count));
    if (!multi_select_ && selected_index_ >= 0 &&
        selected_index_ < static_cast<int>(options_.size())) {
        set_host_attribute("data-value", options_[selected_index_].id);
        set_host_attribute("data-selected-index", std::to_string(selected_index_));
    } else {
        clear_host_attribute("data-value");
        clear_host_attribute("data-selected-index");
    }

    rebuild_shapes(elem);
    update_shapes(elem);

    Transform local_transform = Transform{};
    float opacity = style->opacity;

    root_.draw(commands, local_transform, opacity);

    dirty_ = false;
}

bool ToggleGroupWidget::handle_event(const Event& event, Element& elem) {
    if (elem.has_state("disabled")) return false;

    switch (event.type) {
        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                const flex::Vec2 local_pos =
                    detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
                float local_x = local_pos.x;
                float local_y = local_pos.y;
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

                    size_t selected_count = 0;
                    if (multi_select_) {
                        for (int selected : selected_indices_) {
                            if (selected >= 0 && selected < static_cast<int>(options_.size())) {
                                ++selected_count;
                            }
                        }
                    } else if (selected_index_ >= 0 &&
                               selected_index_ < static_cast<int>(options_.size())) {
                        selected_count = 1;
                    }
                    set_host_attribute("role", multi_select_ ? "group" : "radiogroup");
                    set_host_attribute("data-orientation", "horizontal");
                    set_host_attribute("aria-orientation", "horizontal");
                    if (options_.empty()) {
                        set_host_attribute("data-state", "empty");
                    } else {
                        set_host_attribute("data-state",
                                           selected_count > 0 ? "selected" : "unselected");
                    }
                    set_host_state("selected", selected_count > 0);
                    set_host_attribute("data-selected-count", std::to_string(selected_count));
                    if (!multi_select_ && selected_index_ >= 0 &&
                        selected_index_ < static_cast<int>(options_.size())) {
                        set_host_attribute("data-value", options_[selected_index_].id);
                        set_host_attribute("data-selected-index", std::to_string(selected_index_));
                    } else {
                        clear_host_attribute("data-value");
                        clear_host_attribute("data-selected-index");
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
            const flex::Vec2 local_pos =
                detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
            float local_x = local_pos.x;
            float local_y = local_pos.y;
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
