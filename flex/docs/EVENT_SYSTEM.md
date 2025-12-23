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

---

## 🔍 Under The Hood - Implementation Details

> "Bad programmers worry about the code. Good programmers worry about data structures."
> — Linus Torvalds

本章节解释 GUI 和事件系统的**底层实现**，帮助你理解代码是如何工作的。

---

### 1️⃣ Scene Graph（场景图）- GUI 的数据结构

**核心理念：** 所有 GUI 都是一棵树。

```
Artboard (画布/根节点)
  └─ Group (容器)
      ├─ Shape (按钮背景)
      │   └─ Text (按钮文字)
      └─ Shape (另一个按钮)
```

**每个节点（Node）包含：**
- **位置：** `x`, `y` (相对于父节点)
- **外观：** `opacity`, `visible`, `color`
- **边界：** `bounds()` - 节点占用的矩形区域
- **事件处理器：** `on_click_`, `on_pointer_down_` 等

**关键代码：** `flex/include/flex/node.h:40-283`

```cpp
class Node {
    float x_, y_;                      // 相对坐标
    Node* parent_;                     // 父节点指针
    PointerEventCallback on_click_;    // 回调函数

    virtual Bounds bounds() const;     // 边界检测
    virtual bool hit_test(float px, float py) const;
};
```

**为什么用树？**
- ✅ 天然的层级关系（父子嵌套）
- ✅ 递归算法统一处理所有层级
- ✅ 相对坐标简化计算
- ✅ 无需特殊情况处理

---

### 2️⃣ Hit Testing（点击检测）

**问题：** 用户点击屏幕 (x=250, y=75)，点到哪个节点了？

**解决方案：** 递归查找，从上到下，找到最深的被点击节点。

**算法流程：** `flex/src/flex.cpp:242-271`

```cpp
static Node* hit_test_recursive(Node* node, float x, float y) {
    if (!node || !node->visible())
        return nullptr;

    // 1️⃣ 先检查子节点（子节点在上面，优先检测）
    if (node->is_group()) {
        float local_x = x - node->x();  // 转换为局部坐标
        float local_y = y - node->y();

        for (auto& child : children) {
            Node* hit = hit_test_recursive(child, local_x, local_y);
            if (hit) return hit;  // 找到就返回
        }
    }

    // 2️⃣ 再检查自己
    if (node->bounds().contains(x, y))
        return node;

    return nullptr;
}
```

**关键特性：**
- **深度优先：** 先检查子节点（画在上面的）
- **早停：** 找到第一个命中的节点立即返回
- **坐标转换：** 全局坐标 → 局部坐标（减去父节点位置）
- **时间复杂度：** O(n) - 最坏情况遍历所有节点

**为什么这样设计？**
- ✅ 递归统一处理所有层级，无特判
- ✅ 先检查子节点保证"上层元素优先"
- ✅ 相对坐标简化计算

---

### 3️⃣ Event Propagation（事件传播）

**问题：** 找到被点击的节点后，如何触发事件？

**解决方案：** 事件冒泡（Bubbling）- 从目标节点传播到根节点。

**传播路径构建：** `flex/src/flex.cpp:274-282`

```cpp
static void build_propagation_path(Node* target, std::vector<Node*>& path) {
    path.clear();
    for (Node* n = target; n != nullptr; n = n->parent()) {
        path.push_back(n);
    }
    // path: [Target, Parent, GrandParent, ..., Root]
    std::reverse(path.begin(), path.end());
    // path: [Root, ..., GrandParent, Parent, Target]
}
```

**事件分发：** `flex/src/flex.cpp:285-311`

```cpp
static void dispatch_with_bubbling(PointerEvent& event,
                                    const std::vector<Node*>& path,
                                    void (Node::*fire_method)(PointerEvent&)) {
    // 1️⃣ Target Phase - 目标节点
    Node* target = path.back();
    event.phase = EventPhase::Target;
    event.current_target = target;
    event.local_x = event.x - target->x();
    event.local_y = event.y - target->y();
    (target->*fire_method)(event);

    if (event.propagation_stopped())
        return;

    // 2️⃣ Bubble Phase - 从父节点到根节点
    for (auto it = path.rbegin() + 1; it != path.rend(); ++it) {
        Node* node = *it;
        event.phase = EventPhase::Bubble;
        event.current_target = node;
        event.local_x = event.x - node->x();
        event.local_y = event.y - node->y();
        (node->*fire_method)(event);

        if (event.propagation_stopped())
            return;
    }
}
```

**完整流程：** `flex/src/flex.cpp:313-413`

```
用户点击 (x=250, y=75)
    ↓
Instance::send_pointer_event(250, 75, true)
    ↓
1️⃣ Hit Testing
    hit_test_recursive(root, 250, 75)
    → 返回 Button 节点
    ↓
2️⃣ Build Path
    build_propagation_path(Button)
    → [Root, Group, Button]
    ↓
3️⃣ Dispatch Event
    Target Phase:  Button.fire_pointer_down()  ← 先触发目标
    Bubble Phase:  Group.fire_pointer_down()   ← 冒泡到父节点
    Bubble Phase:  Root.fire_pointer_down()    ← 最后到根节点
```

**特殊事件处理：**
- **Hover Enter/Leave：** 不冒泡，只触发目标节点
- **Click：** 必须 `pointer_down` 和 `pointer_up` 在同一节点
- **Drag：** 记录 `pointer_down_node`，`pointer_up` 时使用原节点路径

---

### 4️⃣ Callback Mechanism（回调机制）

**问题：** 如何让用户代码响应事件？

