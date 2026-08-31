# FlexUI TurboScript 行为控制器实施计划

- 状态：P0、P2 与 P3 私有 adapter 已落地；P1 retained-mode 公开契约与 P4 Box
  默认调度接入仍需单独审查
- 日期：2026-07-15
- 设计依据：[TURBOSCRIPT_CONTROLLER_DESIGN.md](TURBOSCRIPT_CONTROLLER_DESIGN.md)
- 外部源码：`C:\projects\cpp\TurboScript`
- 范围排除：FFmpeg、媒体播放、QuickJS、脚本化逐 track 动画采样

## 2026-07-15 验证记录

- `cmake --build --preset win-dev-user --target test_standard --parallel 4`：通过。
- `ctest --preset win-dev-user -R "^test_standard$" --output-on-failure`：1/1 通过。
- 初次尝试的 `flex_tests` 不是仓库 target；经 Ninja target 列表确认实际最小目标为
  `test_standard`，未修改 CMake 或清理 build tree。

## 使用规则

- 每阶段先运行最小测试，再运行相邻回归。
- 不通过 silent fallback、异常吞噬或自动切换解释器让测试通过。
- 不把 `exprtk_value_t`、MIR handle 或 `Element*` 暴露到跨模块接口。
- 用户工作树已有 Tailwind/charts 改动；脚本接入不修改这些文件，发生 overlap 时停止并
  重新审计。
- P1 retained-mode 的公开接口变化仍需单独审查；P2 TurboScript 宿主 ABI 可按
  `FLEXUI_DESKTOP_PLAN.md` 推进，但冻结 ABI 前仍需完成兼容性测试。

## 依赖顺序

```text
P0 契约与设计
  ├── P1 retained-mode 语义拆分
  └── P2 TurboScript 宿主 ABI
          └── P3 FlexUI 私有 adapter
                  └── P4 Box controller 集成
                          └── P5 性能、安全与发布收口
```

P1 与 P2 可独立实施；P3 依赖 P2；P4 依赖 P3。P5 要求 retained、脚本和无脚本路径都
完成验证。

## P0：契约与设计基线

- [x] 同步 CodeGraph 并阅读 renderer、Box、event、binding、timeline、TurboScript public/
  internal API 和对应测试。
- [x] 记录 retained capability 与 enabled state 混用问题。
- [x] 修正 ThorVG capability 旧测试，使其与当前安全 full-frame 行为一致。
- [x] 明确 `.flex`/CSS/Timeline/动画 MIR 与 TurboScript controller 的职责边界。
- [x] 记录线程、状态所有权、错误语义、迁移、回滚、构建和 benchmark 门槛。
- [x] 明确不实现 FFmpeg。

完成条件：不改变当前运行时行为；ThorVG capability 最小测试通过；后续公开接口改动有
明确文件、测试和回滚路径。

## P1：retained-mode 契约拆分（需确认公开接口变化）

### API 与 backend

- [ ] 将 backend capability 与当前 enabled state 分成独立查询。
- [ ] 让 capability 在 renderer 生命周期内稳定。
- [ ] 为“不支持 retained-mode 的 enable 请求”定义明确错误返回。
- [ ] 更新 ThorVG、NanoVG、D2D、TUI 和测试 renderer/mock。
- [ ] 确保 `Shape` cache 和 `RenderCommandList::replay_retained()` 只依据 capability 进入。
- [ ] ThorVG 在 ordering contract 完成前继续报告不支持。

### 测试

- [ ] capability 不随 enable/disable 改变。
- [ ] unsupported enable fail fast，且不留下部分状态。
- [ ] direct retained replay 无法绕过 capability。
- [ ] subtree insert/remove/replace 的 paint 顺序与 full replay 一致。
- [ ] 运行 `flex_tests` 与 `test_render_semantics`。

完成条件：所有 backend 对 capability/enabled 的语义一致，旧公开调用点完成迁移，渲染
输出不变。

## P2：TurboScript 稳定宿主 ABI（在 TurboScript 仓库实施）

