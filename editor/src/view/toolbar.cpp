/*
 * Toolbar Implementation
 */

#include <editor/view/toolbar.h>
#include <editor/viewmodel/editor_vm.h>
#include <cmath>

namespace editor {

// SVG Icon paths (normalized to 0-16 viewport)
namespace icons {
    // Cursor/Select tool - arrow pointer
    const char* SELECT = "M 3 2 L 3 14 L 6 11 L 9 15 L 11 14 L 8 10 L 12 10 Z";

    // Rectangle tool - outlined rectangle
    const char* RECTANGLE = "M 2 4 h 12 v 8 h -12 Z M 3 5 h 10 v 6 h -10 Z";

    // Ellipse tool - circle outline
    const char* ELLIPSE = "M 8 2 a 6 6 0 1 0 0 12 a 6 6 0 1 0 0 -12 M 8 4 a 4 4 0 1 1 0 8 a 4 4 0 1 1 0 -8";

    // Line tool - diagonal line
    const char* LINE = "M 2 14 L 14 2 M 12 2 h 2 v 2";

    // Pen tool - fountain pen nib
    const char* PEN = "M 2 14 L 4 12 L 12 4 L 14 6 L 6 14 L 4 14 Z M 11 5 L 13 7";

    // Text tool - letter T
    const char* TEXT = "M 3 3 h 10 v 2 h -4 v 9 h -2 v -9 h -4 Z";

    // Hand/Pan tool - open hand for panning
    const char* PAN = "M 7 2 v 5 h -2 v 3 h -2 v 3 l 1 2 h 8 l 2 -4 v -4 h -2 v -3 h -2 v -2 h -2 v -2 h -1 Z";

