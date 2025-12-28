# flexUI Widget Development Guide

## Architecture Overview

```
Widget (logic + state)
   │
   ├── Group (scene graph root)
   │   ├── RectShape (background)
   │   ├── TextShape (label)
   │   ├── CircleShape (indicator)
   │   └── ...
   │
   └── Element (layout + style from CSS)
```

## Core Concepts

### 1. Coordinate System

**All coordinates are LOCAL to parent.**

```cpp
// In render():
Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());

// Shapes use local coordinates (0,0 is top-left of widget)
background_ = root_.add<RectShape>(0, 0, width, height, radius);
```

**Key functions:**
- `elem.absolute_x()`, `elem.absolute_y()` - Position in window coordinates
- `elem.width()`, `elem.height()` - Size from layout engine
- `flex::make_translation(x, y)` - Create translation transform

### 2. Event Handling

**Available events (EventType enum):**
- `MouseMove` - Mouse moved (x, y in window coords)
- `MouseDown` - Button pressed (x, y, button)
- `MouseUp` - Button released
- `MouseWheel` - Scroll (delta_x, delta_y)
- `KeyDown` - Key pressed (key, mods)
- `KeyUp` - Key released
- `TextInput` - Text entered (text string)
- `FocusIn` / `FocusOut` - Focus changes

**Converting window to local coordinates:**
```cpp
bool MyWidget::handle_event(const Event& event, Element& elem) {
    switch (event.type) {
        case EventType::MouseDown: {
            // Convert window coords to local coords
            float local_x = event.x - elem.absolute_x();
            float local_y = event.y - elem.absolute_y();

            // Hit test against local shapes
            if (local_x >= 0 && local_x < elem.width() &&
                local_y >= 0 && local_y < elem.height()) {
                // Handle click
                return true;  // Event consumed
            }
            break;
        }
        default:
            break;
    }
    return false;  // Event not consumed
}
```

### 3. Group/Shape System

**Available shapes:**
- `RectShape` - Rectangle with optional rounded corners
- `CircleShape` - Circle or ellipse
- `TextShape` - Text rendering
- `LineShape` - Line between two points
- `PathShape` - Custom SVG path

**Group usage:**
```cpp
class MyWidget : public Widget {
private:
    Group root_;                    // Scene graph root
    RectShape* background_ = nullptr;
    TextShape* label_ = nullptr;

    void rebuild_shapes(const Element& elem) {
        root_.clear();  // Clear previous shapes

        auto* style = elem.computed_style;
        float radius = style->border_radius[0];

        // Add shapes (coordinates are LOCAL)
        background_ = root_.add<RectShape>(0, 0, elem.width(), elem.height(), radius);
        label_ = root_.add<TextShape>(10, 10, "Hello");
    }
};
```

### 4. Rendering Pattern

```cpp
void MyWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    // 1. Rebuild shapes if dimensions changed
    rebuild_shapes(elem);

    // 2. Update colors/properties from style
    update_shapes(elem);

    // 3. Build world transform
    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());

    // 4. Draw
    float opacity = style->opacity;
    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}
```

### 5. CSS Variables

**Reading CSS variables:**
```cpp
auto* style = elem.computed_style;

// Float values
float size = style->get_variable_float("--button-size", 32.0f);

// Color values
Color bg = style->get_variable_color("--bg-color", color_from_u8(255, 255, 255, 255));

// String values
std::string mode = style->get_variable("--mode", "default");
```

**Common style properties:**
- `style->background_color` - Background color
- `style->text_color` - Text color
- `style->font_family` - Font name
- `style->font_size` - Font size
- `style->border_radius[0]` - Border radius
- `style->opacity` - Opacity (0.0-1.0)

### 6. Animation Pattern

```cpp
void MyWidget::update(float delta_ms, Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    float duration = style->get_variable_float("--transition-duration", 200.0f);
    if (duration <= 0) {
        current_value_ = target_value_;
        return;
    }

    // Animate towards target
    float speed = 1.0f / duration * 1000.0f;
    float delta = speed * (delta_ms / 1000.0f);

    if (std::abs(current_value_ - target_value_) > 0.01f) {
        if (current_value_ < target_value_) {
            current_value_ = std::min(current_value_ + delta, target_value_);
        } else {
            current_value_ = std::max(current_value_ - delta, target_value_);
        }
        elem.mark_paint_dirty();  // Request repaint
    }
}
```

---

## Widget Template

```cpp
// my_widget.h
#ifndef FLEXUI_MY_WIDGET_H
#define FLEXUI_MY_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <functional>

namespace flexUI {

/**
 * MyWidget - Description
 *
 * CSS variables:
 *   --my-size: "32"
 *   --my-color: "r,g,b,a"
 */
class MyWidget : public Widget {
public:
    explicit MyWidget(/* params */);

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "MyWidget"; }

    // Public API
    int value() const { return value_; }
    void set_value(int v) { value_ = v; dirty_ = true; }

    // Callback
    using ChangeCallback = std::function<void(int value)>;
    void set_change_callback(ChangeCallback cb) { callback_ = std::move(cb); }

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);

    // Visual composition
    Group root_;
    RectShape* background_ = nullptr;
    TextShape* label_ = nullptr;

    // State
    int value_ = 0;
    bool hovered_ = false;

    // Animation
    float current_scale_ = 1.0f;
    float target_scale_ = 1.0f;

    // Cached dimensions (for rebuild detection)
    float cached_width_ = 0;
    float cached_height_ = 0;

    ChangeCallback callback_;
};

} // namespace flexUI

#endif
```

