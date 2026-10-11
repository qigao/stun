# FlexUI XML、CSS 与 TurboScript 架构决策

状态：已采用
日期：2026-08-31

## 2026-09-20 架构收敛补充

以下决策覆盖本文中仍把旧实现描述为长期目标的段落；当前代码尚未全部完成这些迁移，因此应区分“目标契约”与“当前实现事实”。

### Text / Unicode

- XML、CSS、TBS 和运行时文本的公开字符串编码统一为严格 UTF-8。
- UTF-8 准入、Unicode scalar 与版本化 Unicode property/boundary 数据由 Salts 字符串层 / `Salts::Unicode` 拥有；FlexUI 不维护第二套 decoder 或 Unicode 数据表。
- 原始合法 UTF-8 bytes 保真，不隐式执行 NFC/NFD normalization。
- source span / storage offset 使用 byte offset；用户可见 cursor/selection 不得切开 extended grapheme cluster。
- grapheme、word、line-break、bidi 基础能力由独立 Unicode 模块提供；CSS text layout、font shaping、glyph measurement 与 GPU font resource 不属于 Unicode 模块。
- 当前 `flexUI/src/text_util.cpp` 与 `text_layout.cpp` 中的私有 UTF-8、emoji range、byte truncation/wrap 逻辑属于待迁移实现，不是长期契约。跟踪：`stun#8`、`salts-utils#101`。

### XML typed schema

XML 不再把所有 element 长期视为同一种无限制节点。Compiler/registry 目标上必须区分：

- structural/container；
- leaf/content widget；
- composite widget；
- text；
- reusable component。

每个 descriptor 定义 content model、properties、events、bindings 以及可选 widget factory。非法 children/property/event/binding 应在 XML compile 阶段带 source location 拒绝，而不是延后到 paint/runtime。

Text-capable widget 目标上支持自然内容，例如：

```xml
<label>保存</label>
<button on:click="save">Save</button>
```

`id` 只在需要 application-visible durable identity 时强制；匿名结构节点仍有 compiler-owned identity。对脚本公开的 `UiHandle` 仍依赖稳定公开 ID + generation，不把匿名内部 identity 暴露为持久 ABI。跟踪：`stun#9`。

### Tailwind compatibility

- 用户 authoring surface 使用标准 `class="..."`，不新增平行 `tw=` 语法。
- 长期目标是固定一个上游 Tailwind compatibility profile，并实现 native token/variant/utility/theme compiler；Node/npm/Tailwind JS 仅可用于开发期 differential oracle，不进入安装运行时。
- state、responsive、dark、data/aria、container query、arbitrary value/property 等语义按兼容 profile 明确支持或明确 Unsupported；不得悄悄解释为另一套本地语义。
- ordinary semantic class 与 Tailwind candidate 共存，最终都进入同一个 StyleEngine cascade。
- 当前 `utility_whitelist.json` / `UtilityCatalog` / exact-token JIT 是正在工作的迁移实现事实，不再作为长期 Tailwind 语义事实源。跟踪：`stun#10`。

## 背景

FlexUI 需要形成类似 Qt `.ui` + QtScript 的桌面应用开发模型，但不引入浏览器 DOM、JavaScript
运行时或 npm 生态。现有 `.flex` DSL 同时承载 UI、动画、状态机、资源和表达式，导致文档结构、
交互语言与动画运行时的职责重叠，也让 `Flex::Compiler` 成为 FlexUI 的构建依赖。

## 决策

- XML 是 UI 结构与静态属性的唯一源格式。
- CSS 与 Tailwind-like utility 是视觉、状态样式、transition 和 keyframes 的唯一源格式。
- TurboScript（TBS）是交互、应用状态和命令编排的唯一可执行脚本语言；TBS 不创建或遍历一个
  通用 DOM，也不逐帧驱动动画。
- Flex 的原生 Timeline、Track、Curve、显式三次贝塞尔、Physics/Box2D 和数值 MIR 保留为运行时
  能力；它们不属于要删除的 Flex DSL。
- `.flex` parser、lexer、AST、binary DSL format 和 `Flex::Compiler` 在迁移完成后删除。
- XML、CSS 和 TBS 都先编译为不可变程序，再在 UI owner thread 上实例化或执行。

