# flexUI Widget Development Guide

## Architecture Overview

```
Widget (logic + state)
   └── Element host (layout + state + CSS)
       ├── track [part=track]
       ├── fill [part=fill]
       ├── thumb [part=thumb]
       └── other stable semantic parts
```

The Element tree is the only style and layout tree. A visual part that users
need to select, inspect, size, color, or animate must be a widget-owned Element.
Do not mirror those parts in a private Group/Shape tree.

## Core Concepts

### Tailwind classes and C++/MIR bindings

`Element` accepts the same whitespace-separated class form used by HTML and
Tailwind. The class text is stored on the rectangle tree, where the utility JIT
discovers it and emits CSS into the normal `StyleEngine` cascade:

```cpp
button->set_classes(
    "inline-flex items-center rounded-md px-3 "
    "focus-visible:ring-1 data-[state=open]:bg-accent");
button->toggle_class("opacity-50", disabled);
button->set_attribute("class", "grid grid-cols-2 gap-4");
```

Dynamic UI state uses the typed C++ binding runtime. Numeric and boolean
expressions are compiled once by the flex MIR backend; string inputs remain
typed direct bindings. Bindings write class, attribute, text, or inline CSS
custom-property declarations on the same Element tree:

```cpp
auto& ui = box.bindings();
ui.inputs().set_bool("open", false);
ui.inputs().set_number("progress", 0.25);
ui.inputs().set_string("title", "Loading");

ui.targets().bind_class(*panel, "is-open", "open");
// ui.targets().bind_classes(*panel, "class-list-input"); // owns all classes
ui.targets().bind_attribute(*panel, "data-state", "open", "open", "closed");
ui.targets().bind_text(*title, "title");
ui.targets().bind_custom_property(*bar, "--progress", "progress * 100", "%");
ui.targets().bind_value(*input, "title"); // two-way editable widget value
```

Dynamic collections use `UiKeyedRepeater`. It reconciles one container's direct
children by stable application keys, while every item remains a normal Element
subtree styled by CSS or Tailwind classes:

```cpp
UiKeyedRepeater rows(box, *list);
rows.reconcile(
    items,
    [](const Item& item) { return item.id; },
    [](Box& owner, const Item&) {
      auto* row = owner.create("button");
      row->set_classes("flex items-center w-full h-10 px-3 rounded-md");
      return row;
    },
    [](Element& row, const Item& item) {
      row.set_text(item.title);
      row.set_attribute("data-state", item.done ? "complete" : "open");
      row.toggle_class("opacity-50", item.done);
    });
```

The repeater owns the container's direct-child order. Do not append, remove, or
reorder those children outside it. Reconciliation is explicit rather than a
per-frame collection scan. Removed keys retain their Element/widget identity so
they can be restored; focus, capture, interaction pseudo-states, transitions,
and animations are cleared while retired. The key capacity bounds retained
history and fails fast when exceeded.

Inputs must be declared before binding and cannot change type. Invalid MIR
expressions, missing inputs, duplicate target ownership, and invalid custom
property names fail when the binding is created. CSS remains the only runtime
style language; MIR evaluates data expressions and does not generate a second
render or style system.

A binding exclusively owns its target until `unbind()` or `clear()`, which
restores the value captured when the binding was created. Referenced inputs
cannot be erased. `bind_text()` updates `Element::text()` (including label
content). `bind_value()` requires a widget implementing `TextValueWidget` and
binds its editable model in both directions. Programmatic input changes do not
invoke the widget's application change callback; user edits update both that
callback and `UiDataContext`. When both sides change before the next update,
the last write to `UiDataContext` wins.

### 1. Coordinate System

**All coordinates are LOCAL to parent.**

```cpp
// In emit_render_commands():
Transform local_transform{};

// Shapes use local coordinates (0,0 is top-left of widget)
background_ = root_.add<RectShape>(0, 0, width, height, radius);
```

**Key functions:**
- `elem.absolute_x()`, `elem.absolute_y()` - Position in window coordinates
- `elem.width()`, `elem.height()` - Size from layout engine
- `flex::make_translation(x, y)` - Create translation transform

Normal widget commands use local coordinates. Overlay commands are different:
`emit_overlay_commands()` emits window coordinates because overlays are replayed
after the element tree without an element transform prefix.

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

### 3. Semantic Part System

Create stable parts once when the Widget binds to its host:

```cpp
void MyWidget::build_semantic_tree() {
    track_ = create_part("track", "track");
    fill_ = create_part("fill", "fill");
}
```

