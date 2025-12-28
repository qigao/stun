/*
 * flexUI - GradientEditorWidget Implementation
 */

#include <flexUI/widgets/gradient_editor_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

GradientEditorWidget::GradientEditorWidget() {
    // Default: two-stop black to white gradient
    linear_.x1 = 0; linear_.y1 = 0;
    linear_.x2 = 1; linear_.y2 = 0;
    linear_.stops.push_back({0.0f, flex::Color::Black});
    linear_.stops.push_back({1.0f, flex::Color::White});

    radial_.cx = 0.5f; radial_.cy = 0.5f;
    radial_.radius = 0.5f;
    radial_.stops.push_back({0.0f, flex::Color::White});
    radial_.stops.push_back({1.0f, flex::Color::Black});
}

void GradientEditorWidget::set_type(GradientType t) {
    if (type_ != t) {
        type_ = t;
        selected_stop_ = 0;
        dirty_ = true;
    }
}

void GradientEditorWidget::set_gradient(const flex::LinearGradient& g) {
    linear_ = g;
    type_ = GradientType::Linear;
    selected_stop_ = linear_.stops.empty() ? -1 : 0;
    dirty_ = true;
}

void GradientEditorWidget::set_gradient(const flex::RadialGradient& g) {
    radial_ = g;
    type_ = GradientType::Radial;
    selected_stop_ = radial_.stops.empty() ? -1 : 0;
    dirty_ = true;
}

int GradientEditorWidget::stop_count() const {
    return type_ == GradientType::Linear
        ? (int)linear_.stops.size()
        : (int)radial_.stops.size();
}

flex::ColorStop GradientEditorWidget::get_stop(int index) const {
    const auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    if (index >= 0 && index < (int)stops.size()) {
        return stops[index];
    }
    return {0, flex::Color::Black};
}

void GradientEditorWidget::set_stop_color(int index, const Color& color) {
    auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    if (index >= 0 && index < (int)stops.size()) {
        stops[index].color = flex::Color(color.r, color.g, color.b, color.a);
        dirty_ = true;
        notify_change();
    }
}

void GradientEditorWidget::add_stop(float offset, const Color& color) {
    auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    flex::ColorStop new_stop{offset, flex::Color(color.r, color.g, color.b, color.a)};

    // Insert in sorted order
    auto it = std::lower_bound(stops.begin(), stops.end(), new_stop,
        [](const flex::ColorStop& a, const flex::ColorStop& b) {
            return a.offset < b.offset;
        });
    int new_index = (int)std::distance(stops.begin(), it);
    stops.insert(it, new_stop);

    selected_stop_ = new_index;
    dirty_ = true;
    notify_change();
}

void GradientEditorWidget::remove_stop(int index) {
    auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    if (stops.size() <= 2) return;  // Need at least 2 stops
    if (index >= 0 && index < (int)stops.size()) {
        stops.erase(stops.begin() + index);
        if (selected_stop_ >= (int)stops.size()) {
            selected_stop_ = (int)stops.size() - 1;
        }
        dirty_ = true;
        notify_change();
    }
}

void GradientEditorWidget::set_selected_stop(int index) {
    int count = stop_count();
    if (index >= 0 && index < count && index != selected_stop_) {
        selected_stop_ = index;
        dirty_ = true;
        if (on_stop_select_) {
            auto stop = get_stop(index);
            on_stop_select_(index, Color{stop.color.r, stop.color.g, stop.color.b, stop.color.a});
        }
    }
}

void GradientEditorWidget::notify_change() {
    if (on_change_) {
        on_change_();
    }
}

float GradientEditorWidget::stop_to_x(float offset, float bar_x, float bar_w) const {
    return bar_x + offset * bar_w;
}

float GradientEditorWidget::x_to_offset(float x, float bar_x, float bar_w) const {
    return std::clamp((x - bar_x) / bar_w, 0.0f, 1.0f);
}

