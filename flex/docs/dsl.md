# Flex DSL - Simplified Specification

**Extension:** `.flex`
**Paradigm:** Declarative, State-Driven.
**Core Principle:** No special cases.

---

## Syntax Rules

**唯一语法：**
```
indent: 4 spaces
key: value
{ indent: block }
```

**没有其他语法！**

**定位规则：**
- 所有x, y坐标都是**相对定位**
- 相对于父容器，不是全局坐标
- 如果没有父容器，scene就是容器
- 和CSS的position: relative一样

**例子：**
```flex
scene {
    rect parent {
        x: 100, y: 100

        // child相对于parent定位
        rect child {
            x: 50   // 距离parent左边50像素
            y: 50   // 距离parent上边50像素
        }
    }
}

// 最终位置：
// parent在 (100, 100)
// child在 (150, 150)
```

---

## 核心概念

6个核心概念：

1. **Scene** - 场景图（支持group嵌套）
2. **Component** - 可复用组件（支持组件嵌套）
3. **Assets** - 资源预加载（图片、音频、字体）
4. **Anim** - 动画
5. **Machine** - 状态机
6. **Input** - 输入绑定

---

## 1. SCENE

```flex
scene name {
    width: 800
    height: 600

    // 几何体 - 所有坐标都是相对于父容器的！
    circle name {
        x: 100, y: 100
        radius: 50
        fill: #ff0000
    }

    rect name {
        x: 200, y: 200
        width: 100, height: 50
        fill: #0000ff
    }

    // 高级几何体
    ellipse name {
        x: 400, y: 100
        rx: 50, ry: 30
        fill: #ffaa00
    }

    polygon name {
        x: 500, y: 100
        sides: 6        // 六边形
        radius: 40
        fill: #00ffaa
    }

    star name {
        x: 600, y: 100
        points: 5
        outerRadius: 50
        innerRadius: 20
        fill: #ffff00
    }

    path name {
        x: 100, y: 400
        d: "M 0 0 L 50 0 L 25 50 Z" // 三角形
        fill: #ff00ff
    }

    line name {
        x: 200, y: 400
        x2: 100, y2: 50  // 终点相对于起点 (x, y)
        stroke: white
        strokeWidth: 2
    }

    ring name {
        x: 400, y: 400
        outerRadius: 50
        innerRadius: 30
        fill: #00ffff
    }

    text name {
        x: 300, y: 300
        content: "Hello"
        color: #ffffff
        fontSize: 24
    }

    image name {
        x: 500, y: 300
        src: "icon.png"
        width: 64, height: 64
    }

    svg name {
        x: 600, y: 300
        src: "logo.svg"
        width: 100, height: 100
    }

    // 容器 - 子元素相对于容器定位（支持嵌套）
    group name {
        x: 0        // 相对于scene
        y: 0

        // 嵌套group
        group nestedContainer {
            x: 50    // 相对于父group
            y: 50

            rect child {
                x: 10    // 相对于nestedContainer
                y: 10
                width: 50
                height: 50
                color: green
            }
        }
    }
}
```

**重要：group 支持深层嵌套；解析器栈上限为 256 个语法状态，超限会明确报错。**

### Repeat Block

使用 `repeat` 生成重复的UI结构：

```flex
scene list {
    width: 400
    height: 600

    group items {
        layout: flex
        flexDirection: column
        gap: 10

        // 生成5个列表项
        repeat 5 {
            group item@index {
                width: 380, height: 50

                rect bg {
                    width: 380, height: 50
                    fill: #ffffff
                }

                text label {
                    x: 20, y: 15
                    content: "Item"
                    fontSize: 16
                }
            }
        }
    }
}
```

**语法：**
```
repeat N {
    node template@index {
        property: @index    // @index 替换为 0, 1, 2, ..., N-1
    }
}
```

