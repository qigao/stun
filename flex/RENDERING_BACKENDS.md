# Flex Engine - Multiple Rendering Backends

Flex Engine supports multiple rendering backends through a clean abstraction layer. The runtime module defines the `Renderer` interface, and specific backends are implemented in the bridge layer.

## Architecture

```
┌─────────────────────────────────────┐
│     flex/runtime/renderer.h         │  ← Abstract Renderer interface
│     (Backend-agnostic)               │
└─────────────────────────────────────┘
                  ↑
                  │ implements
                  │
┌─────────────────┴───────────────────┐
│      Bridge Layer Backends          │
├─────────────────────────────────────┤
│  flex/bridge/renderer_thorvg.h      │  → ThorVG (vector graphics)
│  flex/bridge/renderer_nanovg.h      │  → NanoVG (lightweight)
│  flex/bridge/renderer_skia.h        │  → Skia (Google Chrome)
│  flex/bridge/renderer_d2d.h         │  → Direct2D (Windows)
└─────────────────────────────────────┘
```

## Supported Backends

### 1. ThorVG (Default) - Best for Production

**Pros:**
- ✅ High performance vector graphics
- ✅ Full SVG support
- ✅ Retained mode rendering (120 FPS optimization)
- ✅ Cross-platform (Windows, Linux, macOS, mobile)
- ✅ Small footprint (~200KB)

**Use case:** Production applications, games, embedded systems

**Example:**
```cpp
#include "flex.h"
#include "flex/backends/thorvg/init.h"
#include "flex/bridge/renderer.h"

// Initialize
flex::init();

// Create ThorVG canvas
auto canvas = tvg::SwCanvas::gen();
canvas->target(buffer, width, width, height, tvg::SwCanvas::ARGB8888);

// Create renderer
auto renderer = flex::create_thorvg_renderer(canvas.get());

// Use with Flex
auto scene = flex::Scene::create(800, 600);
scene->render(*renderer);

// Cleanup
flex::shutdown();
```

---

### 2. NanoVG - Best for Lightweight UI

**Pros:**
- ✅ Very lightweight (~50KB)
- ✅ OpenGL-based rendering
- ✅ Great for immediate mode UI
- ✅ Easy integration

**Cons:**
- ❌ No retained mode optimization
- ❌ Limited SVG support

**Use case:** Lightweight desktop applications, tools, editors

**Example:**
```cpp
#include "flex.h"
#include "flex/backends/nanovg/init.h"
#include "flex/bridge/renderer_nanovg.h"
#include <nanovg.h>

// Initialize
flex::init();

// Create NanoVG context (OpenGL)
NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);

// Create renderer
auto renderer = flex::create_nanovg_renderer(vg);

// Use with Flex
auto scene = flex::Scene::create(800, 600);
scene->render(*renderer);

// Cleanup
nvgDeleteGL3(vg);
flex::shutdown();
```

---

### 3. Skia - Best for Browser/Chrome-like Apps

**Pros:**
- ✅ Used by Chrome, Android, Flutter
- ✅ Excellent text rendering
- ✅ GPU acceleration
- ✅ Full 2D graphics capabilities

**Cons:**
- ❌ Large library (~10MB)
- ❌ Complex build setup

**Use case:** Browser-based apps, document viewers, professional graphics tools

**Example:**
```cpp
#include "flex.h"
#include "flex/backends/skia/init.h"
#include "flex/bridge/renderer_skia.h"
#include <include/core/SkCanvas.h>

// Initialize
flex::init();

// Create Skia surface
auto surface = SkSurface::MakeRasterN32Premul(800, 600);
SkCanvas* canvas = surface->getCanvas();

// Create renderer
auto renderer = flex::create_skia_renderer(canvas);

// Use with Flex
auto scene = flex::Scene::create(800, 600);
scene->render(*renderer);

// Cleanup
flex::shutdown();
```

---

### 4. Direct2D (Windows) - Best for Native Windows Apps

**Pros:**
- ✅ Native Windows API
- ✅ Hardware accelerated
- ✅ Excellent performance on Windows
- ✅ Native DPI scaling support

**Cons:**
- ❌ Windows-only
- ❌ Requires Windows 7+

**Use case:** Native Windows desktop applications

**Example:**
```cpp
#include "flex.h"
#include "flex/backends/d2d/init.h"
#include "flex/bridge/renderer_d2d.h"

// Initialize (creates D2D factory)
flex::init();

// Create D2D render target (from HWND)
ID2D1HwndRenderTarget* renderTarget = nullptr;
flex::g_d2d_factory->CreateHwndRenderTarget(
    D2D1::RenderTargetProperties(),
    D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(800, 600)),
    &renderTarget
);

// Create renderer
auto renderer = flex::create_d2d_renderer(renderTarget);

// Use with Flex
auto scene = flex::Scene::create(800, 600);

renderTarget->BeginDraw();
scene->render(*renderer);
renderTarget->EndDraw();

// Cleanup
renderTarget->Release();
flex::shutdown();
```

