# Flex 渲染后端

Flex 只向应用暴露四种平台输出后端：OpenGL、Vulkan、TUI 和 Direct2D。
NanoVG 与 gCanvas 是内部渲染引擎，不参与 backend 注册，也不出现在
`RendererBackend` 中。

## 分层

```text
Flex scene / Renderer contract
             |
             +-- flex_backend_opengl  ---- flex_render_engine_gcanvas ---- gCanvas::OpenGL
             |                       `--- flex_render_engine_nanovg (显式迁移回滚/对比)
             +-- flex_backend_vulkan ---- flex_render_engine_gcanvas ---- gCanvas::Vulkan
             +-- flex_backend_tui
             +-- flex_backend_d2d
             `-- flex_render_engine_gcanvas (能力验证，不注册 backend)
```

| 公开 backend | 枚举 | CMake target | 入口 | 内部实现 |
|---|---|---|---|---|
| OpenGL | `RendererBackend::OpenGL` | `flex_backend_opengl` | `backends/opengl/init.h` | gCanvas；可显式选 NanoVG 做迁移回滚 |
| Vulkan | `RendererBackend::Vulkan` | `flex_backend_vulkan` | `backends/vulkan/init.h` | gCanvas 原生 Vulkan command recording |
| TUI | `RendererBackend::TUI` | `flex_backend_tui` | `backends/tui/init.h` | Tango terminal buffer |
| Direct2D | `RendererBackend::Direct2D` | `flex_backend_d2d` | `backends/d2d/init.h` | Direct2D/WIC/DirectWrite |

内部 target `flex_render_engine_nanovg` 和 `flex_render_engine_gcanvas` 只用于组合
公开 backend、渲染测试和开发工具。
应用不应把它们作为平台选择，也不应依赖其接口保持稳定。

## gCanvas 默认内部引擎

`flex_render_engine_gcanvas` 是 `Renderer` 到 `gcanvas::Context` 的薄适配器。它既可
借用 Vulkan backend 提供的 context，也可拥有 OpenGL External context；同一适配层
保持两种 GPU API 的 Flex 语义一致，但不会注册第五种 `RendererBackend`。

该边界的架构决定如下：

| 候选 | 结果 | 原因与代价 |
|---|---|---|
| 注册 gCanvas 为第五种 backend | 不选 | 把 canvas 实现与平台输出重复建模，公开 API 和选择语义都会扩大 |
| 同时替换 NanoVG 与 ThorVG | 已完成主路径迁移 | ThorVG 已移除；OpenGL/Vulkan 默认均由 gCanvas 服务；NanoVG 仅在显式构建选项下保留作 A/B 与回滚 |
| 内部 `Renderer` adapter（当前） | 选择 | 不改公开枚举；同一语义适配器服务 OpenGL/Vulkan，两者仍保持各自平台 backend 生命周期 |

### 阴影过滤架构决定

背景是 shadow 同时跨越 path tessellation、glyph atlas、image alpha、解析图元和 Flex
状态栈，影响 gCanvas core、两个 GPU backend、Flex adapter 与节点 render path。候选如下：

| 候选 | 结果 | 性能、复杂度与迁移权衡 |
|---|---|---|
| 每类内容各写一套 shadow | 不选 | 状态、alpha 合成与错误语义会在五条路径重复 |
| 全画布离屏 surface + blur pass | 不选作节点 blur | 会把节点级 draw state 扩大为 framebuffer 生命周期、Vulkan layout/barrier 与额外显存，并可能错误采样先前节点 |
| 有界 sampled mask + 解析 inset（当前） | 选择 | path 使用 stencil dilation/erosion，glyph/image 复用 alpha texture 与固定 5x5 disk morphology（13 taps）；无每帧 GPU 资源分配 |

