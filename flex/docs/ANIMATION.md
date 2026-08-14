# Flex Engine - DSL 动画系统

## 概述

Flex Engine 提供了一个强大的基于 DSL（领域特定语言）的动画系统，允许开发者使用声明式语法创建复杂的动画。动画系统支持多属性、多种类型、缓动函数和循环模式。

### 编译式执行模型（架构决策）

**背景**：Timeline 已经预解析属性 ID、目标 selector 与 MIR 表达式，但运行时仍直接遍历
Track 对象并读取其元数据。随着轨道表达式、显式空间切线和物理联动增加，这会让 DSL
表示、采样数据和每实例播放状态继续耦合。

**候选方案**：

1. 保持 Timeline 直接解释执行：迁移成本最低，但无法形成稳定的编译边界。
2. 编译后冻结 Timeline/Track：执行模型最简单，但会破坏现有程序化追加关键帧接口。
3. 使用版本化 `AnimationProgram`：编译只读 operation table，Track mutation 推进 revision，
   Timeline 按需重编译；保留现有动态构建 API。

当前选择方案 3。数据与状态按以下边界归属：

```text
DSL AST / C++ builder
        ↓ lowering
Timeline + Track（可编辑事实源）
        ↓ O(track_count + keyframe_count) compile, revision invalidation
AnimationProgram（只读 operation table + typed keyframe segments）
        ↓
TimelinePlayer（每实例 cursor、blend、resolved targets）
```

- `AnimationProgram` 由 Timeline 独占缓存。标量、`Vec2`、`Color` 轨道 lowering 到各自的连续
  typed keyframe segment（每项紧邻保存 time/value/easing）；字符串或 C++ builder 构造的混合类型轨道
  保留为完整 `Keyframe` generic segment，因此同型相邻区间仍插值、异型区间仍在下一关键帧
  切换。operation 保存逻辑 offset/count、typed storage offset、`PropertyID`、目标 selector、空间
  插值模式，并 retain 已编译 MIR 表达式。执行路径不再追踪 Track 或 arena 中的 storage。
- `Timeline::program()` 缓存命中为 O(1)；重编译时间/空间为
  O(track_count + keyframe_count)，单轨采样为 O(log keyframe_count)。
- Track 的关键帧、MIR 表达式或空间插值改变时推进 revision；下一次执行在采样前重编译。
- `AnimationProgram::sample_all()` 可将同一时刻的全部轨道写入调用方预分配的
  `AnimValue[track_count()]`。输出数量不匹配或非空输出为 null 时，在写入前抛
  `invalid_argument`。Program 不创建内部 scratch；typed 数值/向量/颜色结果不分配，generic
  字符串复制仍可能按目标 `std::string` 容量分配。若某条轨道的 MIR/空间采样失败，异常继续
  传播，失败轨道之前的输出已经更新，之后的输出保持调用前状态。
- 当前线程模型仍为单线程。编辑 Timeline 与执行 Player 不得并发进行。
- DSL lowering 会预热程序；C++ builder 保持懒编译，现有 `.flex` 格式无需迁移。

**失败语义**：编译会对 keyframe 总数做溢出检查；超过 vector 容量抛 `length_error`，分配
失败传播 `bad_alloc`，不会发布半编译 Program，也不会执行旧版本 fallback。

**权衡**：Timeline/Track 是唯一可编辑事实源，Program 是按 revision 重建的不可变快照。
编译会增加一份采样数据内存；常见同类型轨道不再为每个关键帧复制 `variant`、字符串和
`optional`，generic 轨道则以完整 Keyframe 换取 C++ builder 兼容性。Program 不再公开物理
`keyframes()` 容器；调用方通过 operation 元数据、typed keyframe count 与 `sample()`/
`sample_all()` 观察快照。Program 仍由 Timeline 缓存并以 borrowed reference 暴露；下一次
重编译会使旧引用失效。回滚只需恢复统一 Keyframe segment 与通用采样器；DSL、Track builder
和数据格式均未改变。

## 动画 DSL 语法

### 基本结构

```flex
anim "动画名称" {
    duration: 2s        // 动画持续时间
    loop: loop          // 循环模式: loop, pingpong, once
    speed: 1.0          // 播放速度倍数

    track "属性名" {
        keyframe 时间 -> 值
        keyframe 时间 -> 值
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
duration: 1500ms        // 运行时统一存为 1.5 秒
```

时间后缀只允许用于 `duration` 和关键帧时间；其他数值属性带 `s`/`ms` 会在加载时失败。

### 加载期语义验证

`Definition::load()` / `load_file()` 在 AST lowering 前执行独立语义验证。运行时要求：

- 动画、轨道、状态机和 layer 名称在各自作用域内唯一；
- `loop` 只能是 `once`、`loop` 或 `pingpong`；
- 每条轨道至少包含一个关键帧，时间非负且严格递增，重复时间也会失败；
- 同一轨道的关键帧必须属于同一种运行时类型（数值/布尔统一为标量）；
- 轨道目标必须是已知的可动画属性；
- MIR 表达式在加载期编译，语法错误不会延迟到首帧后再静默跳过。

