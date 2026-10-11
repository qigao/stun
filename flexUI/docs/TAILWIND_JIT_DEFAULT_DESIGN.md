# Tailwind-like Utility JIT 默认样式架构

- 状态：已实现，仓库既有回归基线待单独收口
- 日期：2026-07-15
- 决策范围：`flexUI` 默认样式运行时、仓库内应用入口、`charts` UI 适配层
- 相关计划：[TAILWIND_JIT_DEFAULT_PLAN.md](TAILWIND_JIT_DEFAULT_PLAN.md)

> 实施记录：P1-P8 的核心代码和相邻测试已落地；P9 已加入确定性离屏渲染快照与
> 典型/大树性能基线。原生窗口人工 smoke 和仓库既有失败项仍保留在实施计划中。

## 2026-09-20 兼容目标更新

本文主要记录当前 whitelist/JIT 实现及其已有不变量；它不再定义长期对外 Tailwind 语义。

长期 authoring contract 改为：

- 使用标准 `class="..."`；
- 固定一个上游 Tailwind 版本/compatibility profile；
- native parser/compiler 解析 utility、variant、theme、responsive、dark、data/aria、container 与明确支持的 arbitrary syntax；
- 普通 semantic class 与 Tailwind candidate 共存；
- 所有生成结果继续进入现有 StyleEngine cascade；
- official Tailwind toolchain 只作为 differential-test oracle，不成为安装运行时依赖；
- 识别但未实现的 Tailwind 语义明确报 Unsupported，不做本地 fallback/reinterpretation。

因此 `utility_whitelist.json`、`UtilityCatalog` 和 exact-token lookup 在迁移期间继续可用并保持测试，但它们是 current implementation/coverage data，不再是永久公开语义事实源。迁移跟踪：`stun#10`。

## 背景

`flexUI` 已具备基于显式白名单的 Tailwind-like Utility JIT。`Box` 会扫描
Element 树中的 class token，生成确定性的 CSS stylesheet，再交给现有
`StyleEngine` 参与正常 cascade。当前 JIT 需要调用方显式读取
`utility_whitelist.json` 并调用 `Box::enable_utility_jit()`，实际只有测试和
`visual_demo` 完成了这段初始化。

这导致仓库存在三种 styling 路径：

1. 普通 `flexUI::Box` 使用手写 CSS，但默认没有 Utility JIT。
2. `visual_demo` 从源码树读取 whitelist，显式启用 Utility JIT。
3. FlexChart 和 Infographic 直接创建 `flex::Group`、`Text`、`Shape`，完全绕过
   `Box`、Element tree 和 JIT；DotGraph 虽已有 Element adapter，仍直接写入
   大量 `style_` 字段。

目标是让仓库形成一个明确契约：用户可见 UI 默认由 `flexUI::Box` 承载，UI
styling 默认经过 Tailwind-like Utility JIT；领域语法、图表数据和绘图几何不由
JIT 管理。

## 仓库事实

- Element tree 是 flexUI 唯一的 style/layout tree，见
  [ARCHITECTURE.md](ARCHITECTURE.md) 和
  [WIDGET_DEVELOPMENT.md](WIDGET_DEVELOPMENT.md)。
- Utility whitelist 的事实源是
  [`tools/shadcn-ir/schema/utility_whitelist.json`](../../tools/shadcn-ir/schema/utility_whitelist.json)。
  它是受控的 Tailwind-like 子集，不是完整 Tailwind 编译器。
- `Box::enable_utility_jit()` 已具备原子替换 stylesheet、资源上限和失败保持旧
  JIT 状态的测试基础。
- JIT stylesheet 的 cascade 位置在启用时确定，之后替换内容不会改变位置。
- shadcn Chart 已有 component/style/bridge IR，可作为 chart shell 的结构与状态
  契约，但不包含数据模型、坐标轴、series 几何或 tooltip formatter。
