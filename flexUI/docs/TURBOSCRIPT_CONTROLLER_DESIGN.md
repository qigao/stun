# FlexUI TurboScript 行为控制器设计

- 状态：提议，P0 契约与迁移边界已确认
- 日期：2026-07-15
- 范围：`flexUI` 行为脚本、Flex 动画边界、渲染 retained-mode 契约
- 明确排除：FFmpeg、媒体解码、用脚本重写布局/渲染/动画采样

## 1. 决策摘要

FlexUI 保留声明式 UI/样式与原生 C++ 运行时，引入 TurboScript 作为可选的行为控制器，
不引入 QuickJS，也不让 TurboScript 替换 `.flex` DSL、CSS、Timeline 或
`NumericExpression`。

```text
.flex / C++ Element tree / Tailwind JIT
                 │
                 ▼
          Box（UI 唯一事实源）
          ▲                 │
 event snapshot             │ UiMutation batch
          │                 ▼
   TurboScriptController ── narrow host bridge
          │
          └── lifecycle: mount / event / frame / unmount

Timeline + NumericExpression(MIR) ──> native property sampling
RendererCapabilities          ─────> immediate/retained render policy
```

每个 `Box` 至多拥有一个脚本上下文，并且上下文、Element 树和事件分发都归属同一个
UI 线程。脚本只能读取事件快照和通过命令批次申请 UI 变更；它不能持有 `Element*`、
`Renderer*` 或 TurboScript/exprtk 内部值。

## 2. 当前证据

### 事实

- `Box::update()` 先执行 `UiBindingRuntime::update()`，再推进 view pipeline；
  `Box::update_time()` 独立推进 transition、animation 和需要逐帧更新的 widget。
  证据：`flexUI/src/box.cpp`。
- `EventDispatcher` 已集中处理 hit-test、widget 事件、冒泡和全局回调，适合作为脚本事件
  的唯一入口。证据：`flexUI/src/event_dispatcher.cpp`。
- 数字/布尔 binding 已通过 Flex MIR 编译一次后重复求值，字符串 binding 使用类型化
  直接引用。证据：`flexUI/src/binding_runtime.cpp`。
- Timeline 使用原生关键帧查找、easing 与属性写入；只有表达式插值使用
  `NumericExpression` 的 MIR 路径。证据：`flex/modules/runtime/timeline.cpp`、
  `flex/modules/runtime/numeric_expression.cpp`。
- QuickJS 只存在于理论愿景文档，当前 CMake 和实现没有 QuickJS 集成。
  证据：`flex/docs/VISION.md` 与仓库检索结果。
- TurboScript 的公开 `turbo_script_compiled_t` 当前只保存源码，
  `turbo_script_exec()` 会再次走 MIR interpreter 路径；公开 API 没有“按名称调用已加载
  脚本函数”的宿主入口。内部 exprtk 虽可调用函数，但其值和 arena 生命周期不是稳定
  宿主 ABI。证据：`TurboScript/turbo_script/src/turbo_script.c`、
  `TurboScript/exprtk/include/exprtk.h`。
- ThorVG 当前明确把 `RendererCapabilities::retained_mode` 报告为 `false`，因为 subtree
  replacement 下 paint 顺序没有稳定契约。证据：
  `flex/src/backends/renderer_thorvg.cpp`。

### 推论

- Electron-like 的开发体验主要来自“声明式视图 + 事件驱动控制器 + 窄宿主 API”，并不
  要求浏览器 DOM 或完整 JavaScript 兼容层。TurboScript 的语言能力可以承担控制器，
  但前提是先补齐可限制、可诊断、可重复调用的宿主 API。
- 把脚本调用放进每条 animation track 或每个 Element 的逐帧路径会放大跨边界调用、
  分配和错误传播成本；没有 profile 证据前不应采用。

## 3. 目标与非目标

### 目标

- 支持窗口级行为模块：初始化、事件处理、可选帧回调和销毁。
- 保持 Element 树、binding 数据、动画状态与渲染状态各自只有一个事实源。
- 脚本失败时不提交半批 UI 修改，并向宿主返回可定位错误。
- 没有脚本或没有帧回调时，帧路径不进入脚本运行时。
- TurboScript 依赖可在构建时关闭，关闭时不改变现有 FlexUI 行为。

### 非目标

