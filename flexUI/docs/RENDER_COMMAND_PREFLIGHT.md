# 渲染命令提交前检查

本切片承接 [原生 Tailwind / 后端设计](TAILWIND_NATIVE_BACKENDS_DESIGN.md) 和
[效果契约、共享预算基础](../../flex/docs/EFFECT_ADMISSION_PROFILE.md)，属于
[#17](https://github.com/qigao/stun/issues/17) 的 M2 迁移工作。

它检查已经录制的 `RenderCommandList`，在执行前拒绝非法参数、超限输入和
当前 renderer 明确不支持的操作。`RenderManager` 对完整帧检查后才允许
清屏、surface recreation 查询、旧 retained cache 删除或命令重放。
这一边界不增加 CSS 输入、CSS 生成器、CSS fallback 或新的样式执行器。

## 1. 三个层次的责任

| 层次 | 本轮状态 | 责任与边界 |
| --- | --- | --- |
| 命令快照检查 | 接入实际 immediate / retained / RenderManager 路径 | 检查当前快照的值、大小、帧结构，以及现有 capability 明确为假的操作 |
| 语义效果契约与预算 | 已有独立 CPU 基础 API | 匹配 effect/version、adapter、device、产物观察与资源声明，保留共享预算租约 |
| 原生资源准备与发布 | 待接入 | 实际创建 shader/pipeline/surface，计算原生费用，绑定 owner/generation，发布对应 UI/几何/计划并按完成协议退休 |

现有命令不携带完整的语义 effect 身份、所有状态需求、产物观察、资源域及
设备代际。不能根据 `caps.blur == true` 自动注册一个“已验收”的效果实现，
也不能生成一份虚构 manifest 连接上述层次。

本轮没有调用或消费 `prepare_effect_admission` 的租约；后续连接必须交给
实际持有原生资源及提交完成凭据的 owner。`end_frame()` 返回不等于 GPU
完成，也不是释放 in-flight 费用的依据。

## 2. API 与执行边界

`RenderCommandList` 提供以下检查接口：

```cpp
RenderValidationResult validate_replay(
    const Renderer& renderer,
    RenderValidationScope scope = RenderValidationScope::Commands,
    const RenderValidationLimits& limits = {}) const;

void require_valid_replay(
    const Renderer& renderer,
    RenderValidationScope scope = RenderValidationScope::Commands,
    const RenderValidationLimits& limits = {}) const;
```

`validate_replay` 返回带有错误分类、命令索引和静态诊断文本的结果。
`require_valid_replay` 在失败时抛出 `RenderValidationException`；
`diagnostic()` 保留同一结构化结果。

错误分类为 `InvalidLimits`、`ResourceLimit`、`InvalidValue`、
`UnsupportedOperation` 和 `InvalidSequence`。全局配置/帧入口错误使用索引
0；命令数超限指向第一个越界位置；帧结束时发现的未配对 save 指向末尾
`EndFrame`。诊断文本不借用输入字符串。

两个 scope 有明确差别：

| Scope | 检查规则 | 用途 |
| --- | --- | --- |
| `Commands` | 值、存储限额、局部 save 深度、明确不支持的操作；允许继承调用者的帧和 save 状态 | 保留现有绘制片段、单独 frame-start / frame-end、继承 restore 的调用方式 |
| `CompleteFrame` | 在上述检查上要求首条 BeginFrame、末条 EndFrame、没有额外帧标记、没有 restore 下溢、最终 save/restore 平衡 | 独立完整帧的提交边界 |

原有 `replay(renderer)`、`replay_retained(renderer, cache)` 签名继续存在，
内部在任何执行/缓存修改前以 `Commands` scope 检查。
新增重载允许传入 `RenderValidationLimits`。

`RenderManager::render_frame` 对已经拼好的 `frame_signature` 进行
`CompleteFrame` 检查。必须在这一层检查整帧：只在 start、body、end 三个
片段各自重放前检查，会让后置非法命令在清屏之后才被发现。
`render_tree` 和独立 overlay 路径仍使用片段规则。

下面的调用使用实际公开接口，适合直接提交完整的录制帧：

```cpp
#include <flexUI/render_command.h>
#include <flexUI/renderer.h>

void submit_frame(flexUI::Renderer& renderer,
                  const flexUI::RenderCommandList& commands) {
  flexUI::RenderValidationLimits limits;
  commands.require_valid_replay(
      renderer, flexUI::RenderValidationScope::CompleteFrame, limits);
  commands.replay(renderer, limits);
}
```

检查读取执行时 renderer 的 capabilities，不信任录制时保存的能力副本。
capabilities 是读取接口；检查阶段不调用 begin/clear、绘制、retained mode、
push/update/remove 等修改接口。

调用者必须在同一同步 owner 上保持命令、renderer 和所用能力稳定，不能在
检查或重放期间并发修改，也不能在 `begin_frame` 等回调中改变能力。
此 API 没有返回跨线程、跨设备代际的准入令牌。
检查本身不会创建原生资源；capabilities 查询抛出的异常原样传播。

## 3. 值域与有界输入

默认限额如下，所有配置项必须大于零：

| 配置 | 默认值 | 计费范围 |
| --- | ---: | --- |
| `max_commands` | 65,536 | 当前命令 vector 的元素数 |
| `max_payload_bytes` | 8 MiB | 所有字符串字节和所有 paint 内 gradient stop 存储的总和 |
| `max_string_bytes` | 1 MiB | 每个 text/font/path/asset/source/data 字符串的 `size()` |
| `max_gradient_stops` | 256 | 每个 linear 或 radial stop vector 的元素数 |
| `max_state_depth` | 256 | 当前列表中局部 save 栈的最大深度 |

payload 的定义是：

```text
sum(string.size()) + sum(stop_vector.size() * sizeof(flex::ColorStop))
```

重复字符串分别计费；即使 paint 当前为 None/Solid，内含但未激活的
linear/radial stop vector 仍计入存储预算。先用除法验证乘法边界，累计使用
剩余预算比较，避免长度算术溢出。

固定大小的 command variant 由命令数限制；字符串终止符、容器 capacity、
分配器开销和 backend 原生内存不计入上述 payload。录制可能已经分配了这些
对象，因此这是执行输入的限额，不是整个录制过程的峰值内存证明。
`RenderManager` 当前使用默认限额；独立命令重放可显式传配置。

具体值域遵循已有 renderer 的输入约定：

- 数值必须有限；帧宽高和 pixel ratio 必须大于零。
- 尺寸、圆/椭圆半径、线宽、blur 半径必须非负；检查直接矩形边缘和圆/椭圆
  边界的有限性。signed shadow offset/spread 保持可用。
- 颜色和 global alpha 要求有限，保留现有 backend 的有限越界值处理；
  本层不额外强制 `0..1`。
- active gradient 至少有一个 stop，坐标、offset 和颜色有限；保留单 stop、
  未排序 offset 和有限的越界 offset。未知 paint kind 明确失败。
- 矩阵必须有限且是 affine 2D。若 rotation/scaling 都不支持，只允许
  translation 矩阵。这里不通过不可靠的矩阵分解推导独立能力。
- 当前 capabilities 为假时，非恒等 rotate/scale、可见 shadow、非零 blur、
  可见 path/stroke，以及非空且面积为正的 image/SVG 被拒绝。
- 空内容、恒等效果、透明 shadow 和零尺寸操作保留其合法 no-op 语义；
  其中的非法数值和超限存储仍会被检查。

raw append 的空内容、不可见或退化绘制，先完整校验再按与 recording helper
相同的规则跳过 dispatch。retained plan 将这些槽标记为 non-cacheable，移除
同槽原有的可见 handle，不加载空图片或创建无效文字资源。effect/state setter
继续执行，因为设为零 blur 或透明 shadow 可能用于清除之前的效果状态。

验证器直接遍历最终 vector，使用 raw `append` 也不能绕过检查。其内部不
分配内存，时间复杂度为命令数加检查到的 stop 数量，额外空间为常量。
它不模拟变换组合、clip 栈、DPI 放大、效果扩展区域，也不解析文字、路径、
图片或 SVG 的内部内容。

## 4. 失败与脏状态恢复

在稳定 owner 前提下，完整帧的检查失败发生在后端提交与旧 retained cache
修改之前。已有 cache 和 Tango cell 内容因此可继续使用。`replay_retained`
自己的检查也在 `set_retained_mode`、删除/更新/创建 handle 之前。

录制过程中仍保留原先清除 Visual dirty 的时机，避免在提交成功后统一清除
dirty 而误删随后 widget 或 renderer callback 在 Element 上设置的新 Visual flag。
作用域只记录本次清掉的 Visual flag，且条目数受默认命令数限额约束。

- 录制、检查或重放失败时，对仍存活的节点恢复这些 flag。
- 通过节点 lifetime token 跳过已经销毁的元素；不延长元素所有权。
- `build_frame_commands` 只生成诊断/离屏命令，不消费元素的 Visual dirty，正常
  返回前显式恢复。若父节点传播抛出异常，先继续恢复其余节点，再传播首次
  恢复异常。
- 已有异常展开时，析构执行不抛出的恢复，保留原始失败；base Visual flag
  在可能抛出的 parent 传播前已经恢复。
- 正常 `render_frame` / `render_tree` 重放完成后提交本作用域，不覆盖任何
  后来的 dirty 修改。thread-local profiler 指针也由作用域恢复。

这些规则只恢复本轮录制消耗的 Visual dirty。widget 录制期间的业务状态、
style/layout 修改、已开始绘制后的像素/终端输出、驱动错误，以及 retained
执行中途的资源分配失败都不因此获得完整事务回滚。
上层 Box 的调度 flag 与 Node/Element Visual flag 是两个已有状态来源：当前
`Box::run_render_stage` 在 manager 返回后清除自己的 paint flag。本切片不证明
回调新增的 paint 通知能跨越该 Box 调度边界；应用级重新调度还需独立验收。

## 5. Tango 与共享值检查

`flex/core/render_value_validation.h` 提供 shadow/blur 的同一组有限值检查。
D2D、gCanvas 保留原有错误与支持范围，只复用该值检查。

Tango 的直接 setter 现在明确区分：

| 输入 | 结果 |
| --- | --- |
| 非有限 shadow 字段、负 blur 或非有限 blur | `std::invalid_argument` |
| 合法但可见的非空 shadow、合法非零 blur | `std::logic_error` |
| 零 blur、恒等/透明 shadow、clear 操作 | 保留合法 no-op |

Tango 没有 GPU shader。此改动既不假装用 shader 实现终端效果，也不替用户
自动选择另一个后端。其他旧操作的无声忽略、alpha/圆角近似、pixel-to-cell
转换和 TerminalCell 目标迁移仍需按各自语义退役或实现。

## 6. 验证与剩余工作

测试使用实际生产 `render_command.cpp` / validation 源、真实 TinyTest 和
真实 Tango C 库及 renderer。覆盖 raw 注入非法值、完整帧与片段、精确限额
边界、当前能力变化、retained 旧 cache 保持，以及真实含宽字符续格的 cell
buffer 在后置非法效果下保持原样。另有链接实际 Core 的 RenderManager
边界测试，覆盖 dirty 恢复、callback 新 dirty、元素销毁和 surface recreation。

这些目标加入既有普通 suite、Linux XML qualifier 和 Windows/macOS headless
选择；使用原 workflow/cache/evidence 路径。具体执行结果与源 HEAD 记录在
对应 PR 和 #17，不把测试注册、syntax 检查或不同 HEAD 的结果算作本次运行。

本层只能看到已经进入命令列表的操作。当前旧 style producer 会根据 capability
跳过部分不支持的效果，丢掉的需求无法被后置检查重建。下一步必须从原生
typed program 与 Widget 状态提取完整需求，在任何 style/layout/UI publication
前做契约准入和原生资源准备，并覆盖 hover/animation/overlay 等可达状态。

真实 shader 产物加载与反射、原生费用/预算租约绑定、资源 owner/generation、
完整更新原子发布、运行期失败、设备丢失/恢复、提交同步和退休队列仍是
M2/M3 的未完成门槛。CPU 命令测试和真实 Tango buffer 测试不等于新 GPU
效果验收，也不替代实际 PTY/终端输出证据。
