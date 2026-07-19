# Chart DSL 与 Flex UI Document 组合设计

- 状态：设计完成，待实施计划确认
- 日期：2026-07-15
- 范围：FlexChart、Infographic、DOT Graph 的 FlexUI 语义外壳、plot Widget 装配与事务提交
- 关联：[FLEX_UI_DOCUMENT_DESIGN.md](FLEX_UI_DOCUMENT_DESIGN.md)
- 非目标：不合并各 Chart DSL grammar，不替换现有布局/绘图算法，不实现 FFmpeg

## 决策摘要

Chart DSL 继续拥有领域 AST、数据校验、布局和 plot renderer；Flex `ui` DSL 只声明可访问的
应用外壳。`charts::ui` 提供 Adapter，把预先构造好的 plot Widget 装入模板中的类型化 slot，
最后由 flexUI 在一个 Box 批次中提交 Element、Widget、局部索引和可选挂载关系。

```text
Chart / Infographic / DOT source
  -> 各自 parser + domain AST
  -> layout / plot preparation -----------+
                                           |
Flex ui ChartFrame definition              |
  -> immutable UiDocumentDefinition        |
  -> ChartShellComposer                    |
  -> specialized definition + widgets <----+
  -> UiDocumentInstantiator prepare/validate/commit
  -> Box-owned Element + Widget tree
```

不允许失败后改走旧 C++ shell。内建模板或调用方模板失败时返回明确错误，Box 保持调用前状态。

## 仓库事实与问题

- `charts/flexchart/src/chart_parser_dispatch.cpp` 按 mark 类型分派独立 parser，生成
  `flex::chart::AstChart`，不经过 Flex `AstUiDocument`。
- `charts/infographic/src/parser/unified_parser.cpp` 和 DOT parser 也拥有各自语法与领域模型。
- `charts/ui/src/chart_frame.cpp` 手工创建 figure、title、surface、legend 和 tooltip。
- FlexChart 与 Infographic 先创建 C++ shell，再创建 legacy `flex::Group` plot，并通过
  `FlexNodePlotWidget` 桥接到 flexUI。
- DOT 直接创建 Box-owned cluster、edge、node 和 label Element，并为 edge/node 安装专用
  Widget，以保留 CSS、事件和可访问语义。
- `UiDocumentInstantiator` 当前只支持“空 Box + 安装 root”，definition ID 会进入 Box 全局
  ID 索引，也不能在提交批次中安装 Widget。
- ChartFrame 测试要求同一 Box 可创建多个 frame，并且 frame 不依赖全局 Element ID。

`MED`：FlexChart、Infographic 和 DOT 的 builder 在部分 Element 已由 Box 接管后仍可能失败。
返回结果没有暴露 root，不等于 Box 未变化；失败重试可能留下不可见 Element、Widget 或 ID
索引。这是本设计优先修复的状态一致性问题。

## 目标

1. UI 外壳可由 Flex `ui` 自由声明，并继续使用默认 Tailwind JIT。
2. Chart DSL AST 与 Flex UI AST 单向组合，不互相包含第三方类型。
3. FlexChart、Infographic 和 DOT 的公开渲染结果与语义树保持兼容。
4. 所有可预期失败发生在 Box 提交之前；提交失败可完整回滚。
5. 同一 Box 支持多个模板实例，不产生全局 ID 冲突。
6. 内建模板只解析一次；每次实例化成本与模板节点数、legend 数量线性相关。

## 非目标

- 不让 UI DSL 描述 mark encoding、DOT routing 或 infographic layout。
- 不把 legacy Flex Scene 与 Element tree 合并为同一所有权体系。
- 首阶段不实现通用 Widget component registry、运行时 keyed reconciliation 或 TurboScript
  controller；本设计为这些能力保留 slot/handle 边界。
- 不从当前工作目录隐式加载模板，也不在模板错误时使用隐藏 fallback。

## 候选方案