Each part is a real Element with `[part=name]` and participates in normal CSS:

```css
[role=meter] > track {
    position: absolute;
    height: 8px;
    background-color: #e5e7eb;
    border-radius: 4px;
}

[role=meter] > fill {
    position: absolute;
    height: 8px;
    background-color: var(--meter-fill, #3b82f6);
    border-radius: 4px;
}
```

Applications may query and style parts, but the Widget owns their structure:

```cpp
Element* fill = box.query_selector("#volume > fill");
auto parts = box.query_selector_all("#volume > track, #volume > fill");
```

Do not append, remove, reorder, or clear widget-owned parts. Tree mutation APIs
reject those operations. Pointer hits on parts route to the host Widget so the
Widget remains the single owner of interaction state.

### 4. Group/Shape Exception

**Available shapes:**
- `RectShape` - Rectangle with optional rounded corners
- `CircleShape` - Circle or ellipse
- `TextShape` - Text rendering
- `LineShape` - Line between two points
- `PathShape` - Custom SVG path

Use Group/Shape only for atomic drawing that has no useful independent CSS,
query, accessibility, or event identity, such as a transient path inside one
semantic part. It is not the default structure for a new control.

**Exceptional Group usage:**
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

### 5. Rendering Pattern

For semantic parts, synchronize dynamic geometry in the layout semantics stage.
The RenderManager draws the parts from their computed styles:

```cpp
void MyWidget::sync_host_semantics_for_layout(Element& elem) {
    sync_host_semantics();
    fill_->set_layout_bounds(0.0f, 0.0f,
                             elem.width() * normalized_value_, elem.height());
}

void MyWidget::emit_render_commands(const Element&, RenderCommandList&) {
    dirty_ = false;
}
```

Do not update part geometry from `emit_render_commands()`. Render must not create
a new dirty frame. The direct command pattern below applies only to atomic
content that is intentionally not represented by semantic parts.

```cpp
void MyWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    auto* style = elem.computed_style;
    if (!style) return;

    // 1. Rebuild shapes if dimensions changed
    rebuild_shapes(elem);

    // 2. Update colors/properties from style
    update_shapes(elem);

    // 3. Build local transform
    Transform local_transform{};

    // 4. Emit backend-neutral commands
    float opacity = style->opacity;
    root_.draw(commands, local_transform, opacity);

    dirty_ = false;
}
```

`RenderCommandList` is created by RenderManager or the caller that owns a
renderer. It must be constructed with renderer capabilities; widget code should
only emit into the command list it receives.

When one widget emits another widget inside a translated overlay or local
subtree, keep the command-list prefix in sync with the backend transform:

```cpp
commands.save();
commands.translate(x, y);
commands.push_transform_prefix(flex::make_translation(x, y));

child_widget.emit_render_commands(child_elem, commands);

commands.pop_transform_prefix();
commands.restore();
```

The translate applies to direct draw commands. The prefix applies to commands
that call `set_transform()`, including Group/Shape drawing.

The same rule applies to scroll windows: if a helper translates content by a
scroll offset, it must push the same offset into the command-list prefix until
the scrolled content has finished emitting.

### 6. CSS Properties and Compatibility Variables

Use ordinary CSS properties on semantic parts for color, size, borders,
typography, opacity, transforms, and animation. Custom properties are design
tokens or compatibility fallbacks, not a replacement for part structure.

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

### 7. Animation Pattern

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
#include <functional>

namespace flexUI {

/**
 * MyWidget - Description
 *
 * Structure: meter host > track + fill
 */
class MyWidget : public Widget {
public:
    explicit MyWidget(float value = 0.0f) : value_(value) {}

    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void sync_host_semantics_for_layout(Element& elem) override;
    const char* type_name() const override { return "MyWidget"; }

    float value() const { return value_; }
    void set_value(float value);

    using ChangeCallback = std::function<void(float)>;
    void set_change_callback(ChangeCallback cb) { callback_ = std::move(cb); }

private:
    void build_semantic_tree() override;
    void sync_host_semantics() override;
    void update_part_geometry(const Element& elem);
    void invalidate_parts();

    Element* track_ = nullptr;
    Element* fill_ = nullptr;
    float value_ = 0.0f;
    ChangeCallback callback_;
};

} // namespace flexUI

#endif
```

```cpp
// my_widget.cpp
#include <flexUI/widgets/my_widget.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <algorithm>

