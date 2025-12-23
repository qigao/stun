# Component-Level State Machines for Flex

## 概览

扩展 Flex 状态机系统，支持**组件级状态机**，将视觉状态和视觉效果声明在 DSL 中，而不是在 C++ 代码中管理。

---

## 🎯 核心问题

### ❌ 旧方案（MVC 分离）
```cpp
// Model - 视觉状态在 C++
struct ButtonState {
    bool pressed;  // 这是视觉状态，不是业务数据
};

// View - 视觉效果在 C++
void updateButton(const ButtonState& state) {
    node->set_scale(state.pressed ? 1.1 : 1.0);  // 硬编码
}
```

**问题**：
- 添加 `hover` 状态 → 改 C++ Model + View + Controller
- 调整动画参数 → 改 C++ 硬编码值
- 设计师无法独立调整 UI

### ✅ 新方案（DSL 状态机）
```flex
button primaryButton {
    machine {
        layer interaction {
            state normal { bg.fill: #00d9ff, scale: 1.0 }
            state hover { bg.fill: #00e5ff, scale: 1.05 }
            state pressed { bg.fill: #00b8d4, scale: 1.1 }

            transition normal -> hover on "mouseenter"
            transition hover -> pressed on "mousedown"
        }
    }

    rect bg { ... }
    text label { ... }
}
```

**优势**：
- 视觉状态和视觉效果在同一处定义
- 设计师可以直接编辑 .flex 文件
- 添加新状态只需编辑 DSL

---

## 🏗️ 架构设计

### 1. PropertyValue（属性值系统）

```cpp
struct PropertyValue {
    using Value = std::variant<float, Color, std::string, bool>;
    Value value;

    PropertyValue(float v);
    PropertyValue(const Color& c);
    PropertyValue(const std::string& s);
    PropertyValue(bool b);
};
```

**用途**：存储状态属性值（scale, fill, opacity 等）

### 2. State（状态扩展）

```cpp
class State {
public:
    // 原有功能
    void set_animation(const std::string& anim);

    // 新增：属性设置
    void set_property(const std::string& name, const PropertyValue& value);
    void apply_properties(Node* node) const;

private:
    std::unordered_map<std::string, PropertyValue> properties_;
};
```

**示例**：
```cpp
state->set_property("scale", 1.1f);
state->set_property("bg.fill", Color::from_hex("#00d9ff"));
state->set_property("opacity", 0.5f);
```

### 3. Machine（事件系统）

```cpp
class Machine {
public:
    // 新增：事件系统
    void fire_event(const std::string& event_name);
    void clear_events();  // 每帧结束清空
    bool has_event(const std::string& event_name) const;

    // 新增：Owner 节点
    void set_owner(Node* node);
    Node* owner() const;

private:
    std::vector<std::string> fired_events_;  // 事件队列
    Node* owner_;  // 所属节点
};
```

**工作流程**：
1. 鼠标事件 → `machine->fire_event("mouseenter")`
2. 状态机检查转换条件 → `transition normal -> hover on "mouseenter"`
3. 进入新状态 → `state->apply_properties(owner_node)`

### 4. 事件类型

**标准 UI 事件**：
- `mouseenter` - 鼠标进入
- `mouseleave` - 鼠标离开
- `mousedown` - 鼠标按下
- `mouseup` - 鼠标释放
- `click` - 点击
- `focus` - 获得焦点
- `blur` - 失去焦点

**自定义事件**：
- `submit` - 表单提交
- `success` / `error` - 异步操作结果

---

## 📝 DSL 语法

### 基础语法

```flex
group componentName {
    machine {
        layer layerName {
            state stateName {
                initial: true           // 初始状态（可选）
                animation: "animName"   // 动画（可选）

                // 属性设置
                propertyName: value
                child.property: value
            }

            transition from -> to on "event"
            transition from -> to when input > value
            transition from -> to after 2.0s
        }
    }

    // 子节点
    rect bg { ... }
    text label { ... }
}
```

### 属性路径

```flex
state hover {
    scale: 1.05              // 直接属性
    opacity: 0.8             // 直接属性
    bg.fill: #00e5ff         // 子节点属性
    label.content: "Hover!"  // 子节点属性
}
```