**特性：**
- `@index` 在ID中自动替换：`item@index` → `item0`, `item1`, `item2`, ...
- `@index` 在属性值中替换为数字：`y: @index` → `y: 0`, `y: 1`, `y: 2`, ...
- 内部节点不需要 `@index` 后缀（它们在各自的父节点中是唯一的）
- 配合flex布局自动排列生成的元素

**示例：生成网格：**
```flex
group grid {
    layout: flex
    flexDirection: row
    flexWrap: wrap
    gap: 5

    repeat 9 {
        rect cell@index {
            width: 50, height: 50
            fill: #3498db
        }
    }
}
```

### Data Block & For Loop

使用 `data` 定义数据集合，`for` 循环遍历生成UI：

```flex
// 定义数据
data products {
    laptop: { name: "Laptop", price: 999, color: "#3498db" }
    phone: { name: "Phone", price: 699, color: "#e74c3c" }
    tablet: { name: "Tablet", price: 499, color: "#2ecc71" }
}

scene shop {
    width: 600
    height: 400

    group productList {
        layout: flex
        flexDirection: column
        gap: 10

        // 遍历数据生成UI
        for item in products {
            group item {
                width: 300, height: 60

                rect bg {
                    width: 300, height: 60
                    fill: $(item.color)
                }

                text name {
                    x: 20, y: 10
                    content: $(item.name)
                    fontSize: 18
                    color: white
                }

                text price {
                    x: 20, y: 35
                    content: $(item.price)
                    fontSize: 14
                    color: #cccccc
                }
            }
        }
    }
}
```

**语法：**
```
data dataName {
    key1: { prop: value, prop2: value2 }
    key2: { prop: value, prop2: value2 }
}

for iterator in dataName {
    node iterator {
        property: $(iterator.prop)
    }
}
```

**绑定表达式 `$(...)`：**
- `$(iterator.property)` - 替换为数据项的属性值
- `$(index)` - 替换为当前索引 (0, 1, 2, ...)
- 支持数字、字符串、颜色值

**特性：**
- 迭代器名作为节点ID时自动添加索引：`item` → `item0`, `item1`, ...
- 数据属性支持嵌套访问
- 配合flex布局自动排列

**示例：位置数据：**
```flex
data positions {
    p1: { x: 100, y: 50 }
    p2: { x: 200, y: 100 }
    p3: { x: 300, y: 150 }
}

scene dots {
    for pos in positions {
        circle pos {
            x: $(pos.x)
            y: $(pos.y)
            radius: 20
            fill: #ff6600
        }
    }
}
```

---

## 2. ASSETS

资源预加载系统，在应用启动时加载所有资源，避免运行时延迟。

### 基础语法

```flex
assets {
    // 类型 ID: "路径"
    audio click: "sounds/click.wav"
    audio success: "sounds/success.mp3"

    image logo: "images/logo.png"
    image avatar: "images/avatar.jpg"

    font roboto: "fonts/Roboto.ttf"
}
```

### 带选项的资源

```flex
assets {
    // 音频选项
    audio bgm: "sounds/background.mp3" {
        loop: true          // 循环播放
        volume: 0.5         // 音量 0.0-1.0
        preload: true       // 预加载（默认true）
    }

    audio ambient: "sounds/ambient.wav" {
        loop: true
        volume: 0.3
    }

    // 图片选项
    image sprite: "images/sprite.png" {
        preload: true       // 预加载到内存
    }
}
```

### 支持的资源类型

| 类型 | 用途 | 支持格式 |
|------|------|----------|
| `audio` | 音效、背景音乐 | .wav, .mp3, .ogg |
| `image` | 图片资源 | .png, .jpg, .webp |
| `font` | 自定义字体 | .ttf, .otf |
| `svg` | 矢量图形 | .svg |

### 在状态机中使用资源

