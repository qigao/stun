/*
 * Properties Panel Implementation
 */

#include <editor/view/properties_panel.h>
#include <editor/viewmodel/editor_vm.h>
#include <cstdio>
#include <cmath>

namespace editor {

// Input IDs
enum InputId {
    INPUT_X = 0,
    INPUT_Y,
    INPUT_WIDTH,
    INPUT_HEIGHT,
    INPUT_ROTATION,
    INPUT_SCALE_X,
    INPUT_SCALE_Y,
    INPUT_OPACITY,
    INPUT_FILL_COLOR,
    INPUT_STROKE_COLOR,
    INPUT_STROKE_WIDTH,
    INPUT_FONT_SIZE,
    INPUT_FONT_FAMILY,
    INPUT_COUNT
};

PropertiesPanel::PropertiesPanel() {
    input_rects_.resize(INPUT_COUNT);
}

bool PropertiesPanel::isInTitleBar(float mx, float my) const {
    return mx >= x_ && mx < x_ + style_.width &&
           my >= y_ && my < y_ + style_.title_height;
}

void PropertiesPanel::updateFromSelection() {
    if (!vm_) {
        selected_node_ = nullptr;
        selected_shape_ = nullptr;
        selected_text_ = nullptr;
        return;
    }

    auto& sel = vm_->selection();
    if (sel.isEmpty()) {
        selected_node_ = nullptr;
        selected_shape_ = nullptr;
        selected_text_ = nullptr;
    } else {
        selected_node_ = sel.primary().get();
        selected_shape_ = dynamic_cast<ShapeNode*>(selected_node_);
        selected_text_ = dynamic_cast<TextNode*>(selected_node_);
    }
}

void PropertiesPanel::render(flex::Renderer& renderer) {
    updateFromSelection();

    float w = style_.width;
    float y = y_;

    // Background with rounded corners
    std::string bg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(w - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(calculated_height_ - 8) +
        " a 4 4 0 0 1 -4 4" +
        " h " + std::to_string(-(w - 8)) +
        " a 4 4 0 0 1 -4 -4" +
        " v " + std::to_string(-(calculated_height_ - 8)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(bg, flex::Paint::solid({style_.background.r, style_.background.g,
        style_.background.b, style_.background.a}));

    // Title bar
    std::string titleBg = "M " + std::to_string(x_ + 4) + " " + std::to_string(y_) +
        " h " + std::to_string(w - 8) +
        " a 4 4 0 0 1 4 4" +
        " v " + std::to_string(style_.title_height - 4) +
        " h " + std::to_string(-w) +
        " v " + std::to_string(-(style_.title_height - 4)) +
        " a 4 4 0 0 1 4 -4";
    renderer.fill_path(titleBg, flex::Paint::solid({style_.title_bg.r, style_.title_bg.g,
        style_.title_bg.b, style_.title_bg.a}));

    // Drag grip
    if (draggable_) {
        float gripX = x_ + 8;
        float gripY = y_ + style_.title_height / 2 - 4;
        for (int i = 0; i < 3; ++i) {
            std::string line = "M " + std::to_string(gripX) + " " + std::to_string(gripY + i * 3) +
                " h 8";
            renderer.stroke_path(line, flex::Paint::solid({0.5f, 0.5f, 0.5f, 0.5f}), 1.0f);
        }
    }

    // Title
    renderer.draw_text("Properties", x_ + 22, y_ + 16, "Arial", 11, true,
        {style_.title_text.r, style_.title_text.g, style_.title_text.b, 1});

    y += style_.title_height;

    if (!selected_node_) {
        renderer.draw_text("No selection", x_ + style_.padding, y + 15, "Arial", 11, false,
            {0.5f, 0.5f, 0.5f, 1});
        calculated_height_ = 60;
        return;
    }

    // Transform section
    renderSectionHeader(renderer, "Transform", y, transform_expanded_);
    if (transform_expanded_) {
        renderTransformSection(renderer, y);
    }

    // Appearance section
    renderSectionHeader(renderer, "Appearance", y, appearance_expanded_);
    if (appearance_expanded_) {
        renderAppearanceSection(renderer, y);
    }

    // Fill section (for shapes)
    if (selected_shape_) {
        renderSectionHeader(renderer, "Fill", y, fill_expanded_);
        if (fill_expanded_) {
            renderFillSection(renderer, y);
        }

        renderSectionHeader(renderer, "Stroke", y, stroke_expanded_);
        if (stroke_expanded_) {
            renderStrokeSection(renderer, y);
        }
    }

    // Text section
    if (selected_text_) {
        renderSectionHeader(renderer, "Text", y, text_expanded_);
        if (text_expanded_) {
            renderTextSection(renderer, y);
        }
    }

    calculated_height_ = y - y_ + style_.padding;

    // Color picker overlay
    if (show_color_picker_) {
        color_picker_.render(renderer);
    }
}

void PropertiesPanel::renderSectionHeader(flex::Renderer& renderer, const char* title, float& y, bool expanded) {
    float w = style_.width;
    float h = style_.section_header_height;

    // Background
    std::string bg = "M " + std::to_string(x_) + " " + std::to_string(y) +
        " h " + std::to_string(w) + " v " + std::to_string(h) +
        " h " + std::to_string(-w) + " Z";
    renderer.fill_path(bg, flex::Paint::solid({style_.section_bg.r, style_.section_bg.g,
        style_.section_bg.b, style_.section_bg.a}));

    // Expand indicator
    const char* arrow = expanded ? "v" : ">";
    renderer.draw_text(arrow, x_ + 8, y + 18, "Arial", 10, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});

    // Title
    renderer.draw_text(title, x_ + 22, y + 18, "Arial", 11, true,
        {style_.header_text.r, style_.header_text.g, style_.header_text.b, 1});

    y += h;
}

void PropertiesPanel::renderTransformSection(flex::Renderer& renderer, float& y) {
    if (!selected_node_) return;

    char buf[32];

    // Position X/Y
    snprintf(buf, sizeof(buf), "%.1f", selected_node_->x());
    renderPropertyRow(renderer, "X", y, buf, INPUT_X);

    snprintf(buf, sizeof(buf), "%.1f", selected_node_->y());
    renderPropertyRow(renderer, "Y", y, buf, INPUT_Y);

    // Size (for shapes)
    if (selected_shape_) {
        auto bounds = selected_node_->localBounds();
        snprintf(buf, sizeof(buf), "%.1f", bounds.width);
        renderPropertyRow(renderer, "Width", y, buf, INPUT_WIDTH);

        snprintf(buf, sizeof(buf), "%.1f", bounds.height);
        renderPropertyRow(renderer, "Height", y, buf, INPUT_HEIGHT);
    }

    // Rotation
    snprintf(buf, sizeof(buf), "%.1f", selected_node_->rotation());
    renderPropertyRow(renderer, "Rotation", y, buf, INPUT_ROTATION);

    // Scale
    snprintf(buf, sizeof(buf), "%.2f", selected_node_->scaleX());
    renderPropertyRow(renderer, "Scale X", y, buf, INPUT_SCALE_X);

    snprintf(buf, sizeof(buf), "%.2f", selected_node_->scaleY());
    renderPropertyRow(renderer, "Scale Y", y, buf, INPUT_SCALE_Y);
}

void PropertiesPanel::renderAppearanceSection(flex::Renderer& renderer, float& y) {
    if (!selected_node_) return;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f%%", selected_node_->opacity() * 100);
    renderPropertyRow(renderer, "Opacity", y, buf, INPUT_OPACITY);
}

void PropertiesPanel::renderFillSection(flex::Renderer& renderer, float& y) {
    if (!selected_shape_ || !selected_shape_->hasFill()) {
        renderPropertyRow(renderer, "Fill", y, "None", -1);
        return;
    }

    auto fill = selected_shape_->fill();
    Color c = {fill.color.r, fill.color.g, fill.color.b, fill.color.a};
    renderColorRow(renderer, "Color", y, c, INPUT_FILL_COLOR);
}

void PropertiesPanel::renderStrokeSection(flex::Renderer& renderer, float& y) {
    if (!selected_shape_ || !selected_shape_->hasStroke()) {
        renderPropertyRow(renderer, "Stroke", y, "None", -1);
        return;
    }

    auto stroke = selected_shape_->stroke();
    Color c = {stroke.color.r, stroke.color.g, stroke.color.b, stroke.color.a};
    renderColorRow(renderer, "Color", y, c, INPUT_STROKE_COLOR);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", stroke.width);
    renderPropertyRow(renderer, "Width", y, buf, INPUT_STROKE_WIDTH);
}

void PropertiesPanel::renderTextSection(flex::Renderer& renderer, float& y) {
    if (!selected_text_) return;

    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f", selected_text_->fontSize());
    renderPropertyRow(renderer, "Size", y, buf, INPUT_FONT_SIZE);

    renderPropertyRow(renderer, "Font", y, selected_text_->fontFamily(), INPUT_FONT_FAMILY);
}

void PropertiesPanel::renderPropertyRow(flex::Renderer& renderer, const char* label, float& y,
                                        const std::string& value, int inputId) {
    float rowY = y;
    float labelX = x_ + style_.padding;
    float inputX = x_ + style_.label_width + style_.padding;
    float inputW = style_.width - style_.label_width - style_.padding * 2;

    // Label
    renderer.draw_text(label, labelX, rowY + 15, "Arial", 10, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});

    // Input
    if (inputId >= 0) {
        renderNumberInput(renderer, inputX, rowY + 2, inputW, value, inputId);
    } else {
        renderer.draw_text(value, inputX, rowY + 15, "Arial", 10, false,
            {style_.input_text.r, style_.input_text.g, style_.input_text.b, 1});
    }

    y += style_.row_height;
}

void PropertiesPanel::renderColorRow(flex::Renderer& renderer, const char* label, float& y,
                                     const Color& color, int inputId) {
    float rowY = y;
    float labelX = x_ + style_.padding;
    float colorX = x_ + style_.label_width + style_.padding;
    float colorW = style_.width - style_.label_width - style_.padding * 2;
    float colorH = style_.input_height;

    // Label
    renderer.draw_text(label, labelX, rowY + 15, "Arial", 10, false,
        {style_.label_text.r, style_.label_text.g, style_.label_text.b, 1});

    // Color swatch
    std::string swatch = "M " + std::to_string(colorX) + " " + std::to_string(rowY + 2) +
        " h " + std::to_string(colorW) + " v " + std::to_string(colorH) +
        " h " + std::to_string(-colorW) + " Z";
    renderer.fill_path(swatch, flex::Paint::solid({color.r, color.g, color.b, color.a}));
    renderer.stroke_path(swatch, flex::Paint::solid({style_.input_border.r, style_.input_border.g,
        style_.input_border.b, 1}), 1.0f);

    // Hex value
    char hex[10];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X",
        static_cast<int>(color.r * 255),
        static_cast<int>(color.g * 255),
        static_cast<int>(color.b * 255));
    renderer.draw_text(hex, colorX + 5, rowY + 15, "Arial", 9, false,
        {1 - color.r, 1 - color.g, 1 - color.b, 1});  // Inverted for contrast

