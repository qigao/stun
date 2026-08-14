# Flex Engine Architecture

## Overview

Flex Engine 现在推荐用更贴近职责的语义命名来理解模块：

```
┌───────────────────────────────────────────────┐
│              flex_dsl                         │  ← DSL 前端
│  (Parser, Lexer, AST)                         │
│  - 对应头入口: flex/dsl.h                      │
│  - 兼容目标: flex_compiler                     │
└───────────────────────────────────────────────┘
                      ↓
┌───────────────────────────────────────────────┐
│              flex_core                        │  ← 核心执行模型
│  (Scene Graph, Layout, Animation, Binding)    │
│  - 对应头入口: flex/core.h                     │
│  - 兼容目标: flex_runtime                      │
└───────────────────────────────────────────────┘
                      ↓
┌───────────────────────────────────────────────┐
│         lowering + renderer factory + flex    │
│  (AST 装配, Renderer Registry, Backend 接入)   │
│  - 对应头入口: flex/lowering.h                 │
│  - 对应头入口: flex/bridge/renderer.h          │
│  - 仓库内集成头: backends/*                    │
│  - 保留旧头路径: flex/bridge/*                 │
└───────────────────────────────────────────────┘
```

旧名字 `compiler/runtime/bridge` 仍然可用，但只作为兼容层；新代码应优先使用 `dsl/core/lowering` 这组语义命名。backend-specific init / register 头属于集成层，不是面向终端用户的稳定公开表面。

---

## Source And Target Layout

公开头保持在 `flex/include/flex/`，实现按所有权拆到 `flex/modules/`，每个模块由同目录
`CMakeLists.txt` 管理：

| 实现目录 | 兼容目标 | 新命名空间目标 | 职责 |
|---|---|---|---|
| `modules/expression/` | `flex_expression` | `Flex::Expression` | MIR 表达式编译与求值 |
| `modules/animation/` | `flex_animation` | `Flex::Animation` | 时间、关键帧与数值采样 |
| `modules/render/` | `flex_render` | `Flex::Render` | backend-neutral 渲染契约与注册表 |
| `modules/runtime/` | `flex_runtime`, `flex_core` | `Flex::Runtime`, `Flex::Core` | 场景、布局、事件与运行时状态 |
| `modules/dsl/` | `flex_compiler`, `flex_dsl` | `Flex::Compiler`, `Flex::DSL` | lexer、parser、AST 与常量折叠 |
| `modules/engine/` | `flex` | `Flex::Engine` | facade、lowering 与运行时装配 |
| `binary/src/` | `flex_binary` | `Flex::Binary` | 二进制格式适配 |

`flexUI/` 是独立产品目录，其兼容目标仍为 `flexUI`，新代码可使用
`FlexUI::Core`。这次迁移不改变安装头路径、既有 concrete target 名称或运行时数据格式。

### Directory Decision

**背景：** 原实现集中在 `flex/src/`，顶层 CMake 同时拥有解析、运行时、渲染和 facade
的源文件清单，模块所有权和可独立构建边界不清晰。

**候选方案：** 整体重命名 `flex/` 为 `flexEngine/` 会破坏源码路径和消费方；只拆 CMake
而保留混合源码目录不能建立清晰所有权。因此采用 `flex/modules/<capability>/`，同时保持
公开头与兼容目标稳定。

**权衡：** 分层 CMake 和单向 target 依赖更易验证，但 DSL 常量折叠当前通过
`flex::Expr` 使用 Runtime，因此 `Flex::Compiler` 仍显式依赖 `Flex::Runtime`。解除该依赖
需要单独迁移表达式公开 API 和 ABI，不属于目录重构。

**迁移与回滚：** 消费方可以逐步从 concrete target 迁移到 namespaced alias；无需同步
修改源码 include。若构建系统需要回滚，可把实现移回原目录并恢复顶层源清单，公开 API、
目标兼容名和数据格式均无需回退。