```flex
assets {
    audio click: "sounds/click.wav"
    audio hover: "sounds/hover.wav"
    audio success: "sounds/success.mp3"
    audio bgm: "sounds/bgm.mp3" { loop: true }
}

machine buttonState {
    layer main {
        state idle {
            initial: true
        }

        state hover {
            play: hover          // 进入状态时播放 hover 音效
        }

        state pressed {
            play: click          // 播放点击音效
            animation: "pressDown"
        }

        state success {
            play: success
            stop: bgm            // 停止背景音乐
        }

        transition idle -> hover when mouseEnter
        transition hover -> idle when mouseLeave
        transition hover -> pressed when mouseDown
        transition pressed -> success when mouseUp
    }
}
```

### 在动画中切换图片

```flex
assets {
    image avatar_normal: "images/avatar_normal.png"
    image avatar_happy: "images/avatar_happy.png"
    image avatar_sad: "images/avatar_sad.png"
}

scene demo {
    image avatar {
        x: 100, y: 100
        src: avatar_normal      // 引用 assets 中的 ID
        width: 64, height: 64
    }
}

anim "moodChange" {
    duration: 1.0s

    // 切换图片资源
    track "#avatar/src" {
        keyframe 0s -> avatar_normal
        keyframe 0.5s -> avatar_happy
        keyframe 1s -> avatar_sad
    }
}
```

### C++ API

```cpp
// 手动播放/停止音频
instance->play_audio("click");
instance->play_audio("bgm");       // 循环播放（如果设置了loop）
instance->stop_audio("bgm");
instance->set_audio_volume("bgm", 0.5f);

// 获取资源引用
auto* image = instance->get_asset<Image>("logo");
auto* font = instance->get_asset<Font>("roboto");

// 检查资源是否已加载
bool loaded = instance->is_asset_loaded("bgm");
```

---

## 3. COMPONENT

Component 系统让你在 DSL 中直接使用 C++ 注册的组件。

### 基础用法

```flex
scene demo {
    // 使用C++注册的Slider组件
    Slider mySlider {
        x: 100, y: 100
        value: 0.75
        width: 300
        color: #0D6EFD
    }

    // 使用Toggle组件
    Toggle myToggle {
        x: 100, y: 150
        on: true
        width: 50
        height: 26
    }
}
```

### 嵌套组件

组件可以嵌套其他组件（如果C++构建器支持）：

```flex
scene nestedDemo {
    // LabeledSlider内部使用了Slider组件
    LabeledSlider volume {
        x: 50, y: 100
        label: "Volume"
        value: 0.65
        width: 400
        color: #198754
    }

    // SettingsRow内部使用了Toggle组件
    SettingsRow darkMode {
        x: 50, y: 200
        label: "Dark Mode"
        on: false
    }

    // VolumeControl内部使用了Slider + ProgressBar + Toggle
    VolumeControl audio {
        x: 50, y: 300
        volume: 0.8
        muted: false
    }
}
```

### Component定义（C++端）

```cpp
// 注册基础组件
auto slider = Component::create("Slider");
slider->add_prop("value", 0.5f);
slider->add_prop("width", 300.0f);
slider->add_prop("color", uint32_t(0xFF0D6EFD));
slider->set_builder([](const Props& props) {
    // 构建slider的视觉结构
    auto group = Group::create();
    // ... 添加track, fill, thumb等
    return group;
});
ComponentRegistry::instance().register_component(slider);

// 注册嵌套组件（内部使用Slider）
auto labeled = Component::create("LabeledSlider");
labeled->add_prop("label", std::string(""));
labeled->add_prop("value", 0.5f);
labeled->set_builder([](const Props& props) {
    auto group = Group::create();

    // 添加label
    auto label = Text::create();
    group->add_child(label);

    // 嵌套使用Slider组件
    auto slider = create_component_instance("Slider", {
        {"value", props["value"]},
        {"width", 300.0f}
    });
    group->add_child(slider);

    return group;
});
ComponentRegistry::instance().register_component(labeled);
```

**关键点：**
- Component必须先在C++中注册，然后才能在.flex中使用
- Component可以嵌套其他Component（通过`create_component_instance`）
- Props支持float, string, bool, uint32_t（颜色）类型
- Component props 与节点属性重名时，优先按组件 props 处理；未定义则按节点属性处理
- 当组件 props 绑定 width/height 时，布局尺寸与几何尺寸保持一致