    if (inputId >= 0 && inputId < static_cast<int>(input_rects_.size())) {
        input_rects_[inputId] = {colorX, rowY + 2, colorW, colorH};
    }

    y += style_.row_height;
}

void PropertiesPanel::renderNumberInput(flex::Renderer& renderer, float x, float y, float w,
                                        const std::string& value, int inputId) {
    float h = style_.input_height;
    bool focused = (inputId == focused_input_);
    bool hovered = (inputId == hovered_input_);

    // Background
    Color bgColor = focused ? Color{0.3f, 0.3f, 0.3f, 1} :
                    (hovered ? Color{0.28f, 0.28f, 0.28f, 1} : style_.input_bg);

    std::string bg = "M " + std::to_string(x) + " " + std::to_string(y) +
        " h " + std::to_string(w) + " v " + std::to_string(h) +
        " h " + std::to_string(-w) + " Z";
    renderer.fill_path(bg, flex::Paint::solid({bgColor.r, bgColor.g, bgColor.b, bgColor.a}));

    if (focused) {
        renderer.stroke_path(bg, flex::Paint::solid({0.4f, 0.6f, 0.9f, 1}), 1.0f);
    }

    // Value text
    const std::string& displayValue = focused ? input_buffer_ : value;
    renderer.draw_text(displayValue, x + 4, y + 14, "Arial", 10, false,
        {style_.input_text.r, style_.input_text.g, style_.input_text.b, 1});

    // Store rect for hit testing
    if (inputId >= 0 && inputId < static_cast<int>(input_rects_.size())) {
        input_rects_[inputId] = {x, y, w, h};
    }
}

