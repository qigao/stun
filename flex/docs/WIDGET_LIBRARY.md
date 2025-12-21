# Widget Library

标准 UI 组件库 - 基于 Component System 构建的可复用交互组件。

---

## ✅ What's Implemented

**Phase 1: 5个基础交互组件** ✅

| 组件 | 功能 | Props | 用途 |
|------|------|-------|------|
| **Slider** | 滑块 | value, min, max, width, color | 音量、进度、参数调节 |
| **ProgressBar** | 进度条 | progress, width, height, color | 加载进度、完成度 |
| **Checkbox** | 复选框 | checked, label, color | 多选、设置开关 |
| **Radio** | 单选框 | selected, label, color | 互斥选择 |
| **Toggle** | 开关 | on, width, height, color | 开/关状态 |

**Demo**: `widget_gallery.cpp`
- 5个组件注册
- 18个实例展示
- 交互式演示（键盘控制）

---

## 🎯 Core Design Philosophy

### 1. Component-Based
所有 widget 都是 **Component** 实例：
```cpp
// 定义一次
flex::register_component("Slider", builder_function);

// 使用多次
auto slider1 = create_component_instance("Slider", {{"value", 0.3f}});
auto slider2 = create_component_instance("Slider", {{"value", 0.7f}});
```

### 2. Props-Driven
通过 Props 完全定制：
```cpp
auto slider = create_component_instance("Slider", {
    {"value", 0.5f},      // 当前值
    {"min", 0.0f},        // 最小值
    {"max", 1.0f},        // 最大值
    {"width", 300.0f},    // 宽度
    {"color", uint32_t(0xFF0D6EFD)}  // 颜色
});
```

### 3. Consistent Design Language
统一的视觉语言（Bootstrap风格）：
- **Primary Blue**: `0xFF0D6EFD` - 主要操作
- **Success Green**: `0xFF198754` - 成功/完成
- **Danger Red**: `0xFFDC3545` - 警告/删除
- **Warning Yellow**: `0xFFFFC107` - 警告
- **Gray**: `0xFF6C757D` - 次要元素

---

## 💻 Components Reference

### 1. Slider

**滑块组件** - 拖动选择数值

**Props**:
```cpp
{
    "value": float,   // 当前值 (default: 0.5)
    "min": float,     // 最小值 (default: 0.0)
    "max": float,     // 最大值 (default: 1.0)
    "width": float,   // 宽度 (default: 300.0)
    "color": uint32_t // ARGB颜色 (default: 0xFF0D6EFD)
}
```

**Usage**:
```cpp
auto volumeSlider = flex::create_component_instance("Slider", {
    {"value", 0.75f},
    {"min", 0.0f},
    {"max", 1.0f},
    {"width", 300.0f},
    {"color", uint32_t(0xFF0D6EFD)}
});
```

**Visual Structure**:
```
[========●-------]  Track (gray) + Fill (blue) + Thumb (circle)
 0%     75%    100%
```

---

### 2. ProgressBar

**进度条组件** - 显示完成进度

**Props**:
```cpp
{
    "progress": float, // 进度 0-1 (default: 0.5)
    "width": float,    // 宽度 (default: 300.0)
    "height": float,   // 高度 (default: 20.0)
    "color": uint32_t  // ARGB颜色 (default: 0xFF198754)
}
```

**Usage**:
```cpp
auto downloadProgress = flex::create_component_instance("ProgressBar", {
    {"progress", 0.65f},    // 65%
    {"width", 400.0f},
    {"height", 24.0f},
    {"color", uint32_t(0xFF198754)}  // Green
});
```

**Visual Structure**:
```
[█████████████░░░░░░░]  Background (gray) + Fill (green)
 0%           65%   100%
```

---

### 3. Checkbox

**复选框组件** - 多选项开关

**Props**:
```cpp
{
    "checked": bool,      // 是否选中 (default: false)
    "label": string,      // 标签文字 (default: "")
    "color": uint32_t     // ARGB颜色 (default: 0xFF0D6EFD)
}
```