### Module 与调用

- [x] 将 source-backed compiled object 改为持有可重复执行的解析/lower 产物，或新增明确
  命名的 module API 并保留旧入口兼容。
- [x] 新增 export 查询与按名称调用入口。
- [x] 定义公开 tagged value、参数数组、返回值和所有权规则。
- [x] 定义结构化 status/error，禁止由 stdout 充当错误通道。
- [x] 跨 ABI 保持 opaque handle，不暴露 exprtk/MIR 内部结构。

### 资源限制

- [x] 增加 recursion、loop/step、stack、memory 和 interrupt 配置。
- [x] 测试 timeout/interrupt 后 context 是否仍可安全销毁或显式 reload。
- [x] 首版选择同步 callback；若保留 async，增加显式 job pump 和关闭语义。
- [x] 修复或移除未实现且 coroutine 类型不一致的 public declaration。

### 测试与文档

- [x] 覆盖 load/call/repeat/error/type/ownership/stale module/interrupt。
- [x] 修正文档中 runtime 默认 limits 与实现不一致的问题。
- [x] 修正文档把 direct-mapped cache 描述为 LRU 的问题，或实现并验证真正 LRU。
- [x] 运行 TurboScript 最小 API 测试、MIR interpreter/JIT 对照测试和 benchmark。
- [x] 安装并验证 `TurboScriptConfig.cmake`、Debug/Release imported target。

完成条件：独立 C/C++ host 不包含 internal header 即可安全加载模块、重复调用 export、限制
资源并取得结构化错误。

## P3：FlexUI 私有 TurboScript adapter

### 构建

- [x] 增加 `FLEXUI_ENABLE_TURBOSCRIPT`，未启用时不查找、不链接 TurboScript。
- [x] 使用 `find_package(TurboScript CONFIG REQUIRED HINTS ...)`，不使用源码
  `add_subdirectory`。
- [x] TurboScript include/link 只对 adapter 私有可见。

### Adapter

- [x] 新增 `IScriptModule` 小接口和 TurboScript 实现，使用 Pimpl/opaque handle。
- [x] 实现 source/module 生命周期、export 缓存、值转换和错误转换。
- [x] 断言 controller/module context 只由创建它的 UI 线程访问。
- [x] 增加 bounded `ScriptEventSnapshot` 与 `UiMutationBatch`。
- [x] mutation validation 与 commit 分离，失败时整批丢弃。
- [x] 将有界 `ApplicationCommandBatch` 与 UI mutation 分离，先 reserve queue、后 commit UI、
  最后无失败 publish；adapter 只生成 envelope，不直接执行 service。

### 测试

- [x] fake module 测 controller 调度、timeout、injected error 与真实 Box 状态不变性，不依赖
  TurboScript。
- [x] adapter contract 测真实 TurboScript load/call/error/interrupt。
- [ ] stale `UiHandle`、超额 batch、错误类型、删除 subtree 后调用均 fail fast（前三项已完成；
  Box 尚无通用 subtree destruction API）。
- [x] ASan 可用配置下检查 load/mount/event/frame/unmount 生命周期；UBSan 留待
  Linux preset。

完成条件：adapter 可独立验证，不修改 Box 默认事件、update 或 render 路径。

### P3 已实现契约

- `FlexUI::ControllerTurboScript` 是独立可选 target；feature 关闭时不会执行
  `find_package(TurboScript)`。
- `create_turboscript_controller_module()` 复制 source/module name，固定选择 MIR interpreter
  或 JIT，并在创建线程上拥有 context、result、immutable module 与 stateful instance。
- `on_mount`/`on_unmount` 接收零参数，`on_frame` 接收秒单位 `number`，事件 handler 接收
  一个只读 record。事件 record 包含 `kind`、原始 `target{id,generation}`、绑定节点
  `current_target{id,generation}`、坐标、滚轮、按键、text/composition、timestamp、
  handled/propagate 与两个 handle 的 validity 标志。
