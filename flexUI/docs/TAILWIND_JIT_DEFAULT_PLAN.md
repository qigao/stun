# Tailwind-like Utility JIT 默认化实施计划

- 状态：核心实现、离屏视觉回归与性能基线完成；既有仓库失败项已分离
- 日期：2026-07-15
- 设计依据：[TAILWIND_JIT_DEFAULT_DESIGN.md](TAILWIND_JIT_DEFAULT_DESIGN.md)
- 范围：`flexUI`、仓库内 Box 应用入口、DotGraph、FlexChart、Infographic

## 2026-07-15 验证记录

- 已通过 47 个直接及相邻 CTest，覆盖默认 JIT/theme、binding、render semantics、
  ChartFrame、DotGraph、全部 FlexChart mark 和 Infographic parser/layout/adapter。
- registry coverage 覆盖 40 个缓存组件，missing token 为 0。
- 已构建 6 个 flexUI examples 和 4 个受影响 chart examples。
- 已显式构建全部 100 个 CTest 所需目标；完整 CTest 为 96/100。剩余失败已分离为：
  PDF vendor 测试工作目录数据、ThorVG capability 旧断言、Checkbox semantic-part
  旧快照、flowchart 72 个既有 golden mismatch。
- Calendar hover bridge 与 host-painted widget focus ring 已修复；对应 render semantics
  通过。ChartFrame 已覆盖 light/dark/system、tooltip、legend、长标题和 320/640 resize
  的确定性离屏 command snapshot。
- `utility_jit_benchmark` 已记录 501/5001 Element 基线，并据此修复重复 token 在大树上
  错误触发 `max_tokens_per_update` 的问题。
- `FlexPlayer`/FFmpeg 按用户要求明确排除，不修改依赖或部署配置。

## 使用规则

- TODO 只有在代码、测试和相邻文档全部完成后才能勾选。
- 每个阶段必须先跑最小相关测试，再跑相邻回归。
- 任何阶段发现公开行为超出设计文档所列兼容范围时，停止并更新设计决策。
- 不通过 fallback、异常吞噬或自动禁用 JIT 让失败测试变绿。
- 不在迁移中修改 chart DSL、AST 数据格式或 Mermaid grammar。
- 用户工作树已有大量 charts 改动；实现时只修改本计划明确列出的文件，并逐项检查
  overlap。

## 依赖顺序

```text
P0 文档
  └── P1 内嵌 catalog/theme
        └── P2 Box 默认 JIT
              └── P3 utility/class 语义
                    ├── P4 应用入口迁移
                    └── P5 ChartFrame
                          ├── P6 DotGraph
                          ├── P7 FlexChart
                          └── P8 Infographic
                                └── P9 扩大回归与发布收口
```

P1、P2、P3 是 charts 迁移的硬前置。P6、P7、P8 在 P5 完成后可以独立推进，但最终
发布门槛要求三者全部通过。

## P0：架构与计划基线

- [x] 创建默认 Tailwind-like Utility JIT 架构设计文档。
- [x] 记录现状、候选方案、选择理由、状态所有权、错误语义和回滚方式。
- [x] 创建分阶段实施计划和 TODO 清单。
- [x] 在实现开始前检查设计文档链接和涉及文件是否仍与当前代码一致。

完成条件：实现者可以只依赖设计文档和本计划确定模块边界、依赖顺序与验证门槛。

## P1：内嵌默认 Style Assets

### Catalog

- [x] 保持 `tools/shadcn-ir/schema/utility_whitelist.json` 为 utility 唯一事实源。
- [x] 在 `flexUI/CMakeLists.txt` 增加跨平台的构建时内嵌步骤。
- [x] 生成内部 `default_style_assets.generated.cpp`，不提交 build-tree 产物。
- [x] 增加不可变 `UtilityCatalog` 表达，避免各 Box 复制完整 JSON。
- [x] 让 `UtilityJit` 支持共享 `std::shared_ptr<const UtilityCatalog>`。
- [x] 保留现有 JSON 构造入口，供自定义 catalog 和单元测试使用。
- [x] 确认安装后的 `flexUI` 不读取仓库 `tools/` 路径。