- DotGraph 已有 Box-owned Element adapter；FlexChart 和 Infographic 的公开构建
  路径仍返回裸 `flex::Group*`。

## 目标

- `flexUI::Box box(renderer)` 默认启用内置 Utility JIT。
- 默认行为不依赖源码目录、当前工作目录或部署时的外部 JSON 文件。
- utility catalog 和默认 theme 各自只有一个事实源。
- 普通应用 CSS 可以覆盖默认 utility，不破坏现有 cascade 能力。
- 新 UI 对未知 utility fail fast，同时保留旧语义 class 的迁移能力。
- DotGraph、FlexChart 和 Infographic 的 UI chrome 进入同一 Element/JIT 管线。
- 图表 AST、scale、路径、坐标、clipping 和数据驱动绘制继续归 renderer 所有。
- 支持按 `Box` 显式关闭 JIT，供兼容测试或尚未迁移的调用方使用。

## 非目标

- 不实现完整 Tailwind 配置、插件系统或任意 utility 语法。
- 不把 chart DSL、Mermaid grammar 或领域 AST 改写为 Tailwind class。
- 不用 utility class 表达每个数据点的动态坐标或任意运行时颜色。
- 不在本次迁移中删除现有 `flex::Group*` chart API。
- 不引入全局可变 theme/JIT registry，也不让每个 chart 独立拥有 JIT。

## 决策

### 0. TailwindCSS 是独立编译模块

Tailwind-like catalog 编译位于 `flexUI/modules/tailwindcss/`，CMake 目标为
`flexUI_tailwindcss`，新代码通过 `FlexUI::TailwindCSS` 和
`<flexUI/tailwindcss.h>` 使用。模块不依赖 Element、Box、StyleEngine 或 shadcn widget
实例化；它只产生完整 CSS snapshot。Core 扫描 Element 并原子替换 stylesheet，状态
所有权因而保持不变。

`<flexUI/utility_jit.h>` 和 `flexUI::shadcn_ir` 下的 utility emission API 暂时保留为
兼容层。回滚时可以把 `utility_jit.cpp` 重新并入 Core target，但不得让独立模块反向依赖
Core；否则会重新形成静态库循环依赖。

### 1. `Box` 是默认样式运行时外观

`Box` 继续作为 Element、StyleEngine、layout 和 render pipeline 的外观，并通过
组合式配置获得默认 JIT 行为：

```cpp
enum class UtilityJitMode {
    BuiltIn,
    Disabled
};

enum class ThemeMode {
    System,
    Light,
    Dark
};

struct BoxOptions {
    UtilityJitMode utility_jit = UtilityJitMode::BuiltIn;
    ThemeMode theme = ThemeMode::System;
    tailwind::UtilityJitOptions limits{};

    static BoxOptions legacy_without_jit();
};

class Box {
public:
    explicit Box(flex::Renderer* renderer, BoxOptions options = {});
};
```

默认构造的公开语义变为：

```cpp
flexUI::Box box(renderer);
// box.utility_jit_enabled() == true
```

旧行为只能显式申请：

```cpp
flexUI::Box box(
    renderer,
    flexUI::BoxOptions::legacy_without_jit());
```

不提供进程级环境变量或编译期开关改变默认语义。需要兼容时，调用点必须显式
表达，避免相同二进制因外部状态不同而产生两套 styling 行为。

运行时诊断只读当前事实源，不维护第二份状态：

```cpp
box.is_known_utility("rounded-md");
box.utility_jit_revision();
box.active_utility_count();
box.utility_stylesheet_size();
box.missing_utility_tokens();
```

`missing_utility_tokens()` 只报告显式 utility 契约异常；普通 semantic class 不会
进入该列表。revision 只在 active utility program 改变时递增，attribute 和 custom
property 更新不会推进它。

### 2. Whitelist 构建时内嵌，运行时共享不可变 catalog

`utility_whitelist.json` 保持唯一事实源。CMake 在构建时将其生成到 flexUI 的内部
C++ asset 中，安装后的静态库不读取 `tools/` 路径。

