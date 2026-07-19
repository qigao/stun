# flexUI Shadcn Conformance

此目录中的 conformance fixtures 用于验证 `flexUI` 是否能承载 `shadcn` 常见组件所依赖的高频 CSS / state 语义。

## Ready scope

当前目标是 **default utility runtime ready**：普通 `Box(renderer)` 无需额外初始化即可使用内嵌 Tailwind-like utility catalog 与 light/dark/system theme；显式 utility 缺失会在运行时 fail fast。

当前仍不是完整 Tailwind 或完整 shadcn registry pipeline：运行时只接受显式白名单，不包含 Tailwind 配置/插件系统，也不保证 registry 中每个组件都已有 IR 实例化定义。registry coverage 只证明 class token 已有白名单展开，不等于组件实例化或视觉完全一致。

当前闭环包含：

- `tools/shadcn/fetch_registry.ps1`
  从官方 `https://ui.shadcn.com/r/{name}.json` 抓 registry item，并提取 class token 摘要。
- `tools/shadcn/check_registry_coverage.ps1`
  离线检查已缓存 registry summary 是否都能被 `utility_whitelist.json` 覆盖。
- `tools/shadcn-ir/schema/utility_whitelist.json`
  维护 flexUI 当前可从 Tailwind-like class token 生成的 CSS 白名单。
- `flexUI/tests/shadcn/manifests/*_manifest.json`
  分层维护 conformance fixtures：
  - `compute_style_manifest.json`
  - `host_bridge_manifest.json`
  - `behavior_time_manifest.json`
  - `render_semantic_manifest.json`
- `flexUI/tests/test_shadcn_conformance.cpp`
  用 TinyTest 驱动真实 `Box + Widget + CSS` 更新链，校验 create / compute / state bridge。

当前 fixture 覆盖：

- `compute-style`
  - `button` 的 `focus-visible + ring/outline`
  - `input/textarea` 的 `placeholder-shown + selection + caret-color`
- `host-bridge`
  - `select / checkbox / switch / tabs / modal / dropdown`
  - `accordion / toggle-group / radio`
  - `table / tree`
- `behavior-time`
  - `tooltip` 的 delayed-open / side bridge
  - `toast` 的 live region / open state
- `render-semantic`
  - `button` 的 ring / outline draw rect
  - `dropdown` 的 open overlay draw rect + text
  - `tooltip` 的 visible overlay background + text

抓取官方 registry 示例：

```powershell
powershell -ExecutionPolicy Bypass -File tools/shadcn/fetch_registry.ps1 -Name button,input,select,accordion,toast
```

