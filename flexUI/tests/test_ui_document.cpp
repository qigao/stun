#include <flexUI.h>

#include <tinytest.h>

#include <string>

using flexUI::UiDocumentErrorCode;

namespace {

const char *valid_document() {
  return R"(
    ui MainWindow {
      div root {
        utility: "flex",
        class: "app-shell",
        role: "application",

        button save {
          text: "Save",
          focusable: true,
          tab_index: 2,
          attr.label: "Save document",
          on.click: "save_document",
          disabled: false
        }
      }
    }
  )";
}

} // namespace

spec("Flex UI documents instantiate Box-owned Element trees") {
  it("parses and installs one immutable UI definition") {
    const auto parsed = flexUI::parse_ui_document(valid_document());
    check(static_cast<bool>(parsed));
    check_not_null(parsed.definition.get());
    check_string_eq(parsed.definition->name, "MainWindow");
    check_string_eq(parsed.definition->root.tag, "div");
    check_size_eq(parsed.definition->root.children.size(), 1);

    flexUI::Box box(nullptr);
    const auto instantiated = flexUI::UiDocumentInstantiator::instantiate(box, *parsed.definition);
    check(static_cast<bool>(instantiated));
    check_ptr_eq(box.root(), instantiated.tree.root);
    check_ptr_eq(box.get_by_id("root"), instantiated.tree.root);
    check_size_eq(instantiated.tree.elements_by_id.size(), 2);

    auto *root = box.get_by_id("root");
    auto *save = box.get_by_id("save");
    check_not_null(root);
    check_not_null(save);
    check_string_eq(root->tag(), "div");
    check(root->has_class("flex"));
    check(root->has_class("app-shell"));
    check(root->utility_names().count("flex") == 1);
    check_string_eq(*root->attribute("role"), "application");
    check_size_eq(root->child_count(), 1);
    check_ptr_eq(root->child_at(0), save);
    check_string_eq(save->text(), "Save");
    check_true(save->focusable);
    check_int_eq(save->tab_index, 2);
    check_string_eq(*save->attribute("label"), "Save document");
    check_string_eq(*save->attribute("data-flexui-on-click"), "save_document");
    check_false(save->has_attribute("disabled"));
  }

  it("requires an explicit name when source contains multiple documents") {
    const char *source = R"(
      ui First { div first {} }
      ui Second { div second {} }
    )";

    const auto ambiguous = flexUI::parse_ui_document(source);
    check_false(static_cast<bool>(ambiguous));
    check(ambiguous.error.code == UiDocumentErrorCode::AmbiguousDocument);

    const auto selected = flexUI::parse_ui_document(source, "Second");
    check(static_cast<bool>(selected));
    check_string_eq(selected.definition->name, "Second");
    check_string_eq(selected.definition->root.id, "second");
  }

  it("rejects invalid root and identity contracts before touching a Box") {
    const auto multiple_roots = flexUI::parse_ui_document(R"(
      ui Broken { div first {} div second {} }
    )");
    check_false(static_cast<bool>(multiple_roots));
    check(multiple_roots.error.code == UiDocumentErrorCode::InvalidRootCount);

    const auto duplicate_id = flexUI::parse_ui_document(R"(
      ui Broken { div root { span item {} span item {} } }
    )");
    check_false(static_cast<bool>(duplicate_id));
    check(duplicate_id.error.code == UiDocumentErrorCode::DuplicateElementId);

    const auto alias_conflict = flexUI::parse_ui_document(R"(
      ui Broken {
        div root { class: "one", classes: "two" }
      }
    )");
    check_false(static_cast<bool>(alias_conflict));
    check(alias_conflict.error.code == UiDocumentErrorCode::InvalidProperty);

    const auto invalid_type = flexUI::parse_ui_document(R"(
      ui Broken { button save { focusable: "yes" } }
    )");
    check_false(static_cast<bool>(invalid_type));
    check(invalid_type.error.code == UiDocumentErrorCode::InvalidProperty);
  }

  it("enforces configured source and node limits") {
    flexUI::UiDocumentLimits source_limits;
    source_limits.max_source_bytes = 4;
    const auto source_too_large =
        flexUI::parse_ui_document("ui A { div root {} }", {}, source_limits);
    check_false(static_cast<bool>(source_too_large));
    check(source_too_large.error.code == UiDocumentErrorCode::SourceTooLarge);

    flexUI::UiDocumentLimits node_limits;
    node_limits.max_nodes = 1;
    const auto too_many_nodes = flexUI::parse_ui_document(R"(
      ui A { div root { span child {} } }
    )",
                                                          {}, node_limits);
    check_false(static_cast<bool>(too_many_nodes));
    check(too_many_nodes.error.code == UiDocumentErrorCode::NodeLimitExceeded);

    flexUI::UiDocumentLimits name_limits;
    name_limits.max_string_bytes = 3;
    const auto name_too_large = flexUI::parse_ui_document("ui Long { div id {} }", {}, name_limits);
    check_false(static_cast<bool>(name_too_large));
    check(name_too_large.error.code == UiDocumentErrorCode::StringLimitExceeded);

    std::string embedded_nul = "ui A { div root {} }";
    embedded_nul.push_back('\0');
    embedded_nul += "ui Hidden { div hidden {} }";
    const auto truncated_source = flexUI::parse_ui_document(embedded_nul);
    check_false(static_cast<bool>(truncated_source));
    check(truncated_source.error.code == UiDocumentErrorCode::ParseError);
  }

  it("validates the target before committing detached elements") {
    const auto parsed = flexUI::parse_ui_document(valid_document());
    check(static_cast<bool>(parsed));

    flexUI::Box occupied(nullptr);
    auto *existing = occupied.create("div", "existing");
    occupied.set_root(existing);
    const auto occupied_result =
        flexUI::UiDocumentInstantiator::instantiate(occupied, *parsed.definition);
    check_false(static_cast<bool>(occupied_result));
    check(occupied_result.error.code == UiDocumentErrorCode::BoxAlreadyHasRoot);
    check_ptr_eq(occupied.root(), existing);
    check_null(occupied.get_by_id("root"));

    const auto unknown_utility = flexUI::parse_ui_document(R"(
      ui BadUtility { div root { utility: "not-in-catalog" } }
    )");
    check(static_cast<bool>(unknown_utility));
    flexUI::Box empty(nullptr);
    const auto utility_result =
        flexUI::UiDocumentInstantiator::instantiate(empty, *unknown_utility.definition);
    check_false(static_cast<bool>(utility_result));
    check(utility_result.error.code == UiDocumentErrorCode::UnknownUtility);
    check_null(empty.root());
    check_null(empty.get_by_id("root"));

    flexUI::Box jit_disabled(nullptr);
    jit_disabled.disable_utility_jit();
    const auto disabled_result =
        flexUI::UiDocumentInstantiator::instantiate(jit_disabled, *parsed.definition);
    check_false(static_cast<bool>(disabled_result));
    check(disabled_result.error.code == UiDocumentErrorCode::UtilityJitDisabled);
    check_null(jit_disabled.root());
    check_null(jit_disabled.get_by_id("root"));
  }

  it("validates manually constructed definitions before instantiation") {
    flexUI::Box box(nullptr);
    flexUI::UiDocumentDefinition definition;
    definition.name = "Manual";
    definition.root.tag = "div";
    definition.root.id = "root";
    definition.root.properties["utility"] = true;

    const auto result = flexUI::UiDocumentInstantiator::instantiate(box, definition);
    check_false(static_cast<bool>(result));
    check(result.error.code == UiDocumentErrorCode::InvalidProperty);
    check_null(box.root());
    check_null(box.get_by_id("root"));
  }
}