---

## Module Breakdown

### 0. flex_expression / flex_animation (共享数值执行层)

`flex_expression` 独立拥有 MIR 数值表达式编译器，位于 runtime、animation、
flexUI 和 charts 共同依赖的下层。调用方在编译时固定变量顺序，并在热路径通过连续
slot 数组求值；map API 仅保留给兼容调用点。

`flex_animation` 负责播放时间、循环遍历和数值采样。`NumericExpression` 只允许读取
`time`、`progress`、`from`、`to` 四个槽位。MIR 不负责关键帧查找、布局、属性混合、
事件分发或渲染后端；这些状态仍由 C++ 模块拥有。

### 1. flex_dsl / flex_compiler (DSL 前端模块)

**Preferred Headers:** `flex/dsl.h`, `flex/dsl/*`

**Legacy Headers:** `flex/compiler.h`, `flex/compiler/*`

**Location:** `flex/modules/dsl/`

**Responsibilities:**
- Lexical analysis (re2c)
- Syntax parsing (lemon)
- AST generation
- Error reporting

**Key Files:**
```
flex/include/flex/dsl/
  ├── flex_ast.h          # AST node definitions
  ├── flex_parser.h       # Parser API
  ├── flex_token.h        # Token definitions
  └── flex_defs.h         # Common definitions

flex/modules/dsl/
  ├── flex_lexer.re       # Lexer rules (re2c)
  ├── flex_parser.y       # Grammar rules (lemon)
  └── flex_parser_driver.cpp  # Parser driver

Generated (build directory):
  ├── flex_lexer_gen.cpp  # Generated lexer
  └── flex_parser_gen.h   # Generated parser
```

**API Example:**
```cpp
#include "flex/dsl.h"

auto ast = flex::parser::parse(source);
if (!ast) {
    printf("Error: %s\n", flex::parser::get_error());
}
```

---

### 2. flex_core / flex_runtime (核心运行时模块)

**Preferred Headers:** `flex/core.h`, `flex/core/*`

**Legacy Headers:** `flex/runtime.h`, `flex/runtime/*`

**Location:** `flex/modules/runtime/`

**Responsibilities:**
- Scene graph management
- Animation system
- Event handling
- Script bindings (interface only)
- Rendering interface (abstract)

**Key Components:**

#### A. Scene Graph
```
flex/core/node.h            # Base node class
flex/core/group.h           # Container node
flex/core/shape.h           # Geometry node
flex/core/text.h            # Typography node
flex/core/image.h           # Raster image node
flex/core/svg.h             # Vector graphic node
flex/core/scene.h           # Root canvas
flex/core/instance.h        # Component instance (InstanceNode)
```

#### B. Animation System
```
flex/core/timeline.h        # Timeline + keyframes
```

#### C. Interfaces (关键！)
```
flex/core/renderer.h              # 渲染器接口 (抽象)
flex/core/instance_context.h      # 实例上下文接口 (抽象)
```

**Design Principles:**
- ✅ **No DSL dependency** - Core 不知道 AST/Parser 的存在
- ✅ **No backend dependency** - 只有 Renderer 接口，无具体实现
- ✅ **Interface-based** - Script 通过 IInstanceContext 访问功能
- ✅ **Opaque pointers** - InstanceNode 使用 void* 避免循环依赖

**API Example:**
```cpp
#include "flex/core.h"

flex::ArenaAllocator arena(1024 * 1024);
auto* scene = flex::Scene::create(800, 600, arena);
auto* shape = flex::Shape::create(arena);
shape->set_rect(100, 100);
scene->add_child(shape);

// 需要 renderer (由 flex/bridge/renderer.h 提供工厂/注册能力)
scene->render(renderer);
```

---

### 3. lowering / renderer factory / flex (装配与后端模块)

**Preferred Headers:** `flex/lowering.h`, `flex/lowering/*`, `flex/bridge/renderer.h`

**Legacy Headers:** `flex/bridge/*`