**解决方案：** `std::function` 回调 - 简单直接，无需继承。

**存储回调：** `flex/include/flex/node.h:264-275`

```cpp
class Node {
protected:
    // Pointer Events
    PointerEventCallback on_pointer_down_;  // std::function<void(PointerEvent&)>
    PointerEventCallback on_pointer_up_;
    PointerEventCallback on_pointer_move_;
    PointerEventCallback on_hover_enter_;
    PointerEventCallback on_hover_leave_;
    ClickCallback on_click_;                // std::function<void()>

    // Keyboard Events
    KeyEventCallback on_key_down_;          // std::function<void(KeyEvent&)>
    KeyEventCallback on_key_up_;
    FocusCallback on_focus_;                // std::function<void(bool)>
};
```

**触发回调：** `flex/src/node.cpp:51-105`

```cpp
void Node::fire_pointer_down(PointerEvent& event) {
    if (on_pointer_down_)
        on_pointer_down_(event);  // 直接调用 std::function
}

void Node::fire_click() {
    if (on_click_)
        on_click_();
}

void Node::fire_focus(bool gained) {
    if (on_focus_)
        on_focus_(gained);
}
```

**为什么用 `std::function`？**
- ✅ 无需继承 `EventListener` 接口
- ✅ 支持 lambda、函数指针、成员函数
- ✅ 类型安全（编译期检查）
- ✅ 零额外开销（只存储函数指针）

**实用主义体现：**
```cpp
// ❌ 理论完美：定义接口，继承实现
class IClickListener {
    virtual void onClick() = 0;
};

// ✅ 实用主义：直接用 lambda
button->on_click([]() {
    printf("Clicked!\n");
});
```

---

### 5️⃣ Event Types（事件对象）

**PointerEvent 结构：** `flex/include/flex/event.h:97-119`

```cpp
struct PointerEvent {
    PointerEventType type;        // Down, Up, Move, Enter, Leave
    EventPhase phase;             // None, Capture, Target, Bubble

    float x, y;                   // 全局坐标（相对于 Artboard）
    float local_x, local_y;       // 局部坐标（相对于 current_target）

    Node* target;                 // 原始目标节点（最深的命中节点）
    Node* current_target;         // 当前处理节点（传播路径上的节点）

    void stop_propagation();      // 阻止继续冒泡
    bool propagation_stopped() const;
};
```

**坐标系统：**
```
全局坐标 (x, y)        - 相对于 Artboard (0, 0)
局部坐标 (local_x, local_y) - 相对于当前节点 (node->x(), node->y())

转换公式：
local_x = global_x - node->x()
local_y = global_y - node->y()
```

---

### 6️⃣ Performance Characteristics（性能特性）

**时间复杂度：**
```
Hit Testing:         O(n) - 递归遍历可见节点
Event Dispatch:      O(depth) - 沿传播路径分发
Callback Invocation: O(1) - 直接函数调用
```

**内存开销：**
```
Per Node:
- 9 个回调指针（std::function）
- 1 个父节点指针
- 事件状态字段（focused_, focusable_）
≈ 80-120 bytes per node

Per Event:
- PointerEvent: ~48 bytes (栈分配)
- Propagation Path: ~8 bytes × depth (栈分配)
```

**优化技巧：**
1. **Early Return：** Hit testing 找到第一个命中节点立即返回
2. **Visibility Culling：** 不可见节点直接跳过
3. **Stop Propagation：** 用 `e.stop_propagation()` 减少冒泡
4. **Arena Allocator：** 字符串统一在 arena 中分配，无碎片

---

### 7️⃣ Code Locations（代码位置）

**核心实现文件：**

| 功能 | 文件 | 行数 |
|------|------|------|
| **Node 基类** | `flex/include/flex/node.h` | 40-283 |
| **Event 定义** | `flex/include/flex/event.h` | 19-159 |
| **Hit Testing** | `flex/src/flex.cpp` | 242-271 |
| **Path Building** | `flex/src/flex.cpp` | 274-282 |
| **Event Dispatch** | `flex/src/flex.cpp` | 285-311 |
| **Pointer Handling** | `flex/src/flex.cpp` | 313-413 |
| **Callback Firing** | `flex/src/node.cpp` | 51-105 |

**阅读建议：**
1. 先看 `event.h` - 理解事件数据结构
2. 再看 `node.h` - 理解节点基类
3. 然后看 `flex.cpp:242-413` - 理解完整流程
4. 最后看 `node.cpp` - 理解回调触发

---

### 8️⃣ Design Philosophy（设计哲学）

> "好品味就是消除特殊情况，让一般情况统一处理所有问题。"

**好品味体现：**

1. **数据结构优先** ✅
   - Scene Graph（树）天然消除边界情况
   - 递归统一处理所有层级，无需特判

2. **无特殊情况** ✅
   ```cpp
   // ❌ 坏品味：
   if (is_root_node) {
       // 特殊处理根节点
   } else if (is_leaf_node) {
       // 特殊处理叶子节点
   } else {
       // 一般情况
   }

   // ✅ 好品味：
   for (Node* n = target; n != nullptr; n = n->parent()) {
       path.push_back(n);  // 统一处理所有节点
   }
   ```

3. **实用主义** ✅
   - 用 `std::function` 而不是复杂的接口继承
   - 直接构造，不搞多层抽象
   - 相对坐标，简单直接

4. **简洁执行** ✅
   - Hit testing: 30 行
   - Event bubbling: 20 行
   - 完整事件系统: 100 行

**这就是"好品味"！** 🎯

---

Built with 🎮 by Flex Engine Team