namespace flexUI {

void MyWidget::build_semantic_tree() {
    track_ = create_part("track", "track");
    fill_ = create_part("fill", "fill");
}

void MyWidget::sync_host_semantics() {
    set_host_attribute("role", "meter");
    set_host_attribute("aria-valuemin", "0");
    set_host_attribute("aria-valuemax", "100");
    set_host_attribute("aria-valuenow", std::to_string(value_));
}

void MyWidget::set_value(float value) {
    const float clamped = std::clamp(value, 0.0f, 100.0f);
    if (value_ == clamped) return;
    value_ = clamped;
    sync_host_semantics();
    invalidate_parts();
    if (callback_) callback_(value_);
}

void MyWidget::update_part_geometry(const Element& elem) {
    if (!track_ || !fill_) return;
    track_->set_layout_bounds(0.0f, 0.0f, elem.width(), elem.height());
    fill_->set_layout_bounds(0.0f, 0.0f,
                             elem.width() * value_ / 100.0f, elem.height());
}

void MyWidget::sync_host_semantics_for_layout(Element& elem) {
    sync_host_semantics();
    update_part_geometry(elem);
}

void MyWidget::invalidate_parts() {
    dirty_ = true;
    if (auto* host = host_element()) host->mark_paint_dirty();
}

void MyWidget::emit_render_commands(const Element&, RenderCommandList&) {
    dirty_ = false;
}

bool MyWidget::handle_event(const Event& event, Element& elem) {
    if (elem.has_state("disabled")) return false;
    if (event.type != EventType::MouseDown ||
        event.button != MouseButton::Left || elem.width() <= 0.0f) return false;
    const float local_x = event.x - elem.absolute_x();
    set_value(local_x / elem.width() * 100.0f);
    return true;
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
void MyWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    // Base transform is local; RenderManager owns element placement.
    Transform local{};

    // Add scale around center
    if (current_scale_ != 1.0f) {
        float cx = elem.width() / 2;
        float cy = elem.height() / 2;
        Transform scale = flex::make_translation(cx, cy) *
                          flex::make_scale(current_scale_, current_scale_) *
                          flex::make_translation(-cx, -cy);
        local = local * scale;
    }

    root_.draw(commands, local, opacity);
}
```

### Drawing Dynamic Content

```cpp
void MyWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    // ... standard rendering ...

    // Emit additional dynamic content through the same command list
    if (show_indicator_) {
        float cx = indicator_x_;
        float cy = indicator_y_;

        commands.save();
        commands.set_transform(flex::make_translation(cx, cy));
        commands.draw_circle(0, 0, 5.0f, Paint::solid(indicator_color_), Paint::none(), 0);
        commands.restore();
    }

}
```

### Drawing Text

```cpp
void MyWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
    auto* style = elem.computed_style;

    const auto text_block = layout_text_block(
        style, "Label", 12.0f, 0.0f,
        std::max(0.0f, elem.width() - 24.0f), elem.height(),
        style->text_color, TextVerticalAlign::Middle);
    emit_text_block(commands, text_block);

}
```

Text helpers emit into `RenderCommandList`; widgets should not bypass the
command list for text drawing.

---

## Checklist

Before submitting a new widget:

- [ ] Header includes only necessary headers
- [ ] Stable visual parts are widget-owned Elements created once by `build_semantic_tree()`
- [ ] Parts expose `[part=name]` and ordinary CSS properties; custom variables are compatibility fallbacks
- [ ] Dynamic part geometry is synchronized in `sync_host_semantics_for_layout()`, not during render
- [ ] Widget-owned structure is not exposed through append/remove/clear operations
- [ ] Group/Shape is limited to atomic drawing that does not need independent CSS/query/event identity
- [ ] All coordinates are LOCAL (0,0 = widget top-left)
- [ ] `emit_render_commands()` emits local coordinates into the provided command list
- [ ] `emit_overlay_commands()` emits window coordinates when the widget has an overlay
- [ ] `handle_event()` converts event coords to local with `event.x - elem.absolute_x()`
- [ ] Only valid EventType values used (no MouseLeave, MouseEnter)
- [ ] CSS variable names documented in class comment
- [ ] `type_name()` returns unique widget name
- [ ] `dirty_` flag used to avoid unnecessary rebuilds
- [ ] `elem.mark_paint_dirty()` called when state changes
- [ ] No memory leaks (parts owned by Box, Widget owned by Box, optional shapes owned by Widget Group)
- [ ] Added to CMakeLists.txt (both SOURCES and HEADERS)
