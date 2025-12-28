# State Management - Observable Reactive State System

响应式数据绑定，让 UI 自动跟随数据变化更新

**注意**：本文档中的 `ObservableState` 类用于响应式状态管理，与 `machine.h` 中的状态机 `State` 类是不同的概念。

---

## 📋 Overview

**State Management** 是 Flex Engine 的观察者模式状态管理系统，实现了：
- **ObservableState**: 自动通知机制的状态容器
- **Reactive Bindings**: 状态变化自动触发 UI 更新
- **Zero Manual Updates**: 无需手动调用 `rebuild()` 或 `render()`
- **Batch Updates**: 批量状态变更，一次性通知

---

## 🎯 Problem Solved

### Before: Manual UI Updates

```cpp
// 痛点：每次数据变化需要手动更新 UI
float cpu_usage = 0.5f;

// 用户操作改变数据
cpu_usage = 0.8f;

// ❌ 需要手动找到所有相关 UI 组件并更新
slider->set_value(cpu_usage);
progress_bar->set_progress(cpu_usage);
label->set_text("CPU: 80%");
// ...如果忘记更新某个组件就会出 bug
```

**痛点**:
- 数据与 UI 不同步
- 手动管理更新逻辑，容易遗漏
- 代码分散在多处，难以维护

---

### After: Reactive State

```cpp
// ✅ 声明式：状态改变自动更新所有绑定的 UI
auto state = State::create();
state->set("cpu_usage", 0.5f);

// 组件绑定到状态
bind_component_to_state(slider, state, "cpu_usage", "value");
bind_component_to_state(progress_bar, state, "cpu_usage", "progress");

// 数据变化，UI 自动更新！
state->set("cpu_usage", 0.8f);  // slider 和 progress_bar 自动重建
```

**收益**:
- 数据是单一数据源（Single Source of Truth）
- UI 自动同步，零手动更新
- 代码集中，易于理解和维护

---

## 🏗️ Architecture

### 三层结构

```
┌─────────────────────────────────────────┐
│  Application Logic (业务逻辑)           │
│  - state->set("volume", 0.8f)          │
└───────────────┬─────────────────────────┘
                │ Observable Pattern
                ▼
┌─────────────────────────────────────────┐
│  State Management (状态管理)            │
│  - State: Observable container         │
│  - Observers: watch() callbacks        │
└───────────────┬─────────────────────────┘
                │ Binding Layer
                ▼
┌─────────────────────────────────────────┐
│  UI Components (组件层)                 │
│  - ReactiveGroup: Auto-update children │
│  - StateComponentBinding: Rebuild logic│
└─────────────────────────────────────────┘
```

### 核心类

**1. `State` (state.h)**
- 存储键值对数据（`std::variant<float, string, bool, uint32_t>`）
- 提供观察者机制（`watch()`, `watch_all()`）
- 值变化时自动通知所有观察者
- 支持批量更新（`begin_batch()` / `end_batch()`）

**2. `StateComponentBinding` (state_binding.h)**
- 连接 State 和 Component
- 监听 State 变化
- 自动重建 Component（用新 Props）
- 替换场景图中的节点

**3. `ReactiveGroup` (state_binding.h)**
- 特殊的 Group，管理响应式子组件
- 简化 API：`add_bound_component()`
- 自动处理节点替换

---

## 📖 Usage Guide

### 1. Create Observable State

```cpp
#include "flex/state.h"

auto app_state = flex::State::create();

// Set initial values
app_state->set("volume", 0.5f);              // float
app_state->set("username", std::string("Alice"));  // string
app_state->set("muted", false);              // bool
app_state->set("theme_color", 0xFF0D6EFD);  // uint32_t (color)
```

---

### 2. Read State Values