| 方案 | 优点 | 代价与风险 | 结论 |
|---|---|---|---|
| Chart grammar 合并进 Flex `ui` | 单文件语法 | AST/错误语义耦合；每种 chart 都扩大 Flex grammar | 拒绝 |
| 保留全部 C++ shell builder | 无迁移成本 | 重复结构持续存在；无法自由声明 UI；失败非事务 | 仅作迁移期基线 |
| `ui` shell + chart-specific Adapter | 领域边界清晰；可逐模块迁移；可测试 | 需要 fragment、slot、Widget 批量提交能力 | 采用 |
| 立即实现全功能 Component Registry | 最通用 | 一次引入 factory、binding、reconciliation 和脚本生命周期 | 延后 |

采用 Adapter + Composite：UiDocument 负责通用树，ChartShellComposer 只做领域值到 UI slot 的
转换，plot renderer 通过 Widget bridge 组合，不新增 ChartFrame 继承层级或全局 registry。

## Flex UI 模板契约

内建 asset 至少包含 `ChartFrame` 和 `InfographicFrame` 两个命名 document。示例：

```flex
ui ChartFrame {
  figure root {
    slot: "chart",
    utility: "flex flex-col relative w-full box-border rounded-lg border bg-card text-card-foreground p-4 gap-4",
    role: "figure",

    figcaption title {
      slot: "chart-title",
      utility: "text-sm font-medium"
    }

    div surface {
      slot: "chart-surface",
      utility: "relative w-full",
      role: "img",

      div plot {
        slot: "chart-plot",
        utility: "relative w-full overflow-hidden"
      }
    }

    div legend {
      slot: "chart-legend",
      utility: "flex items-center justify-center gap-4 text-sm",
      role: "list"
    }

    div tooltip {
      slot: "chart-tooltip",
      utility: "absolute hidden rounded-md border bg-popover text-popover-foreground p-2 shadow-md text-sm",
      role: "status",
      html.aria_hidden: "true",
      html.data_state: "inactive"
    }
  }
}
```

新增的 definition property 语义：

- `slot: "name"`：必须是 document 内唯一非空字符串；实例化时输出 `data-slot="name"`，
  同时进入实例局部 slot 索引。
- `var.series_color: "#3b82f6"`：映射为 `--series-color` custom property；suffix 中 `_`
  规范化为 `-`。value 只接受 string/finite number，并受 string limits 限制。
- `html.aria_hidden` 与 `html.data_state`：将 suffix 的 `_` 规范化为 `-`，分别输出
  `aria-hidden` 和 `data-state`。已有 `attr.*` 保持精确名称映射，不改变兼容行为。

未知 `slot`、重复 slot、无效 custom property 或规范化后名称冲突必须在 semantic validation
阶段失败，不允许覆盖顺序决定结果。

## 局部 ID 与多实例

definition 的非空 ID 继续作为模板内稳定句柄，但新增实例化策略：

```cpp
enum class UiIdPolicy {
  Global,      // 现有 root document 行为：写入 Element id 与 Box 全局索引
  LocalOnly,   // fragment 默认：仅写入 UiDocumentTree::elements_by_local_id
  Scoped,      // Element id = <scope>--<local-id>，进入 Box 全局索引
};
```

- `instantiate()` 保持 `Global` 和安装 root 的现有语义。
- chart shell 使用 `LocalOnly`，保持 ChartFrame 无全局 ID、可多实例的现有行为。
- 需要 `aria-labelledby` 或外部 ID selector 的调用方显式选择 `Scoped` 并提供非空 scope。
- slot lookup 不依赖 physical Element ID，返回值生命周期仍受 Box 约束。

`LocalOnly` 不是省略校验：definition ID 仍必须唯一，实例结果仍提供 local ID map，只是不把
这些 ID 写入 Box 的全局事实源。

## 建议的 flexUI API

现有 API 保持不变，新增可组合入口：

