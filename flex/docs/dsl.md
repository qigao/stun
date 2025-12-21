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

只有4个概念：

1. **Scene** - 场景图
2. **Physics** - 物理体
3. **Anim** - 动画
4. **State** - 状态机

---

## 1. SCENE

```flex
scene name {
    width: 800
    height: 600

    // 几何体 - 所有坐标都是相对于父容器的！
    circle name {
        x: 100      // 距离左边100像素
        y: 100      // 距离上边100像素
        radius: 50
        color: red
    }

    rect name {
        x: 200      // 相对于scene的左边
        y: 200      // 相对于scene的上边
        width: 100
        height: 50
        color: blue
    }

    text name {
        x: 300
        y: 300
        content: "Hello"
        color: white
        size: 24
    }

    // 容器 - 子元素相对于容器定位
    group name {
        x: 0        // 相对于scene
        y: 0

        child rect {
            x: 10    // 相对于容器左边
            y: 10    // 相对于容器上边
            width: 50
            height: 50
            color: green
        }
    }
}
```

**简单！直接！相对定位！**

---

## 2. PHYSICS

```flex
physics name {
    target: scene_object_name

    type: dynamic  // static, kinematic, dynamic
    mass: 1.0
    density: 1.0
    friction: 0.5
    restitution: 0.0

    // 碰撞体
    collider: box {
        width: 100
        height: 100
    }

    collider: circle {
        radius: 50
    }

    collider: capsule {
        radius: 10
        height: 50
    }
}
```

**简单！附着到场景对象上！**

---

## 3. ANIM

```flex
anim name {
    target: object_name

    // 属性动画
    property x {
        keyframe 0: 0
        keyframe 1: 100
    }

    property y {
        keyframe 0: 0
        keyframe 1: 50
    }

    property rotation {
        keyframe 0: 0
        keyframe 0.5: 180
        keyframe 1: 360
    }

    property opacity {
        keyframe 0: 1.0
        keyframe 0.5: 0.5
        keyframe 1: 0.0
    }

    duration: 2.0
    loop: true
}
```

**简单！每个属性独立！**

---

## 4. STATE

```flex
state machine_name {
    object: object_name

    state idle {
        on: jump
        goto: jumping

        enter: play idle_anim
    }

    state walking {
        on: left_pressed
        goto: walking_left

        on: right_pressed
        goto: walking_right

        on: jump
        goto: jumping
    }

    state jumping {
        on: land
        goto: idle

        enter: play jump_anim
    }
}
```

**简单！状态 + 事件 + 转换！**

---

## 完整示例：平台游戏

```flex
// ===== SCENE =====
scene game {
    width: 800
    height: 600

    // 玩家 - 相对于scene定位
    rect player {
        x: 100        // 从scene左边开始100像素
        y: 450        // 从scene上边开始450像素
        width: 40
        height: 80
        color: white
    }

    // 地面 - 相对于scene定位
    rect ground {
        x: 400
        y: 580
        width: 800
        height: 40
        color: green
    }

    // 障碍物 - 相对于scene定位
    rect obstacle1 {
        x: 400
        y: 520
        width: 50
        height: 60
        color: brown
    }

    rect obstacle2 {
        x: 600
        y: 500
        width: 60
        height: 80
        color: brown
    }

    // 金币 - 相对于scene定位
    circle coin1 {
        x: 350
        y: 400
        radius: 20
        color: gold
    }

    circle coin2 {
        x: 550
        y: 380
        radius: 20
        color: gold
    }
}

// ===== PHYSICS =====
physics player_physics {
    target: player
    type: dynamic
    mass: 1.0
    friction: 0.5

    collider: box {
        width: 40
        height: 80
    }
}

physics ground_physics {
    target: ground
    type: static

    collider: box {
        width: 800
        height: 40
    }
}

// ===== ANIM =====
anim coin_flip {
    target: coin1
    property rotation {
        keyframe 0: 0
        keyframe 1: 360
    }
    loop: true
    duration: 2.0
}

// ===== STATE =====
state player_state {
    object: player

    state idle {
        on: left_pressed
        goto: walking

        on: right_pressed
        goto: walking

        on: jump_pressed
        goto: jumping
    }

    state walking {
        on: left_released
        goto: idle

        on: right_released
        goto: idle

        on: jump_pressed
        goto: jumping
    }

    state jumping {
        on: land
        goto: idle

        enter: set velocity_y = -500
    }
}
```

**看！这个例子只有4个概念，没有其他垃圾！**

---

## 输入控制

```flex
input {
    float player_x
    float player_y
    float camera_x
    bool left_pressed
    bool right_pressed
    bool jump_pressed
}
```

**然后在C++中设置：**
```cpp
instance->set_input("left_pressed", true);
```

---

## 资源

```flex
asset font main {
    file: "fonts/OpenSans.ttf"
}

asset image icon {
    file: "icons/pen.svg"
}
```

**简单！直接！**

---

## 总结

**只有4个关键字：**
- `scene` - 几何体
- `physics` - 物理
- `anim` - 动画
- `state` - 状态机

**没有特殊情况！**
- 没有嵌套几何体
- 没有字符串类型
- 没有特殊符号（$, @, ->, =>）
- 没有复杂查询
- 没有虚拟层
- 没有散射节点

**重要：相对定位**
- 所有x, y都是相对于父容器的
- 容器移动，子元素自动跟随
- 就像CSS的position: relative
- 没有全局坐标的复杂性

**这就是全部！**