### Theme

- [x] 新增 `flexUI/assets/default_theme.css`。
- [x] 定义 light theme 基础 design tokens。
- [x] 定义 dark theme 基础 design tokens。
- [x] 支持 `prefers-color-scheme` 的 system 模式。
- [x] 支持 `[data-theme=light]` 和 `[data-theme=dark]` 显式覆盖。
- [x] 定义 host bridge 使用的 `--color-scheme`。
- [x] 将 theme 与 whitelist 一起内嵌到 flexUI 静态库。
- [x] 删除示例中与默认 theme 完全重复的变量定义，仅保留示例专用值。

### 测试

- [ ] 测试内嵌 catalog 与源 JSON 具有相同 version 和 token keys。
- [x] 测试默认 theme CSS 能被 strict StyleEngine 接受。
- [ ] 测试 catalog 初始化一次后可被多个 Box 共享。
- [ ] 测试在不依赖当前工作目录的进程中加载默认 assets。
- [x] 运行 `test_utility_jit` 和 `test_shadcn_ir_fixtures`。

完成条件：默认 assets 随库部署、解析失败时 fail fast、没有外部运行时文件依赖。

## P2：`Box` 默认启用 JIT

### API

- [x] 在 `box.h` 增加 `UtilityJitMode`、`ThemeMode` 和 `BoxOptions`。
- [x] 保持 `Box(flex::Renderer*)` 调用源码兼容。
- [x] 默认 `BoxOptions` 使用 BuiltIn JIT 和 System theme。
- [x] 增加命名明确的 `BoxOptions::legacy_without_jit()`。
- [x] 保留 `enable_utility_jit()`、`disable_utility_jit()` 和 custom catalog 能力。
- [x] 在头文件文档中说明构造失败条件和默认 cascade 顺序。

### 初始化

- [x] 构造 Box 时先加载 Widget 基础规则。
- [x] 再加载默认 theme stylesheet。
- [x] 再预留并启用 JIT stylesheet slot。
- [x] 确保应用之后加载的 stylesheet 排在默认 JIT 之后。
- [x] 默认 asset 无效时让构造失败，不创建半初始化 Box。
- [x] 确保 layout-only Box 与 renderer-backed Box 使用相同 styling 初始化。

### 测试

- [x] 测试默认 Box 的 `utility_jit_enabled()` 为 true。
- [x] 测试 legacy options 明确关闭 JIT。
- [x] 测试默认 `flex rounded-md` 能改变 computed style。
- [x] 测试后加载应用 CSS 可以覆盖默认 utility。
- [x] 测试 JIT stylesheet 替换不改变 cascade 位置。
- [x] 测试 invalid custom reconfigure 保留当前有效 JIT。
- [x] 测试 light/dark/system theme 与 MediaEnvironment。
- [x] 运行 `test_utility_jit`、`test_style_engine`、`test_render_semantics`。

完成条件：仓库所有普通 `Box(renderer)` 调用点自动获得可用 JIT 和 theme，且显式
legacy path 可重复验证。

## P3：Utility 与普通 Class 的明确语义

### Element API

- [x] 增加 `add_utility()`。
- [x] 增加 `add_utilities()`。
- [x] 增加 `set_utilities()`。
- [x] 增加对应 remove/toggle 操作，避免调用方绕过 utility ownership。
- [x] 明确 class attribute、普通 class 集合和显式 utility 集合的同步关系。
- [x] 确保 binding runtime 可以显式绑定 utility，而不制造第二棵状态树。

### JIT 扫描

