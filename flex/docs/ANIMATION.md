# Flex Engine - DSL 动画系统

## 概述

Flex Engine 提供了一个强大的基于 DSL（领域特定语言）的动画系统，允许开发者使用声明式语法创建复杂的动画。动画系统支持多属性、多种类型、缓动函数和循环模式。

## 动画 DSL 语法

### 基本结构

```flex
anim "动画名称" {
    duration: 2s        // 动画持续时间
    loop: loop          // 循环模式: loop, pingpong, once
    speed: 1.0          // 播放速度倍数

    track "属性名" {
        keyframe 时间 -> 值 [easing 缓动函数]
        keyframe 时间 -> 值 [easing 缓动函数]
        ...
    }

    track "另一个属性" {
        ...
    }
}
```

### 时间单位

- `s` - 秒（默认）
- `ms` - 毫秒

```flex
keyframe 1.5s -> 100
keyframe 1500ms -> 100  // 等同于上面
```

### 值类型

#### 1. 数值 (float)
```flex
track "x" {
    keyframe 0s -> 100
    keyframe 1s -> 200
}
```

#### 2. 颜色 (Color)
```flex
track "fill.color" {
    keyframe 0s -> #ff0000    // 红色
    keyframe 1s -> #00ff00    // 绿色
}
```

支持的格式：
- `#RGB` - 3位十六进制
- `#RRGGBB` - 6位十六进制
- `#RRGGBBAA` - 8位十六进制（带透明度）

#### 3. 字符串 (string)
```flex
track "content" {
    keyframe 0s -> "Hello"
    keyframe 1s -> "World"
}
```

### 缓动函数

支持的缓动函数：
- `linear` - 线性（默认）
- `ease` - 自然缓动
- `ease-in` - 加速
- `ease-out` - 减速
- `ease-in-out` - 先加速后减速

```flex
track "x" {
    keyframe 0s -> 100 easing linear
    keyframe 1s -> 200 easing ease-out
}
```

### 循环模式

#### 1. Loop - 无限循环
```flex
anim "MoveLoop" {
    loop: loop
    track "x" {
        keyframe 0s -> 0
        keyframe 1s -> 100
    }
}
```

#### 2. PingPong - 来回循环
```flex
anim "MovePingPong" {
    loop: pingpong
    track "x" {
        keyframe 0s -> 0
        keyframe 1s -> 100
    }
}
// 播放: 0 -> 100 -> 0 -> 100 -> ...
```

#### 3. Once - 播放一次
```flex
anim "PlayOnce" {
    loop: once
    track "x" {
        keyframe 0s -> 0
        keyframe 1s -> 100
    }
}
// 播放: 0 -> 100 (停止)
```

## 可动画属性

### 变换属性
- `x` - X 坐标
- `y` - Y 坐标
- `rotation` - 旋转角度（度）
- `scaleX` - X 轴缩放
- `scaleY` - Y 轴缩放

### 外观属性
- `opacity` - 透明度 [0.0, 1.0]
- `visible` - 可见性 (bool)
- `fill.color` - 填充颜色
- `stroke.color` - 描边颜色
- `stroke.width` - 描边宽度

### 几何属性
- `width` - 宽度
- `height` - 高度
- `radius` - 半径（圆形）
- `cornerRadius` - 圆角半径

### 文本属性
- `content` - 文本内容
- `fontSize` - 字体大小
- `color` - 文本颜色

## 完整示例

```flex
// 定义动画
anim "PlayerMove" {
    duration: 3s
    loop: loop

    track "x" {
        keyframe 0s -> 100
        keyframe 0.5s -> 300
        keyframe 1s -> 500
        keyframe 1.5s -> 300
        keyframe 2s -> 100
        keyframe 2.5s -> 300
        keyframe 3s -> 500
    }

    track "y" {
        keyframe 0s -> 400
        keyframe 0.5s -> 300
        keyframe 1s -> 400
        keyframe 1.5s -> 300
        keyframe 2s -> 400
        keyframe 2.5s -> 300
        keyframe 3s -> 400
    }
}

anim "FadeInOut" {
    duration: 2s
    loop: pingpong

    track "opacity" {
        keyframe 0s -> 0.0
        keyframe 0.5s -> 1.0
        keyframe 1s -> 0.5
        keyframe 1.5s -> 1.0
        keyframe 2s -> 0.0
    }
}

anim "Spin" {
    duration: 4s
    loop: loop

    track "rotation" {
        keyframe 0s -> 0
        keyframe 1s -> 90
        keyframe 2s -> 180
        keyframe 3s -> 270
        keyframe 4s -> 360
    }
}

anim "ColorShift" {
    duration: 5s
    loop: loop

    track "fill.color" {
        keyframe 0s -> #ff0000  // Red
        keyframe 1.25s -> #00ff00  // Green
        keyframe 2.5s -> #0000ff  // Blue
        keyframe 3.75s -> #ffff00  // Yellow
        keyframe 5s -> #ff0000  // Back to Red
    }
}

anim "Bounce" {
    duration: 1s
    loop: loop

    track "scaleX" {
        keyframe 0s -> 1.0
        keyframe 0.25s -> 1.2
        keyframe 0.5s -> 1.0
        keyframe 0.75s -> 1.2
        keyframe 1s -> 1.0
    }

    track "scaleY" {
        keyframe 0s -> 1.0
        keyframe 0.25s -> 0.8
        keyframe 0.5s -> 1.0
        keyframe 0.75s -> 0.8
        keyframe 1s -> 1.0
    }
}

// 在场景中使用
scene GameScene {
    player = Group {
        id: "player"
        x: 100
        y: 400

        body = Shape {
            geometry: rect
            width: 60
            height: 60
            fill.color: #3498db
        }
    }
}

// 在状态机中应用
state GameState {
    layer Movement {
        initial: Idle

        state Moving {
            animation: "PlayerMove"
        }

        state Fading {
            animation: "FadeInOut"
        }

        transition Idle -> Moving on "start_move"
        transition Moving -> Fading on "start_fade"
    }
}
```