```cpp
// Type-safe getters
float vol = app_state->get<float>("volume", 0.0f);
std::string name = app_state->get<std::string>("username", "Guest");
bool is_muted = app_state->get<bool>("muted", false);

// Specialized helpers
float vol2 = app_state->get_float("volume", 0.0f);
std::string name2 = app_state->get_string("username", "Guest");
bool muted2 = app_state->get_bool("muted", false);
uint32_t color = app_state->get_color("theme_color", 0xFFFFFFFF);

// Check existence
if (app_state->has("volume")) {
    // ...
}
```

---

### 3. Watch State Changes (Manual)

```cpp
// Watch specific key
size_t observer_id = app_state->watch("volume", [](const std::string& key, const StateValue& value) {
    float vol = std::get<float>(value);
    std::cout << "Volume changed to: " << vol << std::endl;
});

// Watch all keys
size_t global_id = app_state->watch_all([](const std::string& key, const StateValue& value) {
    std::cout << "State changed: " << key << std::endl;
});

// Unwatch
app_state->unwatch(observer_id);
```

---

### 4. Bind State to Components (Recommended)

#### 方式 A: ReactiveGroup (最简单)

```cpp
#include "flex/state_binding.h"

// Create reactive group
auto reactive_group = flex::ReactiveGroup::create(app_state);
reactive_group->set_position(50, 100);

// Add bound component
reactive_group->add_bound_component(
    "Slider",                          // Component name
    {{"value", 0.5f}, {"width", 300.0f}},  // Initial props
    {{"volume", "value"}},            // Bindings: state_key -> prop_name
    0, 40                             // Position (x, y)
);

// Add to scene
scene->add_child(reactive_group);

// State change → Slider auto-updates!
app_state->set("volume", 0.8f);
```

#### 方式 B: Manual Binding (更灵活)

```cpp
// Create binding manually
auto binding = flex::StateComponentBinding(
    app_state,
    "Slider",
    {{"value", 0.5f}, {"width", 300.0f}}
);

// Bind state keys to props
binding->bind("volume", "value");
binding->bind("theme_color", "color");

// Setup rebuild callback
binding->on_rebuild([scene](Node::Ptr old_node, Node::Ptr new_node) {
    // Replace in scene graph
    scene->remove_child(old_node);
    scene->add_child(new_node);
});

// Get component
auto slider = binding->component();
scene->add_child(slider);
```

---

### 5. Batch Updates (Performance Optimization)

```cpp
// Without batching: 4 individual notifications
app_state->set("cpu", 0.5f);     // Notify
app_state->set("memory", 0.6f);  // Notify
app_state->set("disk", 0.7f);    // Notify
app_state->set("network", 0.4f); // Notify

// With batching: 1 combined notification
app_state->begin_batch();
app_state->set("cpu", 0.5f);
app_state->set("memory", 0.6f);
app_state->set("disk", 0.7f);
app_state->set("network", 0.4f);
app_state->end_batch();  // Trigger all updates at once
```

**Use batching when**:
- Updating multiple related values
- Animation loops (update 4+ values per frame)
- Loading data from network/file

---

## 🎨 Complete Example

```cpp
#include "flex.h"
#include "flex/state.h"
#include "flex/state_binding.h"

int main() {
    flex::init();

    // 1. Create state
    auto app_state = flex::State::create();
    app_state->set("volume", 0.5f);
    app_state->set("brightness", 0.7f);

    // 2. Register components (Slider, ProgressBar)
    register_components();

    // 3. Build UI with reactive bindings
    auto instance = flex::Instance::create(800, 600);
    auto scene = instance->scene();

    // Volume section (reactive group)
    auto volume_section = flex::ReactiveGroup::create(app_state);
    volume_section->set_position(50, 100);

    // Slider bound to "volume"
    volume_section->add_bound_component(
        "Slider",
        {{"value", 0.5f}, {"width", 400.0f}, {"color", 0xFF0D6EFD}},
        {{"volume", "value"}},
        0, 0
    );

    // Progress bar also bound to "volume"
    volume_section->add_bound_component(
        "ProgressBar",
        {{"progress", 0.5f}, {"width", 400.0f}, {"color", 0xFF0D6EFD}},
        {{"volume", "progress"}},
        0, 50
    );

    scene->add_child(volume_section);

    // Brightness section
    auto brightness_section = flex::ReactiveGroup::create(app_state);
    brightness_section->set_position(50, 250);

    brightness_section->add_bound_component(
        "Slider",
        {{"value", 0.7f}, {"width", 400.0f}, {"color", 0xFFFFC107}},
        {{"brightness", "value"}},
        0, 0
    );

    scene->add_child(brightness_section);

    // 4. Simulate state changes
    float time = 0;
    while (running) {
        time += dt;

        // Update state - UI auto-updates!
        app_state->begin_batch();
        app_state->set("volume", 0.5f + 0.3f * std::sin(time));
        app_state->set("brightness", 0.7f + 0.2f * std::cos(time));
        app_state->end_batch();

        instance->render(renderer);
        SDL_Delay(16);
    }

    flex::shutdown();
    return 0;
}
```