**开发规范（必须遵守）：**
- 组件要显式声明 props，并保持类型一致（错误类型会导致加载失败）
- 组件 builder 若使用 width/height，应同步设置 layout 尺寸
- 只有明确需要时才与节点属性重名，并理解优先级规则

**重名优先级示例：**
```flex
component Badge {
    x: 10 // 这里的 x 是组件 prop
    rect icon { width: $x, height: 6 }
}

scene demo {
    Badge badge {
        x: ${$badgeW}  // 会驱动组件 prop，不会改节点位置
    }
}
```

---

## 4. ANIM

```flex
anim "moveAndFade" {
    duration: 2.0s
    loop: loop    // once, loop, pingpong

    track "x" {
        expression: ${lerp(from, to, smoothstep(0, 1, progress))}
        keyframe 0s -> 0
        keyframe 2s -> 100
    }

    track "opacity" {
        keyframe 0s -> 1.0
        keyframe 1s -> 0.5
        keyframe 2s -> 0.0
    }

    trigger 1s -> "halfway"

    track "#statusText/content" {
        keyframe 0s -> "Loading..."
        keyframe 2s -> "Complete!"
    }

    track "#statusText/text.color" {
        keyframe 0s -> #888888
        keyframe 2s -> #00ff88
    }
}
```

**Track语法：**
- 简单属性：`"x"`, `"y"`, `"opacity"`, `"rotation"`
- 带ID定位：`"#nodeId/property"`, `"#nodeId/text.color"`
- 支持 float、string、Color 和 `vec2(x, y)` 值
- float 轨道可用 `expression: ${...}` 覆盖默认插值；固定输入为 `time`、`progress`、`from`、`to`，加载时编译为 MIR/JIT
- `position` 轨道可设置 `interpolation: catmullRom`，自动生成经过二维关键点的平滑路径
- `position` 轨道可设置 `interpolation: cubicBezier`，并用
  `bezier(position, inTangent, outTangent)` 为每个关键帧显式给出相对空间切线
- `trigger <time> -> "event"` 在播放跨过时间点时发送交互事件
- 每条 track 至少有一个关键帧；时间必须非负并严格递增，重复时间会在加载时失败
- 动画名和同一动画内的 track 路径必须唯一，`loop` 只接受 `once`、`loop`、`pingpong`
- `#node/property` 会在加载期检查节点是否存在、该节点类型是否支持目标属性，以及
  keyframe 是 scalar、string、color 还是 vec2；错误目标不会再在播放时静默跳过
- scene 中显式声明的节点 ID 必须唯一。注册组件及其 builder 内部结构属于不透明边界，
  无法静态证明的内部目标保留到运行时解析

---

## 5. MACHINE

```flex
machine statusTracker {
    layer status {
        state neutral {
            initial: true
            animation: "toNeutral"
        }

        state positive {
            animation: "toPositive"
        }

        state high {
            animation: "toHigh"
        }

        // 输入条件转换
        transition neutral -> positive when counter > 0
        transition positive -> high when counter > 5

        // 反向转换
        transition high -> positive when counter < 5.1
        transition positive -> neutral when counter < 0.1

        // 事件触发转换
        transition neutral -> high when buttonClicked
    }
}
```

**Condition类型：**
- 输入比较：`when input > value`, `when input < value`, `when input == value`
- 事件触发：`when eventName`

`after 2.0s` 与 `on_anim_end` 当前尚未进入 grammar，不属于可用语法。

加载期会验证同一 layer 内 state 名唯一、最多一个 `initial: true`，以及 transition
两端必须引用该 layer 中已声明的 state。state 的 `animation` 也必须引用当前 Definition
中已声明的动画。条件、set action 和动画参数表达式都会在 lowering 前编译为 MIR/JIT；
失败时不会创建部分状态机。`set #node.property` 还会验证节点存在、属性可动画且能够
接收 MIR 返回的 scalar 值；字符串、颜色和 vec2 属性不能由当前数值 set action 写入。