检查已缓存 registry 的 Tailwind-like utility 覆盖：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/shadcn/check_registry_coverage.ps1
```

## Registry vs IR support

`registry utility coverage`、`IR sample coverage` 与 `widget bridge coverage` 是三条不同的支持口径，不能互相替代：

- `registry utility coverage`：证明已缓存官方 registry summary 中抽出的 class token 都能在 `tools/shadcn-ir/schema/utility_whitelist.json` 中找到展开规则。它只覆盖 utility token 白名单，不证明组件已有 IR 样例、Widget 桥接或视觉一致性。
- `IR sample coverage`：证明 `tools/shadcn-ir/samples/{component}.component.json`、`{component}.style.json`、`{component}.bridge.json` 能通过 schema / target 校验，并且可被 `test_shadcn_ir_fixtures` 加载。它不自动覆盖官方 registry 中的新 token，也不代表运行时 Widget 行为已经接上。
- `widget bridge coverage`：证明 `flexUI/tests/shadcn/manifests/*_manifest.json` 中的场景能通过真实 `Box + Widget + CSS` 链路验证 create / compute / state bridge / render semantic。它不要求该组件已经进入官方 registry cache，也不替代 utility whitelist 检查。

当前 `ready` 口径应按改动范围组合验证：

- 只新增或调整 shadcn class token：运行 `test_shadcn_ir_fixtures.exe`，并运行 `tools/shadcn/check_registry_coverage.ps1` 覆盖相关 registry summary；若 token 来自 `visual_demo`，还要通过 `covers visual demo utility tokens`。
- 新增或调整 IR 样例：补齐并校验对应的 `*.component.json`、`*.style.json`、`*.bridge.json`，运行 `test_shadcn_ir_fixtures.exe`。
- 新增或调整 Widget 桥接语义：补相邻 manifest 场景，运行 `test_shadcn_conformance.exe`；若影响渲染输出，还要覆盖 `render_semantic_manifest.json` 中的 draw call 断言。
- 宣称某个 shadcn 组件进入 staged migration ready：至少需要 registry utility coverage 无缺口、IR sample coverage 通过，以及该组件所需的关键 widget bridge 场景通过；仍不宣称完整 Tailwind / shadcn registry pipeline 或像素级一致。

新增 shadcn 组件时，按实际支持面补文件和测试：

- registry utility：用 `tools/shadcn/fetch_registry.ps1 -Name <component>` 刷新 `flexUI/tests/shadcn/registry-cache/<component>.registry.json` 与 `<component>.summary.json`，把确属当前迁移范围的缺失 token 补到 `tools/shadcn-ir/schema/utility_whitelist.json`，并让 `check_registry_coverage.ps1 -Name <component>` 通过。
- IR sample：新增 `tools/shadcn-ir/samples/<component>.component.json`、`<component>.style.json`、`<component>.bridge.json`，并在 `flexUI/tests/test_shadcn_ir_fixtures.cpp` 的 sample 校验中加载和验证。
- widget bridge：在 `flexUI/tests/shadcn/manifests/compute_style_manifest.json`、`host_bridge_manifest.json`、`behavior_time_manifest.json`、`render_semantic_manifest.json` 中补最贴近该组件风险的场景，并确保 `flexUI/tests/test_shadcn_conformance.cpp` 已覆盖对应 fixture 类型。

## Registry 分类维护

registry 分类只用于维护口径，不能替代上面的三类 coverage：

- `IR supported`：registry item 已有对应本地 sample，并进入 schema / style / bridge 校验；其中 registry 名和 sample 名不一定相同。
- `utility-only`：registry item 已缓存并纳入 utility whitelist 覆盖，但当前没有对应本地 IR sample 或 Widget 桥接承诺；这代表 CSS token ready，不代表组件行为 ready。
- `out-of-scope / 缺样例`：当前没有官方 registry cache，或只有本地实验 sample；不得据此宣称 registry 支持。

当前需要固定的非同名映射：

| registry 名 | 本地 sample 名 | 维护说明 |
| --- | --- | --- |
| `dropdown-menu` | `dropdown` | IR / bridge 用 `DropdownWidget` 覆盖菜单打开、选项与 overlay 语义。 |
| `radio-group` | `radio` | IR / bridge 用 `RadioWidget` 覆盖单选项与组状态。 |
| `command` | `searchbox` | 以命令面板的输入、过滤、选中建议语义映射到 `SearchBoxWidget`。 |
| `sonner` | `notification` | 以通知堆叠、位置、自动关闭语义映射到 `NotificationWidget`。 |

当前 `IR supported` registry 名包括：`button`、`input`、`select`、`textarea`、`checkbox`、`switch`、`slider`、`calendar`、`tabs`、`accordion`、`dialog`、`popover`、`tooltip`、`toast`、`toggle-group`、`dropdown-menu`、`radio-group`、`command`、`sonner`、`alert`、`avatar`、`badge`、`card`、`chart`、`drawer`、`form`、`hover-card`、`input-otp`、`label`、`menubar`、`navigation-menu`、`pagination`、`progress`、`resizable`、`scroll-area`、`separator`、`sheet`、`skeleton`、`table`、`toggle`。

其中 `alert`、`avatar`、`badge`、`card`、`chart`、`drawer`、`form`、`hover-card`、`input-otp`、`label`、`menubar`、`navigation-menu`、`pagination`、`progress`、`resizable`、`scroll-area`、`separator`、`sheet`、`skeleton`、`table`、`toggle` 走 generic element IR sample：它们证明结构、样式和 bridge 投影可导入，但没有新增专用 Widget 行为。`progress` 目前只保证 `role=progressbar`、`aria-valuenow`、`data-value` 等属性投影；`chart` 目前只覆盖 chart container / surface / tooltip / legend 结构和状态投影，不包含 Recharts 数据模型、坐标轴、series 渲染或 tooltip formatter 行为；`table` 目前是静态 table 结构，不等同于 `TableWidget` 数据表行为；`form` 目前只覆盖 field/item/control/message 的 aria 与状态投影，不包含 react-hook-form/zod 行为；`menubar`、`navigation-menu` 目前只覆盖 Radix 结构、ARIA/data-state/motion 投影和代表性样式，不包含 roving focus、typeahead、portal positioning、collision detection 或完整键鼠菜单交互；`resizable` 目前只覆盖 panel group / panel / handle 结构、方向和状态投影，不包含真实拖拽 resize 布局；`hover-card`、`scroll-area`、`input-otp`、`sheet`、`drawer` 目前不承诺完整 Radix、Vaul 或外部包交互。

`slider` 当前走 `SliderWidget` backed IR sample，覆盖 horizontal、单 thumb、基础 min/max/value/step、disabled 与 focus-visible bridge 语义；它不宣称完整 Radix Slider 的多 thumb、orientation、form integration 或全部 pointer/touch 行为。

`calendar` 当前走 `CalendarWidget` backed IR sample，覆盖单月视图、单选日期、view month/year host attributes 与 focus-visible bridge 语义；它不宣称完整 DayPicker 的 range/multiple mode、outside days、dropdown caption 或 locale formatting。

当前没有仅停留在 `utility-only` 的已缓存 registry item。新增 registry cache 时，若只有 token whitelist 覆盖而没有 IR sample / Widget bridge 承诺，应先放回 `utility-only`，不要直接宣称组件 ready。

当前 `out-of-scope / 缺样例` 只记录维护边界：`menu` sample 来源是 `shadcn/context-menu`，但当前没有对应 registry cache；`sidebar` sample 有本地 IR，但当前没有对应 registry cache；`spinner`、`toolbar`、`listview` 是本地 sample，不按 shadcn registry coverage 宣称支持。若后续补 registry cache 或改名，必须同步更新本节和相邻测试清单。

## Tailwind-like utility CSS

当前 utility 支持是显式白名单，不是完整 Tailwind 编译器。`tools/shadcn-ir/schema/utility_whitelist.json` 中的 `tokens` 是主事实源：每个 key 是一个 class token，value 描述它应展开成的 CSS。常见形态包括：

- `kind: "decl"`：直接声明，如 `bg-primary`、`w-[1200px]`、`[&_svg]:size-4`。
- `kind: "conditional"`：带状态或属性条件，如 `focus-visible:ring-1`、`data-[state=open]:animate-in`。
- `kind: "pseudo"`：映射到伪元素，如 `placeholder:text-muted-foreground`、`file:text-sm`。
- `kind: "macro"`：展开为多个已知 token，用于复用组合语义。

离线生成或静态资产可先用 `flexUI::shadcn_ir::missing_utility_tokens(whitelist, tokens)` 检查输入，再通过 `flexUI::shadcn_ir::emit_utility_css(whitelist, tokens)` 生成 CSS。运行时默认由 `Box` 内嵌 catalog；新 UI 使用 `add_utility()` / `add_utilities()`，自定义 catalog 或测试才调用 `enable_utility_jit()`。

`emit_utility_css` 本身不会扫描组件树，也不会自动补齐 registry 中尚未列出的 token。JIT 同样只编译 whitelist 中已知的 utility；catalog 外普通 class 保留给 CSS selector，不产生 missing 噪声。显式 utility 未知时立即失败；`Box::missing_utility_tokens()` 仅表达已声明 utility 在 catalog 重配置后缺失的异常状态。

## IR instantiation API

`flexUI::shadcn_ir::instantiate_component(...)` 保留旧语义：创建组件树后会把组件 root 设为 `Box` root。它适合单组件 fixture 或独立预览。

组合多个 IR 组件到已有界面时，应使用 `flexUI::shadcn_ir::instantiate_component_tree(...)`。该函数只创建 `InstantiatedTree`，不会调用 `Box::set_root`；调用方负责把 `tree.root` append 到自己的容器里。`shadcn_ir_demo` 使用这个路径，避免多个组件实例化时互相覆盖 app root。

## Registry coverage

`tools/shadcn/fetch_registry.ps1` 用于抓取官方 registry item，并在 `flexUI/tests/shadcn/registry-cache` 写入 `*.registry.json` 与 `*.summary.json`。summary 中的 `extracted_class_tokens` 是 coverage 脚本和 `test_shadcn_ir_fixtures` 使用的输入。

按组件刷新 registry cache：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/shadcn/fetch_registry.ps1 -Name button,input,select,accordion,toast
```

检查指定组件的 utility 覆盖：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/shadcn/check_registry_coverage.ps1 -Name button,input,select
```

检查全部已缓存 summary：

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/shadcn/check_registry_coverage.ps1
```

`check_registry_coverage.ps1` 是离线检查：它读取本地 registry cache 和 `utility_whitelist.json`，过滤明显的源码标识符 token 后，报告缺失的 utility token。脚本失败时应优先判断 token 是否属于 flexUI 当前迁移范围；若属于，则补 whitelist 和相邻 fixture；若不属于，应在迁移计划中记录缺口，不要把未知 token 静默忽略。

## visual_demo staged migration

`flexUI/examples/visual_demo.cpp` 直接使用默认 Box catalog/theme，不再定位源码树 JSON。它通过显式 utility API 构建界面；Box 从矩形树扫描活跃 token，生成完整 CSS snapshot，再原子替换自己的 JIT stylesheet。

`visual_demo` 是 smoke 示例：whitelist 读取或 JIT 构造失败会让 demo 初始化失败。registry 外 class 保留给普通 CSS 使用，并由 `missing_utility_tokens()` 与 fixture 提供可检查的诊断。

这个模式适合分阶段迁移现有手写 CSS：

- 矩形树的 class 原文是活跃 utility token 的唯一事实源，不再维护平行的手工 token 清单。
- 未知 token 通过 `Box::missing_utility_tokens()` 暴露；registry fixture 同时检查 demo 中实际使用的 token。
- whitelist 只补当前阶段确实需要、且 flexUI style engine 能消费的 token。
- `test_shadcn_ir_fixtures` 中的 `covers visual demo utility tokens` 用例保证 demo token 与 whitelist 同步。

## 已知限制

- 当前支持目标是 shadcn 高频组件和 demo 迁移所需的 Tailwind-like 子集，不承诺兼容完整 Tailwind 语法、插件系统或主题解析。
- `utility_whitelist.json` 是手工维护的显式白名单；缺失 token 会由 `missing_utility_tokens` 或 registry coverage 暴露。
- 生成出的 CSS 仍受 flexUI style engine 能力约束。选择器、伪类、属性条件、媒体查询、伪元素、`ring` / `ring-offset` 等支持范围以 `flexUI/docs/CSS_SUPPORT_MATRIX.md` 和相邻测试为准。
- registry coverage 只证明 token 已有白名单展开，不等同于完整视觉一致性；仍需 conformance fixture、渲染语义测试或人工/截图验证补充。
- `visual_demo` 是迁移样例，不是全量 shadcn registry 覆盖清单。

本地验证：

```powershell
$vc='C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat'
cmd /c "`"$vc`" >nul && cmake --build build\codex-structural --target test_shadcn_conformance --config Debug -j 1"
```

```powershell
build\codex-structural\flexUI\tests\test_shadcn_conformance.exe
```

utility / registry 相关验证：

```powershell
$vc='C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat'
cmd /c "`"$vc`" >nul && cmake --build build\codex-structural --target test_shadcn_ir_fixtures --config Debug -j 1"
```

```powershell
build\codex-structural\flexUI\tests\test_shadcn_ir_fixtures.exe
```

若可执行文件启动时报 `0xc0000135`，先确认 `lexbor.dll`、`turbo_utils.dll` 与 MSVC debug runtime 在 `PATH` 中；这是 loader 依赖问题，不是 fixture 断言失败。

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File tools/shadcn/check_registry_coverage.ps1
```

visual demo 迁移样例的本地 smoke run：

```powershell
$vc='C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat'
cmd /c "`"$vc`" >nul && cmake --build build\codex-structural --target visual_demo --config Debug -j 1"
```

```powershell
build\codex-structural\flexUI\examples\visual_demo.exe
```
