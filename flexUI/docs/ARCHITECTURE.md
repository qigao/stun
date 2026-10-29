# flexUI 架构

flexUI 的目标是 **XML 描述结构、原生 Tailwind 描述样式、TurboScript 描述行为**。
同一棵 Element 树与原生样式结果服务布局、文字、交互和后端绘制。

2026-10-29 的 [Tailwind 与多后端效果契约](TAILWIND_NATIVE_BACKENDS_DESIGN.md)
是样式与 shader 设计的当前依据：目标删除 CSS 输入、生成 CSS 和 CSS fallback；
按显式 D2D、gCanvas GL/GLES/Vulkan 或 Tango 目标验证效果、参数域和资源。
这次更新是设计变更，下面标为“当前实现”的 CSS 路径尚未完成迁移。

## 分层

1. Element tree
   管理节点、属性、伪类、widget 绑定和事件命中所需状态。它是 UI 状态的入口。
   Widget 的可独立样式化视觉部件也必须是这棵树中的真实 Element，不维护平行的私有视觉树。

2. 原生样式求值
   目标直接消费不可变 `TailwindProgram`，把 utility 条件、主题、继承、覆盖顺序和
   动画声明计算成统一样式结果。现有 `ComputedStyle` 和属性 registry 可以继续复用。
   当前实现仍由 StyleEngine 解析 CSS；该前端、stylesheet API 和文本记录是迁移删除项。

2.1 当前 Utility JIT 与默认 theme 实现（待迁移）
   Tailwind-like 编译能力由独立的 `FlexUI::TailwindCSS` 静态模块拥有，源码位于
   `flexUI/modules/tailwindcss/`，公开入口为 `flexUI/tailwindcss.h`。该模块只依赖
   nlohmann-json，负责 catalog 校验、token → CSS、缓存、revision 和资源上限，不读取或
   修改 Element 树。`flexUI/utility_jit.h` 是源码兼容头；旧的
   `shadcn_ir::emit_utility_css` / `missing_utility_tokens` 是委托给该模块的兼容包装。

   `Box(renderer)` 会在内建 widget 规则之后加载内嵌 theme，并预留默认 Utility JIT
   stylesheet slot。utility 定义的唯一事实源是
   `tools/shadcn-ir/schema/utility_whitelist.json`；构建时将其内嵌，运行时多个 Box
   共享同一个不可变 `UtilityCatalog`，但 active token、revision 和 stylesheet 仍由
   各 Box 独立拥有。当前 app CSS override 和 `BoxOptions::legacy_without_jit()`
   只描述既有实现，不是目标作者契约。目标使用标准 class、类型化 Theme/Preset 和
   有界规则缓存，删除 CSS 中转及关闭 JIT 后退回 CSS 的入口。

2.2 C++ binding runtime
   管理类型化输入与 Element target 的单向投影。数值/布尔表达式在绑定创建时编译为
   MIR program，输入版本变化后才重新求值；class、attribute、text 和现有 custom
   property 的最终状态仍写回 Element 树。CSS custom-property 入口需迁移为类型化
   theme/property/effect 参数，不删除对应动态行为。每个 target 在绑定期间只有一个 owner，解除绑定时
   恢复创建绑定前的值；仍被引用的输入不可删除。可编辑文本控件通过窄接口
   `TextValueWidget` 适配真实 value model，用户编辑回写同一个 `UiDataContext`，不占用应用
   callback。binding runtime 不持有后端程序或设备资源。

2.3 Keyed collection reconciliation
   `UiKeyedRepeater` 按应用稳定 key 显式协调一个容器的直接子 Element。它只负责创建、复用、
   排序和退役节点，不拥有业务集合、样式程序或每帧更新。条目仍是普通 Element/Widget 子树，class、
   attribute、事件和 binding 继续走同一矩形树。

2.4 CMeta 属性类型
   Flex 的 `flex/core/cmeta_types.h` 定义 Color、Vec2、String 的语义身份；FlexUI 属性 registry
   通过 `cmeta_type_equal()` 比较语义身份，再由 `style_value_type_matches()` 校验 native
   kind/size/alignment。描述符地址不是跨翻译单元的类型身份，语义相等不代表存储布局兼容。
   属性读写、compound write preflight、typed transition/animation 的准入与查询共享这一规则。
   非法布局不会覆盖已有轨道。轨道借用 property/type metadata，直至移除；复制值不延长这些
   metadata 的生命周期。公开这些头文件的 target 显式
   PUBLIC 链接 `Salts::CMeta`。
   描述符只提供类型信息；`AnimValue`、`TransitionValue` 继续拥有值存储，FlexUI 继续拥有
   原生样式求值、插值与 dirty impact。元数据不授予对象构造、写入、销毁或模块保留权限。

3. ViewPipeline
   调度一帧更新的生命周期：style -> layout -> semantics -> positioning -> render。
   它只通过 `ViewPipelineHost` 请求阶段执行，不依赖具体的 `Box` 实现；
   它只表达阶段顺序，不拥有 Element、样式程序、layout 或 backend 状态。

4. LayoutManager
   把 `ComputedStyle` 同步成布局输入，执行 flex/layout，并把布局结果写回 Element。

5. RenderManager
   接收 `RenderFrame`，拥有 backend frame lifecycle，读取布局后的 Element 树和
   `ComputedStyle`，生成 `RenderCommandList` 并回放到后端。

5.1 目标效果准入与计划
   原生效果用版本化 EffectId 和类型化参数表达；后端分别绑定 native draw、effect graph、
   shader program 或 terminal cell 操作。准备阶段检查所有对目标可达的状态/动画需求、
   参数域、格式和预算，并在发布前准备资源。现有布尔 RendererCapabilities 还不足以
   表达该契约，不能把这些目标接口写成已实现。