状态事实源仍是 `Flex::Shadow`；adapter 只转换 affine、alpha 和单位，采样核只由
gCanvas core 生成，OpenGL/Vulkan 不各自推进效果状态。失败发生在拥有语义的绘制边界。
迁移成本限于内部 engine 与新增的 gCanvas 0.8 API；公开绘制 API 保持源码兼容，受保护的 backend bridge 因 signed-spread/blur 参数扩展需要同步实现。若像素或性能
回归，可回滚 sampled 调用而保留原有解析图元阴影，不影响四种公开 backend 枚举。

状态仍只有一个事实源：Flex scene/layout/animation 拥有业务状态，gCanvas context
只接收当帧绘制命令并拥有 GPU 资源。迁移路径是先补齐 capability 与双后端像素测试，
OpenGL 默认已切换到 gCanvas；显式设置 `FLEX_OPENGL_RENDER_ENGINE=nanovg` 即可回滚，
不影响四种公开 backend 枚举或调用侧 `Renderer` 契约。
新增依赖只在 `vendor/gCanvas` 内，版本、上游 commit、本地裁剪范围与 MIT 许可记录
在其 `README.md` 和 `LICENSE` 中。

当前已验证能力如下：

| Flex 语义 | 状态 | 验证方式 |
|---|---|---|
| DSL lowering 与 flex layout | 支持 | 标准 row/gap/padding 场景的真实像素坐标 |
| 实心矩形、圆角矩形、圆 | 支持 | immediate adapter contract；矩形 GPU 像素 |
| 嵌套平移、轴对齐缩放、opacity | 支持 | 状态栈 contract；半透明 GPU 像素 |
| 矩形裁剪 | 支持 | 轴对齐时使用 OpenGL/Vulkan scissor；仿射裁剪使用独立 stencil bit |
| 默认字体与已注册字体（含透明度） | 支持 | 默认字体 GPU 像素；注册失败向上抛出 |
| 栅格图像（含全局透明度与 affine） | 支持 | context 所有的 image cache；旋转纹理与 tint alpha 双 GPU 像素 |
| 动画结果 | 支持 | `markerMove` 在 0.5 秒的旧/新位置像素 |
| path、line、gradient、非圆 ellipse | 支持 | adapter contract；OpenGL/Vulkan 真实像素覆盖线性/径向渐变、Bezier、fill/stroke |
| vector rotation/shear、非均匀圆缩放 | 支持 | affine path 栅格与双 GPU 像素 |
| rotation/shear 与非均匀缩放文字 | 支持 | 保持逻辑字号并提交 affine glyph quad；adapter contract 与双 GPU 像素 |
| rotation/shear clip 与嵌套交集 | 支持 | adapter 凸多边形交集 contract；独立 clip/path stencil bit 与双 GPU 像素 |
| SVG（含 affine） | 支持 | PlutoSVG 栅格化、context image cache、适配器 contract 与双 GPU 像素 |
| 外阴影（矩形/圆角矩形/圆） | 支持 | 解析 SDF；offset/spread/blur/opacity 与状态栈通过 adapter contract 及双 GPU 像素验证 |
| arbitrary path/line/ellipse 外阴影 | 支持 | core 有界采样核 + GPU stencil tessellation；adapter affine contract 与双 GPU 像素验证 |
| text/image/SVG 外阴影 | 支持 | glyph/image alpha-mask silhouette；无 CPU readback；双 GPU 像素验证 |
| rect/rounded/circle/ellipse inset shadow | 支持 | 主内容后解析 SDF overlay；双 GPU 像素验证 |
| path/line/rotated ellipse inset shadow | 支持 | 原始/偏移轮廓使用独立 stencil bit 求差；支持 fill/stroke、affine 与 active clip |
| text/image/SVG inset shadow | 支持 | 原始与偏移 alpha 在 fragment shader 中求差；无 CPU readback/临时 framebuffer |
| sampled negative spread | 支持 | path 使用有界 stencil dilation/erosion；text/image/SVG 使用固定 5x5 disk alpha morphology；双 GPU 像素验证 |
| blur filter | 支持 | 节点 draw-state 使用有界 source-color Gaussian taps；path/text/image/SVG、状态恢复与双 GPU 像素验证 |
| retained mode | 不支持 | capability 为 false 或抛出 `logic_error` |
| 半透明 path stroke | 支持 | 共享 paint alpha 与 GPU 合成像素 |
| 半透明原生图元 stroke/text/image | 支持 | straight-alpha shader 与 OpenGL/Vulkan GPU 合成像素 |