```mermaid
flowchart LR
    XML[main.ui.xml] --> XP[XML adapter]
    CSS[theme.css] --> CP[CSS / utility compiler]
    TBS[controller.tbs] --> TS[TurboScript module]

    XP --> UIIR[CompiledUiProgram]
    CP --> StyleIR[CompiledStyleProgram]
    CP --> AIR[Native Timeline / Curve IR]
    UIIR --> Box[FlexUI Box + typed widgets]
    StyleIR --> Box
    AIR --> Runtime[Flex animation / physics runtime]
    TS --> Controller[FlexUI Controller ABI]
    Controller --> Mutations[Transactional UI mutations]
    Controller --> Commands[Typed application commands]
    Mutations --> Box
    Commands --> Host[C++ application services]
```

## 职责与状态归属

| 层 | 主事实源 | 可变性 | 错误边界 |
|---|---|---|---|
| XML adapter | XML source | load/reload 时可替换 | 语法、schema、source location |
| `CompiledUiProgram` | UI definition、事件和 binding 表 | 不可变、可共享 | 语义、资源上限、重复 ID |
| CSS compiler | stylesheet source | load/reload 时可替换 | selector、property、animation 引用 |
| Box/widget tree | 已实例化 UI 状态 | 仅 UI owner thread 修改 | mutation commit |
| TurboScript controller | controller locals | 仅 controller owner thread 修改 | ABI、limits、handler export |
| Timeline/Physics | 动画与物理状态 | runtime owner thread 修改 | 非法参数、缺失 target、资源上限 |

TBS 的一次事件返回一个不可变 effect envelope。宿主先完整校验 `mutations` 与 `commands`，再提交；
校验失败时 Box 与应用服务都不得观察到部分结果。动画的长期状态由原生 runtime 持有，TBS 只发送
类型化的 `play`、`pause`、`seek`、`set_parameter` 等命令。

## 编译与装载顺序

```mermaid
sequenceDiagram
    participant App as Application
    participant XML as XML adapter
    participant CSS as CSS compiler
    participant TBS as TurboScript adapter
    participant Runtime as FlexUI runtime

    App->>XML: parse + schema validate
    XML->>XML: compile CompiledUiProgram
    App->>CSS: compile style/animation programs
    App->>TBS: compile module + resolve exports
    App->>Runtime: validate cross references
    Runtime->>Runtime: instantiate detached widget tree
    Runtime->>Runtime: atomically publish application instance
```

任一阶段失败都立即返回结构化错误。旧实例在 reload 成功提交前继续作为唯一活动实例；不进行静默
降级，也不混用新旧文档的部分产物。

## XML 契约

XML adapter 只负责格式适配，不持有业务状态。元素名称通过 `WidgetRegistry` 映射到具体 widget
factory；未知 tag、重复 ID、重复属性和不合法类型均为 load error。通用属性映射到
`UiNodeDefinition::properties`，事件使用 `on:*`，binding 使用 `bind:*`，命名空间在 adapter 边界
转换为现有 canonical key（如 `on.click`、`bind.text`）。资源使用独立、受限的 `<resources>` 区域，
最终产生 `UiResourceDefinition`。