- 不提供浏览器 DOM、Node.js、npm、Web API 或 QuickJS 兼容层。
- 不允许脚本直接绘制、操作 cached paint 或决定 retained-mode。
- 不用 TurboScript 替换 Tailwind Utility JIT、CSS cascade、layout 或 `.flex` parser。
- 不用 TurboScript 替换 Timeline、easing、关键帧插值或 NumericExpression JIT。
- 不实现 FFmpeg 或任何媒体播放路径。

## 4. retained-mode 决策

### 4.1 是否仍然需要

需要保留“retained rendering 是可选优化”这一概念，但必须拆开两个语义：

- `supports_retained_mode()`：后端是否承诺 cached paint 的创建、删除、更新和顺序稳定。
- `retained_mode()`：当前帧/实例是否启用该策略。

当前基类默认 capability 从 `supports_retained_mode()` 推导，而 ThorVG 的
`supports_retained_mode()` 又返回当前 enabled 状态，混合了静态能力和动态状态。
这会让直接调用 retained replay 的代码绕过 `capabilities()` 的安全结论。

### 4.2 目标契约

- capability 在 renderer 生命周期内稳定，不因 `set_retained_mode()` 改变。
- 请求后端不支持的 retained-mode 必须返回明确错误，不能静默进入部分 retained 状态。
- FlexUI 只依据 capability 选择策略；脚本无权修改策略。
- ThorVG 在 paint 顺序契约和 subtree replacement 测试通过前保持 capability 为 false。

该拆分会改变公开 renderer 接口语义，实施前需用户确认。P0 只修正与当前实现冲突的旧
测试，不改变运行时行为。

## 5. TurboScript 运行时边界

### 5.1 分层

1. `TurboScriptHostAdapter`：只负责 TurboScript C API、模块生命周期、值转换和错误转换。
2. `ScriptController`：负责 mount/event/frame/unmount 调度、预算和 fault 状态。
3. `UiScriptBridge`：把只读快照转换为脚本值，并收集 `UiMutation`。
4. `Box`：验证并原子提交 mutation，然后按现有 dirty/binding/pipeline 机制更新。

高层只依赖小接口，不包含 `exprtk_value_t`、MIR handle 或 TurboScript 内部头文件。

### 5.2 生命周期

```text
load source
  -> compile/validate
  -> create module instance
  -> on_mount(host_snapshot)

native event dispatch
  -> widget handling/bubbling
  -> immutable ScriptEvent
  -> on_event(event)
  -> validate mutation batch
  -> commit once or discard all

frame(dt)
  -> on_frame(dt) only when exported/registered
  -> commit mutation batch
  -> bindings.update()
  -> transition/animation update
  -> layout/paint pipeline

unmount
  -> reject new callbacks
  -> on_unmount()
  -> destroy module/context
```

事件回调的插入点保持现有 widget consumption 语义：默认只把未被 widget 消费且仍在 route 中的
事件交给脚本，避免一次用户操作产生两套独立状态迁移。快照分别保存原始 `target` 与当前 binding
节点 `current_target`；脚本不持有 `Element*`。显式 consumed-event post-widget notification 尚未进入
XML schema，因此当前不会隐式启用。

### 5.3 UI 句柄与变更批次

脚本使用 `{id, generation}` 形式的 `UiHandle`，不使用地址。generation 由 Box ID index 单独拥有；
ID 改名、重复 ID owner 切换或 ID 复用后，旧句柄立即 stale。retained/detached 节点仍由 Box 持有，
句柄解析不替代 active-tree 与 target ownership 校验；未来删除 subtree 时必须先移除 index entry，
再释放元素。

首批 mutation 只开放稳定的高层动作：

- 设置/移除 attribute、class、utility token。
- 设置 binding input 的 number/bool/string。
- 设置 text/value。
- 启停命名 animation 或发送命名 trigger。
- focus/capture 等动作只有在现有 Box API 能表达完整不变量时才开放。

一次脚本 callback 只产生一个有容量上限的 batch。所有参数先验证，全部成功后再按顺序
提交；任一命令失败时整批丢弃。Element 树仍是 UI 事实源，mutation queue 不是第二份
状态。

### 5.4 错误语义

- load/parse/validate/required-export 失败：创建 controller 失败，Box 不进入半初始化状态。
- callback 超时、被中断或运行错误：丢弃本次 mutation batch，返回结构化错误。
- stale handle、类型错误、越界值、超额 batch：立即失败，不自动修复。
- 错误只在宿主消费边界记录一次，包含 module、callback、event/phase、error code 和位置。
- controller fault 后不继续执行其他 callback；恢复必须由宿主显式 reload。