```cpp
enum class UiMountKind { Root, DetachedFragment, Child };

struct UiMountTarget {
  UiMountKind kind = UiMountKind::DetachedFragment;
  Element* parent = nullptr;       // 仅 Child 使用，必须由目标 Box 拥有
  UiIdPolicy id_policy = UiIdPolicy::LocalOnly;
  std::string id_scope;            // 仅 Scoped 使用
};

struct UiPreparedWidgetSlot {
  std::string slot;
  std::unique_ptr<Widget> widget;
};

struct UiComposition {
  std::vector<UiPreparedWidgetSlot> widget_slots;
};

struct UiDocumentTree {
  Element* root = nullptr;
  std::unordered_map<std::string, Element*> elements_by_id; // 保留现有字段
  std::unordered_map<std::string, Element*> elements_by_local_id;
  std::unordered_map<std::string, Element*> slots;
};

UiDocumentInstantiateResult instantiate(
    Box& box,
    const UiDocumentDefinition& definition,
    const UiMountTarget& target,
    UiComposition composition);
```

约束：

- `Root` 要求 Box 尚未安装 root；`DetachedFragment` 不改变 Box root；`Child` 在同一事务内
  append，且 parent 必须属于 Box、不是 widget-owned leaf。
- 每个 prepared Widget 必须命中一个且仅一个 slot；一个 slot 最多安装一个 Widget。
- Widget 在 prepare 阶段已完成领域对象、尺寸与 renderer 构造，不允许 slot factory 在
  commit 阶段解析 DSL、布局 Chart 或访问外部 I/O。
- Widget bind 必须是 Box 内可回滚操作，不得发送外部消息、启动线程或执行不可逆 I/O；需要
  此类副作用的 Widget 必须在 commit 成功后通过独立 activate 阶段启动。
- 所有 vectors/maps 在 commit 前预分配；Box 记录 batch checkpoint。Widget bind 或 parent
  append 抛出时，回滚 elements、widgets、active widget、ID index、root/parent child list 和
  dirty flags。
- `UiComposition` 是 move-only；失败后其资源由 result/局部 RAII 销毁，不把所有权还给调用方。

新增结构化错误至少覆盖：`InvalidMount`、`MissingSlot`、`DuplicateSlot`、
`SlotAlreadyBound`、`InvalidIdScope`、`WidgetBindFailed`。Chart 的旧 string error 从该结构化
错误单向格式化，不在两处维护独立失败状态。

## ChartShellComposer

`charts::ui::ChartShellComposer` 是 chart-specific Adapter，不放入 flexUI core：

```cpp
struct ChartLegendEntry {
  std::string series;
  std::string label;
  std::string css_color;
};

struct PreparedChartShell {
  flexUI::UiDocumentDefinition definition;
  flexUI::UiComposition composition;
};

class ChartShellComposer {
public:
  ChartShellPrepareResult prepare(
      const flexUI::UiDocumentDefinition& template_definition,
      const ChartFrameOptions& options,
      std::vector<ChartLegendEntry> legend,
      std::unique_ptr<flexUI::Widget> plot) const;
};
```

prepare 只修改 template 的值语义副本：

1. 校验 `chart`、`chart-surface`、`chart-plot` 等必需 slot。
2. 写入 title、accessible label、chart data attribute。
3. 根据 options 从副本中移除 title/legend/tooltip 可选 subtree。
4. 以输入顺序追加 legend item definitions，并验证 `ChartCompositionLimits::max_legend_items`。
5. 把 plot Widget 移入 `chart-plot` prepared slot。
6. 返回 specialized definition；不接触 Box。

模板节点的查找、移除和追加使用独立的 definition visitor/helper，避免在 FlexChart、
Infographic 和 DOT 各复制递归逻辑。该 helper 只操作 `UiNodeDefinition`，不依赖 Chart AST。

## 状态与所有权