int PropertiesPanel::inputAt(float mx, float my) const {
    for (size_t i = 0; i < input_rects_.size(); ++i) {
        if (input_rects_[i].contains({mx, my})) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool PropertiesPanel::onMouseDown(float mx, float my, int button) {
    if (show_color_picker_ && color_picker_.onMouseDown(mx, my, button)) {
        return true;
    }

    if (button != 0) return false;

    // Check for panel drag start on title bar
    if (draggable_ && isInTitleBar(mx, my)) {
        dragging_ = true;
        drag_offset_x_ = mx - x_;
        drag_offset_y_ = my - y_;
        return true;
    }

    // Check if clicking on a color input
    int idx = inputAt(mx, my);
    if (idx == INPUT_FILL_COLOR || idx == INPUT_STROKE_COLOR) {
        show_color_picker_ = true;
        color_picker_target_ = idx;

        Color c;
        if (idx == INPUT_FILL_COLOR && selected_shape_ && selected_shape_->hasFill()) {
            auto fill = selected_shape_->fill();
            c = {fill.color.r, fill.color.g, fill.color.b, fill.color.a};
        } else if (idx == INPUT_STROKE_COLOR && selected_shape_ && selected_shape_->hasStroke()) {
            auto stroke = selected_shape_->stroke();
            c = {stroke.color.r, stroke.color.g, stroke.color.b, stroke.color.a};
        }

        color_picker_.setColor(c);
        color_picker_.setPosition(x_ - color_picker_.width() - 10, my - 50);
        color_picker_.setVisible(true);

        color_picker_.onChange([this](const Color& newColor) {
            if (color_picker_target_ == INPUT_FILL_COLOR && selected_shape_) {
                selected_shape_->setFillColor(newColor);
            } else if (color_picker_target_ == INPUT_STROKE_COLOR && selected_shape_) {
                auto stroke = selected_shape_->stroke();
                selected_shape_->setStrokeColor(newColor, stroke.width);
            }
        });

        color_picker_.onClose([this]() {
            show_color_picker_ = false;
        });

        return true;
    }

    // Focus text input
    if (idx >= 0) {
        focused_input_ = idx;
        input_buffer_.clear();
        return true;
    }

    // Click outside inputs
    focused_input_ = -1;

    // Check section headers for expand/collapse
    float y = y_ + style_.title_height;
    float sections[] = {y, y, y, y, y};  // Placeholder - would track actual positions

    return mx >= x_ && mx <= x_ + style_.width && my >= y_ && my <= y_ + calculated_height_;
}

bool PropertiesPanel::onMouseMove(float mx, float my) {
    if (show_color_picker_ && color_picker_.onMouseMove(mx, my)) {
        return true;
    }

    // Handle panel dragging
    if (dragging_) {
        x_ = mx - drag_offset_x_;
        y_ = my - drag_offset_y_;
        return true;
    }

    hovered_input_ = inputAt(mx, my);
    return hovered_input_ >= 0 || isInTitleBar(mx, my);
}

bool PropertiesPanel::onMouseUp(float mx, float my, int button) {
    if (show_color_picker_) {
        color_picker_.onMouseUp(mx, my, button);
    }
    if (dragging_) {
        dragging_ = false;
        return true;
    }
    return false;
}

bool PropertiesPanel::onTextInput(const std::string& text) {
    if (focused_input_ < 0) return false;

    input_buffer_ += text;
    return true;
}

bool PropertiesPanel::onKeyDown(int key) {
    if (focused_input_ < 0) return false;

    if (key == 8 && !input_buffer_.empty()) {  // Backspace
        input_buffer_.pop_back();
        return true;
    }

    if (key == 13) {  // Enter - apply change
        applyPropertyChange(focused_input_, input_buffer_);
        focused_input_ = -1;
        return true;
    }

    if (key == 27) {  // Escape - cancel
        focused_input_ = -1;
        return true;
    }

    return false;
}

void PropertiesPanel::applyPropertyChange(int inputId, const std::string& value) {
    if (!selected_node_ || value.empty()) return;

    float f = 0;
    try {
        f = std::stof(value);
    } catch (...) {
        return;
    }

    switch (inputId) {
        case INPUT_X:
            selected_node_->setPosition(f, selected_node_->y());
            break;
        case INPUT_Y:
            selected_node_->setPosition(selected_node_->x(), f);
            break;
        case INPUT_ROTATION:
            selected_node_->setRotation(f);
            break;
        case INPUT_SCALE_X:
            selected_node_->setScale(f, selected_node_->scaleY());
            break;
        case INPUT_SCALE_Y:
            selected_node_->setScale(selected_node_->scaleX(), f);
            break;
        case INPUT_OPACITY:
            selected_node_->setOpacity(f / 100.0f);
            break;
        case INPUT_STROKE_WIDTH:
            if (selected_shape_ && selected_shape_->hasStroke()) {
                auto stroke = selected_shape_->stroke();
                selected_shape_->setStrokeColor(
                    Color{stroke.color.r, stroke.color.g, stroke.color.b, stroke.color.a}, f);
            }
            break;
        case INPUT_FONT_SIZE:
            if (selected_text_) {
                selected_text_->setFontSize(f);
            }
            break;
        default:
            break;
    }
}

} // namespace editor