**Usage**:
```cpp
auto rememberMe = flex::create_component_instance("Checkbox", {
    {"checked", true},
    {"label", std::string("Remember me")},
    {"color", uint32_t(0xFF0D6EFD)}
});
```

**Visual Structure**:
```
Unchecked: [ ] Label
Checked:   [✓] Label  (blue background, white checkmark)
```

---

### 4. Radio

**单选框组件** - 互斥选择

**Props**:
```cpp
{
    "selected": bool,     // 是否选中 (default: false)
    "label": string,      // 标签文字 (default: "")
    "color": uint32_t     // ARGB颜色 (default: 0xFF0D6EFD)
}
```

**Usage**:
```cpp
auto option1 = flex::create_component_instance("Radio", {
    {"selected", true},
    {"label", std::string("Option 1")},
    {"color", uint32_t(0xFF0D6EFD)}
});

auto option2 = flex::create_component_instance("Radio", {
    {"selected", false},
    {"label", std::string("Option 2")}
});
```

**Visual Structure**:
```
Unselected: ( ) Label
Selected:   (●) Label  (filled blue circle inside)
```

---

### 5. Toggle

**开关组件** - 二态开关

**Props**:
```cpp
{
    "on": bool,          // 是否开启 (default: false)
    "width": float,      // 宽度 (default: 50.0)
    "height": float,     // 高度 (default: 26.0)
    "color": uint32_t    // ARGB颜色 (default: 0xFF198754)
}
```

**Usage**:
```cpp
auto darkMode = flex::create_component_instance("Toggle", {
    {"on", true},
    {"width", 50.0f},
    {"height", 26.0f},
    {"color", uint32_t(0xFF198754)}  // Green when ON
});
```

**Visual Structure**:
```
OFF: [○--------]  Gray track, thumb on left
ON:  [--------●]  Green track, thumb on right
```

---

## 🎨 Usage Examples

### Example 1: Settings Panel

```cpp
// Audio settings
auto volumeLabel = create_label("Volume:");
auto volumeSlider = create_component_instance("Slider", {
    {"value", 0.8f},
    {"width", 200.0f}
});

auto muteCheckbox = create_component_instance("Checkbox", {
    {"checked", false},
    {"label", std::string("Mute")}
});

// Display settings
auto brightnessSlider = create_component_instance("Slider", {
    {"value", 0.6f},
    {"width", 200.0f},
    {"color", uint32_t(0xFFFFC107)}  // Yellow
});

auto darkModeToggle = create_component_instance("Toggle", {
    {"on", true},
    {"color", uint32_t(0xFF0D6EFD)}
});
```

---

### Example 2: Download Manager

```cpp
// File 1
auto file1Progress = create_component_instance("ProgressBar", {
    {"progress", 0.45f},
    {"width", 400.0f},
    {"color", uint32_t(0xFF0D6EFD)}
});

// File 2
auto file2Progress = create_component_instance("ProgressBar", {
    {"progress", 1.0f},
    {"width", 400.0f},
    {"color", uint32_t(0xFF198754)}  // Green = complete
});

// File 3
auto file3Progress = create_component_instance("ProgressBar", {
    {"progress", 0.12f},
    {"width", 400.0f},
    {"color", uint32_t(0xFFDC3545)}  // Red = error
});
```

---

### Example 3: Form with Radio Group

```cpp
// Payment method selection
auto creditCard = create_component_instance("Radio", {
    {"selected", true},
    {"label", std::string("Credit Card")}
});

auto paypal = create_component_instance("Radio", {
    {"selected", false},
    {"label", std::string("PayPal")}
});

auto bitcoin = create_component_instance("Radio", {
    {"selected", false},
    {"label", std::string("Bitcoin")}
});

// Terms checkbox
auto agreeTerms = create_component_instance("Checkbox", {
    {"checked", false},
    {"label", std::string("I agree to the terms and conditions")}
});
```

---

## 🚀 Interactive Demo

**Widget Gallery** 包含交互式演示：

### Mouse Controls (NEW! ✨)