int GradientEditorWidget::hit_test_stop(float x, float y, const Element& elem) const {
    float padding = 8.0f;
    float bar_h = 24.0f;
    float stop_size = 12.0f;
    float bar_x = padding;
    float bar_w = elem.width() - padding * 2;
    float stop_y = padding + bar_h + 4.0f;

    const auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    for (int i = 0; i < (int)stops.size(); i++) {
        float sx = stop_to_x(stops[i].offset, bar_x, bar_w);
        float sy = stop_y + stop_size / 2;
        float dx = x - sx;
        float dy = y - sy;
        if (dx * dx + dy * dy <= (stop_size / 2 + 4) * (stop_size / 2 + 4)) {
            return i;
        }
    }
    return -1;
}

void GradientEditorWidget::render(const Element& elem, Renderer& renderer) {
    auto& r = renderer.flex();

    // Background
    r.draw_rect(0, 0, elem.width(), elem.height(), 4,
                Paint::solid(Color{0.15f, 0.15f, 0.15f, 1.0f}), Paint::none(), 0);

    render_gradient_bar(r, elem);
    render_stops(r, elem);
}

void GradientEditorWidget::render_gradient_bar(flex::Renderer& r, const Element& elem) {
    float padding = 8.0f;
    float bar_h = 24.0f;
    float bar_x = padding;
    float bar_y = padding;
    float bar_w = elem.width() - padding * 2;

    // Draw gradient using segments
    const int segments = 32;
    float seg_w = bar_w / segments;

    const auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
    if (stops.empty()) return;

    for (int i = 0; i < segments; i++) {
        float t = (i + 0.5f) / segments;

        // Find surrounding stops
        flex::Color c;
        if (stops.size() == 1) {
            c = stops[0].color;
        } else {
            // Find the two stops that surround t
            int low = 0, high = (int)stops.size() - 1;
            for (int j = 0; j < (int)stops.size() - 1; j++) {
                if (stops[j].offset <= t && stops[j + 1].offset >= t) {
                    low = j;
                    high = j + 1;
                    break;
                }
            }
            if (t <= stops[0].offset) {
                c = stops[0].color;
            } else if (t >= stops.back().offset) {
                c = stops.back().color;
            } else {
                float range = stops[high].offset - stops[low].offset;
                float local_t = (range > 0) ? (t - stops[low].offset) / range : 0;
                c.r = stops[low].color.r + (stops[high].color.r - stops[low].color.r) * local_t;
                c.g = stops[low].color.g + (stops[high].color.g - stops[low].color.g) * local_t;
                c.b = stops[low].color.b + (stops[high].color.b - stops[low].color.b) * local_t;
                c.a = stops[low].color.a + (stops[high].color.a - stops[low].color.a) * local_t;
            }
        }

        float x = bar_x + i * seg_w;
        r.draw_rect(x, bar_y, seg_w + 0.5f, bar_h, 0,
                    Paint::solid(Color{c.r, c.g, c.b, c.a}), Paint::none(), 0);
    }

    // Border
    r.draw_rect(bar_x, bar_y, bar_w, bar_h, 4,
                Paint::none(), Paint::solid(Color{0.4f, 0.4f, 0.4f, 1.0f}), 1);
}