```text
utility_whitelist.json
          │ CMake generation
          ▼
default_style_assets.generated.cpp
          │ parse once
          ▼
shared immutable UtilityCatalog
          │
          ├── Box A / active token cache
          ├── Box B / active token cache
          └── Box C / active token cache
```

共享对象只包含不可变 catalog，不包含 Element、active tokens、stylesheet revision
或其他 UI 状态。每个 `Box` 继续独立拥有 active-token cache 和 JIT stylesheet。
这不违反 `Box` 对跨帧 UI 状态的所有权约束。

建议内部接口为：

```cpp
std::shared_ptr<const tailwind::UtilityCatalog>
builtin_utility_catalog();
```

`UtilityJit` 接受共享 catalog，避免每创建一个 `Box` 都复制和解析完整 JSON。

### 3. 默认 theme 与 utility 一起提供

颜色类如 `bg-background`、`text-foreground` 依赖 `--background`、
`--foreground` 等 CSS variables。仅默认启用 JIT 而不提供基础变量会生成不完整
样式，因此新增单一默认 theme 事实源：

```text
flexUI/assets/default_theme.css
```

它至少定义：

- light theme design tokens；
- dark theme design tokens；
- `@media (prefers-color-scheme: dark)` 的 system 模式；
- `[data-theme=light]` 和 `[data-theme=dark]` 的显式覆盖；
- `--color-scheme` 等现有 host bridge 所需变量。

示例中重复的 shadcn theme variables 迁回该文件；示例只保留自身布局或演示专用
规则。

### 4. Cascade 顺序固定

默认 stylesheet 顺序为：

1. Widget/host 内建基础规则；
2. 默认 theme variables；
3. 构造 `Box` 时保留的 JIT stylesheet slot；
4. 调用方之后通过 `load_css()` / `load_stylesheet()` 加载的应用规则。

JIT 更新只替换第 3 层的内容，不改变其位置。应用 CSS 因为后加载，可以覆盖
默认 utility；内建默认值不会获得不可覆盖的特殊优先级。

### 5. 区分普通 class 与显式 utility

当前 JIT 把扫描到但不在 whitelist 中的所有 class 都报告为 missing。JIT 默认
启用后，`.dot-node`、`.card` 等普通 CSS/语义 class 会形成噪声，无法区分真正的
utility typo。

Element 增加显式 utility API：

```cpp
element->add_class("legacy-semantic-class");
element->add_utility("flex");
element->add_utilities("flex flex-col gap-4");
element->set_utilities("relative w-full h-full");
```

语义规则如下：

- `add_class()` 保持普通 CSS class 兼容性。
- 普通 class 若恰好存在于 catalog，仍可被 JIT 编译，兼容当前调用点。
- 普通 class 不存在于 catalog 时不作为必需 utility 报错。
- `add_utility()` / `add_utilities()` 声明调用方要求该 token 由 JIT 提供。
- 显式 utility 未知、超长或超过资源上限时立即返回明确错误或抛出仓库既有类型
  的异常；不降级成普通 class。
- 新代码使用 `data-slot`、`part`、`role`、`data-state` 等表达语义身份，class
  主要用于 styling。

`missing_utility_tokens()` 调整为只报告显式 utility 声明的缺失项。离线 registry
coverage 仍验证完整 shadcn token 清单，不因运行时普通 class 兼容策略而放宽。

### 6. Charts 使用公共 `ChartFrame`

在 `charts/ui` 增加组合式 `ChartFrame`，统一承载 chart UI chrome：

```cpp
struct ChartFrame {
    flexUI::Element* root = nullptr;
    flexUI::Element* title = nullptr;
    flexUI::Element* surface = nullptr;
    flexUI::Element* legend = nullptr;
    flexUI::Element* tooltip = nullptr;
};

ChartFrame create_chart_frame(
    flexUI::Box& box,
    const ChartFrameOptions& options);
```