### 转换类型

```flex
// 事件触发
transition normal -> hover on "mouseenter"

// Input 条件
transition neutral -> positive when counter > 0

// 时间延迟
transition success -> idle after 2.0s

// 动画结束
transition enter -> idle on_anim_end
```

---

## 🔧 实现状态

### ✅ 已完成

1. **PropertyValue 系统** (machine.h:29-50)
   - 支持 float, Color, string, bool
   - 类型安全的 variant

2. **State 属性扩展** (machine.h:157-193)
   - `set_property()` - 设置属性
   - `apply_properties()` - 应用到节点（TODO）
   - `properties_` - 属性存储

3. **Machine 事件系统** (machine.h:279-328)
   - `fire_event()` - 触发事件
   - `clear_events()` - 清空队列
   - `has_event()` - 检查事件
   - `owner_` - 所属节点

4. **实现文件** (machine.cpp)
   - State 方法实现
   - Machine 事件方法实现

5. **示例语法** (ui_components_with_statemachine.flex)
   - Button 状态机
   - Toggle 状态机
   - Slider 状态机
   - Checkbox 状态机
   - Loading 状态机

### ⏳ 待完成

1. **DSL 解析器扩展**
   - 解析 `machine { }` 块
   - 解析 `state { property: value }`
   - 解析 `transition ... on "event"`

2. **Node 集成**
   - Node 类添加 `Machine::Ptr machine_`
   - 实现 `State::apply_properties(Node*)`
   - 属性路径解析（"bg.fill" → find("bg")->set_fill()）

3. **事件分发**
   - SDL 事件 → Machine 事件映射
   - 鼠标进入/离开检测
   - Hit testing 集成

4. **示例应用**
   - 重写 ui_components_demo.cpp
   - 使用新的状态机语法
   - 展示 MVC vs 状态机的对比

---

## 📊 对比：MVC vs 状态机

| 场景 | MVC（旧） | 状态机（新） |
|------|----------|-------------|
| 添加 hover 状态 | 改 Model + View + Controller（3处） | .flex 添加 `state hover`（1处） |
| 调整动画参数 | 改 C++ 硬编码 | .flex 改数值 |
| 添加新组件 | 写 C++ 类（50+ 行） | .flex 定义状态（10 行） |
| 设计师调整 | 需要程序员 | 直接编辑 .flex |
| 代码量 | ~500 行 C++ | ~50 行 DSL + ~50 行 C++ |

---

## 🚀 下一步

1. **实现 DSL 解析器** - 解析内联 machine 语法
2. **Node 集成** - 添加状态机支持
3. **属性应用** - 实现 `State::apply_properties()`
4. **事件分发** - SDL → Machine 事件
5. **创建 Demo** - 展示新语法的威力

---

## 💡 设计哲学

### "Good Taste" - 消除特殊情况

**之前**：pressed/hover/focus 每个都是"特殊情况"
```cpp
if (button.pressed) scale = 1.1;
else if (button.hover) scale = 1.05;
else scale = 1.0;
```

**现在**：所有状态都是"正常情况"
```flex
state normal { scale: 1.0 }
state hover { scale: 1.05 }
state pressed { scale: 1.1 }
```

### "数据结构优先"

**错误设计**：视觉状态（pressed）在 Model，视觉效果（scale）在 View
**正确设计**：视觉状态 + 视觉效果在 DSL

### "Never break userspace"

设计师改 UI → 不破坏业务逻辑（关注点分离）

---

## 📁 文件清单

### 核心实现
- `flex/include/flex/machine.h` - 状态机头文件（已扩展）
- `flex/src/machine.cpp` - 状态机实现（已扩展）

### 示例
- `flex/examples/ui_components_with_statemachine.flex` - 新语法示例

### 待实现
- `flex/src/dsl_parser.cpp` - 解析器扩展
- `flex/include/flex/node.h` - Node 类扩展
- `flex/examples/ui_statemachine_demo.cpp` - 新 Demo

---

这就是 Flex 组件级状态机的完整架构设计！