---

## 🔧 Implementation Details

### How It Works

**Step 1: State Change**
```cpp
state->set("volume", 0.8f);
```

**Step 2: State Notifies Observers**
```cpp
// Inside State::set()
if (values_[key] != new_value) {
    values_[key] = new_value;
    notify(key, new_value);  // Trigger all watchers
}
```

**Step 3: StateComponentBinding Rebuilds**
```cpp
// Inside StateComponentBinding::bind()
state->watch("volume", [this, prop_name](const string& key, const StateValue& val) {
    props_[prop_name] = val;  // Update prop
    rebuild();                // Recreate component
});
```

**Step 4: Component Recreated**
```cpp
void rebuild() {
    auto old = component_;
    component_ = create_component_instance(component_name_, props_);

    // Copy position, opacity, etc.
    component_->set_position(old->x(), old->y());

    // Replace in scene graph
    if (rebuild_callback_) {
        rebuild_callback_(old, component_);
    }
}
```

---

### Smart Change Detection

```cpp
// No notification if value unchanged
state->set("volume", 0.5f);
state->set("volume", 0.5f);  // Same value, no notify

// Only second call triggers update
state->set("volume", 0.5f);
state->set("volume", 0.8f);  // Different value, notify!
```

---

## 🎯 Design Patterns

### 1. Single Source of Truth

```cpp
// ❌ Bad: Multiple sources of truth
float slider_value = 0.5f;
float progress_value = 0.5f;
float label_value = 0.5f;
// Which is the real value?

// ✅ Good: One source, all UI reads from it
auto state = State::create();
state->set("volume", 0.5f);
// All UI components bound to state["volume"]
```

---

### 2. Unidirectional Data Flow

```
User Action → Update State → UI Auto-Updates
    ↓              ↓               ↓
 onClick()    state->set()    watch callback
```

```cpp
// User clicks button
button->on_click([state]() {
    float current = state->get_float("volume", 0.0f);
    state->set("volume", current + 0.1f);  // Update state
    // UI updates automatically via watchers!
});
```

---

### 3. Declarative Bindings

```cpp
// Declarative: What to bind (not how to update)
reactive_group->add_bound_component(
    "Slider",
    {{"value", 0.5f}},
    {{"volume", "value"}}  // volume state → value prop
);

// Imperative equivalent (old way):
auto slider = create_slider();
state->watch("volume", [slider](auto key, auto val) {
    slider->set_value(std::get<float>(val));
    slider->rebuild();
    // ... more manual work
});
```

---

## 📊 Performance Characteristics

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| `state->set()` | O(N) observers | N = number of watchers on that key |
| `state->get()` | O(1) | Direct map lookup |
| Component rebuild | O(C) | C = component complexity (builder function cost) |
| Batch update | O(N * K) | N observers × K changed keys (but only 1 render) |

**Optimization Tips**:
1. Use batching for multiple related updates
2. Avoid watching `*_all` unless necessary
3. Keep component builders lightweight
4. Unwatch observers when components destroyed

---

## 🚀 Advanced Use Cases

### 1. Computed State (Derived Values)