稳定结构参考已有 shadcn Chart IR：

```text
chart root [data-slot=chart]
├── title [data-slot=chart-title]
├── surface [data-slot=chart-surface]
├── legend [data-slot=chart-legend]
│   └── item [data-slot=chart-legend-item]
└── tooltip [data-slot=chart-tooltip][data-state=inactive|active]
```

`ChartFrame` 只负责结构和 UI 状态，不解析 DSL、不计算 scale，也不绘制 series。
三个 chart 模块通过 adapter 把各自领域模型投影到该结构，避免继承层次和大型统一
chart 基类。

### 7. Plot 保持专用 Widget/renderer

chart surface 内使用模块自己的 Widget 发出 backend-neutral render commands：

```text
ChartFrame.surface
└── module PlotWidget
    ├── axes geometry
    ├── marks/series geometry
    ├── clipping
    └── hit testing
```

Tailwind-like JIT 管理：

- chart 容器和 responsive layout；
- 标题、图例、tooltip、toolbar；
- 字体、间距、边框、背景、阴影；
- hover、selected、disabled、open 等 UI 状态。

Renderer 管理：

- 数据点、线、柱、弧、路径和图像；
- scale、坐标、projection、clipping；
- 与数据直接关联的视觉编码。

动态值通过 Element custom properties 投影：

```cpp
item->set_custom_property("--series-color", css_color);
tooltip->set_custom_property("--tooltip-x", x_px);
tooltip->set_custom_property("--tooltip-y", y_px);
```

不得按数据值构造无限数量的动态 utility token。

### 8. 各 chart 模块迁移方式

#### DotGraph

- 复用现有 `create_flexui_dotgraph(Box&, ...)` adapter。
- 新增 `data-slot` / `data-*` 语义，旧 class 保留一个迁移周期。
- 固定布局和 chrome 改用 utility。
- DOT 提供的颜色写入 custom properties；路径、node/edge geometry 继续由 Widget
  绘制。

#### FlexChart

新增 Box-owned adapter：

```cpp
FlexUiChartResult create_flexui_chart(
    flexUI::Box& box,
    const AstChart& chart,
    const ChartViewOptions& options = {});
```

现有 parser、AST、mark renderer registry 不变。标题、legend、tooltip 和容器迁到
`ChartFrame`；mark geometry 进入 FlexChart PlotWidget。

#### Infographic

新增 Box-owned adapter：

```cpp
FlexUiInfographicResult create_flexui_infographic(
    flexUI::Box& box,
    const UnifiedInfographic& infographic,
    const InfographicViewOptions& options = {});
```

现有 UnifiedInfographic、layout engine 和 SVG 输出不改变。交互式 flexUI 输出使用
`ChartFrame` 或模块自己的 Element shell；静态 SVG 导出不强制经过 JIT。

现有 `flex::Group* build(...)` API 暂时保留并标记 deprecated。它们属于兼容路径，
不再作为新应用的默认入口。

Mermaid parser/AST 不受本决策影响；后续若增加交互式 Mermaid viewer，该 viewer
遵守相同的 Box/Element/JIT 契约。

## 状态与所有权

| 状态 | 主事实源 | 派生/消费者 |
| --- | --- | --- |
| Utility 定义 | `utility_whitelist.json` | 内嵌 `UtilityCatalog` |
| Theme tokens | `default_theme.css` | 每个 Box 的 StyleEngine |
| UI 树、class、attribute | `Box` 所有的 Element tree | JIT、layout、render |
| Chart 领域数据 | 各模块 AST/IR | plot geometry、legend model |
| Hover/selection/tooltip | ChartFrame/PlotWidget 状态 | `data-state`、ARIA、custom properties |
| Geometry snapshot | 模块 renderer/PlotWidget | RenderCommandList |
| Active utility tokens | 每个 Box | 该 Box 的 JIT stylesheet |

