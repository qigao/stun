# Flex Engine - ThorVG + SDL2 动画渲染指南

## 概述

Flex Engine 支持通过 **ThorVG**（向量图形渲染引擎）和 **SDL2**（窗口和输入管理）进行实时动画渲染。本指南介绍如何设置、编写和运行动画渲染程序。

## 架构

```
┌─────────────────────────────────────┐
│           SDL2 Window               │
│  ┌─────────────────────────────┐   │
│  │      SDL Surface            │   │
│  └─────────────────────────────┘   │
│              ↓                      │
│  ┌─────────────────────────────┐   │
│  │     ThorVG Canvas           │   │
│  │  - SwCanvas (Software)      │   │
│  │  - ARGB8888 Color Space     │   │
│  └─────────────────────────────┘   │
│              ↓                      │
│  ┌─────────────────────────────┐   │
│  │    Flex Scene Graph         │   │
│  │  - Nodes (Group, Shape...)  │   │
│  │  - Animations (Timeline)    │   │
│  └─────────────────────────────┘   │
└─────────────────────────────────────┘
```

## 组件说明

### SDL2 (Simple DirectMedia Layer 2)
- **作用**: 窗口管理、输入事件处理、时间管理
- **优点**: 跨平台、简单易用、性能稳定
- **使用**: 创建窗口、处理事件、管理帧率

### ThorVG (Thor Vector Graphics)
- **作用**: 向量图形渲染
- **优点**: 高质量矢量渲染、SVG 支持、硬件加速
- **使用**: 渲染形状、文本、路径到 SDL 表面

### Flex Engine
- **作用**: 场景图管理、动画系统
- **优点**: 声明式 DSL、C++ API、类型安全
- **使用**: 创建场景、定义动画、更新状态

## 完整示例代码

```cpp
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>
#include <flex/renderer.h>
#include <flex/group.h>
#include <flex/shape.h>
#include <flex/text.h>
#include <flex/artboard.h>
#include <iostream>

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;

class AnimationRenderer {
public:
    bool init() {
        // 1. 初始化 SDL2
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed\n";
            return false;
        }

        // 2. 创建窗口
        window_ = SDL_CreateWindow(
            "Flex Animation",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WIDTH,
            HEIGHT,
            SDL_WINDOW_SHOWN
        );

        // 3. 获取表面
        surface_ = SDL_GetWindowSurface(window_);

        // 4. 初始化 ThorVG
        tvg::Initializer::init(0);

        // 5. 创建画布
        canvas_.reset(tvg::SwCanvas::gen());
        canvas_->target(
            static_cast<uint32_t*>(surface_->pixels),
            surface_->pitch / 4,
            surface_->w,
            surface_->h,
            tvg::ColorSpace::ARGB8888
        );

        // 6. 初始化 Flex Engine
        flex::init();

        // 7. 创建渲染器
        renderer_ = flex::create_thorvg_renderer(canvas_.get());

        // 8. 创建实例
        instance_ = flex::Instance::create(WIDTH, HEIGHT);

        // 9. 创建场景和动画
        create_scene();
        create_animations();

        return true;
    }

    void run() {
        while (running_) {
            handle_events();
            update(delta_time_);
            render();
            SDL_Delay(16);  // ~60 FPS
        }
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Surface* surface_ = nullptr;
    std::unique_ptr<tvg::SwCanvas> canvas_;
    std::unique_ptr<flex::Renderer> renderer_;
    flex::Instance::Ptr instance_;
    bool running_ = true;
    float delta_time_ = 0.016f;

    flex::Shape::Ptr player_;
    flex::TimelinePlayer* player_anim_ = nullptr;

    void create_scene() {
        auto* artboard = instance_->artboard();

        // 背景
        auto bg = flex::Shape::create();
        bg->set_rect(WIDTH, HEIGHT);
        bg->set_fill(flex::Color(0.1f, 0.1f, 0.15f));
        artboard->add_child(bg);

        // 玩家对象
        player_ = flex::Shape::create();
        player_->set_rect(80, 80);
        player_->set_position(100, 350);
        player_->set_fill(flex::Color(0.2f, 0.6f, 0.86f));
        artboard->add_child(player_);
    }

    void create_animations() {
        // 创建动画
        auto anim = flex::Timeline::create("Move");
        anim->set_loop_mode(flex::LoopMode::Loop);

        // 添加关键帧
        auto track = anim->add_track("x");
        track->add_keyframe(0.0f, 100.0f);
        track->add_keyframe(1.0f, 500.0f);
        track->add_keyframe(2.0f, 100.0f);

        instance_->add_timeline(anim);

        // 开始播放
        player_anim_ = instance_->play("Move", player_.get());
    }

    void handle_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running_ = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running_ = false;
                }
            }
        }
    }

    void update(float dt) {
        instance_->advance(dt);
    }

    void render() {
        // 清空背景
        SDL_FillRect(surface_, nullptr, 0xFF1a1f29);

        // 开始帧
        renderer_->begin_frame(WIDTH, HEIGHT, 1.0f);

        // 渲染场景
        auto* artboard = instance_->artboard();
        artboard->render(*renderer_);

        // 结束帧
        renderer_->end_frame();

        // 更新窗口
        SDL_UpdateWindowSurface(window_);
    }
};

int main() {
    AnimationRenderer renderer;

    if (!renderer.init()) {
        return 1;
    }

    renderer.run();
    return 0;
}
```