| 状态 | 唯一所有者 | 生命周期/可变性 |
|---|---|---|
| Chart/DOT/Infographic AST | 各领域 parser/API | prepare 期间只读；需调整尺寸时复制 |
| 内建 UiDocumentDefinition | immutable asset cache | 进程内只读，可并发共享 |
| specialized definition | ChartShellComposer | 单次 prepare 的局部值 |
| Flex Instance + legacy Group | `FlexNodePlotWidget` | Widget 创建后独占其运行期关系 |
| prepared Widget | `UiComposition` | commit 前 RAII；成功后移交 Box |
| Element、Widget、全局 ID index | Box | commit 后唯一事实源 |
| local ID/slot map | instantiate result 的非 owning view | 不得超过 Box/Element 生命周期 |

Box 仍按现有约定视为单线程对象。领域 parse/layout 可以在工作线程完成，但 composition commit
和后续 Element/Widget mutation 必须在 Box 所在线程执行。

## 事务流程与错误语义

```text
validate input/options/template
  -> prepare domain plot + Widget
  -> clone/specialize definition
  -> validate slots, utilities, limits, mount and IDs
  -> build detached Elements
  -> attach prepared Widgets to detached hosts
  -> reserve Box containers + create checkpoint
  -> adopt Elements/Widgets/index
  -> mount root/child
  -> bind Widget hosts
  -> publish result
```

| 失败阶段 | Box 状态 | 返回错误 |
|---|---|---|
| Chart parse/layout/plot prepare | 未变化 | 领域错误 + stage |
| template parse/specialize | 未变化 | UiDocument/slot 错误 |
| utility/ID/mount validation | 未变化 | 明确 token、ID 或 parent 原因 |
| detached allocation | 未变化 | BuildFailed |
| commit/bind/mount | checkpoint 完整回滚 | WidgetBindFailed/BuildFailed |

不得仅记录日志后返回成功；同一错误只在 Chart API 边界转换一次。内建模板错误视为构建产物
错误，不尝试改用手工 C++ shell。

## 各模块迁移

### FlexChart

1. 在 Box mutation 前复制 AstChart、创建 Instance、legacy plot 与 FlexNodePlotWidget。
2. 由 ChartShellComposer 生成 title、legend 和 plot slot。
3. 使用 `DetachedFragment + LocalOnly` 一次提交。
4. 保持 `FlexUiChartResult.root/plot`、data-slot、可访问属性和 render commands 行为。

### Infographic

- `TemplateCategory::Chart` 复用 ChartFrame definition。
- 其他 category 使用 InfographicFrame definition，但共享 composer visitor、mount 和事务。
- legacy Infographic Group 仍由现有 renderer 构建，不把 infographic layout 迁入 UI DSL。

### DOT Graph

DOT 不在首阶段降级为单一 plot Widget，因为当前 node/edge/cluster Element 是 CSS、事件和
accessibility 的语义事实。迁移分两步：

1. 先让纯 C++ `LayoutSnapshot -> DotGraphViewDefinition` 在 Box 外生成通用节点定义和
   prepared node/edge Widget slots。
2. 再用同一 UiDocument batch 提交 outer shell 与 DOT semantic subtree。

迁移前保留现有 DOT builder；不得用单 Widget 替换语义树作为临时 fallback。DOT 需要额外的
node/edge/cluster 数量限制，输入字节上限 `DOTGRAPH_MAX_INPUT_BYTES` 继续生效。

## 模板提供与定制

- 内建 `.flex` 文件作为构建 asset 嵌入 `chart_ui`，不依赖进程工作目录。
- 内建 source 通过线程安全的 immutable cache 解析一次；缓存只保存 definition/result，不含
  Box、Element 或 Widget。
- 新增重载允许调用方传入 `shared_ptr<const UiDocumentDefinition>`。传入非空自定义 definition
  后若契约不满足，明确失败，不 fallback 到内建模板。
- 第一阶段不开放任意运行时文件路径；宿主可自行用受控文件 API 读取并调用
  `parse_ui_document`，从而自行承担路径、权限与热重载策略。

## Tailwind JIT、主题与动态状态