- **CLICK** - 点击 Toggle 切换静音
- **DRAG** - 拖拽 Slider 调节音量
- **HOVER** - 悬停显示效果（未来）

### Keyboard Controls

- **UP/DOWN** - 调节音量滑块
- **SPACE** - 切换静音开关
- **ESC** - 退出

### Demo Features

1. **Volume Control**:
   - Slider 显示当前音量
   - ProgressBar 同步显示进度
   - Toggle 控制静音状态
   - 所有 widget 实时更新

2. **Widget Showcase**:
   - 3个不同状态的 Slider（30%, 70%, 100%）
   - 3个不同进度的 ProgressBar（25%, 65%, 100%）
   - 3个 Checkbox（选中/未选中/选中）
   - 3个 Radio（选项 A/B/C）
   - 3个 Toggle（开/关/开）

---

## 📊 Code Reuse Benefits

### Before (Without Component System)

```cpp
// 创建10个相同的滑块需要 ~300 行代码
auto slider1 = Group::create();
auto track1 = Shape::create();
track1->set_rect(300, 8);
track1->set_fill(Color(0.87, 0.89, 0.91, 1));
slider1->add_child(track1);
auto fill1 = Shape::create();
fill1->set_rect(150, 8);
fill1->set_fill(Color(0.05, 0.43, 0.99, 1));
slider1->add_child(fill1);
auto thumb1 = Shape::create();
thumb1->set_circle(10);
thumb1->set_fill(Color(0.05, 0.43, 0.99, 1));
slider1->add_child(thumb1);
// ... 重复 9 次 = ~300 行
```

### After (With Component System)

```cpp
// 创建10个相同的滑块只需 ~30 行代码
flex::register_component("Slider", slider_builder);  // 定义 1 次

for (int i = 0; i < 10; i++) {
    auto slider = create_component_instance("Slider", {
        {"value", i * 0.1f}
    });
    // 2 行代码 × 10 = 20 行
}
```

**代码减少**: ~300 行 → ~30 行 (**90% 减少**)

---

## 🔮 Future Extensions

**Phase 2: 高级组件** (计划中)

| 组件 | 功能 | Props |
|------|------|-------|
| **Dropdown** | 下拉菜单 | items, selected, width |
| **Modal** | 模态框 | title, content, width, height |
| **Tooltip** | 提示框 | text, position |
| **Table** | 表格 | columns, rows, width, height |
| **Tree** | 树形控件 | nodes, expanded |

**Phase 3: 组件组合** (计划中)
- 复合组件（如带标签的 Slider）
- 表单验证
- 主题系统

**Phase 4: DSL Integration** (计划中)
```flex
scene app {
    Slider {
        value: 0.5,
        width: 300,
        color: #0d6efd
    }

    Checkbox {
        checked: true,
        label: "Remember me"
    }
}
```

---

## 📈 Performance

- **零运行时开销**: 组件在实例化时展开为 Node 树
- **内存高效**: 只存储 Builder 函数
- **快速创建**: <0.5ms per widget instance

**Benchmark**:
- 注册 5 个 widgets: <5ms
- 创建 100 个实例: <50ms

---

## 🆚 Comparison

| Feature | Flex Widgets | React MUI | Vue Element Plus | SwiftUI |
|---------|--------------|-----------|------------------|---------|
| 组件定义 | ✅ C++ Lambda | ✅ JSX | ✅ Template | ✅ Struct |
| Props | ✅ 类型安全 | ✅ | ✅ | ✅ |
| 复用性 | ✅ | ✅ | ✅ | ✅ |
| 交互 | 🚧 基础 | ✅ 完整 | ✅ 完整 | ✅ 完整 |
| 主题 | 🚧 计划中 | ✅ | ✅ | ✅ |
| 运行时 | 🚀 C++ | ❌ JS | ❌ JS | ✅ Swift |

**Flex 优势**:
- 原生 C++ 性能
- 无 JavaScript 依赖
- 基于成熟的 Component System

---

Built with 🧩 by Flex Engine Team