## 关键步骤解析

### 1. 初始化顺序

**正确的初始化顺序**：
1. SDL2 - 窗口和输入
2. ThorVG - 图形渲染引擎
3. Flex Engine - 场景和动画
4. 创建场景和动画

**为什么顺序很重要**：
- SDL2 需要首先创建窗口
- ThorVG 需要 SDL 表面作为渲染目标
- Flex Engine 需要 ThorVG 渲染器来绘制

### 2. 渲染循环

```cpp
void run() {
    while (running_) {
        // 1. 处理输入事件
        handle_events();

        // 2. 更新动画状态
        update(delta_time_);

        // 3. 渲染当前帧
        render();

        // 4. 控制帧率
        SDL_Delay(16);  // ~60 FPS
    }
}
```

### 3. 帧更新流程

```
用户输入 → 事件处理 → 更新动画状态 → 渲染场景 → 显示帧
    ↓
SDL_PollEvent  → instance_->advance(dt)  → renderer_->begin_frame()
                                                   ↓
                                           artboard->render(*renderer_)
                                                   ↓
                                           renderer_->end_frame()
                                                   ↓
                                       SDL_UpdateWindowSurface()
```

### 4. 动画属性映射

Flex 动画可以修改以下节点属性：

| 属性名 | 类型 | 描述 |
|--------|------|------|
| `x` | float | X 坐标 |
| `y` | float | Y 坐标 |
| `rotation` | float | 旋转角度（度） |
| `scaleX` | float | X 轴缩放 |
| `scaleY` | float | Y 轴缩放 |
| `opacity` | float | 透明度 [0.0, 1.0] |
| `fill.color` | Color | 填充颜色 |

## 编译和运行

### CMake 配置

```cmake
find_package(SDL2 CONFIG QUIET)

if(SDL2_FOUND)
    add_executable(my_animation my_animation.cpp)
    target_link_libraries(my_animation
        PRIVATE
        flex
        SDL2::SDL2
        SDL2::SDL2main
    )
endif()
```

### 运行程序

```bash
# 编译
cmake --build build --target my_animation

# 运行
./build/my_animation.exe
```

## 性能优化

### 1. 帧率控制

