/*
 * flexUI - PaginationWidget Implementation
 */
#include <flexUI/widgets/pagination_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <climits>
#include <stb_sprintf.h>

namespace flexUI {

PaginationWidget::PaginationWidget(int total_pages, int current_page)
    : total_pages_(total_pages), current_page_(current_page) {}

void PaginationWidget::set_total_pages(int pages) {
    total_pages_ = std::max(1, pages);
    if (current_page_ > total_pages_) {
        current_page_ = total_pages_;
    }
    dirty_ = true;
}

void PaginationWidget::set_current_page(int page) {
    int new_page = std::clamp(page, 1, total_pages_);
    if (new_page != current_page_) {
        current_page_ = new_page;
        dirty_ = true;
        if (page_change_callback_) {
            page_change_callback_(current_page_);
        }
    }
}

std::vector<int> PaginationWidget::get_visible_page_numbers() const {
    std::vector<int> pages;

    if (total_pages_ <= visible_pages_ + 2) {
        // Show all pages
        for (int i = 1; i <= total_pages_; ++i) {
            pages.push_back(i);
        }
    } else {
        // Always show first page
        pages.push_back(1);

        int half = visible_pages_ / 2;
        int start = std::max(2, current_page_ - half);
        int end = std::min(total_pages_ - 1, current_page_ + half);

        // Adjust range
        if (current_page_ - half < 2) {
            end = std::min(total_pages_ - 1, 1 + visible_pages_);
        }
        if (current_page_ + half > total_pages_ - 1) {
            start = std::max(2, total_pages_ - visible_pages_);
        }

        // Left ellipsis
        if (start > 2) {
            pages.push_back(-1); // ellipsis
        }

        // Middle pages
        for (int i = start; i <= end; ++i) {
            pages.push_back(i);
        }

        // Right ellipsis
        if (end < total_pages_ - 1) {
            pages.push_back(-1); // ellipsis
        }

        // Always show last page
        pages.push_back(total_pages_);
    }

    return pages;
}

void PaginationWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    if (elem.width() == cached_width_ &&
        elem.height() == cached_height_ &&
        !page_backgrounds_.empty()) {
        return;
    }

    root_.clear();
    page_backgrounds_.clear();
    page_texts_.clear();
    page_numbers_.clear();
    prev_bg_ = nullptr;
    prev_text_ = nullptr;
    next_bg_ = nullptr;
    next_text_ = nullptr;

    cached_width_ = elem.width();
    cached_height_ = elem.height();

    float btn_size = style->get_variable_float("--pagination-size", 32.0f);
    float gap = style->get_variable_float("--pagination-gap", 4.0f);
    float radius = style->get_variable_float("--pagination-radius", 4.0f);

    float x = 0;
    float y = (elem.height() - btn_size) / 2;

    // Prev button
    if (show_prev_next_) {
        prev_bg_ = root_.add<RectShape>(x, y, btn_size, btn_size, radius);
        float text_x = x + btn_size / 2 - style->font_size * 0.3f;
        float text_y = y + (btn_size - style->font_size) / 2;
        prev_text_ = root_.add<TextShape>(text_x, text_y, "<");
        prev_text_->set_font_family(style->font_family);
        prev_text_->set_font_size(style->font_size);
        x += btn_size + gap;
    }

    // Page buttons
    page_numbers_ = get_visible_page_numbers();
    for (int page_num : page_numbers_) {
        auto* bg = root_.add<RectShape>(x, y, btn_size, btn_size, radius);
        page_backgrounds_.push_back(bg);

        std::string label;
        if (page_num == -1) {
            label = "...";
        } else {
            char buf[16];
            stbsp_snprintf(buf, sizeof(buf), "%d", page_num);
            label = buf;
        }

        float char_width = style->font_size * 0.6f;
        float text_width = label.size() * char_width;
        float text_x = x + (btn_size - text_width) / 2;
        float text_y = y + (btn_size - style->font_size) / 2;

        auto* text = root_.add<TextShape>(text_x, text_y, label);
        text->set_font_family(style->font_family);
        text->set_font_size(style->font_size);
        page_texts_.push_back(text);

        x += btn_size + gap;
    }

    // Next button
    if (show_prev_next_) {
        next_bg_ = root_.add<RectShape>(x, y, btn_size, btn_size, radius);
        float text_x = x + btn_size / 2 - style->font_size * 0.3f;
        float text_y = y + (btn_size - style->font_size) / 2;
        next_text_ = root_.add<TextShape>(text_x, text_y, ">");
        next_text_->set_font_family(style->font_family);
        next_text_->set_font_size(style->font_size);
    }
}