XML 解析采用仓库 vcpkg manifest 已声明的 pugixml，并通过其 CMake imported target 隔离在 adapter
实现内；公共头不暴露 pugixml 类型。上游接口与解析行为参考
[pugixml manual](https://pugixml.org/docs/manual.html)。

`WidgetRegistry` 是实例化的唯一工厂事实源。内置 widget 在启动时显式注册；插件只能通过版本化
宿主接口注册 factory，不得把第三方类型或 native pointer 写入 XML IR。

当前 `WidgetRegistry::builtins()` 显式注册：

- 结构元素：`div`、`span`、`main`、`section`、`article`、`header`、`footer`、`nav`、
  `aside`、`form`、`fieldset`、`legend`、`p`、`a`、`ul`、`ol`、`li`、`window`、`view`、
  `box`、`content`。
- 基础控件：`button`、`label`、`input`、`checkbox`、`radio`、`switch`、`slider`、
  `progress`、`textarea`、`select`、`image`。
- 组合控件：`tooltip`、`modal`、`tabs`、`badge`、`spinner`、`divider`、`toast`、
  `dropdown`、`accordion`、`calendar`、`table`、`tree`、`colorpicker`、`group-button`、
  `toggle-group`、`avatar`、`card`、`breadcrumb`、`pagination`、`stepper`、
  `gradient-editor`、`markdown`。

基础控件 factory 消费已类型化的常用属性，例如 `text`、`value`、`placeholder`、`disabled`、
`checked`、`min/max/step`。类型不匹配返回 `WidgetFactoryFailed`。Registry 通过实例化入口显式注入；
不带 Registry 的旧入口继续创建通用 `Element`，只用于迁移兼容。

## 表达式策略

短期保留现有数值 MIR 作为 CSS animation、binding 与原生 runtime 的编译后执行后端。这不是保留
Flex DSL：用户不再编写 `.flex` 文档，MIR 也不负责 UI 结构或事件编排。

如果后续要求所有表达式都使用 TBS 语法，应在 TurboScript 仓库提供受限的 pure-expression profile：
禁止 I/O、宿主调用、分配和循环，编译一次后通过类型化输入槽执行。该能力可替换表达式前端，但不应
让完整 TBS VM 进入每帧逐属性热路径。在 TurboScript 提供并通过数值一致性与 benchmark 验证前，
不得假定该 API 已存在。

## 兼容性与迁移

迁移期间采用双前端、单 IR：旧 `.flex` parser 与 XML adapter 都只能输出同一个
`UiDocumentDefinition`/`CompiledUiProgram`，后续实例化、binding、事件和测试完全共享。双前端是有
移除期限的迁移机制，不是长期公开能力。

删除旧 DSL 的门槛：

1. FlexUI、FlexPlayer、examples、FlexChart 与工具中的 `.flex` 消费者均已迁移或明确移除。
2. XML 对 UI tree、属性、事件、binding 和资源具备等价测试。
3. CSS/native runtime 对仍需保留的 animation、curve、physics 行为具备回归测试。
4. 安装树不再导出 `Flex::Compiler`、`Flex::DSL` 或 DSL headers。
5. 全仓库 configure/build/test 和 install-tree consumer 验证通过。

回滚以 frontend feature 为单位：XML 成为默认后，旧 parser 可在一个发布周期内由显式兼容选项启用；
它仍输出相同 IR，不维护第二套 runtime。删除 release 之后的回滚只能恢复独立的兼容分支或版本，
不能在主线重新引入双事实源。

## 当前实现与风险

- `事实`：`DesktopApplicationBuilder::xml_entry()` 已是默认入口，并默认注入
  `WidgetRegistry::builtins()`；TurboScript 通过 `ScriptModuleFactory` 注入，因此 Core 公共边界不暴露
  TurboScript 类型。候选只有在 strict CSS、typed widget、required export 与 mount 全部成功后发布。
- `事实`：`DesktopApplication::reload()` 在 owner thread 构建完整候选并一次交换；失败保持活动
  Box/program/controller，跨线程调用返回 `WrongThread`。
- `事实`：`DesktopApplication::dispatch_event()` 已将 hit-test、widget consumption、native callback、
  target→ancestor bubbling 与 controller 串成同步 owner-thread 链路。被 widget 消费的事件默认不进入
  脚本；成功 click 快照同时包含原始 `target` 与绑定节点 `current_target`。
- `HIGH`（事实）：多个工具、示例和模块仍直接消费 `.flex`；立即删除 parser 会破坏公开构建与示例。
- `MED`（事实）：当前 XML event schema 尚未提供“即使 widget 已消费也发送只读通知”的显式选项；
  focus/capture 的完整组合矩阵也仍待补齐，因此 consumed event 统一 fail-closed 为不进入脚本。
- `MED`（事实）：DesktopHost/native window、plugin service 与 application package loader 尚未进入
  当前 Facade；现有实现验证的是 headless application candidate 事务。
- `MED`（推论）：完整 TBS VM 若逐帧求每个属性，会增加调用、分配与 ABI 成本。依据是当前架构的
  per-frame native Timeline 热路径；需以 benchmark 决定是否增加 pure-expression profile。
- `LOW`（常用做法）：XML/CSS/TBS 的职责分离更接近成熟桌面 UI 工具链，但本决策的主要依据仍是
  仓库现有 IR、runtime 与 controller 边界，而不是形式相似性。