外阴影在主内容前提交，解析 inset 在主内容后提交。primitive stroke 的外半宽计入
外阴影轮廓；sampled kernel 由 `ResourceLimits::max_shadow_samples` 限制。path 命令量为
`O(Q*S)`，其中 `Q` 是有界 tessellation quad 数、`S <= 25`；纹理 morphology 每 fragment
读取 13 个有界 alpha 样本。完整 Shadow 契约通过后 capability 为 `true`。

### Blur filter 架构决定

`Flex::BlurFilter` 是状态事实源，`set_blur`/`clear_blur` 只影响其间提交的节点绘制；
adapter 负责 affine 尺度换算，gCanvas core 独占 Gaussian 核生成与 source-over alpha
归一化，OpenGL/Vulkan 只消费同一组有序 taps。候选与权衡如下：

| 候选 | 结果 | 影响 |
|---|---|---|
| 全画布 ping-pong | 不选 | 会采入先前节点，且增加双 surface、resize、barrier 与外部 Vulkan target ownership |
| 每个 backend 自建 blur 状态 | 不选 | 核、alpha 和错误语义会分叉 |
| core 有界 source-color taps（当前） | 选择 | 不做 CPU readback；path 只 tessellate/获取 paint texture 一次，text/image 复用原纹理；代价为 `O(Q*S)` GPU draws |

资源协议是单线程 context 所有：frame command 从 empty → recording → consumed → empty，
blur 不拥有跨帧 surface。`S <= max_blur_samples`（默认 25），单次 path blur 在提交前
检查 `(Q+1)*S <= max_blur_commands`（默认 65,536）并一次 reserve；失败不提交该 blur
命令组。backend command vector 保留帧高水位容量并按 1.5 倍有界增长，避免较大帧中
每个 path 都触发精确扩容和历史命令搬迁。resize/device teardown 无新增资源顺序。
回滚只需将 adapter capability 关闭并
恢复普通 draw 调用，不涉及持久资源迁移或公开 backend 枚举。

适配器不做隐式降级：无法保持 Flex 语义的操作会立即失败。借用 context 时，
`end_frame()` 只调用 `draw_frame()`，宿主随后调用 `read_pixels()`（如需）和
`present_frame()`；拥有 OpenGL External context 时，`end_frame()` 同时调用
`present_frame()` 释放当帧资源 pin，但缓冲交换仍由宿主执行。Flex frame 的宽高必须
与 context 一致；高 DPI 窗口应传入相同逻辑尺寸，像素测试使用 native-pixel 窗口
消除系统缩放的不确定性。

构建与验证：

```cmake
option(FLEX_BUILD_RENDER_ENGINE_GCANVAS
       "Build the internal gCanvas rendering engine" ON)
option(FLEX_BUILD_GCANVAS_GPU_TESTS
       "Build real OpenGL/Vulkan Flex-on-gCanvas pixel tests" OFF)
set(FLEX_OPENGL_RENDER_ENGINE "gcanvas" CACHE STRING
    "Flex OpenGL drawing engine: gcanvas or nanovg")
option(FLEX_BUILD_RENDER_ENGINE_NANOVG
       "Build the legacy NanoVG engine for migration parity tests" OFF)
option(FLEX_BUILD_OPENGL_ENGINE_PARITY_TESTS
       "Build same-context NanoVG/gCanvas OpenGL parity tests and benchmark" OFF)
```

```bash
cmake -S . -B build -DFLEX_BUILD_GCANVAS_GPU_TESTS=ON
cmake --build build --target test_gcanvas_renderer test_gcanvas_gpu benchmark_gcanvas_blur
ctest --test-dir build -R "^test_gcanvas_(renderer|gpu)$" --output-on-failure
./build/bin/benchmark_gcanvas_blur
```

