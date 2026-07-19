# flexUI 架构

flexUI 的目标是用 CSS 描述界面状态，再把同一棵 Element 树投射到不同渲染后端。
当前分层只保留一个主事实源：Element 树和它的 computed style。

## 分层

1. Element tree
   管理节点、属性、伪类、widget 绑定和事件命中所需状态。它是 UI 状态的入口。
   Widget 的可独立样式化视觉部件也必须是这棵树中的真实 Element，不维护平行的私有视觉树。

2. StyleEngine
   解析 CSS，并把选择器、媒体条件、容器查询和动画声明计算成 `ComputedStyle`。

2.1 Utility JIT 与默认 theme
   `Box(renderer)` 会在内建 widget 规则之后加载内嵌 theme，并预留默认 Utility JIT
   stylesheet slot。utility 定义的唯一事实源是
   `tools/shadcn-ir/schema/utility_whitelist.json`；构建时将其内嵌，运行时多个 Box
   共享同一个不可变 `UtilityCatalog`，但 active token、revision 和 stylesheet 仍由
   各 Box 独立拥有。应用随后加载的 CSS 位于 JIT slot 之后，可以正常覆盖 utility。
   兼容调用点必须显式使用 `BoxOptions::legacy_without_jit()`，默认构造不读取源码树
   或当前工作目录。

2.2 C++ binding runtime
   管理类型化输入与 Element target 的单向投影。数值/布尔表达式在绑定创建时编译为
   MIR program，输入版本变化后才重新求值；class、attribute、text 和 inline custom
   property 的最终状态仍写回 Element 树。每个 target 在绑定期间只有一个 owner，解除绑定时
   恢复创建绑定前的值；仍被引用的输入不可删除。可编辑文本控件通过窄接口
   `TextValueWidget` 适配真实 value model，用户编辑回写同一个 `UiDataContext`，不占用应用
   callback。binding runtime 不持有 CSS 或渲染状态。

2.3 Keyed collection reconciliation
   `UiKeyedRepeater` 按应用稳定 key 显式协调一个容器的直接子 Element。它只负责创建、复用、
   排序和退役节点，不拥有业务集合、CSS 或每帧更新。条目仍是普通 Element/Widget 子树，class、
   attribute、事件和 binding 继续走同一矩形树。

3. ViewPipeline
   调度一帧更新的生命周期：style -> layout -> semantics -> positioning -> render。
   它只通过 `ViewPipelineHost` 请求阶段执行，不依赖具体的 `Box` 实现；
   它只表达阶段顺序，不拥有 Element、CSS、layout 或 backend 状态。

4. LayoutManager
   把 `ComputedStyle` 同步成布局输入，执行 flex/layout，并把布局结果写回 Element。

5. RenderManager
   接收 `RenderFrame`，拥有 backend frame lifecycle，读取布局后的 Element 树和
   `ComputedStyle`，生成 `RenderCommandList` 并回放到后端。

6. Renderer backend
   实现实际绘制目标，例如 2D、3D 或 terminal。backend 不重新解释 CSS 语义。

## 约束

- CSS 语义只在 StyleEngine 和 LayoutManager 层落地。
- `UiKeyedRepeater` 独占其容器的直接子节点顺序；应用不得绕过 repeater 增删或重排这些节点。
  集合变化时由应用显式调用 `reconcile()`，不得在每帧无条件扫描。退役节点保留稳定身份，但会清除
  focus、capture、交互伪状态、transition 和 animation；历史 key 数量受容量上限约束。
- Widget 通过 `build_semantic_tree()` / `create_part()` 创建稳定的内部部件；部件使用
  `[part=name]` 标识，并参与普通 selector、cascade、布局、查询和 RenderManager。
- 内部部件的结构所有权属于 Widget。应用可以用 `Box::query_selector()` /
  `query_selector_all()` 查询并修改其 class、attribute 或 inline style，但不能重新挂载、删除或清空部件。
- Widget-owned 部件命中 pointer event 时，EventDispatcher 把交互路由回宿主 Widget；部件默认使用
  `pointer-events: none`，即使 CSS 显式启用 pointer events，也不改变 Widget 的状态所有权。
- Widget 状态是动态几何和语义属性的主事实源。布局后的 `semantics` 阶段把状态同步到部件几何；
  RenderManager 只读取结果，渲染阶段不得再修改 Element 布局或脏标记。
- 新 Widget 的稳定视觉部件优先使用普通 CSS 属性，例如 `background-color`、`border`、`width` 和
  `height`。既有 `--widget-*` 变量只作为兼容 fallback，不应成为新部件唯一的样式入口。
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
- 滚动内容同样遵循这条规则：CSS overflow scroll 和 `ScrollViewWidget::begin_scroll()`
  都必须同时推进 backend translate 和 command-list prefix。
- `RenderCommandList` 只承载已解析的低层绘制与状态命令，例如 frame、transform、
  clip、path、rect、line、circle、ellipse、text、image、svg、shadow 和 blur；CSS 选择器、动画采样、
  布局判定和 backend capability 分支仍属于上层语义。
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
- 新增后端时优先补 backend 能力和绘制映射，不复制 CSS/layout 逻辑。
- 测试可以停在 layout 阶段；这不是 fallback，而是无 backend 的确定性管线截断。