```cpp
// Derived value: total = cpu + memory + disk
state->watch_all([state](const string& key, const StateValue& val) {
    if (key == "cpu" || key == "memory" || key == "disk") {
        float total = state->get_float("cpu", 0) +
                     state->get_float("memory", 0) +
                     state->get_float("disk", 0);
        state->set("total_usage", total / 3.0f);
    }
});
```

### 2. Two-Way Binding (User Input → State)

```cpp
// UI → State
slider->on_drag([state](float new_value) {
    state->set("volume", new_value);
});

// State → UI (already handled by binding)
reactive_group->add_bound_component(...);
```

### 3. State Persistence

```cpp
// Save state to file
void save_state(State::Ptr state, const string& path) {
    std::ofstream file(path);
    for (const auto& key : state->keys()) {
        auto val = state->get<float>(key, 0.0f);
        file << key << "=" << val << "\n";
    }
}

// Load state from file
void load_state(State::Ptr state, const string& path) {
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        auto pos = line.find('=');
        string key = line.substr(0, pos);
        float val = std::stof(line.substr(pos + 1));
        state->set(key, val);  // UI auto-updates!
    }
}
```

---

## ⚠️ Common Pitfalls

### 1. Forgetting to Unwatch

```cpp
// ❌ Memory leak: observer never cleaned up
{
    auto temp_component = ...;
    size_t id = state->watch("key", [temp_component](...) {
        // This lambda captures temp_component
    });
    // temp_component destroyed, but observer still alive!
}

// ✅ Always unwatch when done
StateComponentBinding binding(...);  // RAII: unwatches in destructor
```

### 2. Infinite Update Loops

```cpp
// ❌ Infinite loop
state->watch("a", [state](...) {
    state->set("b", ...);  // Triggers b watchers
});

state->watch("b", [state](...) {
    state->set("a", ...);  // Triggers a watchers → infinite loop!
});

// ✅ Use flags to prevent cycles
bool updating = false;
state->watch("a", [&updating, state](...) {
    if (updating) return;
    updating = true;
    state->set("b", ...);
    updating = false;
});
```

### 3. Expensive Rebuilds in Hot Loops

```cpp
// ❌ Rebuilding every frame (expensive)
while (running) {
    state->set("time", current_time);  // Triggers rebuild
    // Component rebuilds 60 times/second!
}

// ✅ Throttle updates or use time-based logic
while (running) {
    if (current_time - last_update > 0.1f) {  // Update every 100ms
        state->set("time", current_time);
        last_update = current_time;
    }
}
```

---

## 📚 Comparison with Existing Binding System

| Feature | `binding.h` (Old) | `state.h` (New) |
|---------|------------------|----------------|
| **Pattern** | Pull-based | Push-based |
| **Update** | Manual `evaluate()` | Automatic on `set()` |
| **Expressions** | ✅ Complex math expressions | ❌ Simple key-value |
| **Simplicity** | Complex (parser, AST) | Simple (variant + callbacks) |
| **Use Case** | Animation curves, formulas | Reactive UI state |

**When to use what**:
- **state.h**: Simple reactive UI (volume slider, checkbox states, theme toggles)
- **binding.h**: Complex expressions (`x = sin(time * 2) * 100`)

They complement each other! You can use both in the same app.

---

## 🔮 Future Enhancements

**Phase 2: DSL State Binding** (planned):
```flex
state appState {
    volume: 0.5
    brightness: 0.7
}

scene app {
    Slider volumeSlider {
        value: @appState.volume  // Bind to state
        width: 300
    }
}
```

**Phase 3: Middleware** (planned):
```cpp
state->add_middleware([](const string& key, const StateValue& old_val, const StateValue& new_val) {
    // Logging
    std::cout << key << ": " << old_val << " → " << new_val << std::endl;

    // Validation
    if (key == "volume" && std::get<float>(new_val) > 1.0f) {
        return old_val;  // Reject change
    }

    return new_val;  // Accept change
});
```

---

Built with 🔄 Reactive Programming Principles