- [x] catalog 中已知的普通 class 继续参与编译，保持迁移兼容。
- [x] catalog 外普通 class 不进入 required-utility missing 列表。
- [x] 显式 utility 不存在于 catalog 时 fail fast。
- [x] 显式 utility 超过长度/数量/active-token 上限时 fail fast。
- [x] `missing_utility_tokens()` 只表达显式 utility 缺失。
- [x] class/tree 变化继续只在必要时标记 JIT dirty。
- [x] `data-state` 或 custom property 更新不触发 token 重编译。

### 语义规范

- [x] 新 UI 使用 `data-slot`、`part`、`role`、`data-state` 表达语义身份。
- [x] 更新 `WIDGET_DEVELOPMENT.md` 的 class/utility 示例与约束。
- [x] 更新 `SHADCN_CONFORMANCE.md` 的 missing-token 诊断口径。
- [x] 审计 IR instantiation：component node 不持有 registry utility token；utility CSS
  仍由 style IR/registry 单一事实源生成，无可迁移的隐式 class 写入点。

### 测试

- [x] 测试普通 `.dot-node` 等 class 不产生 missing utility。
- [x] 测试普通已知 `flex` class 仍可由 JIT 编译。
- [x] 测试显式未知 utility 立即失败。
- [x] 测试 utility add/remove/toggle 正确更新 active stylesheet。
- [x] 测试 binding 更新 utility 时保持单一 target owner。
- [x] 运行 `test_utility_jit`、`test_binding_runtime`、`test_keyed_repeater`。
- [x] 运行全部 registry utility coverage。

完成条件：默认 JIT 不因语义 class 产生噪声，新代码对 utility typo 保持 fail fast。

## P4：仓库应用入口迁移

- [x] 移除 `visual_demo` 对源码路径 `utility_whitelist.json` 的运行时读取。
- [x] 移除 `visual_demo` 的手工 `enable_utility_jit()`。
- [ ] 保留 demo 对 JIT missing token 的可见诊断。
- [ ] 检查 NanoVG、Direct2D、TUI、Markdown 和 shadcn IR demos 的 Box 默认行为。
- [x] 检查 Meta Editor 的 Box 创建点和 CSS cascade；现有 class 均为 semantic class。
- [x] 检查 TextEdit 的 layout-only Box 测试。
- [ ] 只有确有兼容阻塞的调用点才使用 `legacy_without_jit()`，并记录移除条件。
- [x] 将新 UI 的 semantic class 逐步迁移到 `data-slot` / `part`。
- [x] 运行所有 flexUI examples 的可构建 smoke 验证。

完成条件：仓库应用不再自行定位 whitelist，默认行为由 Box 一处提供。

## P5：公共 ChartFrame

### 模块边界

- [x] 在 `charts/ui` 建立最小公共模块和 CMake target。
- [x] 定义 `ChartFrameOptions`，避免超过五个松散构造参数。
- [x] 定义 `ChartFrame`，暴露 root/title/surface/legend/tooltip 窄引用。
- [x] 实现 `create_chart_frame(Box&, options)`。
- [x] 不让 ChartFrame 依赖 FlexChart、DotGraph 或 Infographic AST。
- [x] 不在 ChartFrame 中实现 scale、series、projection 或 parser 逻辑。

### 结构与 styling

- [x] root 使用 `data-slot=chart` 和适当 role。
- [x] surface 使用 `data-slot=chart-surface` 和可访问名称。
- [x] legend/item 使用稳定 `data-slot` 和 `data-series`。
- [x] tooltip 使用 `role=status`、`data-state` 和 `aria-hidden`。
- [x] 使用 whitelist 已覆盖的 chart registry utility tokens。
- [x] series 颜色、tooltip 坐标等动态值使用 custom properties。
- [x] 对齐已有 shadcn chart component/style/bridge IR 的结构和状态语义。

### 状态与测试

- [x] 明确 hover、highlight、selection、tooltip open 的 owner。
- [x] 状态变化只写回 Element attribute/pseudo state/custom property。
- [x] 测试多个 ChartFrame 实例不会发生 id 或 stylesheet 冲突。
- [x] 测试 tooltip active/inactive 的 computed style、ARIA 和 render semantics。
- [x] 测试 legend highlighted 状态。
- [x] 测试 light/dark/system theme。

