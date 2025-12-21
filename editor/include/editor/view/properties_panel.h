/*
 * Properties Panel
 *
 * Panel for editing properties of selected nodes.
 * Displays transform, fill, stroke, and type-specific properties.
 */

#pragma once

#include "../core/types.h"
#include "../model/node.h"
#include "color_picker.h"
#include "gradient_picker.h"
#include <flex/flex.h>
#include <functional>
#include <string>

namespace editor {

// Forward declaration
class EditorViewModel;

// Property section types
enum class PropertySection {
    Transform,
    Appearance,
    Fill,
    Stroke,
    Text,
    Effects
};

// Properties Panel style
struct PropertiesPanelStyle {
    float width = 260;
    float title_height = 24;
    float section_header_height = 28;
    float row_height = 24;
    float label_width = 70;
    float input_height = 20;
    float padding = 10;

    Color background = {0.18f, 0.18f, 0.18f, 0.95f};
    Color title_bg = {0.22f, 0.22f, 0.22f, 1.0f};
    Color title_text = {0.9f, 0.9f, 0.9f, 1.0f};
    Color section_bg = {0.15f, 0.15f, 0.15f, 1.0f};
    Color header_text = {0.9f, 0.9f, 0.9f, 1.0f};
    Color label_text = {0.7f, 0.7f, 0.7f, 1.0f};
    Color input_bg = {0.25f, 0.25f, 0.25f, 1.0f};
    Color input_text = {0.9f, 0.9f, 0.9f, 1.0f};
    Color input_border = {0.35f, 0.35f, 0.35f, 1.0f};
};

// Properties Panel
class PropertiesPanel {
public:
    PropertiesPanel();

    void setViewModel(EditorViewModel* vm) { vm_ = vm; }

    // Position
    void setPosition(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }
    float width() const { return style_.width; }
    float height() const { return calculated_height_; }

    // Draggable
    bool isDraggable() const { return draggable_; }
    void setDraggable(bool d) { draggable_ = d; }
    bool isDragging() const { return dragging_; }

    // Update with current selection
    void updateFromSelection();

    // Style
    PropertiesPanelStyle& style() { return style_; }

    // Rendering
    void render(flex::Renderer& renderer);

    // Input handling
    bool onMouseDown(float mx, float my, int button);
    bool onMouseMove(float mx, float my);
    bool onMouseUp(float mx, float my, int button);
    bool onTextInput(const std::string& text);
    bool onKeyDown(int key);

private:
    void renderTitleBar(flex::Renderer& renderer, float& y);
    void renderSectionHeader(flex::Renderer& renderer, const char* title, float& y, bool expanded);
    void renderTransformSection(flex::Renderer& renderer, float& y);
    void renderAppearanceSection(flex::Renderer& renderer, float& y);
    void renderFillSection(flex::Renderer& renderer, float& y);
    void renderStrokeSection(flex::Renderer& renderer, float& y);
    void renderTextSection(flex::Renderer& renderer, float& y);

    void renderPropertyRow(flex::Renderer& renderer, const char* label, float& y,
                          const std::string& value, int inputId);
    void renderColorRow(flex::Renderer& renderer, const char* label, float& y,
                       const Color& color, int inputId);
    void renderNumberInput(flex::Renderer& renderer, float x, float y, float w,
                          const std::string& value, int inputId);

    int inputAt(float mx, float my) const;
    void applyPropertyChange(int inputId, const std::string& value);
    bool isInTitleBar(float mx, float my) const;

    EditorViewModel* vm_ = nullptr;
    float x_ = 0, y_ = 0;
    float calculated_height_ = 400;

    // Panel drag state
    bool draggable_ = true;
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;

    // Current selection cache
    EditorNode* selected_node_ = nullptr;
    ShapeNode* selected_shape_ = nullptr;
    TextNode* selected_text_ = nullptr;

    // Section expand state
    bool transform_expanded_ = true;
    bool appearance_expanded_ = true;
    bool fill_expanded_ = true;
    bool stroke_expanded_ = true;
    bool text_expanded_ = true;

    // Input state
    int focused_input_ = -1;
    int hovered_input_ = -1;
    std::string input_buffer_;
    std::vector<Rect> input_rects_;

    // Color picker
    bool show_color_picker_ = false;
    int color_picker_target_ = -1;
    ColorPicker color_picker_;

    PropertiesPanelStyle style_;
};

} // namespace editor