- template 和动态 legend definitions 的显式 utility 在 commit 前通过目标 Box catalog 验证。
- 实例化只标记 utility tree dirty 一次；不在每个 legend item 创建时重编 stylesheet。
- light/dark/system 继续由 Box theme root 和 CSS variables 驱动，不把颜色值复制进模板分支。
- tooltip 与 legend highlight 的现有 C++ mutation API 保留；它们只改变已提交 Element 的
  attribute、custom property 和 utility，不触发 definition 双向同步。
- 后续 TurboScript 只能通过 slot/local handle 和事件快照访问树，不持有裸 Element 指针。

## 性能与资源边界

- 内建模板 parse：进程内一次，复杂度 `O(template bytes)`。
- specialized definition clone：`O(template nodes + legend items)`；典型 shell 很小，不进入
  renderer 热路径。
- 实例化/校验：`O(nodes + utility tokens + widget slots)`，临时内存 `O(nodes)`。
- Chart layout/render 仍是主要成本；本设计不改变 mark renderer 的算法复杂度。
- `ChartCompositionLimits` 必须命名并可配置，至少包含 legend item、specialized node 和
  widget slot 上限；不得把阈值散落在 adapter 中。
- DOT 使用独立的 node/edge/cluster 上限，因为输入字节限制不能约束展开后的资源量。

## 兼容性影响

- 各 Chart DSL 文本格式、AST 和 parser 不变。
- 现有 Chart result 指针仍由 Box 拥有，调用方 append/set_root 流程不变。
- ChartFrame 的 `data-slot`、role、aria、utility、tooltip/legend 状态保持现有测试契约。
- 新增 UiDocument fragment、slot、ID policy 和 composition API；现有 root instantiate 行为不变。
- C++ 手工 shell 实现完成迁移后废弃一个发布周期，再删除；迁移期新路径与旧路径不做运行时
  fallback，只允许测试中对比同一输入的语义结果。
- UiDocument/Chart public header 的新增类型是源兼容的；若修改已有 result layout 或 options
  layout，实施前需单独确认 ABI 策略。

## 验证范围

### flexUI

- slot/var/attribute normalization、重复与冲突校验。
- Root、DetachedFragment、Child 三种 mount；Global、LocalOnly、Scoped 三种 ID policy。
- 同 Box 多实例、parent ownership、widget-owned parent 拒绝路径。
- missing/duplicate/already-bound slot 和 unknown utility。
- 在 Element allocation、Widget bind、mount 各阶段注入失败，验证 Box root、container sizes、
  ID lookup、active widgets、parent children 和 dirty flags 均恢复。

### charts::ui

- 内建模板包含全部必需 slot，定制模板缺 slot 时 fail fast。
- title/legend/tooltip 可选 subtree、legend 顺序、颜色 custom property 和资源上限。
- 同 Box 两个 ChartFrame 保持空 global ID，local/slot lookup 互不串扰。
- light/dark/system command snapshot 与当前基线一致。

### FlexChart / Infographic / DOT

- parser/AST 测试不变。
- 成功路径继续产生非空 render commands、正确 data-slot/aria 和零 missing utility。
- plot/layout/template/Widget 失败时 Box 无新增 Element/Widget/ID。
- DOT 逐 node/edge/cluster 的 CSS selector、事件 hit target 和 accessibility tree 不退化。

## 迁移顺序

1. flexUI 增加 slot metadata、local ID、fragment mount 和 Widget batch，先以故障注入测试固定
   事务语义。
2. charts::ui 增加内建 Flex template、ChartShellComposer 与 definition visitor。
3. FlexChart 迁移并对照现有语义/render command 测试。
4. Infographic 迁移并复用 ChartFrame/InfographicFrame。
5. DOT 拆分纯 view definition 与批量 Widget slots，再迁移 semantic tree。
6. 标记 C++ ChartFrame builder deprecated；一个兼容周期后删除重复实现。

每一步都可以独立回滚到上一步提交。回滚不修改 Chart DSL 数据格式，不迁移持久化数据；若
新 UiDocument API 尚无生产调用，可直接删除新增 adapter。进入去除旧 builder 的阶段后，回滚
方式是恢复上一发布的 adapter 实现，而不是在运行时根据错误自动切换路径。
