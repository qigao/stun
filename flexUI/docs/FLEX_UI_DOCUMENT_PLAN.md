# Flex UI Document 实施计划

- 状态：P0/P1/P2 完成，P3 待实施
- 日期：2026-07-15
- 设计依据：[FLEX_UI_DOCUMENT_DESIGN.md](FLEX_UI_DOCUMENT_DESIGN.md)

## P0：架构与影响面

- [x] 同步 CodeGraph。
- [x] 阅读 Element、Box、Flex Group/Scene、AST/parser/lowering、Component 和现有 IR
  instantiator。
- [x] 确认依赖方向为 `flexUI -> Flex parser`，不让 Flex core 依赖 flexUI。
- [x] 记录 ownership、错误语义、资源限制、兼容与回滚。

## P1：最小 vertical slice

- [x] lexer/lemon grammar 增加 `ui` 顶层块。
- [x] AST 增加 `AstUiDocument` 与 `ui_documents`。
- [x] AstBuilder 支持多个命名 UI document、node nesting 与 loop expansion。
- [x] 增加只读解析结果 `UiDocumentDefinition`、limits 和结构化 result。
- [x] 增加 Box-owned detached build/commit instantiator。
- [x] 支持 class/utility/text/focus/tab/attribute/on-event property mapping。
- [x] FlexUI 私有链接 `flex_compiler`，公共头不暴露 parser 类型。
- [x] 更新 FlexUI umbrella header。
- [x] 明确首版属性逗号分隔契约；换行不作为 token。

## P2：测试与验收

- [x] lexer/parser 测 `ui`、property-key token 冲突及 scene 共存。
- [x] semantic validation 测 root、ID、alias、type、limits 和 document selection。
- [x] instantiation 测 ownership、ID index、child order、Utility JIT 和 event metadata。
- [x] 失败路径验证 Box root 不变。
- [x] 构建并运行 `test_lexer_parser`、新 UiDocument test、`test_utility_jit` 和
  `test_style_engine`。

2026-07-15 本地验收：

- `test_lexer_parser`、`test_style_engine`、`test_utility_jit`、`test_ui_document`：4/4
  通过。
- `test_standard`、`test_binding_runtime`：2/2 通过。
- `test_ui_document` 在改用 umbrella header 并补充手工 definition/type/JIT-disabled
  用例后单独复跑：1/1 通过。
- 增加 UI 属性分隔符 fail-fast 与 NUL 输入校验后，`test_lexer_parser`、`test_standard`、
  `test_ui_document`：3/3 通过。

## P3：Component 与行为扩展

- [ ] 设计注入式 `UiComponentRegistry`，禁止全局 singleton。
- [ ] 注册 Widget factory，并定义 props validation 与生命周期。
- [ ] 支持 slots、conditional/repeat binding 和 keyed reconciliation。
- [ ] 把 `on.*` metadata 接入 TurboScript controller 的事件快照。
- [ ] 增加 Canvas/Flex Scene 显式 bridge，不共享裸 Node ownership。

P3 不阻塞 P1/P2 交付；只有完成 factory contract 与测试后，`Button` 等 tag 才升级为真实
Widget。