---

## 6. INPUT

```flex
var counter = 0
var username = "Guest"
var enabled = true

scene dashboard {
    text greeting {
        content: $username
    }
}
```

顶层 `var` 声明一个带默认值的运行时输入。类型由默认值推断，并在该 Definition
创建的每个 Instance 中独立保存；支持 `number`、`string`、`boolean`。`$name` 用于
直接绑定，`${...}` / `$(...)` 用于 MIR 数值表达式。字符串变量不能进入数值表达式，
已声明变量也不能在 C++ 端改成另一种类型，这些错误会立即报告。

`const` 和裸 `var` 名称仍会在解析属性时替换为声明值；需要运行时响应变化的属性必须
显式写成 `$name` 或表达式绑定。未声明输入仍可由 C++ 动态注入，以兼容宿主应用按需
提供的数据。

**C++端使用：**
```cpp
for (const auto& [name, defaultValue] : definition->input_schema()) {
    // 可在创建 Instance 前检查宿主需要提供的输入及其推断类型
}

instance->set_input("counter", 5.0f);
instance->set_input("username", "Alice");
```

当前 `.flexb` 格式尚未携带运行时 input schema；`flex-compiler` 遇到 `var` 会明确
失败，避免生成丢失默认值和绑定语义的二进制。此类 Definition 暂时应直接从 `.flex`
源码加载。

**状态机中引用：**
```flex
transition idle -> active when counter > 10
```

---

## 完整示例：带Component的计数器

```flex
scene counterApp {
    width: 400
    height: 300

    // 背景
    rect background {
        x: 0, y: 0
        width: 400, height: 300
        fill: #1a1a2e
    }

    // 主布局
    group mainLayout {
        layout: flex
        flexDirection: column
        justifyContent: center
        alignItems: center
        x: 0, y: 0
        width: 400, height: 300
        gap: 30

        // 标题
        text title {
            content: "Counter App"
            fontSize: 28
            color: #00d9ff
        }

        // 计数器显示
        group counterBox {
            width: 160, height: 80

            rect displayBg {
                x: 0, y: 0
                width: 160, height: 80
                fill: #2c2c54
            }

            group counterLabel {
                layout: flex
                justifyContent: center
                alignItems: center
                width: 160, height: 80

                text counterValue {
                    content: "0"
                    fontSize: 48
                    color: #00ff88
                }
            }
        }

        // 按钮行
        group buttonRow {
            layout: flex
            flexDirection: row
            gap: 40
            width: 400, height: 60

            // 使用Component - 增加按钮
            Button incrementButton {
                label: "+"
                width: 80
                height: 45
                color: #00d9ff
            }

            // 使用Component - 减少按钮
            Button decrementButton {
                label: "-"
                width: 80
                height: 45
                color: #ff006e
            }
        }

        // 状态文本
        text statusText {
            content: "Neutral"
            fontSize: 18
            color: #888888
        }
    }
}

// 状态动画
anim "toPositive" {
    duration: 0.1s
    track "#statusText/content" { keyframe 0s -> "Positive" }
    track "#statusText/text.color" { keyframe 0s -> #00ff88 }
}

anim "toNeutral" {
    duration: 0.1s
    track "#statusText/content" { keyframe 0s -> "Neutral" }
    track "#statusText/text.color" { keyframe 0s -> #888888 }
}

anim "toNegative" {
    duration: 0.1s
    track "#statusText/content" { keyframe 0s -> "Negative" }
    track "#statusText/text.color" { keyframe 0s -> #ffaa00 }
}

// 状态机
machine statusTracker {
    layer status {
        state neutral { initial: true, animation: "toNeutral" }
        state positive { animation: "toPositive" }
        state negative { animation: "toNegative" }

        transition neutral -> positive when counter > 0
        transition positive -> neutral when counter < 0.1

        transition neutral -> negative when counter < 0
        transition negative -> neutral when counter > -0.1
    }
}

---

## 7. 高级图形与可视控件 (Design Complex Shapes)

通过组合 `path` 和 `Component`，可以轻松设计复杂的视觉控件和图标（如 AWS 图标、架构图组件）。

### AWS Cloud Icon 示例
```flex
component AwsCloud {
    fill: #FF9900
    width: 64
    height: 40

    path icon {
        // 复杂的云朵路径
        d: "M 15 20 A 10 10 0 1 1 25 10 A 15 15 0 1 1 45 15 A 12 12 0 1 1 40 35 L 15 35 A 10 10 0 1 1 15 20 Z"
        fill: $fill
        width: $width
        height: $height
    }
}
```

### Use Case 示例 (Actor & Use Case)
```flex
component Actor {
    color: white

    group stickman {
        // 头
        circle head { x: 0, y: -20, radius: 10, stroke: $color }
        // 身体
        line body { x: 0, y: -10, x2: 0, y2: 30, stroke: $color }
        // 手
        line arms { x: -20, y: 5, x2: 40, y2: 0, stroke: $color }
        // 腿
        line leftLeg { x: 0, y: 20, x2: -15, y2: 25, stroke: $color }
        line rightLeg { x: 0, y: 20, x2: 15, y2: 25, stroke: $color }
    }
}

