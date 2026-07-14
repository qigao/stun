/*
 * flexUI - BreadcrumbWidget Implementation
 */

#include <flexUI/widgets/breadcrumb_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/text_layout.h>

namespace flexUI {

BreadcrumbWidget::BreadcrumbWidget() {}

void BreadcrumbWidget::sync_host_semantics() {
    set_host_attribute("role", "navigation");
    set_host_attribute("aria-label", "Breadcrumb");
    set_host_attribute("data-count", std::to_string(items_.size()));
    if (items_.empty()) {
        set_host_attribute("data-state", "empty");
        clear_host_attribute("data-current-id");
    } else {
        set_host_attribute("data-state", "ready");
        set_host_attribute("data-current-id", items_.back().id);
    }
    if (hovered_index_ >= 0 && hovered_index_ < static_cast<int>(items_.size())) {
        set_host_attribute("data-hovered-id", items_[hovered_index_].id);
    } else {
        clear_host_attribute("data-hovered-id");
    }
}

void BreadcrumbWidget::set_items(const std::vector<Item>& items) {
    items_ = items;
    item_texts_.clear();
    separator_texts_.clear();
    item_x_positions_.clear();
    item_widths_.clear();
    sync_host_semantics();
    dirty_ = true;
}

void BreadcrumbWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    if (elem.width() == cached_width_ && !item_texts_.empty()) {
        return;
    }

    root_.clear();
    item_texts_.clear();
    separator_texts_.clear();
    item_x_positions_.clear();
    item_widths_.clear();

    cached_width_ = elem.width();

    if (items_.empty()) return;

    float gap = style->get_variable_float("--breadcrumb-gap", 8.0f);
    float x = 0;
    float y = (elem.height() - style->font_size) / 2;

    for (size_t i = 0; i < items_.size(); ++i) {
        // Item text
        item_x_positions_.push_back(x);
        float text_width = approximate_segmented_text_width(style, items_[i].label);
        item_widths_.push_back(text_width);

        auto* item = root_.add<TextShape>(x, y, items_[i].label);
        item->set_font_family(style->font_family);
        item->set_font_size(style->font_size);
        item_texts_.push_back(item);

        x += text_width;

        // Separator (except for last item)
        if (i < items_.size() - 1) {
            x += gap;
            float sep_width = approximate_segmented_text_width(style, separator_);

            auto* sep = root_.add<TextShape>(x, y, separator_);
            sep->set_font_family(style->font_family);
            sep->set_font_size(style->font_size);
            separator_texts_.push_back(sep);

            x += sep_width + gap;
        }
    }
}

void BreadcrumbWidget::update_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    Color normal_color = style->get_variable_color("--breadcrumb-color", color_from_u8(107, 114, 128, 255));
    Color active_color = style->get_variable_color("--breadcrumb-color-active", color_from_u8(17, 24, 39, 255));
    Color hover_color = style->get_variable_color("--breadcrumb-color-hover", color_from_u8(59, 130, 246, 255));
    Color sep_color = style->get_variable_color("--breadcrumb-separator-color", color_from_u8(156, 163, 175, 255));

    for (size_t i = 0; i < item_texts_.size(); ++i) {
        bool is_last = (i == item_texts_.size() - 1);
        bool is_hovered = (static_cast<int>(i) == hovered_index_);

        Color color;
        if (is_last) {
            color = active_color;
        } else if (is_hovered) {
            color = hover_color;
        } else {
            color = normal_color;
        }

        item_texts_[i]->set_color(color);
    }

    for (auto* sep : separator_texts_) {
        sep->set_color(sep_color);
    }
}

int BreadcrumbWidget::hit_test(float x, float y, const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return -1;

    float height = style->font_size;
    float text_y = (elem.height() - height) / 2;

    if (y < text_y || y > text_y + height) {
        return -1;
    }

    for (size_t i = 0; i < item_x_positions_.size(); ++i) {
        if (x >= item_x_positions_[i] && x < item_x_positions_[i] + item_widths_[i]) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void BreadcrumbWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    auto* style = elem.computed_style;
    if (!style) return;
    sync_host_semantics();

    rebuild_shapes(elem);
    update_shapes(elem);

    Transform local_transform = Transform{};
    float opacity = style->opacity;

    root_.draw(commands, local_transform, opacity);

    dirty_ = false;
}

bool BreadcrumbWidget::handle_event(const Event& event, Element& elem) {
    switch (event.type) {
        case EventType::MouseMove: {
            const flex::Vec2 local_pos =
                detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
            float local_x = local_pos.x;
            float local_y = local_pos.y;
            int index = hit_test(local_x, local_y, elem);

            // Don't hover the last (current) item
            if (index == static_cast<int>(items_.size()) - 1) {
                index = -1;
            }

            if (index != hovered_index_) {
                hovered_index_ = index;
                sync_host_semantics();
                elem.mark_paint_dirty();
            }
            break;
        }

        case EventType::MouseDown:
            if (event.button == MouseButton::Left && hovered_index_ >= 0) {
                if (click_callback_) {
                    click_callback_(hovered_index_, items_[hovered_index_].id);
                }
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

void BreadcrumbWidget::update(float delta_ms, Element& elem) {
    (void)delta_ms;
    (void)elem;
}

} // namespace flexUI