`benchmark_gcanvas_blur` 不注册为 CTest。它在隐藏的真实 OpenGL/Vulkan 窗口上预热后，
分别测量 16 个图元的典型帧和 96 个图元的压力帧；每组三分之一为 arbitrary path，
TinyTest 的主结果包含 Flex adapter、path 解析/tessellation、命令提交、GPU 执行与
present/fence；附加 segment 输出把 `record`、`submit`、`present` 分开。OpenGL 的
`submit` 包含 draw 提交（不强制 `glFinish`），Vulkan 的 `submit` 包含 upload/command recording、
acquire 和 queue submit，故 segment 只用于同一 backend、同一机器和构建配置的前后
对比，不作为跨 API 的纯 GPU 时间，也不设置跨机器的硬编码耗时阈值。

仓库内的 GLFW meta_editor、flexUI visual/shadcn/markdown demos，以及 SDL OpenGL
flexUI Designer 均已迁移到公开 `flex_backend_opengl`，用于验证外部窗口/context 生命周期。
旧 SDL 软件画布 chart demos 不再进入默认构建；其 renderer-independent 核心与测试仍保留。
FlexPlayer 还依赖仓库外 FFmpeg SDK 且需要原生 GPU 视频合成器，因此暂不进入顶层支持构建，
避免把已退休的软件画布路径伪装成可用 fallback。

## Backend-neutral 创建

所有 backend 都注册到同一个工厂表：

```cpp
flex::register_renderer_backend(backend, factory);
auto renderer = flex::create_renderer(backend, canvas);
```

`CanvasHandle` 的实际契约由选中的 backend 入口头定义。工厂没有注册或 canvas
不满足前置条件时返回 `nullptr`，不会自动回退到另一后端。

## OpenGL

OpenGL backend 默认创建并销毁外部呈现模式的 gCanvas context。宿主负责保持 OpenGL
context current、提供 procedure loader，并在 `end_frame()` 后交换窗口缓冲：

```cpp
#include "backends/opengl/init.h"

// 宿主已使 OpenGL 3 context current，并已初始化 GLAD。
flex::opengl_backend::OpenGLCanvas canvas;
canvas.get_proc_address = [](void*, const char* name) {
    return reinterpret_cast<flex::opengl_backend::OpenGLProcAddress>(
        glfwGetProcAddress(name));
};

flex::opengl_backend::register_backend();
auto renderer = flex::opengl_backend::create_renderer(&canvas);
```

所有权与生命周期：

- `OpenGLCanvas` 本身不被 renderer 持有；`proc_loader_user_data` 是借用值，必须活到
  renderer 析构完成。
- renderer 拥有 gCanvas context、字体/图像/渐变缓存；`end_frame()` 提交并关闭
  gCanvas 帧，但 External presentation mode 不调用 swap。
- 宿主拥有窗口和 OpenGL context；创建、绘制、销毁 renderer 时，同一个兼容
  OpenGL context 必须处于 current 状态。
- 一个 renderer 限定在创建它的 OpenGL context 和调用线程使用。
- `FLEX_OPENGL_RENDER_ENGINE=nanovg` 保留旧引擎；该路径不要求 proc loader，但不作为
  默认配置，也不会在 gCanvas 初始化失败时自动触发。

## Vulkan

Vulkan backend 通过 gCanvas 直接向宿主提供的 command buffer 录制原生 Vulkan
render pass、pipeline、vertex/index draw 与 layout barrier；不再生成全帧 CPU BGRA
surface，也不再执行 `vkCmdCopyBufferToImage`。

```cpp
#include "backends/vulkan/init.h"

flex::vulkan_backend::VulkanCanvas canvas;
canvas.instance = instance;
canvas.physical_device = physical_device;
canvas.device = device;
canvas.queue = graphics_queue;
canvas.queue_family_index = graphics_queue_family;
canvas.command_buffer = recording_command_buffer;
canvas.image = destination_image;
canvas.extent = {width, height};
canvas.image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
canvas.final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

flex::vulkan_backend::register_backend();
auto renderer = flex::vulkan_backend::create_renderer(&canvas);
```