完成条件：三个 chart 模块可以复用同一 UI shell，同时保持各自领域和 renderer 独立。

## P6：DotGraph 迁移

- [x] 复用现有 `create_flexui_dotgraph(Box&, ...)`，不新增平行 UI 树。
- [x] 将 root/node/edge/cluster/label 身份补为 `data-slot`。
- [ ] 旧 `.dot-*` class 保留一个迁移周期并记录 deprecation。
- [x] 固定容器、label 和 chrome 样式改用 utility。
- [x] DOT fill/stroke/font color 投影到命名 custom properties。
- [x] node/edge path、transform、hit testing 继续由现有 Widget 管理。
- [ ] parse 或 options 校验失败时保持不暴露 partial tree。
- [x] 更新 query-selector 测试到 `data-slot`，同时验证旧 selector 兼容。
- [x] 增加 JIT computed-style 测试。
- [ ] 增加 light/dark 视觉或 render-semantic 覆盖。
- [ ] 运行 DotGraph 全部测试和 example smoke。

完成条件：DotGraph UI styling 默认来自 Box JIT/theme，图形几何行为不回归。

## P7：FlexChart 迁移

### Adapter

- [x] 新增 `create_flexui_chart(Box&, AstChart, options)` 公开入口。
- [x] 定义包含 root/error 的明确结果类型。
- [x] 输入无效时不挂载 partial tree。
- [x] 保持 parser、AstChart、mark renderer registry 的公开语义不变。
- [x] 保留现有 `ChartComponent::build(..., flex::Instance&)` 兼容入口。
- [x] 为旧入口增加 deprecation 文档，不立即删除。

### UI 与 renderer

- [x] title、legend、tooltip 和外层布局使用 ChartFrame。
- [x] 将当前硬编码的 title/axis/legend 字体与间距分为 UI styling 和 plot geometry。
- [x] UI styling 改为 utility/theme/custom properties。
- [x] 新增 FlexChart PlotWidget 或等价 adapter 发出 mark render commands。
- [x] mark renderer 继续拥有 point/line/bar/arc/image/geoshape 等 geometry。
- [x] 明确 axis label 属于可样式化 Element 还是 plot 原子绘制，并为选择写测试。
- [x] entry animation 继续由一个 owner 驱动，不与 CSS animation 双重推进。

### 测试

- [x] 测试 Box-owned semantic tree。
- [x] 测试 title、legend、tooltip 的 JIT computed style。
- [ ] 测试每种已支持 mark 至少一条 adapter 渲染路径。
- [ ] 比较迁移前后代表性 mark geometry。
- [ ] 测试无 title、无 legend、空数据和多 series。
- [ ] 测试 light/dark theme 和 tooltip 状态。
- [x] 运行 FlexChart parser、mark renderer 和 adapter 全部测试。

完成条件：新 FlexChart UI 默认进入 Element/JIT 管线，AST 和 mark geometry 无公开回归。

## P8：Infographic 迁移

### Adapter

- [x] 新增 `create_flexui_infographic(Box&, UnifiedInfographic, options)`。
- [x] 定义包含 root/error 的明确结果类型。
- [x] 保持 UnifiedInfographic、layout engine 和 parser 语义不变。
- [x] 保留静态 SVG 输出路径，不要求离线 SVG 经过 Box/JIT。
- [x] 保留旧 `InfographicComponent::build(..., flex::Instance&)` 兼容入口。
- [x] 为旧入口增加 deprecation 文档，不立即删除。

### UI 与 renderer

- [x] 交互式 chart template 复用 ChartFrame。
- [x] 非 chart template 使用 Box-owned Element shell 和模块 renderer。
- [x] 标题、legend、卡片、label 等 UI styling 改为 utility/theme。
- [x] layout engine 继续拥有模板节点几何和布局结果。
- [ ] 数据颜色使用 custom properties，不生成无界 utility。
- [x] 明确 SVG renderer 与 interactive renderer 的共享数据边界。