读取路径不推进业务状态。JIT stylesheet、computed style 和 geometry snapshot 都是
可由其事实源重建的派生数据，不允许反向修改 AST 或 Widget 状态。

## 错误语义

- 内嵌 catalog 或默认 theme 无效：`Box` 构造失败，不创建半初始化 UI。
- 自定义 JIT reconfigure 无效：保留当前已生效 JIT，返回/抛出明确错误。
- 显式 utility 未知：fail fast，不静默当作普通 class。
- 普通兼容 class 未在 whitelist：由普通 CSS selector 处理，不属于 JIT 错误。
- 超过 token 数量、长度或 active-token 上限：沿用现有限制，拒绝本次更新。
- chart adapter 输入无效：不挂载 partial semantic tree，返回包含阶段和原因的错误。
- 不提供“加载默认 catalog 失败后自动禁用 JIT”的 fallback。

## 线程模型

- `Box`、Element tree、JIT active-token cache 和 ChartFrame 默认单线程访问。
- 内嵌 `UtilityCatalog` 初始化后不可变，可被多个 Box 安全共享。
- 不在 JIT 扫描或 stylesheet 替换期间跨线程修改 Element class。
- 若未来需要后台生成 stylesheet，必须先以不可变 token snapshot 为输入，再在 UI
  线程原子提交；本次不引入该复杂度。

## 性能边界

当前 Box 在 class/tree dirty 时扫描 Element tree，复杂度为
`O(element_count + class_token_count)`。JIT 对已知 token 有缓存，stylesheet 只在
revision 变化时替换。树扫描会先去重 token，因此大量节点重复使用少量 utility
不会错误消耗 `max_tokens_per_update` 配额。

`utility_jit_benchmark` 的 2026-07-15 Windows Release 基线如下（单次运行，仅用于
同机回归趋势，不作为跨机器硬阈值）：

| 场景 | Element | active token | CSS bytes | 首次 update | attribute update | utility update |
|---|---:|---:|---:|---:|---:|---:|
| typical | 501 | 13 | 510 | 15.266 ms | 11.558 ms | 14.711 ms |
| large | 5001 | 13 | 510 | 130.000 ms | 123.477 ms | 141.186 ms |

数据表明更新时间目前随 Element 数近似线性增长；尚无证据表明扫描占总 update 的
20% 以上，因此不引入每 Element token diff 或引用计数。复验命令：
`cmake --build --preset win-release-user --target utility_jit_benchmark &&
build\\Msvc-Release\\bin\\utility_jit_benchmark.exe`。

## 候选方案比较

### 方案 A：每个应用或 chart 自行调用 `enable_utility_jit()`

不采用。初始化重复、容易漏接，chart 模块会各自决定 catalog/theme 来源，无法形成
仓库默认行为。

### 方案 B：默认 JIT 从源码路径读取 JSON

不采用。安装包、不同工作目录和嵌入式消费者无法保证存在 `tools/`，属于部署相关的
隐式失败。

### 方案 C：进程级全局可变 StyleManager

不采用。它会让 Box 依赖隐式共享状态，并使测试、theme 切换和多窗口隔离复杂化。
仅共享不可变 catalog asset；所有 active styling 状态仍归 Box。

### 方案 D：让所有 chart 几何都变成 Element + utility

不采用。大量数据点会膨胀 Element tree，动态坐标和颜色会制造无界 token，且会把
renderer 职责错误地推给 CSS。

### 方案 E：立即删除旧 `flex::Group` chart API

不采用。它会造成公开 API 破坏，并把架构迁移与 parser/renderer 行为回归绑定在同一
次改动中。采用 adapter 和 deprecation 分阶段迁移。

## 兼容性影响

### 公开行为

`Box(renderer)` 的默认行为发生变化。元素若已有 `flex`、`hidden`、`grid` 等已知
utility class，但过去没有显式 CSS，现在会获得对应样式。这是目标行为，但需要通过
现有测试和视觉回归验证。

