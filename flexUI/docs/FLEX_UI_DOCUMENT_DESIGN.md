# Flex DSL 到 FlexUI Element Tree 设计

- 状态：最小 vertical slice 已实现并验证
- 日期：2026-07-15
- 范围：Flex `ui` 语法、不可变 UI definition、Box-owned Element tree
- 关联：[TURBOSCRIPT_CONTROLLER_DESIGN.md](TURBOSCRIPT_CONTROLLER_DESIGN.md)

## 决策

Flex 可以声明完整 UI，但 Flex core 不依赖 `flexUI::Element`。依赖方向保持为：

```text
.flex source
  -> Flex lexer/parser
  -> parser-owned AstUiDocument
  -> shared_ptr<const flexUI::UiDocumentDefinition>
  -> UiDocumentInstantiator(Box&)
  -> Box-owned Element tree
```

现有 `scene` 保持矢量场景语义；新增 `ui` 顶层块表达应用 UI。两者可以出现在同一文件，
后续由 Canvas/widget bridge 显式组合，禁止共享裸节点所有权。

## 当前事实

- `flexUI::Element` 已继承 `flex::Group`，可以复用 Node 变换、dirty flags 与 Group 布局。
- `Box` 通过 `elements_` 独占 Element 生命周期，并维护 ID 索引、Widget、事件、Utility
  JIT 与 theme root。
- 当前 Flex AST lowering 创建 arena/shared-owned `Group/Shape/Text/...`，不经过 Box。
- 当前 Flex `Component` builder 返回 `flex::Node`；直接返回 Element 会绕过 Box ownership，
  因此不能作为 UI document 的创建入口。

## `ui` 语法

```flex
ui MainWindow {
    div root {
        utility: "flex min-h-screen flex-col",
        role: "application",

        header toolbar {
            class: "app-toolbar",
            text: "Editor"
        }

        button save {
            utility: "rounded-md px-4 py-2",
            text: "Save",
            focusable: true,
            on.click: "save_document"
        }
    }
}
```

每个 document 必须有且只有一个 root。每个节点必须有非空且 document 内唯一的 ID。
节点 type 直接成为 Element tag，因此首版允许任意 tag，不在 parser 中维护 HTML tag 白名单。
首版属性之间必须使用逗号；换行仅是空白，不能作为属性分隔符。最后一个属性后的逗号可省略。

## Definition 契约

`UiDocumentDefinition` 是值语义定义：name、root、tag、id、typed properties 和有序 children。
解析入口以 `shared_ptr<const UiDocumentDefinition>` 交付只读定义；C++ 调用方手工构造的定义会在
实例化入口重新执行同一套校验。定义不包含 `Element*`、Widget、computed style、renderer 或
TurboScript value。

解析入口返回结构化 result：

- lexer/parser 错误包含 Flex parser 的 line/column。
- document 缺失、名称歧义、root 数量、重复 ID 和 property 类型错误在 semantic validation
  阶段失败。
- source、节点数、深度和单节点 property 数受 `UiDocumentLimits` 限制。

最小完整用法：

```cpp
#include <flexUI.h>

int main() {
  const auto parsed = flexUI::parse_ui_document("ui Main { div root {} }");
  if (!parsed) {
    return 1;
  }
  flexUI::Box box(nullptr);
  const auto installed = flexUI::UiDocumentInstantiator::instantiate(box, *parsed.definition);
  return installed ? 0 : 2;
}
```

## Instantiation 与状态所有权

`UiDocumentInstantiator` 采用 validate/build/commit 三阶段：

1. 验证目标 Box 尚未安装 root、ID 不冲突、显式 utilities 在当前 catalog 中存在。
2. 在 Box 外用 `unique_ptr<Element>` 构建完整 detached tree。
3. 预分配 Box ownership/index 容量后一次提交，并安装 root。

提交成功后 Box 是唯一事实源；definition 仍是只读模板。首版不提供 definition 与 Element 的
双向同步，也不维护第二棵 Flex Scene 镜像。

## Property mapping

- `class` / `classes`：普通 CSS classes。
- `utility` / `utilities`：显式 Tailwind-like utilities，要求 JIT 已启用且 token 已知。
- `text` / `content`：Element text。
- `focusable`：bool。
- `tab_index`：有限整数。
- `attr.name`：显式 attribute；bool 转换为 `true`/`false` 字符串。
- `on.event`：保存为 `data-flexui-on-event`，供后续 TurboScript controller 绑定。
- 其他 property：作为同名 attribute；bool true 表示存在，false 表示不存在。

同一 alias 组不能重复，例如不能同时设置 `class` 与 `classes`，避免 unordered property map
造成覆盖顺序不确定。

## 扩展边界

后续 `UiComponentRegistry` 通过构造函数注入到 instantiator，把 tag 映射到 Widget/subtree
factory。Registry 必须先做纯验证，再参与 detached build；不能使用全局单例，也不能返回
不受 Box 管理的 Element。

TurboScript 只接收 `{id, generation}` handle 和 `on.*` 事件，不接收裸 Element 指针。
脚本 mutation 仍提交到同一 Box tree。

## 兼容性与回滚

- 新增 `ui` token 和 `AstProgram::ui_documents`，不改变现有 `scene`/component lowering。
- `ui` 成为 Flex 保留关键字；旧 DSL 若把裸 `ui` 用作 tag、property 或其他标识符，需要在
  迁移时改名。该语法兼容性变化由 lexer/parser 测试固定。
- FlexUI 增加对 `flex_compiler` 的私有链接；公共 UiDocument 头不暴露 parser AST。
- 关闭或不调用 UiDocument API 时，现有手写 Box tree 行为不变。
- 回滚只需移除 `ui` parser 分支与 flexUI adapter，不涉及 scene binary format。

## 验证范围

- lexer 识别 `ui`，parser 保留 document/name/tree/properties。
- scene 与 ui 共存且互不覆盖。
- definition 拒绝多 root、重复 ID、错误 alias/type 和资源超限。
- instantiator 保留 child order、class/utility/text/attributes/event metadata。
- 安装失败时 Box root 保持不变；成功后 ID 查询和 Utility JIT 正常工作。