6. Renderer backend（目标契约；现有适配器待迁移）
   实现已准入的绘制目标，不重新解释 Tailwind。D2D、GL、GLES、Vulkan 的程序与设备
   profile 分别验证；Tango 没有 GPU shader。资源属于 backend owner，按 device/frame
   generation 和在途提交完成回收。失败不换后端、不忽略效果。

## 约束

- Tailwind 属性/条件语义由统一的原生求值与 LayoutManager 落地；目标不保留 CSS 前端。
- 当前 `FlexUI::TailwindCSS` 不依赖 FlexUI Core；现有依赖方向为
  `FlexUI::Core → FlexUI::TailwindCSS → nlohmann-json`。Element 扫描、stylesheet slot
  和 cascade 状态仍归 `Box` / `StyleEngine` 所有。目标替换为原生程序发布，保持编译器
  不拥有 Element/设备资源的边界；不以旧模块名承诺 CSS 兼容接口。
- `UiKeyedRepeater` 独占其容器的直接子节点顺序；应用不得绕过 repeater 增删或重排这些节点。
  集合变化时由应用显式调用 `reconcile()`，不得在每帧无条件扫描。退役节点保留稳定身份，但会清除
  focus、capture、交互伪状态、transition 和 animation；历史 key 数量受容量上限约束。
- Widget 通过 `build_semantic_tree()` / `create_part()` 创建稳定的内部部件；部件使用
  part 身份标识，并参与统一条件、样式、布局、查询和 RenderManager。
- 内部部件的结构所有权属于 Widget。目标通过 ID/part/类型化查询修改受支持的 class、
  attribute 和类型化参数，不能重新挂载、删除或清空部件。当前 `query_selector()` /
  `query_selector_all()` 和 inline CSS 的消费者需迁移，不作为新的作者接口。
- Widget-owned 部件命中 pointer event 时，EventDispatcher 把交互路由回宿主 Widget；部件默认使用
  禁止 pointer 命中；显式启用已支持的 pointer-events utility 也不改变 Widget 的状态所有权。
- Widget 状态是动态几何和语义属性的主事实源。布局后的 `semantics` 阶段把状态同步到部件几何；
  RenderManager 只读取结果，渲染阶段不得再修改 Element 布局或脏标记。
- 新 Widget 的稳定视觉部件按目标使用 Tailwind Preset、Theme token 和类型化动态参数。
  既有 `--widget-*` 的功能必须迁移，不能保留为 CSS fallback 或直接删除其行为。
- 样式基线、容器查询宽度等跨帧 UI 状态归属 `Box`，不放入进程级静态 registry。
- RenderManager 不修改布局和样式状态。
- Box 和 ViewPipeline 不直接 begin/end backend frame。
- Box 只在实际传入 backend 时创建 Renderer/RenderManager；layout-only 更新停在 pipeline 的
  layout 边界，不持有包着空 backend 的渲染适配器。
- 一帧渲染输入通过 `RenderFrame` 表达，不把 root、viewport、clear color 分散传递。
- RenderManager 每次树渲染只采样一次 backend capabilities，并用它创建本次递归内的
  `RenderCommandList`。
- 后端无关绘制操作通过 `RenderCommandList` 表达，再由 Renderer 适配到实际 backend。
- `RenderCommandList` 必须携带 renderer capabilities 构造；widget 只接收上层传入的命令列表，
  不自行创建无上下文命令列表。
- 嵌套复用 widget 发命令时，调用方必须同时维护 backend transform 命令和
  command-list transform prefix。直接 draw 命令依赖 transform stack；
  Group/Shape 的 `set_transform` 命令依赖 prefix 组合。
- 滚动内容同样遵循这条规则：overflow scroll 语义和 `ScrollViewWidget::begin_scroll()`
  都必须同时推进 backend translate 和 command-list prefix。
- `RenderCommandList` 只承载已解析的低层绘制与状态命令，例如 frame、transform、
  clip、path、rect、line、circle、ellipse、text、image、svg、shadow 和 blur；utility 条件、动画采样、
  布局判定和 backend capability 准入仍属于上层语义。
- 共享文本布局只生成命令：调用方使用 `emit_segmented_text_line` 和 `emit_text_block`
  写入 `RenderCommandList`，不再通过 Renderer 直绘包装函数绕过命令层。
- 当前迁移采用增量方式：frame lifecycle、元素 transform/opacity/overflow clip、
  filter/backdrop blur 状态、实体背景填充、伪元素 rect、普通文本块、共享分段文本、
  shadow、复杂背景、border/ring/outline，以及 Image/Badge/Panel/ListView/Sidebar
  /Divider/Tooltip/Toast/Select/Dropdown/Accordion/Table/Tree/Calendar/DatePicker/Spinner/Tabs/Splitter
  /ScrollView/VirtualizedList/Notification/Toolbar/Menu/SearchBox/Dialog/Popover/TimePicker/Modal
  /ColorPicker/GradientEditor/Input/TextArea 等 widget 绘制已经通过
  `RenderCommandList` 回放；Group/Shape 组合层也只生成 command list，再由 widget 统一
  replay 到 Renderer。
- Renderer backend 只消费绘制命令，不拥有 UI 状态。
- 新增后端时补齐精确的能力域、效果实现和资源契约，不复制 Tailwind/layout 逻辑。
- 测试可以停在 layout 阶段；这不是 fallback，而是无 backend 的确定性管线截断。

