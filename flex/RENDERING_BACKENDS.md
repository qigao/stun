# Flex Engine - Rendering Backends

Flex 的渲染后端已经改成“核心运行时 + 后端工厂注册 + 可选 backend 模块”三层结构。

当前约束：

- `flex/core/renderer.h` 定义纯抽象 `Renderer` 接口
- `flex/bridge/renderer.h` 提供公开的 backend-neutral registry / factory API
- `backends/*/init.h` 是仓库内或宿主应用的集成入口，用来初始化并注册具体后端
- `flex_backend_*` 是分离的可选 CMake 目标，不再直接并进 `flex` 主库
- 终端用户代码应优先走 `create_renderer(handle)`，而不是在业务层到处写死具体 backend 枚举

## Current Layout

```text
┌───────────────────────────────────────────────┐
│ flex/core/renderer.h                          │
│   Renderer 抽象接口                            │
└───────────────────────────────────────────────┘
                      │
                      ▼
┌───────────────────────────────────────────────┐
│ flex/bridge/renderer.h                        │
│   renderer_backend_factory()                  │
│   register_renderer_backend()                 │
│   set_default_renderer_backend()              │
│   create_renderer(CanvasHandle)               │
└───────────────────────────────────────────────┘
                      │
                      ▼
┌───────────────────────────────────────────────┐
│ backends/<name>/init.h                        │
│   <name>_backend::init()                      │
│   <name>_backend::register_backend()          │
│   <name>_backend::load_font(...)              │
└───────────────────────────────────────────────┘
                      │
                      ▼
┌───────────────────────────────────────────────┐
│ src/backends/renderer_<name>.cpp              │
│ flex_backend_<name>                           │
└───────────────────────────────────────────────┘
```

## Public Surface Vs Integration Surface

安装后的公开 API 重点是：

- `#include <flex.h>`
- `#include <flex/bridge/renderer.h>`

backend-specific init 头例如：

- `backends/thorvg/init.h`
- `backends/nanovg/init.h`
- `backends/d2d/init.h`
- `backends/tui/init.h`

这些头现在属于集成层细节。仓库内示例、测试、宿主应用可以直接包含；面向终端用户的安装包不应把它们当成稳定公开表面。

## Backend Selection Model

推荐流程是两步：

1. 启动阶段由宿主应用初始化并注册一个具体 backend
2. 业务代码统一调用 `create_renderer(handle)`

这样业务层只依赖“默认 renderer 工厂”，不依赖具体 backend 类型。

### Registry API

```cpp
#include <flex/bridge/renderer.h>

flex::RendererFactory current = flex::default_renderer_factory();
bool ok = flex::set_default_renderer_backend(flex::RendererBackend::Direct2D);
auto renderer = flex::create_renderer(canvas_handle);
```

## Supported Backends In Tree

### ThorVG

适合：

- 软件栅格化
- SVG/矢量图较多的场景
- 跨平台宿主

示例：

```cpp
#include <flex.h>
#include "backends/thorvg/init.h"
#include <thorvg.h>

flex::thorvg_backend::init();
flex::thorvg_backend::register_backend();
flex::thorvg_backend::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf");

auto canvas = tvg::SwCanvas::gen();
canvas->target(buffer, width, width, height, tvg::ColorSpace::ARGB8888);

auto renderer = flex::create_renderer(static_cast<flex::CanvasHandle>(canvas.get()));
if (!renderer) {
    return;
}

renderer->begin_frame(static_cast<float>(width), static_cast<float>(height), 1.0f);
instance->render(*renderer);
renderer->end_frame();

flex::thorvg_backend::shutdown();
```

### NanoVG

适合：

- OpenGL 宿主
- 即时绘制型桌面工具
- 不依赖 ThorVG 的轻量窗口集成

示例：

```cpp
#include <flex.h>
#include "backends/nanovg/init.h"
#include <nanovg.h>

flex::nanovg_backend::init();
flex::nanovg_backend::register_backend();
flex::nanovg_backend::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf");

NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
auto renderer = flex::create_renderer(static_cast<flex::CanvasHandle>(vg));
if (!renderer) {
    return;
}

renderer->begin_frame(window_width, window_height, pixel_ratio);
renderer->clear(instance->scene()->background());
instance->render(*renderer);
renderer->end_frame();

nvgDeleteGL3(vg);
flex::nanovg_backend::shutdown();
```

### Direct2D

适合：

- Windows 原生桌面程序
- HWND / D2D render target 集成
- 希望把 Windows 默认 backend 切到原生 2D API 的宿主

当前行为：

- `d2d_backend::register_backend()` 会注册 `RendererBackend::Direct2D`
- 在 Windows 上它还会把默认 backend 直接切成 Direct2D

示例：