数据路径协议：

| 项目 | 约束 |
|---|---|
| 主事实源 | `VulkanCanvas::image_layout` 必须等于 image 的实际当前 layout |
| backend 所有权 | pipeline/render pass、framebuffer/image view、VMA 管理的几何/纹理资源 |
| 宿主所有权 | instance、physical device、device、graphics queue、command buffer、image、提交与同步 |
| 帧入口 | command buffer 已 recording；上一帧引用 backend 资源的提交已经完成 |
| 帧提交 | `end_frame()` 只记录命令并更新 `image_layout`，不提交、不等待、不 present |
| 尺寸变更 | instance/device/queue/queue family/extent/format 改变后必须重建 renderer |
| 线程 | 单 renderer 单线程；宿主不得在 renderer 调用期间修改 canvas |

gCanvas 现使用 OpenGL/Vulkan 原生 stencil 路径：共享 CPU tessellator 只生成有界
triangle/quad mesh，GPU 完成 even-odd fill、stroke union、paint cover、混合与纹理采样。
这不是 tessellation shader 细分；选择共享 flattening 是为了让双后端保持同一几何
事实源。渐变使用固定大小 paint lookup texture；Context 持有最多
`max_path_surfaces` 个缓存槽，相同 paint/bounds 跨帧复用，槽位在当帧命令消费前不可
驱逐。半透明图案使用有界临时 texture，不透明图案直接复用源 GPU sampler；三条路径
都不上传全帧 BGRA surface。
几何和临时 paint 资源分别受 `max_path_mesh_quads`、`max_path_surfaces` 和
`path_paint_texture_size` 约束，越界立即报错。

Vulkan loader、headers、tools、VMA 与 utility libraries 均通过 vcpkg 接入；SVG
解析和栅格化由 PlutoSVG/PlutoVG 提供，第三方类型不会进入 Flex 的场景核心接口。

## TUI

```cpp
#include "backends/tui/init.h"

tui_terminal_t *terminal = tui_terminal_create();
tui_terminal_init(terminal);

flex::tui_backend::register_backend();
auto renderer = flex::tui_backend::create_renderer(terminal);
```

terminal 由宿主拥有，必须活到 renderer 析构之后。TUI 使用离散字符网格，不承诺
与像素 backend 完全一致的曲线、旋转、阴影或模糊能力；调用方应通过
`RendererCapabilities` 查询能力。

## Direct2D

```cpp
#include "backends/d2d/init.h"

flex::d2d_backend::register_backend();
auto renderer = flex::d2d_backend::create_renderer(render_target);
```

`ID2D1RenderTarget` 由宿主持有。若 target 丢失，renderer 通过
`requires_surface_recreation()` 暴露状态；宿主完成替换后调用
`acknowledge_surface_recreation()`。

Direct2D 当前不实现 shadow 或 `BlurFilter`，因此 `capabilities().shadow == false` 且
`capabilities().blur == false`。无可见效果的状态可安全清除；非法参数抛出
`invalid_argument`，可见 shadow 或正半径 blur 立即抛出 `logic_error`，不会静默绘制
缺失效果的内容。需要这些节点效果的调用方应在提交前检查 capability。

## 构建选项

```cmake
option(FLEX_BUILD_BACKEND_OPENGL "Build OpenGL renderer backend" ON)
option(FLEX_BUILD_BACKEND_VULKAN "Build native Vulkan renderer backend" ON)
option(FLEX_BUILD_BACKEND_TUI "Build terminal UI renderer backend" ON)
option(FLEX_BUILD_BACKEND_D2D "Build Direct2D renderer backend" ON)
```

消费端只链接所选平台 target：

```cmake
target_link_libraries(my_gl_app PRIVATE flex_backend_opengl)
target_link_libraries(my_vk_app PRIVATE flex_backend_vulkan)
```

关闭某个选项会使对应 target 和入口头不可用；构建系统不会自动选择其他 backend。