void PaginationWidget::update_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    Color bg_normal = style->get_variable_color("--pagination-bg", color_from_u8(255, 255, 255, 255));
    Color bg_active = style->get_variable_color("--pagination-bg-active", color_from_u8(59, 130, 246, 255));
    Color bg_hover = style->get_variable_color("--pagination-bg-hover", color_from_u8(243, 244, 246, 255));
    Color text_normal = style->get_variable_color("--pagination-text", color_from_u8(55, 65, 81, 255));
    Color text_active = style->get_variable_color("--pagination-text-active", color_from_u8(255, 255, 255, 255));
    Color border = style->get_variable_color("--pagination-border", color_from_u8(209, 213, 219, 255));
    Color disabled = style->get_variable_color("--pagination-disabled", color_from_u8(156, 163, 175, 255));

    // Prev button
    if (prev_bg_ && prev_text_) {
        bool can_prev = current_page_ > 1;
        Color bg = (hovered_index_ == -2 && can_prev) ? bg_hover : bg_normal;
        prev_bg_->set_fill(bg);
        prev_bg_->set_stroke(border, 1.0f);
        prev_text_->set_color(can_prev ? text_normal : disabled);
    }

    // Page buttons
    for (size_t i = 0; i < page_backgrounds_.size(); ++i) {
        int page_num = page_numbers_[i];
        bool is_current = (page_num == current_page_);
        bool is_ellipsis = (page_num == -1);
        bool is_hovered = (static_cast<int>(i) == hovered_index_);

        Color bg;
        if (is_current) {
            bg = bg_active;
        } else if (is_hovered && !is_ellipsis) {
            bg = bg_hover;
        } else {
            bg = bg_normal;
        }

        page_backgrounds_[i]->set_fill(bg);
        if (!is_ellipsis) {
            page_backgrounds_[i]->set_stroke(border, 1.0f);
        }

        Color text_color = is_current ? text_active : text_normal;
        page_texts_[i]->set_color(text_color);
    }

    // Next button
    if (next_bg_ && next_text_) {
        bool can_next = current_page_ < total_pages_;
        Color bg = (hovered_index_ == INT_MAX && can_next) ? bg_hover : bg_normal;
        next_bg_->set_fill(bg);
        next_bg_->set_stroke(border, 1.0f);
        next_text_->set_color(can_next ? text_normal : disabled);
    }
}

int PaginationWidget::hit_test(float x, float y, const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return -1;

    float btn_size = style->get_variable_float("--pagination-size", 32.0f);
    float gap = style->get_variable_float("--pagination-gap", 4.0f);
    float btn_y = (elem.height() - btn_size) / 2;

    if (y < btn_y || y > btn_y + btn_size) {
        return -1;
    }

    float curr_x = 0;

    // Prev button
    if (show_prev_next_) {
        if (x >= curr_x && x < curr_x + btn_size) {
            return -2; // prev
        }
        curr_x += btn_size + gap;
    }

    // Page buttons
    for (size_t i = 0; i < page_numbers_.size(); ++i) {
        if (x >= curr_x && x < curr_x + btn_size) {
            return static_cast<int>(i);
        }
        curr_x += btn_size + gap;
    }

    // Next button
    if (show_prev_next_) {
        if (x >= curr_x && x < curr_x + btn_size) {
            return INT_MAX; // next
        }
    }

    return -1;
}

void PaginationWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    rebuild_shapes(elem);
    update_shapes(elem);

    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());
    float opacity = style->opacity;

    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool PaginationWidget::handle_event(const Event& event, Element& elem) {
    switch (event.type) {
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

        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                if (hovered_index_ == -2) {
                    // Prev
                    if (current_page_ > 1) {
                        set_current_page(current_page_ - 1);
                        elem.mark_paint_dirty();
                        return true;
                    }
                } else if (hovered_index_ == INT_MAX) {
                    // Next
                    if (current_page_ < total_pages_) {
                        set_current_page(current_page_ + 1);
                        elem.mark_paint_dirty();
                        return true;
                    }
                } else if (hovered_index_ >= 0 && hovered_index_ < static_cast<int>(page_numbers_.size())) {
                    int page = page_numbers_[hovered_index_];
                    if (page > 0 && page != current_page_) {
                        set_current_page(page);
                        elem.mark_paint_dirty();
                        return true;
                    }
                }
            }
            break;

        default:
            break;
    }

    return false;
}

void PaginationWidget::update(float delta_ms, Element& elem) {
    (void)delta_ms;
    (void)elem;
}

} // namespace flexUI