```cpp
// my_widget.cpp
#include <flexUI/widgets/my_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

MyWidget::MyWidget(/* params */) : value_(0) {}

void MyWidget::rebuild_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    // Skip rebuild if dimensions unchanged
    if (elem.width() == cached_width_ &&
        elem.height() == cached_height_ &&
        background_ != nullptr) {
        return;
    }

    root_.clear();
    cached_width_ = elem.width();
    cached_height_ = elem.height();

    float radius = style->border_radius[0];

    // Background (LOCAL coordinates: 0,0 is top-left of widget)
    background_ = root_.add<RectShape>(0, 0, elem.width(), elem.height(), radius);

    // Label centered
    float text_x = elem.width() / 2;
    float text_y = (elem.height() - style->font_size) / 2;
    label_ = root_.add<TextShape>(text_x, text_y, "Label");
    label_->set_font_family(style->font_family);
    label_->set_font_size(style->font_size);
}

void MyWidget::update_shapes(const Element& elem) {
    auto* style = elem.computed_style;
    if (!style || !background_) return;

    // Colors from CSS variables with defaults
    Color bg = style->get_variable_color("--my-bg", style->background_color);
    Color text = style->get_variable_color("--my-text", style->text_color);

    background_->set_fill(bg);

    if (label_) {
        label_->set_color(text);
    }
}

void MyWidget::render(const Element& elem, Renderer& renderer) {
    auto* style = elem.computed_style;
    if (!style) return;

    rebuild_shapes(elem);
    update_shapes(elem);

    // World transform: position widget at its absolute location
    Transform world_transform = flex::make_translation(elem.absolute_x(), elem.absolute_y());

    float opacity = style->opacity;
    root_.draw(renderer.flex(), world_transform, opacity);

    dirty_ = false;
}

bool MyWidget::handle_event(const Event& event, Element& elem) {
    if (elem.has_state("disabled")) return false;

    switch (event.type) {
        case EventType::MouseMove: {
            // Convert to local coordinates
            float local_x = event.x - elem.absolute_x();
            float local_y = event.y - elem.absolute_y();

            bool inside = local_x >= 0 && local_x < elem.width() &&
                          local_y >= 0 && local_y < elem.height();

            if (inside != hovered_) {
                hovered_ = inside;
                elem.mark_paint_dirty();
            }
            break;
        }

        case EventType::MouseDown:
            if (event.button == MouseButton::Left) {
                value_++;
                if (callback_) {
                    callback_(value_);
                }
                elem.mark_paint_dirty();
                return true;  // Event consumed
            }
            break;

        default:
            break;
    }

    return false;
}

void MyWidget::update(float delta_ms, Element& elem) {
    auto* style = elem.computed_style;
    if (!style) return;

    // Animate scale
    float duration = style->get_variable_float("--transition-duration", 150.0f);
    if (duration > 0 && std::abs(current_scale_ - target_scale_) > 0.001f) {
        float speed = 1.0f / duration * 1000.0f;
        float delta = speed * (delta_ms / 1000.0f);

        if (current_scale_ < target_scale_) {
            current_scale_ = std::min(current_scale_ + delta, target_scale_);
        } else {
            current_scale_ = std::max(current_scale_ - delta, target_scale_);
        }
        elem.mark_paint_dirty();
    }
}

} // namespace flexUI
```

---

## Common Patterns

### Hit Testing Multiple Items

```cpp
int MyWidget::hit_test(float local_x, float local_y, const Element& elem) {
    auto* style = elem.computed_style;
    float item_size = style->get_variable_float("--item-size", 32.0f);
    float gap = style->get_variable_float("--gap", 4.0f);

    for (int i = 0; i < item_count_; ++i) {
        float item_x = i * (item_size + gap);
        if (local_x >= item_x && local_x < item_x + item_size &&
            local_y >= 0 && local_y < item_size) {
            return i;
        }
    }
    return -1;  // No hit
}
```

### Applying Scale Transform

```cpp
void MyWidget::render(const Element& elem, Renderer& renderer) {
    // Base transform
    Transform world = flex::make_translation(elem.absolute_x(), elem.absolute_y());

    // Add scale around center
    if (current_scale_ != 1.0f) {
        float cx = elem.width() / 2;
        float cy = elem.height() / 2;
        Transform scale = flex::make_translation(cx, cy) *
                          flex::make_scale(current_scale_, current_scale_) *
                          flex::make_translation(-cx, -cy);
        world = world * scale;
    }

    root_.draw(renderer.flex(), world, opacity);
}
```

### Drawing Dynamic Content

```cpp
void MyWidget::render(const Element& elem, Renderer& renderer) {
    // ... standard rendering ...

    // Draw additional dynamic content directly
    if (show_indicator_) {
        float cx = elem.absolute_x() + indicator_x_;
        float cy = elem.absolute_y() + indicator_y_;

        renderer.flex().save();
        renderer.flex().set_transform(flex::make_translation(cx, cy));
        renderer.flex().draw_circle(0, 0, 5.0f, Paint::solid(indicator_color_), Paint::none(), 0);
        renderer.flex().restore();
    }
}
```

---

## Checklist

Before submitting a new widget:

- [ ] Header includes only necessary headers
- [ ] All coordinates in shapes are LOCAL (0,0 = widget top-left)
- [ ] `render()` uses `elem.absolute_x/y()` for world transform
- [ ] `handle_event()` converts event coords to local with `event.x - elem.absolute_x()`
- [ ] Only valid EventType values used (no MouseLeave, MouseEnter)
- [ ] CSS variable names documented in class comment
- [ ] `type_name()` returns unique widget name
- [ ] `dirty_` flag used to avoid unnecessary rebuilds
- [ ] `elem.mark_paint_dirty()` called when state changes
- [ ] No memory leaks (shapes owned by Group, Group owned by Widget)
- [ ] Added to CMakeLists.txt (both SOURCES and HEADERS)
