# Event System Integration

Widget Gallery 现在支持**完整的鼠标交互**！

---

## ✅ What's Implemented

**Event System** ✅ (已存在于 Flex Engine)
- **PointerEvent** - 鼠标事件（Down, Up, Move, Enter, Leave）
- **KeyEvent** - 键盘事件（Down, Up）
- **Event Propagation** - 事件传播（Capture → Target → Bubble）
- **Hit Testing** - 自动检测鼠标点击了哪个 Node
- **Event Callbacks** - Node 回调接口（on_click, on_pointer_down, on_hover_enter 等）

**Widget Gallery Integration** ✅ (NEW!)
- **Slider** - 可拖拽调节音量
- **Toggle** - 可点击切换静音
- **ProgressBar** - 自动同步显示进度
- **实时更新** - 交互后立即重新渲染

---

## 🎮 Interactive Demo

### Mouse Controls

| 操作 | 功能 |
|------|------|
| **点击 Toggle** | 切换静音开关 |
| **拖拽 Slider** | 调节音量（0-100%）|
| **悬停 Widget** | Hover 效果（未来）|

### Keyboard Controls (保留)

| 按键 | 功能 |
|------|------|
| **UP/DOWN** | 调节音量 ±5% |
| **SPACE** | 切换静音 |
| **ESC** | 退出 |

---

## 💻 Code Example

### 1. Register Event Callbacks

```cpp
// Volume Slider - 拖拽交互
volume_slider_->on_pointer_down([this](flex::PointerEvent& e) {
    dragging_slider_ = true;
    e.stop_propagation();  // 阻止事件传播
});

// Mute Toggle - 点击交互
mute_toggle_->on_click([this]() {
    muted_ = !muted_;
    update_demo_widgets();  // 重新渲染
});
```

### 2. Send Mouse Events to Instance

```cpp
void handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_MOUSEBUTTONDOWN:
                instance_->send_pointer_event(
                    event.button.x,
                    event.button.y,
                    true  // is_down = true
                );
                break;

            case SDL_MOUSEBUTTONUP:
                instance_->send_pointer_event(
                    event.button.x,
                    event.button.y,
                    false  // is_down = false
                );
                break;

            case SDL_MOUSEMOTION:
                instance_->send_pointer_event(
                    event.motion.x,
                    event.motion.y,
                    dragging_slider_  // is_down = dragging state
                );
                break;
        }
    }
}
```

### 3. Update State During Drag

```cpp
case SDL_MOUSEMOTION:
    if (dragging_slider_) {
        // Calculate new volume from mouse position
        float slider_x = 100.0f;
        float slider_width = 300.0f;
        float mouse_x = event.motion.x;

        float new_volume = (mouse_x - slider_x) / slider_width;
        new_volume = std::max(0.0f, std::min(1.0f, new_volume));

        // Update state and re-render
        if (std::abs(new_volume - volume_) > 0.01f) {
            volume_ = new_volume;
            update_demo_widgets();
        }
    }
    break;
```

---

## 🔧 Event Flow

### Complete Event Lifecycle

```
1. SDL Mouse Event
   ↓
2. Instance::send_pointer_event(x, y, is_down)
   ↓
3. Hit Testing (递归遍历 scene graph)
   → Find deepest Node at (x, y)
   ↓
4. Build Propagation Path
   → [Root, Parent, Parent, ..., Target]
   ↓
5. Dispatch Event
   → Capture Phase: Root → Target
   → Target Phase: At Target
   → Bubble Phase: Target → Root
   ↓
6. Fire Callbacks
   → on_hover_enter/leave (no bubbling)
   → on_pointer_down/up/move (with bubbling)
   → on_click (pointer down + up on same node)
   ↓
7. Update State & Re-render
```

---

## 📊 Available Event Callbacks (Node API)

### Pointer Events