---

## Choosing a Backend

| Backend    | Size   | Speed | Platform       | Best For                    |
|------------|--------|-------|----------------|-----------------------------|
| **ThorVG** | 200KB  | ⭐⭐⭐⭐⭐ | All            | Production apps, games      |
| **NanoVG** | 50KB   | ⭐⭐⭐⭐  | All (OpenGL)   | Lightweight UI, tools       |
| **Skia**   | 10MB   | ⭐⭐⭐⭐⭐ | All            | Browser-like apps           |
| **D2D**    | Native | ⭐⭐⭐⭐⭐ | Windows only   | Native Windows apps         |

---

## Implementing a Custom Backend

To add your own rendering backend:

### 1. Create Factory Header

```cpp
// flex/bridge/renderer_mybackend.h
#pragma once
#include "flex/runtime/renderer.h"
#include <memory>

namespace flex {
std::unique_ptr<Renderer> create_mybackend_renderer(CanvasHandle canvas);
}
```

### 2. Implement Renderer Class

```cpp
// flex/src/bridge/renderer_mybackend.cpp
#include "flex/bridge/renderer_mybackend.h"

namespace flex {

class MyBackendRenderer : public Renderer {
public:
    explicit MyBackendRenderer(void* canvas) : canvas_(canvas) {}

    void begin_frame(float width, float height, float pixel_ratio) override {
        // Initialize frame
    }

    void draw_rect(float x, float y, float w, float h, float r,
                   const Paint& fill, const Paint& stroke, float stroke_width) override {
        // Draw rectangle using your backend API
    }

    // ... implement all virtual methods ...

private:
    void* canvas_;
};

std::unique_ptr<Renderer> create_mybackend_renderer(CanvasHandle canvas) {
    return std::make_unique<MyBackendRenderer>(canvas);
}

}
```

### 3. Create Init Header

```cpp
// flex/backends/mybackend/init.h
#pragma once
#include <tlog.h>

namespace flex {

inline void init() {
     // Initialize your backend here
}

inline void shutdown() {
    // Cleanup your backend
 }

}
```

### 4. Use Your Backend

```cpp
#include "flex.h"
#include "flex/backends/mybackend/init.h"
#include "flex/bridge/renderer_mybackend.h"

flex::init();
auto renderer = flex::create_mybackend_renderer(my_canvas);
// ... use renderer ...
flex::shutdown();
```

---

## CMake Integration

Different backends can be compiled conditionally:

```cmake
# Optional: ThorVG backend (default)
option(FLEX_BACKEND_THORVG "Enable ThorVG backend" ON)
if(FLEX_BACKEND_THORVG)
    target_sources(flex PRIVATE src/bridge/renderer_thorvg.cpp)
    target_link_libraries(flex PUBLIC thorvg)
endif()

# Optional: NanoVG backend
option(FLEX_BACKEND_NANOVG "Enable NanoVG backend" OFF)
if(FLEX_BACKEND_NANOVG)
    find_package(NanoVG REQUIRED)
    target_sources(flex PRIVATE src/bridge/renderer_nanovg.cpp)
    target_link_libraries(flex PUBLIC nanovg)
endif()

# Optional: Skia backend
option(FLEX_BACKEND_SKIA "Enable Skia backend" OFF)
if(FLEX_BACKEND_SKIA)
    find_package(Skia REQUIRED)
    target_sources(flex PRIVATE src/bridge/renderer_skia.cpp)
    target_link_libraries(flex PUBLIC skia)
endif()

# Optional: Direct2D backend (Windows only)
if(WIN32)
    option(FLEX_BACKEND_D2D "Enable Direct2D backend" OFF)
    if(FLEX_BACKEND_D2D)
        target_sources(flex PRIVATE src/bridge/renderer_d2d.cpp)
        target_link_libraries(flex PUBLIC d2d1)
    endif()
endif()
```

---

## Good Taste Architecture 🎯

**Linus 会认可的设计：**

✅ **消除特殊情况** - 所有后端使用统一接口，无需 #ifdef 判断
✅ **实用主义** - CanvasHandle (void*) 避免模板膨胀
✅ **简洁执念** - 用户代码只需 3 行切换后端
✅ **不破坏用户** - 添加后端不影响现有代码

```cpp
// 从 ThorVG 切换到 Skia 只需改 2 行！
- #include "flex/backends/thorvg/init.h"
- #include "flex/bridge/renderer.h"
+ #include "flex/backends/skia/init.h"
+ #include "flex/bridge/renderer_skia.h"
```

这就是"好品味"的代码 - 让特殊情况消失在统一的抽象中。🚀
