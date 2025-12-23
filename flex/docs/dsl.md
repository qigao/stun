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

5个核心概念：

1. **Scene** - 场景图（支持group嵌套）
2. **Component** - 可复用组件（支持组件嵌套）
3. **Anim** - 动画
4. **Machine** - 状态机
5. **Input** - 输入绑定

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
        color: red
    }

    rect name {
        x: 200, y: 200
        width: 100, height: 50
        color: blue
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
        fill: #cyan
    }

    text name {
        x: 300, y: 300
        content: "Hello"
        color: white
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

**重要：支持任意深度的group嵌套！**

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

## 2. COMPONENT

Component系统让你在DSL中直接使用C++注册的组件。

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

---

## 3. ANIM

```flex
anim "moveAndFade" {
    duration: 2.0s
    loop: loop    // once, loop, pingpong

    track "x" {
        keyframe 0s -> 0
        keyframe 2s -> 100
    }

    track "opacity" {
        keyframe 0s -> 1.0
        keyframe 1s -> 0.5
        keyframe 2s -> 0.0
    }

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
- 支持float, string, Color值

---

## 4. MACHINE

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
- 时间条件：`after 2.0s`
- 动画结束：`on_anim_end`

---

## 5. INPUT

```flex
// 不需要显式声明input，直接在C++中设置
```

**C++端使用：**
```cpp
instance->set_input("counter", 5.0f);
instance->set_input("username", "Alice");
```

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

## 6. 高级图形与可视控件 (Design Complex Shapes)

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
- Group支持任意深度嵌套
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

**8个关键字：**
- `scene` - 场景图（支持group嵌套）
- `repeat` - 重复生成UI结构（静态次数）
- `data` - 定义数据集合
- `for...in` - 遍历数据生成UI
- `ComponentName` - 组件实例（支持组件嵌套）
- `anim` - 动画
- `machine` - 状态机
- *(input通过C++设置，无需关键字)*

**绑定语法：**
- `@index` - repeat块中的索引替换
- `$(iterator.prop)` - for循环中的数据绑定
- `$(index)` - for循环中的索引

**重要特性：**
- ✅ 支持任意深度的group嵌套
- ✅ 支持任意深度的Component嵌套
- ✅ repeat生成重复元素，@index自动替换
- ✅ data + for循环数据驱动UI生成
- ✅ 统一的相对定位规则
- ✅ 简洁的状态机语法
- ✅ 强大的动画系统

**这就是全部！**
