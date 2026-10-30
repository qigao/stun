# FlexUI XML 模块化 Schema 审查：React / Qt .ui / QML

- 评审：2026-10-30。状态：**设计提案，未实现/未验收**。归属：[#16 XML Schema](https://github.com/qigao/stun/issues/16)；产品标准：[#1](https://github.com/qigao/stun/issues/1)；模块生命周期：[#275](https://github.com/qigao/stun/issues/275) / [#298](https://github.com/qigao/stun/issues/298)；Native Tailwind：[#17](https://github.com/qigao/stun/issues/17)。
- 审查基线：[`master@375d3373`](https://github.com/qigao/stun/tree/375d3373fe73b34fc17421404ae820272553c7af)。本文件属于 Draft PR #333，仅定义提议的 Schema/设计和验收，不修改生产 C++、C11、XML parser 或 UI ABI。
- 统一目标：**FlexUI = XML + Native Tailwind + TurboScript**，达到“XML 完整描述实际 Studio 应用 UI、不需应用专有 C++ 拼装界面”的 P0 标准。不是浏览器兼容工程；不引入 CSS/JS runtime 和第二 UI 树。

## 1. 当前源码事实与缺口

| 文件 | 已存在 | 尚未形成的声明式契约 |
| --- | --- | --- |
| [`ui_xml.cpp`](../src/ui_xml.cpp) | pugixml/UTF-8 admission；`<ui name>` 恰好一个根，最多一个 `<resources>`；`on:`/`bind:` 前缀及字符串、bool、float 属性；不可变编译结果 | 模块 import、组件公开接口、组件实例、children/slot、混合文本；有意义文本+元素混合仍报错 |
| [`ui_node_schema.h`](../include/flexUI/ui_node_schema.h) | `UiNodeKind {Container,Widget}`；Empty/Text/SingleChild/Children/TextAndChildren 简单内容检查 | 完整属性、允许子节点类型、Widget 参数/event/binding/slot contract |
| [`widget_registry.cpp`](../src/widget_registry.cpp) | 23 个普通结构 Element 和 33 个原生 Widget；严格 tag 查找、注册工厂 | 父子语义装配：现有 Tabs/Tree/Table/Accordion 的 `add_tab/add_node/add_row/add_section` C++ API 不等于 XML 自动创建这些模型 |
| [`ui_document.h`](../include/flexUI/ui_document.h) 与 [`ui_document.cpp`](../src/ui_document.cpp) | `UiScopedComponent` + immutable merged `CompiledUiProgram`，局部 ID/ARIA 引用修订，单一 Box owner，输入别名 | 仍须 C++ Builder 指定 scope、base host、component program；无法从 XML 直接导入组件、传 typed props/slots；组件只能插入基础文档结构 host；组件资源受限 |
| [`keyed_repeater.h`](../include/flexUI/keyed_repeater.h) | C++ 稳定 Key reconcile、明确预算 | 尚无 XML `repeat`/Items/ItemTemplate；物理销毁受 #298 生命周期门禁 |
| [Designer 导出器](../../flexui_designer/src/template_generator.cpp) | XML 导出 | `<ui><widget type=...><position/><size/>` 与 production `<ui name=...><registered-tag .../></ui>` 不同文法 |

**结论**：保留现有 XML→CompiledUiProgram→Element/Box 路线，补足**module/component public API、child-role、data template**，不要为 Flex/Grid/Stack 增加布局 Widget 或新 UI Runtime。

## 2. 主流框架借鉴矩阵

| 来源 | 应吸收 | 不采用 |
| --- | --- | --- |
| [React Props/Children](https://react.dev/learn/passing-props-to-a-component) | 参数只读、组合复用、children 允许调用方提供内容、组件私有实现 | JSX、JavaScript 函数 props、React runtime、Virtual DOM |
| [React keys](https://react.dev/learn/rendering-lists) | 重复数据子项稳定且兄弟内唯一 Key；可在重排中保存焦点/状态 | 数组 index、随机 key、无界 retained components |
| [React state ownership](https://react.dev/learn/sharing-state-between-components) | 每项数据一个 owner、显式 props 和数据上下文；事件传变更意图 | 隐式第二套 Context/store 或未经声明的全局状态 |
| [Qt Widgets Designer .ui Schema](https://doc.qt.io/qt-6/designer-ui-file-format.html) | type/property、layout、resources、customwidgets、signal/slot、Designer 与 loader 同一格式 | 不直接照搬 `<widget class>` 和 `<layout class>` 串接结构 |
| [Qt Custom Widgets](https://doc.qt.io/qt-6/designer-using-custom-widgets.html) | Native Widget 描述与 Designer 必须来自统一注册表 | 不允许未知 tag 自动变成 div/默认占位 Widget |
| [Qt .ui compiler/QUiLoader](https://doc.qt.io/qt-6/designer-using-a-ui-file.html) | 严格编译/装载同一 UI 源，无论编译时或运行时解析 | 不把应用专有的 C++ setupUi 生成当作 FlexUI 产品目标 |
| [QML reusable types](https://doc.qt.io/qt-6/qtqml-documents-definetypes.html)、[URI modules](https://doc.qt.io/qt-6/qtqml-modules-identifiedmodules.html) | 单文件定义类型、局部 ID、公开 Property/Event、模块 URI+版本+导入隔离 | QML/JS 解释器、自动网络 import、第二套对象图 |
| [WPF DataTemplate](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/data/data-templating-overview) | ItemTemplate 将数据源与具体绘制内容分开 | 不搬入 XAML 所有 property-element 语法与 Style 机制 |

## 3. 建议的最小 Schema 词汇

- **Element**：`div` 为通用容器，Flex/Grid/Stack/position/clip/scroll 都由 `class` 和同一 native Tailwind/Layout 控制。`main`、`header`、`span` 等仅有结构/语义差异，不能有专用布局引擎。
- **Widget**：`input`、`tabs`、`table`、`tree` 等真正拥有专用交互/模型的原生对象；公开 typed props、events、bind targets、child roles、parts、capabilities。
- **Component**：XML 定义的编译期可复用类型，独立文件，公开 typed Props/Events/Slots；模板展开进入**同一个** `CompiledUiProgram`、`UiScopedComponentOwnership` 与 Box，不产生另一个 runtime `ComponentWidget`。
- **Text**：有序文本内容节点，可与元素混合；保留 source span、空白/CDATA 和 Grapheme 语义，最终投影到现有 Element/TextLayout，而非另造 parser。
- **Module**：显式 import 的 namespace URI + 组件 local name + 版本/来源身份。XML prefix 只是文件局部别名，未知 URI 必须拒绝。解析模块依赖图时验证哈希/来源、循环/版本冲突、路径逃逸、总节点/字节/深度预算。
- **Slot**：组件定义内 `<slot name>` 插入点；调用处 `<fill name>` 与未命名 default children。注意当前老 `slot="..."` 是结构占位索引，**不能悄悄改为调用处填充语义**；迁移需版本化、回归。
- **Data Template**：编译期 `<repeat>` 与 `<when>` 结构指令，实际可用应在 keyed identity、borrower/physical retirement 和 UI revision 门禁满足后；不作为新 Layout Widget，不允许任意 JavaScript eval。

## 4. 组件定义示例（目标文法：当前 master 不接受）

`components/EditorPanel.ui.xml`：

~~~xml
<component name="EditorPanel" namespace="urn:studio:ui" version="1"
           xmlns:bind="urn:flexui:binding">
  <interface>
    <prop name="title" type="string" required="true"/>
    <slot name="actions" cardinality="zero-or-more"/>
    <slot name="default" cardinality="one"/>
    <event name="requestClose" payload="none"/>
  </interface>
  <template>
    <section class="flex min-h-0 flex-col">
      <header class="flex items-center gap-2">
        <label bind:text="$props.title"/>
        <slot name="actions"/>
      </header>
      <div class="min-h-0 flex-1">
        <slot name="default"/>
      </div>
    </section>
  </template>
</component>
~~~

组件 Props 为**只读输入**；内部状态通过已有 UiDataContext/Controller 显式引用，向父发送状态变更意图使用公开的 typed event，而非修改 Props 或导出长期 Element 裸指针。组件实例 `class` 与组件默认 class 的合并遵循唯一 Tailwind 属性语义/优先级，不靠属性文本顺序模拟 CSS cascade。

## 5. 在应用 XML 中导入和使用（目标文法：当前 master 不接受）

~~~xml
<ui name="Studio"
    xmlns:studio="urn:studio:ui"
    xmlns:on="urn:flexui:event"
    xmlns:bind="urn:flexui:binding">
  <imports>
    <import namespace="urn:studio:ui"
            source="components/EditorPanel.ui.xml"
            version="1"/>
  </imports>

  <main id="app" class="flex h-full flex-col">
    <studio:EditorPanel id="editor" title="Documents">
      <fill name="actions">
        <button id="save" on:click="save_document">Save</button>
      </fill>
      <textarea id="document" bind:value="$document"/>
    </studio:EditorPanel>
  </main>
</ui>
~~~

组件调用的 title/slot/event 必须先解析到唯一类型描述，再验证必填项、类型、重名 slot、默认 children 数、handler payload、SourceSpan。传入 slot 的子元素使用**调用者词法数据/事件作用域**；组件模板内部只允许访问组件 Props 和显式声明/映射的本地数据源。内部 ID 和 ARIA/for 引用按组件作用域重写、禁止外部越权引用，支持同一组件多实例和嵌套组件。

`<imports>` 和 `<fill>` 是**编译时声明**；`<template>`、`<slot>` 不产生额外 Element，也不从不受信任的 URI 运行时拉取代码。源文件可从受信任的安装 SDK/构建 manifest 解析，原生资源/Service 单独准入。

## 6. Native Widget 的 typed child-role，而不是新布局控件

~~~xml
<tabs id="workspace">
  <tab id="source" title="Source">
    <textarea id="edit" bind:value="$document"/>
  </tab>
  <tab id="preview" title="Preview">
    <markdown id="preview" bind:text="$preview"/>
  </tab>
</tabs>
~~~

`tab` 是**只允许在 tabs 内**的 child-role/adapter，含 title/id/disabled + 恰好一份页面内容，而不是独立的通用 Widget。编译器在发布前把 `tab` 绑定到已有 TabsWidget 的 Tab model 和一个真正拥有的 Element page。类似 Tree 的 node、Table 的 column/row、Accordion 的 section 都通过**父级 descriptor**指定允许 child kinds、排序及 Cardinality。不允许 `<tab>` 在 `<div>` 下被默认解释为 div。

需要支持列表、排序和数据模板时，再在 typed Items model 上实现 `repeat items="$documents" key="item.id"`，所有 Key 在同一父集中稳定；实例应跨重新排序保留正确状态，退出要满足 #298/#302 的 callback/observer/worker join 门禁。

## 7. 单一 WidgetRegistry / CMeta Schema 设计

延伸现有 `UiNodeDescriptor`（或者通过现有 Registry 的**单一**查找入口）作为 Element、Widget、ComponentDefinition 的共同类型元数据；Component/Template 是 compile-only 描述而不是 runtime Widget。设计示例**不是当前 C++ ABI**：

~~~cpp
struct UiTypeDescriptor {
    QualifiedTypeId type;         // namespace/local/major version
    UiTagKind kind;              // Element, Widget, ComponentDefinition
    UiPropertySchema properties; // type, required, default, range
    UiChildSchema children;      // child roles, cardinality, order
    UiSlotSchema slots;          // default/named, accepted content
    UiEventSchema events;        // payload type, handler validation
    UiBindingSchema bindings;    // target kind, type, direction, owner
    UiCapabilityRequirements requirements;
    NativeFactory factory;       // only for real native Widget
};
~~~

**不要另造样式属性类型表。** Tailwind 样式属性由现有 `StylePropertyDesc`、CMeta/原生 typed evaluator 拥有；Widget 参数/child role 使用 WidgetRegistry 的同一描述符。旧 `UiDocumentValue = variant<float,string,bool>` 作为迁移输入事实保留，但目标 `CompiledUiProgram` 应携带明确来源、类型、解析阶段与错误上下文。对 Designer/IDE 补全/文档/生成 XSD 的元数据应由这一规范 Descriptor 派生，不能手写第二份独立 XSD 权威来源。

**编译顺序：**
1. 有界 UTF-8/XML 解析，规范化的受信任 Import 图和 QName/版本解析。
2. 查询同一个 Registry + CMeta 类型，验证 Props、Events、Slots、Widget child roles、Binding、资源和服务能力。
3. 有界模板展开：caller 词法 Scope、局部 ID、ARIA/IDREF、稳定 Key，构建 immutable component ownership ledger。
4. 继续沿用 `compile_ui_definition` / `CompiledUiProgram` 与单一 Box/Element tree；先准备、验完整 Candidate，后 Owner-thread 发布。
5. Reload 失败保留旧 UI；动态组件和 native worker/Widget observer 完整销毁依赖 #298/#302，没有证明就 fail closed。

## 8. 分期与 P0 验收（全部 OPEN）

| 阶段 | 唯一目标 | 必须观察的 RED/绿色证据 |
| --- | --- | --- |
| M0 | 本 Schema 提案与旧 Slot/Designer/Widget 接口审计 | 不删已有功能、不把设计当成实现 |
| M1 | 完整 typed Registry property/event/binding/child-role | 未知属性、错类型、错 child、重复/缺 Props/Slot 在 native factory 前失败，SourceSpan 可信 |
| M2 | 独立 Component 文件、Import、Props/Children/Slots、可嵌套实例 | 同一组件多实例 + caller slot binding、私有 ID、循环 import、版本/hash/预算越界，失败无部分发布 |
| M3 | Tabs/Tree/Table/Accordion 真正由 XML 装配，完成 `bind:value` | 对比现有 C++ 控件语义，真正按键/鼠标/编辑/Tab+Tree+Table 动态行为，无应用专有 UI 装配代码 |
| M4 | `repeat/when` + ItemTemplate + 可验证物理退休 | Key 重排/删改、Controller/Scope/stale generation、observer/worker join、OOM、取消/reload、Sanitizer |
| M5 | Designer 使用同一 Schema + 完整 Studio 原生 App | Designer Save→Compile→Mount→Reload→Export→Compile；独立 SDK consumer；真实 GUI 窗口、TTY PTY/ConPTY，精确 HEAD 和错误 RED oracle |

**验收真值**：UI 的布局、Tree、Table、Tab、弹窗、事件、数据源、主题、绑定及服务连接，完全靠 XML+Tailwind+TurboScript 作者文件变化；不得由 Demo/Host 自己写 C++ `add_tab/add_node/add_row` 来冒充 XML 功能。通用 Native Widget/Service 的 C++/C11 SDK 实现可以继续保留。

**明确排除**：浏览器 HTML/CSS/JS runtime、第二 UI/Binding/Event/Parser 引擎、Qt `.ui` 原格式的永久兼容入口、通用 Container Widget、静默未知标签 fallback、无界 dynamic module/网络加载、未经验证直接发给 TTY/GPU 的脚本指令。产品 #1 / Schema #16 / Native Tailwind #17 保持 Open；Draft PR #333 不因本设计合并。