```cpp
node->on_pointer_down([](flex::PointerEvent& e) {
    // 鼠标按下
    std::cout << "Down at (" << e.x << ", " << e.y << ")\n";
    std::cout << "Local: (" << e.local_x << ", " << e.local_y << ")\n";
    std::cout << "Target: " << e.target->id() << "\n";
});

node->on_pointer_up([](flex::PointerEvent& e) {
    // 鼠标释放
});

node->on_pointer_move([](flex::PointerEvent& e) {
    // 鼠标移动
});

node->on_hover_enter([](flex::PointerEvent& e) {
    // 鼠标进入 Node bounds
});

node->on_hover_leave([](flex::PointerEvent& e) {
    // 鼠标离开 Node bounds
});

node->on_click([]() {
    // 点击（MouseDown + MouseUp on same node）
    std::cout << "Clicked!\n";
});
```

### Keyboard Events (未来)

```cpp
node->on_key_down([](flex::KeyEvent& e) {
    if (e.key == flex::KeyCode::Enter) {
        std::cout << "Enter pressed\n";
    }
});

node->on_key_up([](flex::KeyEvent& e) {
    // 键盘释放
});
```

---

## 🎨 Event Propagation Control

### Stop Propagation

```cpp
button->on_pointer_down([](flex::PointerEvent& e) {
    // Handle event
    handle_button_click();

    // Stop event from bubbling up to parent
    e.stop_propagation();
});
```

### Event Phases

```cpp
button->on_pointer_down([](flex::PointerEvent& e) {
    if (e.phase == flex::EventPhase::Capture) {
        std::cout << "Capture phase (Root → Target)\n";
    } else if (e.phase == flex::EventPhase::Target) {
        std::cout << "Target phase (At this node)\n";
    } else if (e.phase == flex::EventPhase::Bubble) {
        std::cout << "Bubble phase (Target → Root)\n";
    }
});
```

---

## 🚀 Practical Patterns

### Pattern 1: Draggable Widget

```cpp
class DraggableWidget {
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;

    void setup() {
        widget_->on_pointer_down([this](flex::PointerEvent& e) {
            dragging_ = true;
            drag_offset_x_ = e.local_x;
            drag_offset_y_ = e.local_y;
            e.stop_propagation();
        });

        // Handle mouse move at Instance level
        instance_->send_pointer_event(...);  // In event loop

        if (dragging_) {
            float new_x = mouse_x - drag_offset_x_;
            float new_y = mouse_y - drag_offset_y_;
            widget_->set_position(new_x, new_y);
        }
    }
};
```

### Pattern 2: Toggle Button

```cpp
auto toggle = create_component_instance("Toggle", {
    {"on", is_enabled_}
});

toggle->on_click([this]() {
    is_enabled_ = !is_enabled_;
    recreate_widget();  // Re-render with new state
});
```

### Pattern 3: Value Slider

```cpp
slider->on_pointer_down([this](flex::PointerEvent& e) {
    dragging_slider_ = true;
});

// In mouse move handler:
if (dragging_slider_) {
    float t = (mouse_x - slider_x) / slider_width;
    value_ = lerp(min_value_, max_value_, clamp(t, 0, 1));
    update_widgets();
}
```

---

## 🔮 Future Enhancements

**Phase 1: Basic Events** ✅
- Pointer events (Down, Up, Move, Enter, Leave)
- Click detection
- Hit testing
- Event propagation

**Phase 2: Advanced Interactions** 🚧
- Drag & Drop
- Scroll events
- Multi-touch (pinch, rotate)
- Gesture recognition

**Phase 3: Focus & Keyboard** 🚧
- Focus management
- Tab navigation
- Keyboard shortcuts
- Text input events

**Phase 4: Accessibility** 🔮
- Screen reader support
- Keyboard-only navigation
- ARIA attributes

---

## 📈 Performance

- **Hit Testing**: O(n) 递归遍历，但通常只需检查可见 nodes
- **Event Dispatch**: O(depth) 沿着 propagation path 分发
- **Memory**: 零额外开销（回调存储在 Node 中）

**Optimization Tips**:
- 使用 `e.stop_propagation()` 减少不必要的传播
- 避免在 event handlers 中做重计算
- 批量更新：在 mouse move 中限制更新频率

---

Built with 🎮 by Flex Engine Team