### 测试

- [x] 测试 Box-owned semantic tree。
- [ ] 覆盖 chart、list、timeline、tree 等代表性 template。
- [ ] 对比静态 SVG 输出，确认 adapter 不改变 export 行为。
- [ ] 测试 theme override、空 items、长 label 和布局边界。
- [x] 测试 parse/render 失败无 partial tree。
- [x] 运行 Infographic parser、layout、SVG 和 adapter 全部测试。

完成条件：交互式 Infographic 默认使用 JIT styling，静态导出和领域数据保持兼容。

## P9：性能、回归与发布收口

### 性能

- [x] 为 JIT tree scan 增加典型 Element tree benchmark。
- [x] 增加大树和高 class-count benchmark。
- [x] 记录 active-token 数量、stylesheet 字节数、首次更新时间和增量更新时间。
- [x] 确认普通 `data-state` 高频变化不触发全量 token revision。
- [x] 只有 profiling 证明扫描占显著比例时才设计增量 token refcount；当前无证据，
  不增加 refcount 双状态。

### 全量验证

- [x] 运行全部 flexUI CTest；失败基线见顶部验证记录。
- [x] 运行全部 charts CTest；flowchart golden 基线见顶部验证记录。
- [x] 运行 shadcn registry coverage。
- [x] 构建所有受影响 examples。
- [ ] 运行 NanoVG 或当前可用后端的代表性视觉 smoke。
- [x] 检查 light、dark、system 三种主题。
- [x] 检查 `git diff --check`。
- [x] 运行 `codegraph affected` 确认遗漏的调用点和测试候选。

### 文档与发布

- [x] 更新 `ARCHITECTURE.md`，记录 Box 默认 JIT 和共享不可变 catalog。
- [x] 更新 `WIDGET_DEVELOPMENT.md`，记录显式 utility API 和 `data-slot` 规则。
- [x] 更新 `SHADCN_CONFORMANCE.md`，删除 staged opt-in 描述。
- [x] 更新 examples 文档，删除手工 whitelist 初始化说明。
- [x] 为旧 chart `flex::Group` API 增加 `@deprecated` 和替代示例。
- [x] 记录公开默认行为变化、二进制体积影响和回滚入口。
- [x] 确认没有新增无归属 TODO/FIXME/HACK。

完成条件：所有默认 Box 和交互式 chart UI 使用统一 JIT styling 契约，测试、文档、
examples 与安装包行为一致。

## 最终 Definition of Done

- [x] `Box(renderer)` 无额外初始化即可使用仓库内置 Tailwind-like utilities。
- [x] 默认 theme variables 在 light/dark/system 模式下完整可用。
- [x] 安装或移动二进制后不依赖源码树 assets。
- [x] Utility catalog 是唯一共享只读定义，active state 仍按 Box 隔离。
- [x] 新 UI 的未知显式 utility fail fast。
- [x] 普通语义 class 不污染 missing-utility 诊断。
- [x] DotGraph、FlexChart、Infographic 的交互式默认入口均为 Box-owned Element tree。
- [x] Chart UI chrome 使用 JIT，plot geometry 保持 renderer-owned。
- [x] 旧 chart API 保持兼容并有清晰 deprecation 路径。
- [ ] 核心、相邻和扩大回归全部通过。
- [ ] 视觉 smoke 未发现未记录的公开行为回归。
- [ ] 设计文档、实施计划和开发指南与最终实现一致。

## 验证记录模板

每完成一个阶段，在对应变更说明中记录：

```text
阶段：P<n>
改动文件：
最小测试：
相邻回归：
扩大回归：
性能数据（如适用）：
已知剩余风险：
回滚入口：
```

不要在本计划中粘贴长构建日志；只勾选完成项，并在提交或变更说明中保留可复验
命令和结果。