## C++ API 使用

### 创建动画

```cpp
// 创建时间线
auto anim = flex::Timeline::create("MyAnimation");
anim->set_loop_mode(flex::LoopMode::Loop);

// 添加轨迹
auto track = anim->add_track("x");

// 添加关键帧
track->add_keyframe(0.0f, 100.0f, flex::Easing::linear());
track->add_keyframe(1.0f, 200.0f, flex::Easing::ease_out());

// 添加到实例
instance->add_timeline(anim);
```

### 播放动画

```cpp
// 在节点上播放动画
auto* node = player.get();
auto* player = instance->play("MyAnimation", node);

// 控制播放
player->play();   // 开始播放
player->pause();  // 暂停
player->stop();   // 停止

// 设置混合模式
player->set_blend_mode(flex::BlendMode::Override);  // 覆盖（默认）
player->set_blend_mode(flex::BlendMode::Additive);  // 叠加
player->set_blend_mode(flex::BlendMode::Multiply);  // 相乘

// 设置权重
player->set_blend_weight(0.5f);  // 50% 强度

// 设置层级
player->set_layer(1);  // 较高层级后应用
```

### 动画更新

```cpp
// 在游戏循环中更新
void game_loop() {
    float dt = delta_time;  // 帧间隔时间（秒）

    // 更新所有动画
    instance->advance(dt);

    // 渲染场景
    render();
}
```

## 最佳实践

### 1. 合理设置持续时间
```flex
// ✓ 好：明确的持续时间
anim "QuickBounce" {
    duration: 0.5s
    ...
}

// ✗ 坏：没有明确持续时间（依赖关键帧自动计算）
anim "UnclearDuration" {
    track "x" { keyframe 0s -> 0; keyframe 2s -> 100; }
}
```

### 2. 使用合适的缓动函数
```flex
// ✓ 好：根据动画特性选择缓动
track "x" {
    keyframe 0s -> 0 easing ease-out  // 减速停止更自然
    keyframe 1s -> 100
}

// ✗ 坏：所有动画都用 linear
track "x" {
    keyframe 0s -> 0 easing linear
    keyframe 1s -> 100 easing linear
}
```

### 3. 组合多个属性
```flex
// ✓ 好：同时动画多个属性
anim "Jump" {
    track "y" { keyframe 0s -> 0; keyframe 0.5s -> -50; keyframe 1s -> 0; }
    track "scaleY" { keyframe 0s -> 1; keyframe 0.25s -> 1.2; keyframe 1s -> 1; }
}
```

### 4. 使用状态机管理动画
```flex
// ✓ 好：状态机控制动画切换
state PlayerState {
    layer Main {
        initial: Idle

        state Idle { animation: "" }
        state Walking { animation: "WalkAnim" }
        state Jumping { animation: "JumpAnim" }

        transition Idle -> Walking on "move"
        transition Walking -> Jumping on "jump"
        transition Walking -> Idle on "stop"
        transition Jumping -> Idle on "land"
    }
}
```

## 性能优化

1. **避免过多关键帧** - 每个关键帧都需要计算和插值
2. **复用动画** - 相同动画在不同节点间复用
3. **合理使用循环模式** - PingPong 比 Loop 消耗稍多
4. **批量更新** - 尽量在一次 advance() 调用中更新所有动画

## 故障排除

### 动画不播放
- 检查时间线是否已添加到实例
- 检查节点是否有效
- 检查动画名称是否正确

### 值不正确
- 检查关键帧时间是否按顺序
- 检查缓动函数是否有效
- 检查属性名是否正确

### 性能问题
- 减少同时播放的动画数量
- 简化关键帧数量
- 检查是否有内存泄漏

## 参考示例

- `examples/flex_simple_demo.cpp` - 基础动画示例
- `examples/flex_animation_test.cpp` - 动画系统测试
- `examples/flex_advanced_animation_test.cpp` - 复杂动画示例
- `examples/advanced_animation.flex` - DSL 动画定义示例