void GradientEditorWidget::render_stops(flex::Renderer& r, const Element& elem) {
    float padding = 8.0f;
    float bar_h = 24.0f;
    float stop_size = 12.0f;
    float bar_x = padding;
    float bar_w = elem.width() - padding * 2;
    float stop_y = padding + bar_h + 4.0f;

    const auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;

    for (int i = 0; i < (int)stops.size(); i++) {
        float x = stop_to_x(stops[i].offset, bar_x, bar_w);
        float y = stop_y + stop_size / 2;

        // Stop marker (triangle pointing up + circle)
        bool selected = (i == selected_stop_);

        // Draw triangle pointer
        std::string triangle = "M " + std::to_string(x) + " " + std::to_string(stop_y - 2) +
                              " L " + std::to_string(x - 5) + " " + std::to_string(stop_y + 6) +
                              " L " + std::to_string(x + 5) + " " + std::to_string(stop_y + 6) + " Z";
        r.fill_path(triangle, Paint::solid(Color{0.3f, 0.3f, 0.3f, 1.0f}));

        // Stop color circle
        Color stop_color{stops[i].color.r, stops[i].color.g, stops[i].color.b, stops[i].color.a};
        Color border_color = selected ? Color{0.2f, 0.6f, 1.0f, 1.0f} : Color{0.5f, 0.5f, 0.5f, 1.0f};
        float border_width = selected ? 2.0f : 1.0f;

        r.draw_circle(x, y + 6, stop_size / 2, Paint::solid(stop_color), Paint::solid(border_color), border_width);
    }
}

bool GradientEditorWidget::handle_event(const Event& event, Element& elem) {
    float padding = 8.0f;
    float bar_h = 24.0f;
    float bar_x = padding;
    float bar_w = elem.width() - padding * 2;

    float local_x = event.x - elem.absolute_x();
    float local_y = event.y - elem.absolute_y();

    if (event.type == EventType::MouseDown) {
        int hit = hit_test_stop(local_x, local_y, elem);
        if (hit >= 0) {
            // Click on existing stop
            set_selected_stop(hit);
            dragging_stop_ = hit;
            auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
            drag_start_offset_ = stops[hit].offset;
            elem.mark_paint_dirty();
            return true;
        }

        // Click on gradient bar to add new stop
        if (local_y >= padding && local_y <= padding + bar_h &&
            local_x >= bar_x && local_x <= bar_x + bar_w) {
            float offset = x_to_offset(local_x, bar_x, bar_w);

            // Interpolate color at this position
            const auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
            flex::Color c = flex::Color::White;
            if (!stops.empty()) {
                for (int j = 0; j < (int)stops.size() - 1; j++) {
                    if (stops[j].offset <= offset && stops[j + 1].offset >= offset) {
                        float range = stops[j + 1].offset - stops[j].offset;
                        float t = (range > 0) ? (offset - stops[j].offset) / range : 0;
                        c.r = stops[j].color.r + (stops[j + 1].color.r - stops[j].color.r) * t;
                        c.g = stops[j].color.g + (stops[j + 1].color.g - stops[j].color.g) * t;
                        c.b = stops[j].color.b + (stops[j + 1].color.b - stops[j].color.b) * t;
                        c.a = stops[j].color.a + (stops[j + 1].color.a - stops[j].color.a) * t;
                        break;
                    }
                }
            }

            add_stop(offset, Color{c.r, c.g, c.b, c.a});
            elem.mark_paint_dirty();
            return true;
        }
    }

    if (event.type == EventType::MouseMove && dragging_stop_ >= 0) {
        auto& stops = (type_ == GradientType::Linear) ? linear_.stops : radial_.stops;
        float new_offset = x_to_offset(local_x, bar_x, bar_w);

        // Don't allow crossing other stops
        if (dragging_stop_ > 0) {
            new_offset = std::max(new_offset, stops[dragging_stop_ - 1].offset + 0.01f);
        }
        if (dragging_stop_ < (int)stops.size() - 1) {
            new_offset = std::min(new_offset, stops[dragging_stop_ + 1].offset - 0.01f);
        }

        stops[dragging_stop_].offset = new_offset;
        dirty_ = true;
        notify_change();
        elem.mark_paint_dirty();
        return true;
    }

    if (event.type == EventType::MouseUp) {
        if (dragging_stop_ >= 0) {
            dragging_stop_ = -1;
            return true;
        }
    }

    return false;
}

void GradientEditorWidget::update(float delta_ms, Element& elem) {}

} // namespace flexUI