**Location:** `flex/include/flex.h`, `flex/modules/engine/`, `flex/modules/render/`, `flex/backends/`

**Responsibilities:**
- Connect dsl + core
- Provide Definition/Instance classes
- Implement backend renderers
- Provide complete API
- Hide backend-specific wiring behind registry + optional backend modules

**Key Files:**

#### A. Lowering + Backend Entry
```
flex.h                               # 完整 facade API
flex/lowering/ast_to_runtime.h       # AST → core lowering
flex/bridge/renderer.h               # Public renderer registry/factory
modules/engine/lowering/ast_to_runtime.cpp
modules/render/renderer_factory.cpp
```

#### B. Definition & Instance
```
modules/engine/flex.cpp                    # Definition + Instance 实现
modules/engine/lowering/instance_node_runtime.cpp  # InstanceNode integration 实现
```

#### C. Backend Implementations
```
flex/bridge/renderer.h           # 公开 registry/factory 入口
flex/bridge/renderer_d2d.h       # Direct2D 平台适配声明

backends/opengl/                 # OpenGL 公开 backend
backends/vulkan/                 # Vulkan 公开 backend
backends/tui/                    # TUI 公开 backend
backends/d2d/                    # Direct2D 公开 backend
render/engines/nanovg/           # OpenGL 显式回滚与 A/B 对比引擎
render/engines/gcanvas/          # OpenGL/Vulkan 默认内部引擎适配器
```

gCanvas 适配器是内部 Adapter/Bridge：高层仍只依赖 `Renderer`；Vulkan context 由
backend 注入，OpenGL backend 则创建 External context。它不进入 backend registry，
避免把 canvas 实现误当成平台输出类型；不完整语义通过 capability 与明确异常暴露，
不做静默 fallback。

**API Example:**
```cpp
#include "flex.h"
#include "backends/opengl/init.h"

flex::opengl_backend::init();
flex::opengl_backend::register_backend();

// Load .flex file → Definition (dsl + core)
auto def = flex::Definition::load_file("app.flex");

// Create executable instance
auto instance = flex::Instance::create(def);

// Render through the backend-neutral default factory
flex::opengl_backend::OpenGLCanvas canvas;
canvas.get_proc_address = host_gl_proc_loader;
auto renderer = flex::opengl_backend::create_renderer(&canvas);
instance->render(*renderer);

renderer.reset();
flex::opengl_backend::shutdown();
```

---

## Key Design Patterns

### 1. Interface Abstraction (依赖倒置)

**Problem:** Core 需要 Instance，但 Instance 在 facade/lowering 层

**Solution:** IInstanceContext 接口

```cpp
// flex/core/instance_context.h (Core 层)
class IInstanceContext {
public:
    virtual void set_input(const char* name, float value) = 0;
    virtual Scene* scene() const = 0;
    // ...
};

// flex.h (Facade 层)
class Instance : public IInstanceContext {
    // 实现接口
};

// flex/modules/runtime/... (Core 层)
void bind_inputs(ScriptContext* ctx, IInstanceContext* instance) {
    // 只依赖接口，不依赖具体 Instance 类
}
```

### 2. Opaque Pointers (避免循环依赖)

**Problem:** InstanceNode (core) 需要 Instance/Definition (facade/lowering)

**Solution:** void* + lowering/facade 实现

```cpp
// flex/core/instance.h
class InstanceNode : public Node {
private:
    void* definition_;  // std::shared_ptr<Definition>*
    void* instance_;    // std::shared_ptr<Instance>*
};

// flex/modules/runtime/instance.cpp (占位实现)
bool InstanceNode::load() {
    return false;  // 占位
}

// flex/modules/engine/lowering/instance_node_runtime.cpp (真实实现)
bool InstanceNode::load() {
    #define INST_PTR (reinterpret_cast<std::shared_ptr<Instance>*>(instance_))
    *INST_PTR = Instance::create(*DEF_PTR);
    // ...
}
```