    // Zoom tool - magnifying glass
    const char* ZOOM = "M 6 2 a 4 4 0 1 0 0 8 a 4 4 0 1 0 0 -8 M 9 9 L 14 14";
}

Toolbar::Toolbar(ToolbarOrientation orient)
    : orientation_(orient) {}

float Toolbar::width() const {
    if (orientation_ == ToolbarOrientation::Vertical) {
        return style_.button_size + style_.padding * 2;
    } else {
        float w = style_.padding;
        for (const auto& btn : buttons_) {
            w += btn.separator ? style_.separator_size : style_.button_size;
            w += style_.padding;
        }
        return w;
    }
}

float Toolbar::height() const {
    if (orientation_ == ToolbarOrientation::Horizontal) {
        return style_.button_size + style_.padding * 2;
    } else {
        // Add drag handle height at top
        float h = style_.padding + 16;  // 16px for drag handle
        for (const auto& btn : buttons_) {
            h += btn.separator ? style_.separator_size : style_.button_size;
            h += style_.padding;
        }
        return h;
    }
}

void Toolbar::addButton(const ToolbarButton& btn) {
    buttons_.push_back(btn);
}

void Toolbar::addSeparator() {
    buttons_.push_back(ToolbarButton::Separator());
}

void Toolbar::clearButtons() {
    buttons_.clear();
}

void Toolbar::setupDefaultTools() {
    clearButtons();

    // Selection tool
    ToolbarButton select;
    select.id = "select";
    select.icon = "V";
    select.svgPath = icons::SELECT;
    select.tooltip = "Select (V)";
    select.shortcut = "V";
    addButton(select);

    // Pan tool
    ToolbarButton pan;
    pan.id = "pan";
    pan.icon = "H";
    pan.svgPath = icons::PAN;
    pan.tooltip = "Pan (H)";
    pan.shortcut = "H";
    addButton(pan);

    addSeparator();

    // Rectangle tool
    ToolbarButton rect;
    rect.id = "rectangle";
    rect.icon = "R";
    rect.svgPath = icons::RECTANGLE;
    rect.tooltip = "Rectangle (R)";
    rect.shortcut = "R";
    addButton(rect);

    // Ellipse tool
    ToolbarButton ellipse;
    ellipse.id = "ellipse";
    ellipse.icon = "E";
    ellipse.svgPath = icons::ELLIPSE;
    ellipse.tooltip = "Ellipse (E)";
    ellipse.shortcut = "E";
    addButton(ellipse);

    // Line tool
    ToolbarButton line;
    line.id = "line";
    line.icon = "L";
    line.svgPath = icons::LINE;
    line.tooltip = "Line (L)";
    line.shortcut = "L";
    addButton(line);

    addSeparator();

    // Pen tool
    ToolbarButton pen;
    pen.id = "pen";
    pen.icon = "P";
    pen.svgPath = icons::PEN;
    pen.tooltip = "Pen (P)";
    pen.shortcut = "P";
    addButton(pen);

    // Text tool
    ToolbarButton text;
    text.id = "text";
    text.icon = "T";
    text.svgPath = icons::TEXT;
    text.tooltip = "Text (T)";
    text.shortcut = "T";
    addButton(text);
}

bool Toolbar::isInDragArea(float mx, float my) const {
    if (orientation_ == ToolbarOrientation::Vertical) {
        // Top 16px is drag area
        return mx >= x_ && mx < x_ + width() &&
               my >= y_ && my < y_ + 16 + style_.padding;
    } else {
        // Left 16px is drag area
        return mx >= x_ && mx < x_ + 16 + style_.padding &&
               my >= y_ && my < y_ + height();
    }
}

void Toolbar::render(flex::Renderer& renderer) {
    float w = width();
    float h = height();

    // Background with rounded corners
    std::string bg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(w - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(h - 8) +
        " a 4 4 0 0 1 -4 4" +
        " h " + std::to_string(-(w - 8)) +
        " a 4 4 0 0 1 -4 -4" +
        " v " + std::to_string(-(h - 8)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(bg, flex::Paint::solid({style_.background.r, style_.background.g,
        style_.background.b, style_.background.a}));

    // Draw drag handle (grip lines)
    if (draggable_) {
        float gripY = y_ + style_.padding + 6;
        float gripX = x_ + (w - 16) / 2;
        for (int i = 0; i < 3; ++i) {
            std::string line = "M " + std::to_string(gripX) + " " + std::to_string(gripY + i * 3) +
                " h 16";
            renderer.stroke_path(line, flex::Paint::solid({0.5f, 0.5f, 0.5f, 0.5f}), 1.0f);
        }
    }

    // Render buttons (offset by drag handle height)
    float bx = x_ + style_.padding;
    float by = y_ + style_.padding + 16;  // Start below drag handle

    for (size_t i = 0; i < buttons_.size(); ++i) {
        const auto& btn = buttons_[i];

        if (btn.separator) {
            // Draw separator line
            if (orientation_ == ToolbarOrientation::Vertical) {
                float sy = by + style_.separator_size / 2;
                std::string sep = "M " + std::to_string(bx + 4) + " " + std::to_string(sy) +
                    " h " + std::to_string(style_.button_size - 8);
                renderer.stroke_path(sep, flex::Paint::solid({0.4f, 0.4f, 0.4f, 1}), 1.0f);
                by += style_.separator_size + style_.padding;
            } else {
                float sx = bx + style_.separator_size / 2;
                std::string sep = "M " + std::to_string(sx) + " " + std::to_string(by + 4) +
                    " v " + std::to_string(style_.button_size - 8);
                renderer.stroke_path(sep, flex::Paint::solid({0.4f, 0.4f, 0.4f, 1}), 1.0f);
                bx += style_.separator_size + style_.padding;
            }
        } else {
            bool hovered = (static_cast<int>(i) == hovered_button_);
            bool active = (btn.id == active_tool_id_);

            renderButton(renderer, btn, bx, by, hovered, active);

            if (orientation_ == ToolbarOrientation::Vertical) {
                by += style_.button_size + style_.padding;
            } else {
                bx += style_.button_size + style_.padding;
            }
        }
    }

    // Render tooltip
    if (show_tooltip_ && hovered_button_ >= 0) {
        renderTooltip(renderer);
    }
}

void Toolbar::renderSvgIcon(flex::Renderer& renderer, const std::string& svgPath,
                           float x, float y, float size, const Color& color) {
    // Scale factor from 16x16 icon to target size
    float scale = size / 16.0f;

    // Transform the SVG path to the target position and scale
    // For now, we'll render using stroke since these are outline icons
    renderer.save();
    renderer.translate(x, y);
    renderer.scale(scale, scale);

    renderer.stroke_path(svgPath, flex::Paint::solid({color.r, color.g, color.b, color.a}), 1.5f / scale);

    renderer.restore();
}

void Toolbar::renderButton(flex::Renderer& renderer, const ToolbarButton& btn,
                          float bx, float by, bool hovered, bool active) {
    float s = style_.button_size;

    // Button background with rounded corners
    Color bgColor = active ? style_.button_active :
                    (hovered ? style_.button_hover : style_.button_normal);

    std::string btnBg = "M " + std::to_string(bx + 3) + " " + std::to_string(by) +
        " h " + std::to_string(s - 6) +
        " a 3 3 0 0 1 3 3" +
        " v " + std::to_string(s - 6) +
        " a 3 3 0 0 1 -3 3" +
        " h " + std::to_string(-(s - 6)) +
        " a 3 3 0 0 1 -3 -3" +
        " v " + std::to_string(-(s - 6)) +
        " a 3 3 0 0 1 3 -3";
    renderer.fill_path(btnBg, flex::Paint::solid({bgColor.r, bgColor.g, bgColor.b, bgColor.a}));

    // Render SVG icon if available, otherwise fall back to text
    if (!btn.svgPath.empty()) {
        float iconPad = (s - style_.icon_size) / 2;
        renderSvgIcon(renderer, btn.svgPath, bx + iconPad, by + iconPad,
                     style_.icon_size, style_.icon_color);
    } else {
        // Fall back to text icon
        float iconX = bx + (s - style_.icon_size) / 2 + 2;
        float iconY = by + s / 2 + style_.icon_size / 3;
        renderer.draw_text(btn.icon, iconX, iconY, "Arial", style_.icon_size, true,
            {style_.icon_color.r, style_.icon_color.g, style_.icon_color.b, style_.icon_color.a});
    }
}

void Toolbar::renderTooltip(flex::Renderer& renderer) {
    if (hovered_button_ < 0 || hovered_button_ >= static_cast<int>(buttons_.size())) return;

    const auto& btn = buttons_[hovered_button_];
    if (btn.separator || btn.tooltip.empty()) return;

    // Calculate button position
    float bx = x_ + style_.padding;
    float by = y_ + style_.padding + 16;  // Account for drag handle

    for (int i = 0; i < hovered_button_; ++i) {
        if (buttons_[i].separator) {
            if (orientation_ == ToolbarOrientation::Vertical) {
                by += style_.separator_size + style_.padding;
            } else {
                bx += style_.separator_size + style_.padding;
            }
        } else {
            if (orientation_ == ToolbarOrientation::Vertical) {
                by += style_.button_size + style_.padding;
            } else {
                bx += style_.button_size + style_.padding;
            }
        }
    }

    // Tooltip position
    float tipX, tipY;
    float tipW = btn.tooltip.length() * 7 + 16;
    float tipH = 22;

    if (orientation_ == ToolbarOrientation::Vertical) {
        tipX = x_ + width() + 5;
        tipY = by + (style_.button_size - tipH) / 2;
    } else {
        tipX = bx + (style_.button_size - tipW) / 2;
        tipY = y_ + height() + 5;
    }

    // Background
    std::string tipBg = "M " + std::to_string(tipX) + " " + std::to_string(tipY) +
        " h " + std::to_string(tipW) + " v " + std::to_string(tipH) +
        " h " + std::to_string(-tipW) + " Z";
    renderer.fill_path(tipBg, flex::Paint::solid({style_.tooltip_bg.r, style_.tooltip_bg.g,
        style_.tooltip_bg.b, style_.tooltip_bg.a}));

    // Text
    renderer.draw_text(btn.tooltip, tipX + 8, tipY + 15, "Arial", 11, false,
        {style_.tooltip_text.r, style_.tooltip_text.g, style_.tooltip_text.b, 1});
}

int Toolbar::buttonAt(float mx, float my) const {
    // Check if in drag area first
    if (isInDragArea(mx, my)) {
        return -1;
    }

    if (mx < x_ || mx > x_ + width() || my < y_ || my > y_ + height()) {
        return -1;
    }

    float bx = x_ + style_.padding;
    float by = y_ + style_.padding + 16;  // Account for drag handle

    for (size_t i = 0; i < buttons_.size(); ++i) {
        const auto& btn = buttons_[i];

        if (btn.separator) {
            if (orientation_ == ToolbarOrientation::Vertical) {
                by += style_.separator_size + style_.padding;
            } else {
                bx += style_.separator_size + style_.padding;
            }
        } else {
            Rect btnRect = {bx, by, style_.button_size, style_.button_size};
            if (btnRect.contains({mx, my})) {
                return static_cast<int>(i);
            }

            if (orientation_ == ToolbarOrientation::Vertical) {
                by += style_.button_size + style_.padding;
            } else {
                bx += style_.button_size + style_.padding;
            }
        }
    }

    return -1;
}

bool Toolbar::onMouseDown(float mx, float my, int button) {
    if (button != 0) return false;

    // Check for drag start
    if (draggable_ && isInDragArea(mx, my)) {
        dragging_ = true;
        drag_offset_x_ = mx - x_;
        drag_offset_y_ = my - y_;
        return true;
    }

    int idx = buttonAt(mx, my);
    if (idx >= 0 && !buttons_[idx].separator) {
        const auto& btn = buttons_[idx];
        active_tool_id_ = btn.id;

        if (vm_) {
            vm_->setTool(btn.id);
        }

        if (onToolSelected_) {
            onToolSelected_(btn.id);
        }

        return true;
    }

    return false;
}

bool Toolbar::onMouseMove(float mx, float my) {
    // Handle dragging
    if (dragging_) {
        x_ = mx - drag_offset_x_;
        y_ = my - drag_offset_y_;
        return true;
    }

    int idx = buttonAt(mx, my);

    if (idx != hovered_button_) {
        hovered_button_ = idx;
        hover_time_ = 0;
        show_tooltip_ = false;
    } else if (idx >= 0) {
        hover_time_ += 0.016f;  // Assume ~60fps
        if (hover_time_ > 0.5f) {
            show_tooltip_ = true;
        }
    }

    return hovered_button_ >= 0 || isInDragArea(mx, my);
}

bool Toolbar::onMouseUp(float mx, float my, int button) {
    (void)mx; (void)my; (void)button;
    if (dragging_) {
        dragging_ = false;
        return true;
    }
    return false;
}

} // namespace editor