验证失败时 Definition 保持未 lower 的状态，并通过 `has_error()` / `error_message()`
返回明确错误。AST 当前没有通用 source span，因此这类语义错误暂不保证行列号。

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

C++ `Track::add_keyframe()` 支持 `Easing`，但当前 `.flex` grammar 尚未暴露逐关键帧
`easing` 语法。DSL 轨道默认线性插值；需要自定义曲线时使用下述 MIR/JIT
`expression`。不要在 `.flex` 中写 `keyframe ... easing ...`，parser 会拒绝该输入。

### MIR/JIT 数值表达式采样

浮点轨道可直接在 DSL 中声明插值表达式：

```flex
track "x" {
    expression: ${lerp(from, to, smoothstep(0, 1, progress)) + sin(time)}
    keyframe 0s -> 10
    keyframe 2s -> 30
}
```

固定输入为 `time`（轨道绝对采样时间）、`progress`（经过 easing 的区间进度）、
`from` 和 `to`（相邻关键帧值）。除算术、比较和逻辑运算外，可使用
`sin/cos/tan`、`sqrt/abs`、`floor/ceil/round`、`exp/log/pow/fmod`、
`min/max`、`lerp/clamp`、`saturate/step/smoothstep/select`。表达式仅适用于浮点轨道。

`derivative(expr, variable)` 在加载时对表达式进行有界的自动微分，并将导数 AST 编译进
同一条 MIR/JIT 路径。例如 `derivative(sin(time) + time^3, time)` 可直接得到瞬时速度；
嵌套调用可表达高阶导数。比较、逻辑和 `step` 的导数定义为 `0`，分段函数使用当前激活
分支的导数，`abs(0)` 定义为 `0`。由于间断点语义不稳定，`fmod` 和 `%` 不支持求导，
加载时会直接失败。第二个参数必须是已声明输入变量，不接受任意表达式。

加载 `.flex` 时会编译并验证表达式；轨道持有编译后的 MIR/JIT 程序，采样热路径只更新
四个连续输入槽。状态动作和动画参数则由每个 `Instance` 独立缓存，避免跨实例共享可变
输入槽，也避免每次状态切换重新编译。

运行时也可用 `Track::set_numeric_expression()` 安装同样的表达式：

```cpp
track->set_numeric_expression(
    "lerp(from, to, progress * progress) + sin(time)");
```

无效表达式在设置时抛出 `std::invalid_argument`，非有限计算结果在采样时抛出
`std::runtime_error`。

当前 `.flexb` 1.0.4 格式没有轨道表达式字段。为避免静默退化成线性插值，
`BinaryWriter` 遇到 MIR 轨道会 fail fast 并返回空结果；升级二进制格式需要单独的格式迁移。

### 二维位置轨道与自动运动路径

`position` 是类型化的二维属性，避免分别维护 `x`、`y` 轨道而造成关键帧或混合不同步：

```flex
anim "orbit" {
    duration: 3s
    track "#satellite/position" {
        interpolation: catmullRom
        keyframe 0s -> vec2(0, 0)
        keyframe 1s -> vec2(80, -40)
        keyframe 2s -> vec2(160, 40)
        keyframe 3s -> vec2(240, 0)
    }
}
```

`interpolation` 支持：

- `linear`：默认模式，按分量线性插值。
- `catmullRom`：自动生成穿过全部关键点的平滑二维曲线，不需要手工维护空间 Bézier 控制点。
- `cubicBezier`：使用显式空间切线。关键帧写成
  `bezier(position, inTangent, outTangent)`，两条切线均为相对关键帧位置的偏移。

```flex
track "#dot/position" {
    interpolation: cubicBezier
    keyframe 0s -> bezier(vec2(0, 0), vec2(0, 0), vec2(0, 100))
    keyframe 1s -> bezier(vec2(100, 0), vec2(0, 100), vec2(0, 0))
}
```

上述区间的两个实际控制点分别为前一关键帧的
`position + outTangent` 和后一关键帧的 `position + inTangent`。所有关键帧必须
显式携带两条切线；Flex 不会用零切线补齐不完整输入。

Catmull-Rom 采样先用二分查找定位区间，时间复杂度为 O(log n)，随后以 O(1) 计算曲线，
采样过程不分配内存。它要求至少两个关键帧、所有值均为 `vec2`，且轨道必须指向
`position`；不满足条件会在 Definition 加载时失败。

当前 `.flexb` 1.0.4 同样没有 `Vec2` 和空间插值字段，writer 会明确拒绝此类轨道，
不会拆成 `x/y` 或静默退化为线性插值。

### 时间事件

Timeline 可以在跨过指定时间点时发送事件，不需要轮询播放进度：

```flex
anim "deploy" {
    duration: 2s
    trigger 500ms -> "engineStarted"
    trigger 1.5s -> "stageSeparated"

    track "#rocket/position" {
        keyframe 0s -> vec2(0, 500)
        keyframe 2s -> vec2(0, 0)
    }
}
```

应用通过 `AnimationController::set_trigger_callback()` 消费事件。事件遍历遵循 Timeline
的 loop/ping-pong 播放区间，跨越多个循环时会逐段触发。当前 `.flexb` 1.0.4 没有事件记录，
writer 对含 trigger 的 Timeline 同样 fail fast。

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