```cpp
// 固定时间步长
Uint32 last_time = SDL_GetTicks();
while (running_) {
    Uint32 current_time = SDL_GetTicks();
    float dt = (current_time - last_time) / 1000.0f;
    last_time = current_time;

    update(dt);
    render();

    // 限制最大 delta time
    dt = std::min(dt, 0.05f);
}
```

### 2. 批量更新

```cpp
// 一次性更新所有动画
instance_->advance(dt);  // 而不是逐个更新
```

### 3. 可见性裁剪

```cpp
// 只渲染可见的节点
if (node->should_render()) {
    node->render(renderer);
}
```

## 常见问题

### Q: 窗口黑屏？
**A**: 检查：
1. 是否调用了 `SDL_UpdateWindowSurface()`
2. ThorVG 画布是否正确初始化
3. 节点是否有有效的几何形状

### Q: 动画不播放？
**A**: 检查：
1. 时间线是否添加到实例：`instance_->add_timeline()`
2. 动画是否开始播放：`instance_->play()`
3. 动画名称是否正确

### Q: 性能问题？
**A**: 优化：
1. 减少同时播放的动画数量
2. 使用简单的几何形状
3. 启用可见性裁剪

## 示例程序

我们提供了多个示例程序：

### 1. 基础动画 (`flex_animation_test.cpp`)
- 无需 SDL2/ThorVG
- 控制台输出动画状态
- 用于测试动画逻辑

### 2. 简化渲染 (`flex_thorvg_simple.cpp`)
- SDL2 + ThorVG 集成
- 详细的错误检查
- 基础动画演示

### 3. 完整渲染 (`flex_thorvg_animation.cpp`)
- 完整的交互式演示
- 多个动画同时播放
- 键盘控制（空格暂停、1-5 停止动画）
- FPS 显示

### 4. DSL 动画定义 (`advanced_animation.flex`)
- 声明式动画定义
- 复杂动画组合
- 状态机集成

## 进阶功能

### 1. 交互式控制

```cpp
void handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.key.keysym.sym) {
            case SDLK_SPACE:  // 暂停/恢复
                if (player_anim_->is_playing()) {
                    player_anim_->pause();
                } else {
                    player_anim_->play();
                }
                break;

            case SDLK_r:  // 重置动画
                player_anim_->seek(0);
                break;

            case SDLK_s:  // 停止动画
                player_anim_->stop();
                break;
        }
    }
}
```

### 2. 动画混合

```cpp
// 播放多个动画，叠加效果
auto* anim1 = instance_->play("Move", player);
auto* anim2 = instance_->play("Rotate", player);

// 设置混合模式和权重
anim1->set_blend_mode(flex::BlendMode::Override);
anim2->set_blend_mode(flex::BlendMode::Additive);
anim2->set_blend_weight(0.5f);
```

### 3. 动态创建动画

```cpp
void create_dynamic_animation(float start_x, float end_x, float duration) {
    auto anim = flex::Timeline::create("DynamicMove");
    anim->set_duration(duration);

    auto track = anim->add_track("x");
    track->add_keyframe(0.0f, start_x);
    track->add_keyframe(duration, end_x);

    instance_->add_timeline(anim);
    instance_->play("DynamicMove", player_);
}
```

## 最佳实践

1. **遵循初始化顺序**: SDL2 → ThorVG → Flex
2. **控制帧率**: 使用 `SDL_Delay()` 或更精确的计时器
3. **错误检查**: 每个初始化步骤都检查返回值
4. **资源清理**: 程序退出时正确释放资源
5. **动画命名**: 使用描述性的动画名称
6. **性能监控**: 监控 FPS 和内存使用

## 总结

Flex Engine 的 ThorVG + SDL2 集成提供了：
- ✅ 高质量向量图形渲染
- ✅ 流畅的实时动画
- ✅ 跨平台窗口管理
- ✅ 完整的输入事件支持
- ✅ 易于使用的 C++ API
- ✅ 类型安全的动画系统

通过本指南，您应该能够创建自己的实时动画渲染程序！