### 3. Factory Pattern (后端切换)

**Problem:** 多种渲染后端，统一创建

**Solution:** 工厂函数 + 统一接口

```cpp
// 统一接口
class Renderer { virtual void draw_rect(...) = 0; };

enum class RendererBackend { OpenGL, Vulkan, TUI, Direct2D, Custom };

// 平台模块注册工厂，业务层只依赖统一创建入口。
auto renderer = create_renderer(RendererBackend::OpenGL, canvas);
```

### 4. Retained Mode Rendering (保留模式渲染)

**Problem:** 每帧重绘所有形状 = 性能浪费

**Solution:** Dirty Flag + Cached Paint Objects

```
┌─────────────────────────────────────────────────────────────┐
│                    Rendering Flow                            │
├─────────────────────────────────────────────────────────────┤
│  Frame N                          Frame N+1                  │
│  ┌─────────┐                      ┌─────────┐               │
│  │ Shape A │ ──(cached)────────→  │ Shape A │ skip!         │
│  │ dirty=0 │                      │ dirty=0 │               │
│  └─────────┘                      └─────────┘               │
│  ┌─────────┐                      ┌─────────┐               │
│  │ Shape B │ ──(cached)────────→  │ Shape B │ update matrix │
│  │ dirty=T │                      │ dirty=0 │               │
│  └─────────┘                      └─────────┘               │
│  ┌─────────┐                      ┌─────────┐               │
│  │ Shape C │ ──(rebuild)───────→  │ Shape C │ full rebuild  │
│  │ dirty=C │                      │ dirty=0 │               │
│  └─────────┘                      └─────────┘               │
└─────────────────────────────────────────────────────────────┘

DirtyFlags: T=Transform, C=Content, 0=None
```

**Dirty Flags (粒度化脏标记):**
```cpp
enum class DirtyFlags : uint32_t {
    None        = 0,
    Transform   = 1 << 0,   // 位置/旋转/缩放变化
    Content     = 1 << 2,   // 几何/填充/描边变化
    // ...
};
```

**Shape::render() 逻辑:**
```cpp
void Shape::render(Renderer& r) {
    if (r.supports_retained_mode()) {
        // 已缓存且无变化? 完全跳过!
        if (cached_paint() && !is_dirty(Content)) {
            if (is_dirty(Transform)) {
                r.update_transform(cached_paint(), world_transform());
                clear_dirty(Transform);
            }
            return;  // 不重绘!
        }
        // 需要重建
        if (cached_paint()) r.remove_cached(cached_paint());
        set_cached_paint(r.push_rect(...));
        clear_dirty(Content | Transform);
        return;
    }
    // Immediate mode fallback...
}
```

**使用方式:**
```cpp
// Immediate Mode (每帧重建所有对象) - 默认
renderer->begin_frame(w, h, 1.0f);  // canvas->remove() 清空画布
instance->render(*renderer);
renderer->end_frame();

// Retained Mode (只更新脏节点) ⚡ RECOMMENDED
renderer->set_retained_mode(true);  // 启用一次即可
renderer->begin_frame(w, h, 1.0f);  // 保留画布对象
instance->render(*renderer);        // 只更新 dirty 节点
renderer->end_frame();
```

**性能对比 (1000 shapes):**
| Mode | First Frame | Subsequent Frames |
|------|-------------|-------------------|
| Immediate | ~8ms | ~8ms (full rebuild) |
| Retained | ~8ms | ~0.5ms (transform only) |

---

## Dependency Graph