## 6. TurboScript 必需的宿主 API

FlexUI 接入前，TurboScript 需要在其自身仓库提供稳定 C ABI。以下是能力契约，不是最终
函数名：

- 真正持有已解析/已 lower 模块的 compiled/module handle，重复执行不重新解析源码。
- 查询 export、按名称调用 export，并用公开 tagged value 传入/返回 number、bool、string、
  null 和受限数组/record。
- 明确返回 `status + error`，调用失败不能只依赖打印输出。
- 配置 recursion、loop/step、内存、栈、wall-clock/interrupt 限制。
- 安装 interrupt callback，宿主可以在超时、窗口关闭或 reload 时终止执行。
- 若保留 async，则提供显式 job pump；否则首版明确只支持同步 callback。
- 修复或移除只有声明没有实现、类型也不一致的 coroutine context API。

公开 ABI 使用 opaque handle；值的所有权、字符串生命周期、线程约束和错误对象释放方式
必须写入头文件测试。FlexUI 不直接调用 `exprtk_call_internal()`。

## 7. 构建与依赖

- 使用 `find_package(TurboScript CONFIG ...)` 和导出的 shared/imported target。
- 不使用 `add_subdirectory(C:/projects/cpp/TurboScript)`：两个仓库都定义 MIR targets，直接
  合并 target graph 会冲突。
- 增加构建选项 `FLEXUI_ENABLE_TURBOSCRIPT`；默认关闭直到 ABI、资源限制和集成测试全部
  通过。
- `TURBOSCRIPT_ROOT` 只作为 package 搜索 hint，不把本机源码绝对路径写进生产 target。
- TurboScript 类型只出现在 adapter 的 `.cpp`/私有头中，公共 FlexUI 头保持依赖隔离。

引入依赖前仍需核对 TurboScript 的许可证、安装包导出、Debug/Release ABI、Windows/Linux
构建和二进制体积。

## 8. 性能设计与验证

- JIT 用于模块编译和重复的粗粒度 callback，不用于每个 Element、每条 track 或每个
  property 的边界调用。
- event snapshot 和 mutation batch 使用有上限的连续存储；首版不跨线程。
- 只有存在 `on_frame` 时才进入脚本帧路径；静态页面的脚本开销应为零次 callback。
- Timeline target/property 缓存、keyframe cursor 等优化必须先由 profile 证明为热点，
  单独 benchmark，不与脚本接入捆绑。

验收至少记录：模块加载/编译耗时、首次与稳态 callback 延迟、每帧分配、mutation 数量、
超时中断延迟、无脚本页面开销。相对当前基线，非脚本页面延迟回归不得超过 10%，吞吐
回归不得超过 10%，峰值内存回归不得超过 20%。

## 9. 兼容性、迁移与回滚

### 兼容性风险

- `Renderer::supports_retained_mode()` 语义拆分是公开接口变化；所有 backend 和 mock 都需
  同步迁移。
- Box 事件回调若改变 widget 消费顺序，会改变应用行为；必须用 capture/bubble/handled
  组合测试锁定。
- TurboScript package 会增加可选依赖、二进制体积和部署文件。
- 脚本 reload、句柄失效与 mutation 原子提交会引入新的错误类型。

### 迁移路径

- 先修正 retained capability 的当前测试事实，再单独迁移 renderer 契约。
- 先在 TurboScript 仓库完成宿主 ABI 和资源限制，再做 FlexUI adapter。
- adapter 首先作为独立组件测试，不接管 Box 默认路径。
- 通过 feature option 和显式 `BoxOptions` 启用 controller；完成回归后再讨论默认值。

### 回滚

- 关闭 `FLEXUI_ENABLE_TURBOSCRIPT` 即回到现有 C++/binding/animation 路径。
- controller 未创建时不注册事件和 frame hook，不改变 Box 调度。
- retained-mode 迁移可按 backend 独立回滚；ThorVG 始终可安全回到 full-frame replay。

## 10. 决策结论

- 保留 retained-mode，但先修正“能力”与“启用状态”的契约。
- Electron-like 行为采用窗口级 controller 和命令批次，不复制 DOM/浏览器运行时。
- 选择 TurboScript 代替愿景中的 QuickJS，条件是先补齐稳定宿主 ABI 与执行限制。
- 动画 DSL 和原生 Timeline 保留；现有 NumericExpression MIR 已是正确的 JIT 层次。