- 每个 callback 必须显式返回 `null` 或一个 effect record。effect 只允许顶层
  `mutations`/`commands` 数组；未知字段、错误类型、非 finite 数字、无效 target/request ID、
  单条或整批字符串超限都会让本次 callback 整体失败，不返回部分 batch。
- mutation record 支持 `set_text`、`set_attribute`、`remove_attribute`、`set_classes`、
  `set_utilities`、`set_binding_number`、`set_binding_bool`、`set_binding_string`。command record
  使用 `{request_id, capability, operation, payload}`；adapter 只复制为拥有所有权的 batch，
  Controller 仍按“reserve command → prepare/commit UI → publish command”顺序提交。
- export arity 在调用前由 metadata 校验；编译、资源限制、中断、无效 handle 和运行时错误
  转换为 `ScriptModuleError`，controller 继续负责 Faulted 状态迁移。
- adapter 默认拒绝 native plugin；interrupt callback 同步运行于 UI owner thread。所有输入
  view 只借用到同步调用返回，TurboScript result 不越过 adapter 边界。
- Windows 测试会复制 TurboScript 的直接 runtime DLL；CoroNet 当前安装包仍要求可发现配套
  `ssl.dll`/`crypto.dll`，测试配置对此显式 fail fast。

## P4：Box 行为控制器集成（需确认 FlexUI API）

### 调度

- [x] 通过显式 `DesktopApplicationBuilder`/factory 安装 controller，不把脚本参数塞入 renderer。
- [x] 定义 mount/event/unmount export 名称和 required/optional 规则；frame hook 尚未接入。
- [x] 把脚本事件接在 widget consumption 语义之后，并锁定未消费事件的 target→ancestor bubbling。
- [ ] 只有导出/注册 frame callback 时才执行脚本帧 hook。
- [x] callback 成功后一次提交 mutation；frame pipeline 推进留给 DesktopHost。
- [x] controller fault 后停止 callback，显式 reload 才恢复。

### Bridge 能力

- [x] 首批 mutation engine 开放 class/utility/attribute/text/value/binding input/animation trigger。
- [x] 不开放 renderer、paint cache、裸 Element 指针或任意 native function 注册。
- [x] event/controller API 写明参数范围、错误码、句柄生命周期和 owner-thread 约束。
- [x] `desktop_xml_tbs_demo` 提供可独立构建运行的 XML/CSS/TBS + C++ composition 示例；
  native window host 示例留待 DesktopHost 阶段。

### 测试

- [ ] mount/event/frame/unmount 顺序。
- [ ] widget consumed、bubble、propagate、focus/capture 组合。
- [ ] callback 失败不产生部分 DOM/binding 状态。
- [ ] 脚本触发 native Timeline，但不参与逐 track sampling。
- [ ] 无脚本 Box 与 feature-off build 的行为和 command snapshot 不变。

完成条件：脚本可以实现 Electron-like 窗口行为，同时 Element、binding、animation、renderer
状态仍各有唯一 owner。

## P5：性能、安全与发布收口

- [ ] 建立 load、first call、steady call、event、frame、mutation commit benchmark。
- [ ] 记录无脚本基线；延迟/吞吐回归不超过 10%，峰值内存回归不超过 20%。
- [ ] 验证无 `on_frame` 时零脚本 frame callback。
- [ ] 验证超时、无限循环、深递归、超额分配、窗口关闭中断。
- [ ] 验证 module reload、旧句柄失效、context 销毁和错误日志只记录一次。
- [ ] 运行 FlexUI 相关 CTest、Flex animation tests、renderer tests 和 TurboScript tests。
- [ ] 更新 `flex/docs/VISION.md`，把 QuickJS 愿景替换为已验证的 TurboScript 架构。
- [ ] 评估许可证、部署文件、Debug/Release ABI、Windows/Linux 包安装和二进制体积。

完成条件：功能开关打开/关闭均可重复构建测试，性能与资源限制达标，文档只描述已实现
行为。