```
Flex::Animation ─→ Flex::Expression
Flex::Runtime
    ├─→ Flex::Render
    ├─→ Flex::Animation
    └─→ Flex::Expression
Flex::Compiler ─→ Flex::Runtime  (parser 常量折叠使用 flex::Expr)

Flex::Engine (facade / lowering)
    ├─→ Flex::Runtime
    ├─→ Flex::Compiler
    └─→ Flex::Render

FlexUI::Core
    ├─→ Flex::Runtime
    ├─→ Flex::Render
    ├─→ Flex::Animation
    └─→ Flex::Compiler (private)

flex_backend_opengl  → flex_render_engine_gcanvas → gCanvas::OpenGL
                    `→ flex_render_engine_nanovg（显式回滚/对比）
flex_backend_vulkan → flex_render_engine_gcanvas → gCanvas::Vulkan
flex_backend_tui    → tango
flex_backend_d2d    → Direct2D/WIC/DirectWrite
```

**Critical Rule:** Runtime 永远不依赖 DSL、facade 或具体 backends！

`flex_runtime` 可以依赖 backend-neutral 的 `flex_render`（Renderer 契约、注册表和共享
格式化实现），但不得依赖任何 `flex_backend_*` 具体模块。

---

## Module Independence Tests

### Test 1: Build the concrete core target without flex_dsl
```bash
cmake --build --preset win-release-user --target flex_runtime
# flex_core / Flex::Core 是 interface target；实际构建目标是 flex_runtime
```

### Test 2: Use core API standalone
```cpp
#include "flex/core.h"  // 不包含 flex.h

flex::ArenaAllocator arena(1024 * 1024);
auto* scene = flex::Scene::create(800, 600, arena);
auto* shape = flex::Shape::create(arena);
shape->set_rect(100, 100);
scene->add_child(shape);
// 可以构建场景图，但不能加载 .flex 文件
```

### Test 3: Use dsl API standalone
```cpp
#include "flex/dsl.h"  // 不包含 flex.h

auto ast = flex::parser::parse(source);
// API 可独立使用；链接层因常量折叠显式依赖 Flex::Runtime
```

### Test 4: Full integration
```cpp
#include "flex.h"  // 完整 API

auto def = flex::Definition::load_file("app.flex");
auto instance = flex::Instance::create(def);
// dsl + core + lowering/backends 都可用
```

---

## Benefits of This Architecture

### 1. Modularity (模块化)
- ✅ 每个模块可独立开发/测试
- ✅ Runtime 可用于非 DSL 场景（纯 C++ API）
- ✅ Compiler 可用于 DSL 工具链（代码生成器）

### 2. Flexibility (灵活性)
- ✅ 平台选择固定为 OpenGL、Vulkan、TUI、Direct2D
- ✅ NanoVG/gCanvas 可作为内部实现独立演进
- ✅ 切换后端只需改一行代码
- ✅ 可选编译不同后端（CMake options）

### 3. Testability (可测试性)
- ✅ Runtime 单元测试无需编译器
- ✅ Compiler 单元测试无需运行时
- ✅ 集成测试覆盖完整流程

### 4. Performance (性能)
- ✅ 编译时间更快（模块分离）
- ✅ 链接时间更快（按需链接）
- ✅ 可执行文件更小（只链接需要的模块）

---

## Future Extensions

### 1. Additional Backends
- [x] OpenGL implementation（默认内部 gCanvas，NanoVG 可显式回滚）
- [x] Direct2D implementation (Windows)
- [ ] Metal implementation (macOS/iOS)
- [x] Native Vulkan implementation（内部 gCanvas）
- [x] Native OpenGL/Vulkan stencil vector tessellation implementation

### 2. Plugin System
- [ ] Dynamic backend loading (dlopen/LoadLibrary)
- [ ] Custom node types as plugins
- [ ] Custom animation systems

### 3. Multi-threading
- [ ] Parallel rendering (multi-canvas)
- [ ] Background compilation
- [ ] Async asset loading

---

## Conclusion

这个架构实现了 **"关注点分离"** 的终极形态：

- **Compiler** 只关心语法解析
- **Runtime** 只关心场景图逻辑
- **Bridge** 连接一切

每个模块都可以独立进化，互不影响。这就是可维护、可扩展的软件架构。🚀