```cpp
#include <flex.h>
#include "backends/d2d/init.h"
#include <d2d1.h>

flex::d2d_backend::init();
flex::d2d_backend::register_backend();
flex::d2d_backend::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf");

ID2D1HwndRenderTarget* render_target = nullptr;
flex::d2d_backend::g_d2d_factory->CreateHwndRenderTarget(
    D2D1::RenderTargetProperties(),
    D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(800, 600)),
    &render_target);

auto renderer = flex::create_renderer(
    static_cast<flex::CanvasHandle>(render_target));
if (!renderer) {
    render_target->Release();
    return;
}

renderer->begin_frame(800.0f, 600.0f, 1.0f);
renderer->clear(instance->scene()->background());
instance->render(*renderer);
renderer->end_frame();

render_target->Release();
flex::d2d_backend::shutdown();
```

### TUI

适合：

- 终端 UI
- 无窗口系统环境
- 调试和轻量展示

示例：

```cpp
#include <flex.h>
#include <backends/tui/init.h>
#include <tui.h>

tui_terminal_t* term = tui_terminal_create();
tui_terminal_init(term);

flex::tui_backend::register_backend();
auto renderer = flex::create_renderer(static_cast<flex::CanvasHandle>(term));
if (!renderer) {
    return;
}

renderer->begin_frame(800.0f, 600.0f, 1.0f);
renderer->clear(flex::Color{0.1f, 0.1f, 0.15f, 1.0f});
instance->render(*renderer);
renderer->end_frame();
```

## About Skia

`RendererBackend::Skia` 以及 `flex/bridge/renderer_skia.h` 仍保留在接口层，用于兼容和未来扩展；但当前仓库的 CMake 并没有接好一个维护中的 `flex_backend_skia` 目标。不要把它当成“现成可用”的内置 backend。

## Implementing A Custom Backend

自定义 backend 现在建议围绕“注册工厂”实现，而不是直接把具体实现塞进 `flex` 主库。

### 1. 定义 renderer factory

```cpp
// flex/bridge/renderer_mybackend.h
#pragma once

#include "flex/core/renderer.h"

namespace flex {
std::unique_ptr<Renderer> create_mybackend_renderer(CanvasHandle handle);
}
```

### 2. 实现 renderer

```cpp
// src/backends/renderer_mybackend.cpp
#include "flex/bridge/renderer_mybackend.h"

namespace flex {

class MyBackendRenderer : public Renderer {
public:
    explicit MyBackendRenderer(void* handle) : handle_(handle) {}

    void begin_frame(float width, float height, float pixel_ratio) override {}
    void end_frame() override {}

    // 其余虚函数按接口补齐

private:
    void* handle_ = nullptr;
};

std::unique_ptr<Renderer> create_mybackend_renderer(CanvasHandle handle) {
    return std::make_unique<MyBackendRenderer>(handle);
}

} // namespace flex
```

### 3. 提供 backend 集成头

```cpp
// backends/mybackend/init.h
#pragma once

#include "backends/renderer.h"
#include "flex/bridge/renderer_mybackend.h"

namespace flex {
namespace mybackend {

inline bool register_backend() {
    register_renderer_backend(
        RendererBackend::Custom,
        static_cast<RendererFactory>(&create_mybackend_renderer));
    return set_default_renderer_backend(RendererBackend::Custom);
}

} // namespace mybackend
} // namespace flex
```

### 4. 在宿主启动时注册默认工厂

```cpp
mybackend::register_backend();
auto renderer = flex::create_renderer(my_handle);
```

## CMake Integration

当前推荐的编译方式是每个 backend 独立目标：

```cmake
option(FLEX_BUILD_BACKEND_THORVG "Build ThorVG renderer backend" ON)
option(FLEX_BUILD_BACKEND_TUI "Build TUI renderer backend" ON)
option(FLEX_BUILD_BACKEND_NANOVG "Build NanoVG renderer backend" ON)
option(FLEX_BUILD_BACKEND_D2D "Build Direct2D renderer backend" ON)

target_link_libraries(my_app PRIVATE flex)
target_link_libraries(my_app PRIVATE flex_backend_thorvg)
```

也就是说：

- `flex` 提供核心 facade + lowering + renderer factory registry
- `flex_backend_*` 提供具体 backend 实现和依赖
- 哪个宿主需要哪个 backend，就显式链接哪个 backend 目标

## Design Intent

这套设计解决的是两个问题：

- 不让 `flex_runtime` 或 `flex` 核心强绑某个图形库
- 不把 backend-specific 头暴露成终端用户必须理解的公开 API

因此现在的推荐写法不是“业务代码自行挑 backend”，而是“宿主在启动时选 backend，业务层只拿默认 renderer 工厂”。
