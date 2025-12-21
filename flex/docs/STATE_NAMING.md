# State Class Naming

## 重命名原因

**问题**：命名冲突
- `machine.h` 已有 `State` 类（状态机的状态节点）
- 新建的 `state.h` 也定义了 `State`（Observable 状态容器）

**解决方案**：
- 重命名 `state.h` 的类：`State` → `ObservableState`
- 保持 `machine.h` 不变（Never break userspace）

---

## API 变更

### Before (冲突版本)
```cpp
#include "flex/state.h"

auto state = flex::State::create();
state->set("volume", 0.5f);

flex::StateComponentBinding binding(state, ...);
flex::ReactiveGroup::create(state);
```

### After (修复后)
```cpp
#include "flex/state.h"

auto state = flex::ObservableState::create();
state->set("volume", 0.5f);

flex::StateComponentBinding binding(state, ...);
flex::ReactiveGroup::create(state);
```

---

## 两种 State 的区别

| 类名 | 文件 | 用途 |
|------|------|------|
| `flex::State` (machine.h) | machine.h | 状态机系统的状态节点 |
| `flex::ObservableState` (state.h) | state.h | 响应式数据绑定的状态容器 |

**类型安全**：两者现在不会混淆，编译器会检查类型。

---

## 示例代码

```cpp
#include "flex/state.h"
#include "flex/machine.h"

// Observable State (reactive data binding)
auto app_state = flex::ObservableState::create();
app_state->set("volume", 0.8f);
app_state->watch("volume", [](const string& key, const StateValue& val) {
    std::cout << "Volume: " << std::get<float>(val) << std::endl;
});

// State Machine State (FSM logic)
auto machine_state = flex::State::create("idle");
machine_state->add_transition(flex::Condition::event("start"), "running");
```

---

## STATE_MANAGEMENT.md 文档更新

文档中所有提到 `State` 的地方应理解为 `ObservableState`：

- `State::create()` → `ObservableState::create()`
- `State::Ptr` → `ObservableState::Ptr`
- `auto state = State::` → `auto state = ObservableState::`

核心概念和 API 用法保持不变。