component UseCase {
    label: "Do Something"
    
    group ellipseBox {
        ellipse bg {
            rx: 60, ry: 30
            fill: #2c2c54
            stroke: white
        }
        text txt {
            content: $label
            fontSize: 14
            color: white
            alignSelf: center
        }
    }
}
```

### 视觉控件：带图标的按钮
```flex
component IconButton {
    icon: "AwsCloud"
    label: "Deploy"
    
    rect bg {
        width: 150, height: 45, radius: 8
        fill: #0D6EFD
    }
    
    group content {
        layout: flex
        flexDirection: row
        alignItems: center
        gap: 10
        padding: 5
        
        // 动态实例化指定的图标组件
        $icon { width: 24, height: 16 }
        
        text txt {
            content: $label
            color: white
        }
    }
}
```

---

## 设计哲学

**1. No Special Cases（没有特殊情况）**
- 统一的语法：key: value + { block }
- 相对定位适用于所有节点
- Component和内置节点使用相同语法

**2. Composability（可组合性）**
- Group 支持深层嵌套，并受解析器 256 状态栈上限保护
- Component可以嵌套Component
- 动画和状态机可以组合使用

**3. Data-Driven（数据驱动）**
- Input系统统一管理状态
- 状态机响应Input变化
- 动画由状态机触发

**4. Simplicity（简洁性）**
- 只有5个核心概念
- 没有复杂的查询语法
- 没有虚拟层或散射节点
- 没有字符串模板或特殊符号

---

## 总结

**9个关键字：**
- `scene` - 场景图（支持group嵌套）
- `assets` - 资源预加载（音频、图片、字体）
- `repeat` - 重复生成UI结构（静态次数）
- `data` - 定义数据集合
- `for...in` - 遍历数据生成UI
- `ComponentName` - 组件实例（支持组件嵌套）
- `anim` - 动画
- `machine` - 状态机
- *(input通过C++设置，无需关键字)*

**资源动作（状态机中）：**
- `play: assetId` - 进入状态时播放音频
- `stop: assetId` - 进入状态时停止音频

**绑定语法：**
- `@index` - repeat块中的索引替换
- `$(iterator.prop)` - for循环中的数据绑定
- `$(index)` - for循环中的索引

**重要特性：**
- ✅ 支持有界的深层 group 嵌套
- ✅ 支持有界的深层 Component 嵌套
- ✅ repeat生成重复元素，@index自动替换
- ✅ data + for循环数据驱动UI生成
- ✅ 统一的相对定位规则
- ✅ 简洁的状态机语法
- ✅ 强大的动画系统
- ✅ 资源预加载和状态机音频控制

**这就是全部！**