应用在 JIT 之后加载的 CSS 继续具有相同或更高 source order。显式 inline/custom
property 行为保持不变。

### 公开接口

- `Box` 原单参数构造仍可源码兼容。
- 新增 `BoxOptions` 和显式 utility API。
- 现有 `enable_utility_jit()` 保留，用于替换默认 catalog 或测试自定义 catalog。
- chart 新增 Box-owned adapter；旧 API 暂不移除。

### 构建与部署

- `flexUI` 静态库内嵌 whitelist 和默认 theme，二进制体积增加。
- 不增加运行时文件依赖。
- 不增加第三方依赖。

## 迁移与回滚

迁移按 [实施计划](TAILWIND_JIT_DEFAULT_PLAN.md) 分阶段完成。每一阶段都必须保持
可构建、可测试，不能暴露半实现公开 API。

局部兼容回滚使用 `BoxOptions::legacy_without_jit()`，只应用于明确的未迁移调用点。
全局回滚应恢复 `BoxOptions` 的默认值并保留已经完成的 catalog/theme 内嵌能力；不通过
运行时异常捕获自动切回无 JIT 模式。

Chart adapter 回滚时，调用方可以暂时切回旧 `flex::Group` API。AST、数据格式和静态
SVG 输出不随 adapter 迁移，因此无需数据回滚。

## 验证范围

### 核心行为

- 默认 `Box` 的 JIT 已启用。
- 显式 legacy options 可禁用 JIT。
- 无源码目录和外部 JSON 时仍能构造默认 Box。
- catalog 只解析一次并可被多个 Box 共享。
- 已知 utility 被编译并应用到 computed style。
- 应用 CSS 可以覆盖默认 utility。
- light/dark/system theme 正确响应 MediaEnvironment 和 `data-theme`。
- 普通语义 class 不产生 missing-utility 噪声。
- 显式未知 utility fail fast。
- `data-state` 变化不触发不必要的 active-token revision。

### Charts

- DotGraph、FlexChart 和 Infographic 创建 Box-owned semantic tree。
- title、legend、tooltip 的 computed style 来自 JIT/theme。
- plot geometry 与迁移前输出等价。
- parse/render 错误不暴露 partial tree。
- light/dark 和常见交互状态有 render-semantic 或视觉 golden 覆盖。

### 回归

- `test_utility_jit`
- `test_style_engine`
- `test_shadcn_ir_fixtures`
- `test_shadcn_conformance`
- `test_render_semantics`
- DotGraph、FlexChart、Infographic 相邻测试
- registry utility coverage 脚本
- 代表性示例 smoke run

## 设计模式说明

- `Box` 继续作为 styling/layout/render 子系统的 Facade。
- `ChartFrame` 使用组合表达公共 UI shell，不建立多层 chart 继承体系。
- 各 chart 的 Box-owned renderer 是遗留领域模型到 Element tree 的 Adapter。
- Element tree 继续使用 Composite；Widget/renderer 通过既有桥接独立演化。
- 默认 catalog/theme 通过构造配置注入，不使用全局可变服务定位器。

## 相关资料

- [flexUI 架构](ARCHITECTURE.md)
- [Widget 开发指南](WIDGET_DEVELOPMENT.md)
- [Shadcn conformance](SHADCN_CONFORMANCE.md)
- [CSS 支持矩阵](CSS_SUPPORT_MATRIX.md)
- [Utility whitelist](../../tools/shadcn-ir/schema/utility_whitelist.json)
- [Chart component IR](../../tools/shadcn-ir/samples/chart.component.json)
- [Chart style IR](../../tools/shadcn-ir/samples/chart.style.json)
- [Chart bridge IR](../../tools/shadcn-ir/samples/chart.bridge.json)
- [DotGraph flexUI adapter](../../charts/dotgraph/src/flexui_dotgraph.cpp)
- [FlexChart component](../../charts/flexchart/src/chart_component.cpp)
- [Infographic component](../../charts/infographic/src/infographic_component.cpp)
