#include <flexUI.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/radio_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/widgets/tooltip_widget.h>
#include <flexUI/widgets/modal_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/spinner_widget.h>
#include <flexUI/widgets/divider_widget.h>
#include <flexUI/widgets/badge_widget.h>
#include <flexUI/widgets/card_widget.h>
#include <flexUI/widgets/group_button_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/breadcrumb_widget.h>
#include <flexUI/widgets/pagination_widget.h>
#include <flexUI/widgets/stepper_widget.h>
#include <flexUI/widgets/image_widget.h>
#include <flexUI/widgets/calendar_widget.h>
#include <flexUI/widgets/table_widget.h>
#include <flexUI/widgets/tree_widget.h>
#include <flexUI/widgets/colorpicker_widget.h>
#include <flexUI/widgets/avatar_widget.h>
#include <flexUI/widgets/gradient_editor_widget.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/widget.h>
#include <flexUI/keyed_repeater.h>

#include <tinytest.hpp>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

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
  it("compiles an immutable program without replacing the parse API") {
    const auto compiled = flexUI::compile_ui_document(valid_document());
    check(static_cast<bool>(compiled));
    check_not_null(compiled.program.get());
    check_equal(compiled.program->name(), "MainWindow");
    check_equal(compiled.program->definition().root.id, "root");

    flexUI::Box box(nullptr);
    const auto instantiated =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program);
    check(static_cast<bool>(instantiated));
    check_equal(instantiated.tree.root, box.get_by_id("root"));

    const auto parsed = flexUI::parse_ui_document(valid_document());
    check(static_cast<bool>(parsed));
    check_equal(parsed.definition->name, compiled.program->name());
  }

  it("compiles parser-independent definitions into owned immutable programs") {
    flexUI::UiDocumentDefinition definition;
    definition.name = "Manual";
    definition.root.tag = "div";
    definition.root.id = "root";

    flexUI::UiNodeDefinition button;
    button.tag = "button";
    button.id = "save";
    button.properties["on.click"] = std::string("save_document");
    button.property_spans["on.click"] = {7, 5, 8};
    button.properties["bind.class_active"] = std::string("${enabled}");
    button.property_spans["bind.class_active"] = {8, 5, 17};
    definition.root.children.push_back(std::move(button));

    std::vector<flexUI::UiResourceDefinition> resources{
        {"image", "logo", "images/logo.png", {{"preload", true}}}};
    const auto compiled =
        flexUI::compile_ui_definition(definition, resources);
    check(static_cast<bool>(compiled));

    definition.name = "Changed";
    definition.root.children.front().properties["on.click"] =
        std::string("changed_handler");
    resources.front().id = "changed";

    check_equal(compiled.program->name(), "Manual");
    check_equal(compiled.program->event_bindings().size(), 1);
    check_equal(compiled.program->event_bindings().front().handler,
                "save_document");
    check_equal(compiled.program->bindings().size(), 1);
    check_equal(compiled.program->bindings().front().dependencies.size(), 1);
    check_equal(compiled.program->bindings().front().dependencies.front(),
                "enabled");
    check_equal(compiled.program->resources().size(), 1);
    check_equal(compiled.program->resources().front().id, "logo");
  }

  it("validates parser-independent definitions before semantic lowering") {
    flexUI::UiDocumentDefinition definition;
    definition.name = "DuplicateIds";
    definition.root.tag = "div";
    definition.root.id = "same";
    definition.root.children.push_back({"span", "same", {}, {}, {}});

    const auto duplicate = flexUI::compile_ui_definition(definition);
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error.code == UiDocumentErrorCode::DuplicateElementId);

    flexUI::UiDocumentLimits limits;
    limits.max_resources = 0;
    definition.root.children.clear();
    const auto over_limit = flexUI::compile_ui_definition(
        definition, {{"image", "logo", "images/logo.png", {}}}, limits);
    check_false(static_cast<bool>(over_limit));
    check(over_limit.error.code == UiDocumentErrorCode::ResourceLimitExceeded);
  }

  it("lowers supported event properties with source spans") {
    const char *source =
        "ui Main {\n"
        "  button save {\n"
        "    on.click: \"save_document\"\n"
        "  }\n"
        "}\n";

    const auto compiled = flexUI::compile_ui_document(source);
    check(static_cast<bool>(compiled));
    check_equal(compiled.program->event_bindings().size(), 1);

    const auto &binding = compiled.program->event_bindings().front();
    check(binding.event == flexUI::UiEventKind::Click);
    check_equal(binding.element_id, "save");
    check_equal(binding.handler, "save_document");
    check_equal(binding.source.line, 3);
    check_equal(binding.source.column, 5);
    check_equal(binding.source.length, 8);

    flexUI::Box box(nullptr);
    const auto instantiated =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program);
    check(static_cast<bool>(instantiated));
    check_equal(*box.get_by_id("save")->attribute("data-flexui-on-click"),
                "save_document");
  }

  it("finds compiled handlers by element and event kind") {
    const auto compiled = flexUI::compile_ui_document(R"(
      ui Main {
        button save {
          on.click: "save_document",
          on.key_down: "save_shortcut"
        }
      }
    )");
    check(static_cast<bool>(compiled));

    const auto *click = compiled.program->find_event_binding(
        "save", flexUI::UiEventKind::Click);
    check_not_null(click);
    check_equal(click->handler, "save_document");
    const auto *key_down = compiled.program->find_event_binding(
        "save", flexUI::UiEventKind::KeyDown);
    check_not_null(key_down);
    check_equal(key_down->handler, "save_shortcut");
    check_null(compiled.program->find_event_binding(
        "save", flexUI::UiEventKind::FocusIn));
    check_null(compiled.program->find_event_binding(
        "missing", flexUI::UiEventKind::Click));

    flexUI::Box box(nullptr);
    const auto instantiated =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program);
    check(static_cast<bool>(instantiated));
    check_equal(*box.get_by_id("save")->attribute(
                    "data-flexui-on-key_down"),
                "save_shortcut");
  }

  it("distinguishes element ids with colliding symbols") {
    check(flex::Symbol("costarring") == flex::Symbol("liquid"));
    const auto compiled = flexUI::compile_ui_document(R"(
      ui Main {
        div root {
          button costarring { on.click: "first_handler" }
          button liquid { on.click: "second_handler" }
        }
      }
    )");
    check(static_cast<bool>(compiled));

    // These identifiers share the same 32-bit FNV-1a value. Full IDs remain
    // authoritative after the symbol bucket lookup.
    const auto *first = compiled.program->find_event_binding(
        "costarring", flexUI::UiEventKind::Click);
    const auto *second = compiled.program->find_event_binding(
        "liquid", flexUI::UiEventKind::Click);
    check_not_null(first);
    check_not_null(second);
    check_equal(first->handler, "first_handler");
    check_equal(second->handler, "second_handler");
  }

  it("keeps compiled handlers independent from compatibility attributes") {
    const auto compiled = flexUI::compile_ui_document(R"(
      ui Main { button save { on.click: "save_document" } }
    )");
    check(static_cast<bool>(compiled));

    flexUI::Box box(nullptr);
    const auto instantiated =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program);
    check(static_cast<bool>(instantiated));
    auto *save = box.get_by_id("save");
    check_not_null(save);

    save->set_attribute("data-flexui-on-click", "mutated_handler");
    const auto *after_set = compiled.program->find_event_binding(
        "save", flexUI::UiEventKind::Click);
    check_not_null(after_set);
    check_equal(after_set->handler, "save_document");

    save->remove_attribute("data-flexui-on-click");
    const auto *after_remove = compiled.program->find_event_binding(
        "save", flexUI::UiEventKind::Click);
    check_not_null(after_remove);
    check_equal(after_remove->handler, "save_document");
  }

  it("rejects unknown events and configured event limits during compilation") {
    const char *unknown_source = R"(
      ui Main { button save { on.explode: "save_document" } }
    )";
    const auto parsed = flexUI::parse_ui_document(unknown_source);
    check(static_cast<bool>(parsed));

    const auto unknown = flexUI::compile_ui_document(unknown_source);
    check_false(static_cast<bool>(unknown));
    check(unknown.error.code == UiDocumentErrorCode::UnknownEvent);

    flexUI::UiDocumentLimits limits;
    limits.max_event_bindings = 0;
    const auto over_limit = flexUI::compile_ui_document(valid_document(), {}, limits);
    check_false(static_cast<bool>(over_limit));
    check(over_limit.error.code == UiDocumentErrorCode::EventBindingLimitExceeded);
  }

  it("rejects a duplicate event binding at its later source location") {
    const auto compiled = flexUI::compile_ui_document(
        "ui Main {\n"
        "  button save {\n"
        "    on.click: \"save_document\",\n"
        "    on.click: \"save_again\"\n"
        "  }\n"
        "}\n");

    check_false(static_cast<bool>(compiled));
    check(compiled.error.code == UiDocumentErrorCode::DuplicateEventBinding);
    check_equal(compiled.error.line, 4);
    check_equal(compiled.error.column, 5);
    check_contains(compiled.error.message, "save");
  }

  it("preserves last-write-wins for duplicate ordinary properties") {
    const auto parsed = flexUI::parse_ui_document(R"(
      ui Main {
        label status {
          text: "Loading",
          text: "Ready"
        }
      }
    )");

    check(static_cast<bool>(parsed));
    const auto value = parsed.definition->root.properties.find("text");
    check(value != parsed.definition->root.properties.end());
    check_equal(std::get<std::string>(value->second), "Ready");
  }

  it("keeps duplicate binding diagnostics stable at the unique-property limit") {
    flexUI::UiDocumentLimits limits;
    limits.max_properties_per_node = 1;

    const auto duplicate_event = flexUI::compile_ui_document(
        "ui Main {\n"
        "  button save {\n"
        "    on.click: \"save_document\",\n"
        "    on.click: \"save_again\"\n"
        "  }\n"
        "}\n",
        {}, limits);
    check_false(static_cast<bool>(duplicate_event));
    check(duplicate_event.error.code ==
          UiDocumentErrorCode::DuplicateEventBinding);
    check_equal(duplicate_event.error.line, 4);

    const auto duplicate_binding = flexUI::compile_ui_document(
        "ui Main {\n"
        "  label status {\n"
        "    bind.text: $status_text,\n"
        "    bind.text: $replacement_text\n"
        "  }\n"
        "}\n",
        {}, limits);
    check_false(static_cast<bool>(duplicate_binding));
    check(duplicate_binding.error.code ==
          UiDocumentErrorCode::DuplicateBindingTarget);
    check_equal(duplicate_binding.error.line, 4);

    const auto ordinary = flexUI::parse_ui_document(R"(
      ui Main { label status { text: "Loading", text: "Ready" } }
    )", {}, limits);
    check(static_cast<bool>(ordinary));
  }

  it("lowers typed string and MIR class bindings") {
    const char *source =
        "ui Main {\n"
        "  label status {\n"
        "    bind.text: $status_text,\n"
        "    bind.class_active: ${enabled}\n"
        "  }\n"
        "}\n";

    const auto compiled = flexUI::compile_ui_document(source);
    info("compile error: code=%d message=%s at %d:%d",
         static_cast<int>(compiled.error.code), compiled.error.message.c_str(),
         compiled.error.line, compiled.error.column);
    check(static_cast<bool>(compiled));
    check_equal(compiled.program->bindings().size(), 2);

    const auto &text = compiled.program->bindings()[0];
    check(text.target == flexUI::UiBindingTargetKind::Text);
    check(text.source == flexUI::UiBindingSourceKind::StringInput);
    check_equal(text.element_id, "status");
    check_equal(text.expression, "status_text");
    check_equal(text.dependencies.size(), 1);
    check_equal(text.dependencies.front(), "status_text");
    check_false(text.uses_jit);
    check_equal(text.source_span.line, 3);

    const auto &active = compiled.program->bindings()[1];
    check(active.target == flexUI::UiBindingTargetKind::ClassToggle);
    check(active.source == flexUI::UiBindingSourceKind::BoolExpression);
    check_equal(active.target_name, "active");
    check_equal(active.expression, "enabled");
    check_equal(active.dependencies.size(), 1);
    check_equal(active.dependencies.front(), "enabled");
  }

  it("installs compiled bindings without recompiling MIR") {
    auto compiled = flexUI::compile_ui_document(R"(
      ui Main {
        div root {
          label status {
            bind.text: $status_text,
            bind.class_active: ${enabled}
          }
          div class_list { bind.classes: $status_classes }
          div utility_list { bind.utilities: $status_utilities }
        }
      }
    )");
    check(static_cast<bool>(compiled));

    flexUI::Box box(nullptr);
    box.bindings().inputs().set_string("status_text", "Ready");
    box.bindings().inputs().set_string("status_classes", "notice");
    box.bindings().inputs().set_string("status_utilities", "flex");
    box.bindings().inputs().set_bool("enabled", true);

    const auto instantiated =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program);
    check(static_cast<bool>(instantiated));
    compiled.program.reset();
    check_equal(box.bindings().stats().binding_count, 4);
    check_equal(box.bindings().expression_compile_count(), 0);
    check_true(box.bindings().update());

    auto *status = box.get_by_id("status");
    auto *class_list = box.get_by_id("class_list");
    auto *utility_list = box.get_by_id("utility_list");
    check_not_null(status);
    check_not_null(class_list);
    check_not_null(utility_list);
    check_equal(status->text(), "Ready");
    check_true(status->has_class("active"));
    check_true(class_list->has_class("notice"));
    check_true(utility_list->utility_names().count("flex") == 1);

    box.bindings().inputs().set_string("status_text", "Running");
    box.bindings().inputs().set_string("status_classes", "muted");
    box.bindings().inputs().set_string("status_utilities", "");
    box.bindings().inputs().set_bool("enabled", false);
    check_true(box.bindings().update());
    check_equal(status->text(), "Running");
    check_false(status->has_class("active"));
    check_false(class_list->has_class("notice"));
    check_true(class_list->has_class("muted"));
    check_true(utility_list->utility_names().empty());
  }

  it("rolls back a failed compiled binding installation") {
    const auto compiled = flexUI::compile_ui_document(R"(
      ui Main {
        label status {
          bind.text: $status_text,
          bind.class_active: ${missing_flag}
        }
      }
    )");
    check(static_cast<bool>(compiled));

    flexUI::Box box(nullptr);
    auto *existing = box.create("label", "existing");
    const auto existing_handle = box.handle_for(*existing);
    box.bindings().inputs().set_string("seed_text", "Seed");
    box.bindings().inputs().set_string("status_text", "Ready");
    const auto before =
        box.bindings().targets().bind_text(*existing, "seed_text");
    const auto count_before = box.bindings().stats().binding_count;

    const auto instantiated =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program);
    check_false(static_cast<bool>(instantiated));
    check(instantiated.error.code ==
          UiDocumentErrorCode::BindingInstallFailed);
    check_equal(instantiated.error.line, 5);
    check_null(box.root());
    check_null(box.get_by_id("status"));
    check_equal(box.get_by_id("existing"), existing);
    check_equal(box.resolve_handle(existing_handle), existing);
    check_equal(box.bindings().stats().binding_count, count_before);

    auto *after_target = box.create("label", "after");
    const auto after_handle = box.handle_for(*after_target);
    check_equal(after_handle.generation, existing_handle.generation + 1);
    const auto after =
        box.bindings().targets().bind_text(*after_target, "seed_text");
    check_equal(after.id, before.id + 1);
    check_true(box.bindings().update());
    check_equal(existing->text(), "Seed");
  }

  it("rejects a duplicate binding target at its later source location") {
    const auto compiled = flexUI::compile_ui_document(
        "ui Main {\n"
        "  label status {\n"
        "    bind.text: $status_text,\n"
        "    bind.text: $replacement_text\n"
        "  }\n"
        "}\n");

    check_false(static_cast<bool>(compiled));
    check(compiled.error.code == UiDocumentErrorCode::DuplicateBindingTarget);
    check_equal(compiled.error.line, 4);
    check_equal(compiled.error.column, 5);
    check_contains(compiled.error.message, "status");
  }

  it("rejects conflicting class-list ownership during compilation") {
    const auto compiled = flexUI::compile_ui_document(
        "ui Main {\n"
        "  div status {\n"
        "    bind.classes: $status_classes,\n"
        "    bind.class_active: ${enabled}\n"
        "  }\n"
        "}\n");

    check_false(static_cast<bool>(compiled));
    check(compiled.error.code == UiDocumentErrorCode::BindingTargetConflict);
    check_equal(compiled.error.line, 4);
    check_equal(compiled.error.column, 5);
    check_contains(compiled.error.message, "status");
    check_contains(compiled.error.message, "bind.class_active");

    const auto reverse = flexUI::compile_ui_document(R"(
      ui Main {
        div status {
          bind.class_active: ${enabled},
          bind.utilities: $status_utilities
        }
      }
    )");
    check_false(static_cast<bool>(reverse));
    check(reverse.error.code == UiDocumentErrorCode::BindingTargetConflict);

    const auto two_lists = flexUI::compile_ui_document(R"(
      ui Main {
        div status {
          bind.classes: $status_classes,
          bind.utilities: $status_utilities
        }
      }
    )");
    check_false(static_cast<bool>(two_lists));
    check(two_lists.error.code == UiDocumentErrorCode::BindingTargetConflict);
  }

  it("allows independent class-token bindings on one element") {
    const auto compiled = flexUI::compile_ui_document(R"(
      ui Main {
        div status {
          bind.class_active: ${enabled},
          bind.class_selected: ${selected}
        }
      }
    )");

    check(static_cast<bool>(compiled));
    check_equal(compiled.program->bindings().size(), 2);
  }

  it("rejects widget-only value bindings during document compilation") {
    const auto compiled = flexUI::compile_ui_document(R"(
      ui Main { input editor { bind.value: $editor_text } }
    )");
    check_false(static_cast<bool>(compiled));
    check(compiled.error.code == UiDocumentErrorCode::UnknownBindingTarget);
  }

  it("rejects unknown binding targets, invalid MIR, and binding limits") {
    const auto unknown = flexUI::compile_ui_document(R"(
      ui Main { label status { bind.attribute: $status_text } }
    )");
    check_false(static_cast<bool>(unknown));
    check(unknown.error.code == UiDocumentErrorCode::UnknownBindingTarget);

    const auto invalid_expression = flexUI::compile_ui_document(R"(
      ui Main { label status { bind.class_active: ${enabled + } } }
    )");
    check_false(static_cast<bool>(invalid_expression));
    check(invalid_expression.error.code == UiDocumentErrorCode::InvalidBindingExpression);

    flexUI::UiDocumentLimits limits;
    limits.max_bindings = 0;
    const auto over_limit = flexUI::compile_ui_document(R"(
      ui Main { label status { bind.text: $status_text } }
    )", {}, limits);
    info("binding limit error: code=%d message=%s at %d:%d",
         static_cast<int>(over_limit.error.code),
         over_limit.error.message.c_str(), over_limit.error.line,
         over_limit.error.column);
    check_false(static_cast<bool>(over_limit));
    check(over_limit.error.code == UiDocumentErrorCode::BindingLimitExceeded);
  }

  it("parses and installs one immutable UI definition") {
    const auto parsed = flexUI::parse_ui_document(valid_document());
    check(static_cast<bool>(parsed));
    check_not_null(parsed.definition.get());
    check_equal(parsed.definition->name, "MainWindow");
    check_equal(parsed.definition->root.tag, "div");
    check_equal(parsed.definition->root.children.size(), 1);

    flexUI::Box box(nullptr);
    const auto instantiated = flexUI::UiDocumentInstantiator::instantiate(box, *parsed.definition);
    check(static_cast<bool>(instantiated));
    check_equal(box.root(), instantiated.tree.root);
    check_equal(box.get_by_id("root"), instantiated.tree.root);
    check_equal(instantiated.tree.elements_by_id.size(), 2);

    auto *root = box.get_by_id("root");
    auto *save = box.get_by_id("save");
    check_not_null(root);
    check_not_null(save);
    check_equal(root->tag(), "div");
    check(root->has_class("flex"));
    check(root->has_class("app-shell"));
    check(root->utility_names().count("flex") == 1);
    check_equal(*root->attribute("role"), "application");
    check_equal(root->child_count(), 1);
    check_equal(root->child_at(0), save);
    check_equal(save->text(), "Save");
    check_true(save->focusable);
    check_equal(save->tab_index, 2);
    check_equal(*save->attribute("label"), "Save document");
    check_equal(*save->attribute("data-flexui-on-click"), "save_document");
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
    check_equal(selected.definition->name, "Second");
    check_equal(selected.definition->root.id, "second");
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

  it("lowers declared resources into the immutable program") {
    const auto compiled = flexUI::compile_ui_document(R"(
      assets {
        image logo: "images/logo.png" {
          preload: true
          scale: 2
        }
        font body: "fonts/body.ttf"
      }

      ui Main { div root {} }
    )");
    check(static_cast<bool>(compiled));
    check_equal(compiled.program->resources().size(), 2);

    const auto &logo = compiled.program->resources()[0];
    check_equal(logo.type, "image");
    check_equal(logo.id, "logo");
    check_equal(logo.path, "images/logo.png");
    check_equal(std::get<bool>(logo.options.at("preload")), true);
    check_equal(std::get<float>(logo.options.at("scale")), 2.0f);

    const auto &body = compiled.program->resources()[1];
    check_equal(body.type, "font");
    check_equal(body.id, "body");
    check_equal(body.path, "fonts/body.ttf");
    check(body.options.empty());
  }

  it("rejects resource declarations above the configured limit") {
    flexUI::UiDocumentLimits limits;
    limits.max_resources = 1;
    const char *source = R"(
      assets {
        image logo: "images/logo.png"
        font body: "fonts/body.ttf"
      }

      ui Main { div root {} }
    )";

    const auto parsed = flexUI::parse_ui_document(source, {}, limits);
    check(static_cast<bool>(parsed));

    const auto compiled = flexUI::compile_ui_document(source, {}, limits);
    check_false(static_cast<bool>(compiled));
    check(compiled.error.code == UiDocumentErrorCode::ResourceLimitExceeded);
  }

  it("selects a ui document when a sibling scene is present") {
    const auto compiled = flexUI::compile_ui_document(R"(
      scene Preview {
        width: 320
        height: 200
        rect background { width: 320, height: 200 }
      }

      ui Inspector { div root {} }
    )");
    check(static_cast<bool>(compiled));
    check_equal(compiled.program->name(), "Inspector");
    check_equal(compiled.program->definition().root.id, "root");
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
    check_equal(occupied.root(), existing);
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


spec("FlexUI XML fragments stage local slots without a parallel UI tree") {
  it("reuses the same XML component twice with local slots and no global ids") {
    const auto compiled = flexUI::compile_ui_xml(R"xml(
      <ui name="Panel">
        <figure id="panel" slot="frame" class="flex rounded-lg border">
          <figcaption id="caption" slot="caption">Revenue</figcaption>
          <div slot="surface" class="relative w-full" />
        </figure>
      </ui>
    )xml");
    check(static_cast<bool>(compiled));
    auto registry = flexUI::WidgetRegistry::builtins();
    flexUI::Box box(nullptr);
    flexUI::UiMountOptions opts;
    opts.mode = flexUI::UiMountMode::DetachedFragment;
    opts.id_policy = flexUI::UiIdPolicy::LocalOnly;

    const auto first = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, registry, opts);
    const auto second = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, registry, opts);
    check(static_cast<bool>(first));
    check(static_cast<bool>(second));
    check(first.tree.root != second.tree.root);
    check_null(box.root());
    check_null(box.get_by_id("panel"));
    check_equal(first.tree.slots_by_name.size(), 3U);
    check_equal(first.tree.slots_by_name.at("frame"), first.tree.root);
    check_equal(*first.tree.slots_by_name.at("surface")->attribute("data-slot"), "surface");
    check_equal(second.tree.elements_by_id.at("caption")->id(), "caption");

    auto* page = box.create("main");
    page->append(first.tree.root);
    page->append(second.tree.root);
    box.set_root(page);
    box.update();
    check_equal(page->child_count(), 2U);
    check(first.tree.root->computed_style != nullptr);
    check(second.tree.root->computed_style != nullptr);
    check(first.tree.root->computed_style->display == flexUI::Display::Flex);
  }

  it("atomically mounts a scoped fragment under an existing Box-owned parent") {
    const auto compiled = flexUI::compile_ui_xml(R"xml(
      <ui name="ScopedPanel">
        <div id="panel" slot="frame">
          <span id="caption" slot="title">Quarterly revenue</span>
        </div>
      </ui>
    )xml");
    check(static_cast<bool>(compiled));
    auto registry = flexUI::WidgetRegistry::builtins();
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "page");
    auto* existing = box.create("span", "older");
    page->append(existing);
    box.set_root(page);
    const auto before = box.owned_element_count();

    flexUI::UiMountOptions opts;
    opts.mode = flexUI::UiMountMode::Child;
    opts.id_policy = flexUI::UiIdPolicy::Scoped;
    opts.parent = page;
    opts.id_scope = "sales";
    const auto result = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, registry, opts);
    check(static_cast<bool>(result));
    check_equal(page, box.root());
    check_equal(page->child_at(0), existing);
    check_equal(page->child_at(1), result.tree.root);
    check_equal(page->child_count(), 2U);
    check_equal(box.get_by_id("sales::panel"), result.tree.root);
    check_equal(box.get_by_id("sales::caption"), result.tree.elements_by_id.at("caption"));
    check_null(box.get_by_id("panel"));
    check_equal(box.owned_element_count(), before + 2);
    check_equal(result.tree.slots_by_name.at("title"),
                result.tree.elements_by_id.at("caption"));

    const auto collision = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, registry, opts);
    check_false(static_cast<bool>(collision));
    check(collision.error.code == UiDocumentErrorCode::ElementIdConflict);
    check_equal(box.owned_element_count(), before + 2);
    check_equal(page->child_count(), 2U);
  }

  it("rejects duplicate slots before factory invocation or Box mutation") {
    const auto compiled = flexUI::compile_ui_xml(R"xml(
      <ui name="DuplicateSlot">
        <div slot="plot"><span slot="plot" /></div>
      </ui>
    )xml");
    check(static_cast<bool>(compiled));
    flexUI::Box box(nullptr);
    auto* page = box.create("main");
    box.set_root(page);
    const auto before = box.owned_element_count();
    flexUI::UiMountOptions opts;
    opts.mode = flexUI::UiMountMode::Child;
    opts.parent = page;
    const auto result = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, flexUI::WidgetRegistry::builtins(), opts);
    check_false(static_cast<bool>(result));
    check(result.error.code == UiDocumentErrorCode::DuplicateSlot);
    check_equal(box.root(), page);
    check_equal(box.owned_element_count(), before);
    check_equal(page->child_count(), 0U);
  }

  it("rejects a missing required XML slot before owning any new nodes") {
    const auto compiled = flexUI::compile_ui_xml(
        "<ui name=\"Missing\"><div slot=\"frame\"><span slot=\"title\" /></div></ui>");
    check(static_cast<bool>(compiled));
    flexUI::Box box(nullptr);
    auto *page = box.create("main", "existing");
    box.set_root(page);
    const auto before = box.owned_element_count();

    flexUI::UiMountOptions opts;
    opts.mode = flexUI::UiMountMode::Child;
    opts.parent = page;
    opts.required_slots = {"frame", "title", "plot"};
    const auto failed = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, flexUI::WidgetRegistry::builtins(), opts);
    check_false(static_cast<bool>(failed));
    check(failed.error.code == UiDocumentErrorCode::MissingSlot);
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);
    check_equal(box.owned_element_count(), before);
    check_null(box.get_by_id("plot"));
  }

  it("rejects invalid parent, incomplete ID policy and controller exports") {
    auto registry = flexUI::WidgetRegistry::builtins();
    const auto ui = flexUI::compile_ui_xml(
        "<ui name=\"Child\"><div id=\"panel\" slot=\"panel\" /></ui>");
    check(static_cast<bool>(ui));
    flexUI::Box box(nullptr);
    flexUI::Box foreign(nullptr);
    auto* root = box.create("main", "root");
    box.set_root(root);
    auto* outside = foreign.create("main");

    flexUI::UiMountOptions opts;
    opts.mode = flexUI::UiMountMode::Child;
    opts.parent = outside;
    auto wrong_parent = flexUI::UiDocumentInstantiator::instantiate(
        box, *ui.program, registry, opts);
    check_false(static_cast<bool>(wrong_parent));
    check(wrong_parent.error.code == UiDocumentErrorCode::InvalidMountTarget);

    opts.parent = root;
    opts.id_policy = flexUI::UiIdPolicy::Scoped;
    auto invalid_scope = flexUI::UiDocumentInstantiator::instantiate(
        box, *ui.program, registry, opts);
    check_false(static_cast<bool>(invalid_scope));
    check(invalid_scope.error.code == UiDocumentErrorCode::InvalidIdScope);

    const auto with_event = flexUI::compile_ui_xml(R"xml(
      <ui name="Button" xmlns:on="urn:flexui:event">
        <div><button id="go" on:click="run">Go</button></div>
      </ui>
    )xml");
    check(static_cast<bool>(with_event));
    opts.id_scope = "sales";
    const auto not_yet_scoped = flexUI::UiDocumentInstantiator::instantiate(
        box, *with_event.program, registry, opts);
    check_false(static_cast<bool>(not_yet_scoped));
    check(not_yet_scoped.error.code ==
          UiDocumentErrorCode::UnsupportedFragmentBindings);
    check_equal(box.root(), root);
    check_equal(root->child_count(), 0U);
    check_equal(box.owned_element_count(), 1U);
  }

  it("leaves an existing parent unchanged when a widget factory fails") {
    auto registry = flexUI::WidgetRegistry::builtins();
    check(registry.register_widget("broken", [](const flexUI::UiNodeDefinition&) ->
         std::unique_ptr<flexUI::Widget> {
      throw std::runtime_error("intentional chart widget factory failure");
    }).code == flexUI::WidgetRegistryErrorCode::None);
    const auto broken = flexUI::compile_ui_xml(
        "<ui name=\"Broken\"><div slot=\"frame\"><broken /></div></ui>");
    check(static_cast<bool>(broken));
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "page");
    box.set_root(page);
    const auto before = box.owned_element_count();
    flexUI::UiMountOptions opts;
    opts.mode = flexUI::UiMountMode::Child;
    opts.parent = page;
    const auto result = flexUI::UiDocumentInstantiator::instantiate(
        box, *broken.program, registry, opts);
    check_false(static_cast<bool>(result));
    check(result.error.code == UiDocumentErrorCode::WidgetFactoryFailed);
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);
    check_equal(box.owned_element_count(), before);
    check_null(box.get_by_id("frame"));
  }
}


spec("UiDocument stages native DOT presentation without Box side effects") {
  it("validates whole-tree bounds and colors before any Element adoption") {
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "existing-page");
    box.set_root(page);
    const auto before = box.owned_element_count();

    flexUI::UiDocumentDefinition def;
    def.name = "InvalidDot";
    def.root.tag = "div";
    flexUI::UiNodeDefinition node;
    node.tag = "span";
    node.id = "bad-node";
    flexUI::UiNodePresentation p;
    p.has_bounds = true;
    p.absolute = true;
    p.x = std::numeric_limits<float>::quiet_NaN();
    p.y = 1.0f;
    p.width = 20.0f;
    p.height = 20.0f;
    node.presentation = p;
    def.root.children.push_back(std::move(node));
    const auto invalid = flexUI::compile_ui_definition(def);
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code == UiDocumentErrorCode::InvalidProperty);
    check_equal(box.owned_element_count(), before);
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);

    p.x = 5.0f;
    p.background_color = flexUI::Color(1.25f, 0.0f, 0.0f, 1.0f);
    def.root.children.front().presentation = p;
    const auto invalid_color = flexUI::compile_ui_definition(def);
    check_false(static_cast<bool>(invalid_color));
    check(invalid_color.error.code == UiDocumentErrorCode::InvalidProperty);
    check_equal(box.owned_element_count(), before);
  }

  it("stages typed absolute geometry, border and text paint before publication") {
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "existing-page");
    box.set_root(page);
    const auto before = box.owned_element_count();

    flexUI::UiDocumentDefinition def;
    def.name = "Dot";
    def.root.tag = "div";
    flexUI::UiNodeDefinition node;
    node.tag = "span";
    node.id = "node-A";
    node.properties["class"] = std::string("dot-node shape-box");
    node.properties["attr.data-slot"] = std::string("dotgraph-node");
    node.properties["text"] = std::string("A");
    flexUI::UiNodePresentation p;
    p.has_bounds = true;
    p.absolute = true;
    p.x = 17.0f;
    p.y = 26.0f;
    p.width = 42.0f;
    p.height = 19.0f;
    p.background_color = flexUI::Color(0.2f, 0.3f, 0.4f, 1.0f);
    p.border_color = flexUI::Color(0.1f, 0.2f, 0.3f, 1.0f);
    p.border_width = 1.5f;
    p.border_style = flexUI::BorderStyle::Dashed;
    p.border_radius = 3.0f;
    p.opacity = 0.8f;
    p.font_family = std::string("Segoe UI");
    p.font_size = 14.0f;
    p.text_align = flexUI::TextAlign::Center;
    p.z_index = 2;
    node.presentation = std::move(p);
    def.root.children.push_back(std::move(node));
    const auto compiled = flexUI::compile_ui_definition(std::move(def));
    check(static_cast<bool>(compiled));

    flexUI::UiMountOptions mount;
    mount.mode = flexUI::UiMountMode::DetachedFragment;
    mount.id_policy = flexUI::UiIdPolicy::Global;
    const auto result = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, flexUI::WidgetRegistry::builtins(), mount);
    check(static_cast<bool>(result));
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);
    check_equal(box.owned_element_count(), before + 2);
    auto* element = box.get_by_id("node-A");
    check_not_null(element);
    check(element->parent_elem() == result.tree.root);
    check_equal(*element->attribute("data-slot"), "dotgraph-node");
    check_true(std::fabs(element->x() - 17.0f) < 0.001f);
    check_true(std::fabs(element->y() - 26.0f) < 0.001f);
    check_true(std::fabs(element->layout_width() - 42.0f) < 0.001f);
    check(element->style_.position == flexUI::Position::Absolute);
    check(element->style_.border_style[0] == flexUI::BorderStyle::Dashed);
    check_true(std::fabs(element->style_.border_width[0] - 1.5f) < 0.001f);
    check_true(std::fabs(element->style_.background_color.r - 0.2f) < 0.001f);
    check_true(std::fabs(element->style_.opacity - 0.8f) < 0.001f);
    check_equal(element->style_.font_family, "Segoe UI");
    check(element->style_.text_align == flexUI::TextAlign::Center);
  }
}



spec("Box structural subtree retirement rejects unsafe lifetime owners") {
  it("retires all doomed string and MIR Binding targets but preserves siblings") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *host = box.create("section", "host");
    auto *panel = box.create("div", "panel");
    auto *caption = box.create("div", "panel::caption");
    auto *badge = box.create("div", "panel::badge");
    auto *progress = box.create("div", "panel::progress");
    auto *sibling = box.create("div", "sibling");
    root->append(host);
    host->append(panel);
    panel->append(caption);
    panel->append(badge);
    panel->append(progress);
    host->append(sibling);
    box.set_root(root);

    auto &inputs = box.bindings().inputs();
    inputs.set_string("caption", "Revenue");
    inputs.set_bool("enabled", true);
    inputs.set_number("quantity", 3.0);
    inputs.set_string("shared", "Stable");
    const auto text = box.bindings().targets().bind_text(*caption, "caption");
    const auto active = box.bindings().targets().bind_class(
        *badge, "active", "enabled && quantity > 1");
    const auto numeric = box.bindings().targets().bind_custom_property(
        *progress, "--progress", "quantity * 10", "%");
    const auto kept = box.bindings().targets().bind_text(*sibling, "shared");
    check(text && active && numeric && kept);
    box.update();
    check_equal(caption->text(), "Revenue");
    check(badge->has_class(flexUI::Symbol("active")));
    check_equal(sibling->text(), "Stable");
    const auto old_caption = box.handle_for(*caption);
    const auto old_badge = box.handle_for(*badge);
    const auto sibling_handle = box.handle_for(*sibling);
    const auto before_count = box.owned_element_count();
    check_equal(box.bindings().stats().binding_count, std::size_t{4});

    // M0 is intentionally strict. M1 permits ownership discard only when
    // the entire candidate subtree is being destroyed atomically.
    check_false(box.retire_unbound_structural_subtree("panel"));
    check_equal(box.bindings().stats().binding_count, std::size_t{4});
    check(box.retire_bound_structural_subtree("panel"));
    check_equal(box.owned_element_count(), before_count - 4u);
    check_equal(box.bindings().stats().binding_count, std::size_t{1});
    check_null(box.get_by_id("panel"));
    check_null(box.resolve_handle(old_caption));
    check_null(box.resolve_handle(old_badge));
    check_equal(box.resolve_handle(sibling_handle), sibling);
    check_equal(sibling->text(), "Stable");
    check_false(box.bindings().targets().unbind(text.id));
    check_false(box.bindings().targets().unbind(active.id));
    check_false(box.bindings().targets().unbind(numeric.id));

    // Only still-bound inputs remain protected. No retired Binding may
    // retain a stale Element pointer or stale typed input dependency.
    check(inputs.erase("caption"));
    check(inputs.erase("enabled"));
    check(inputs.erase("quantity"));
    bool shared_refused = false;
    try {
      (void)inputs.erase("shared");
    } catch (const std::invalid_argument &) {
      shared_refused = true;
    }
    check(shared_refused);
    inputs.set_string("shared", "Surviving sibling");
    box.update();
    check_equal(sibling->text(), "Surviving sibling");
    check_equal(box.resolve_handle(sibling_handle), sibling);

    auto *reused = box.create("div", "panel::caption");
    host->append(reused);
    check(box.handle_for(*reused).generation != old_caption.generation);
    check_null(box.resolve_handle(old_caption));
  }

  it("preserves all Binding records on event, callback and Widget rejection") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *panel = box.create("div", "panel");
    auto *bound = box.create("div", "caption");
    auto *event = box.create("div", "action");
    root->append(panel);
    panel->append(bound);
    panel->append(event);
    box.set_root(root);
    box.bindings().inputs().set_string("caption_value", "First");
    const auto handle = box.bindings().targets().bind_text(*bound, "caption_value");
    check(handle);
    const auto saved = box.handle_for(*bound);

    event->set_attribute("data-flexui-on-click", "save_document");
    check_false(box.retire_bound_structural_subtree("panel"));
    check_equal(box.bindings().stats().binding_count, std::size_t{1});
    event->remove_attribute("data-flexui-on-click");
    event->on_click([] {});
    check_false(box.retire_bound_structural_subtree("panel"));
    check_equal(box.resolve_handle(saved), bound);
    event->on_click({});

    auto *widget = box.create_widget<flexUI::LabelWidget>(
        "label", "inline-widget", std::string("Native"));
    panel->append(widget);
    check_false(box.retire_bound_structural_subtree("panel"));
    check_equal(box.bindings().stats().binding_count, std::size_t{1});
    box.bindings().inputs().set_string("caption_value", "Second");
    box.update();
    check_equal(bound->text(), "Second");
    check_equal(box.resolve_handle(saved), bound);
    check_false(box.retire_unbound_structural_subtree("panel"));
  }

  it("removes only the selected mounted subtree and retires indexed handles") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *host = box.create("section", "host");
    auto *left = box.create("div", "left");
    auto *panel = box.create("div", "scope::panel");
    auto *inner = box.create("div", "scope::inner");
    auto *anonymous = box.create("div");
    auto *right = box.create("div", "right");
    root->append(host);
    host->append(left);
    host->append(panel);
    panel->append(inner);
    inner->append(anonymous);
    host->append(right);
    box.set_root(root);
    box.update();

    const auto old_panel = box.handle_for(*panel);
    const auto old_inner = box.handle_for(*inner);
    const auto right_handle = box.handle_for(*right);
    const auto count = box.owned_element_count();
    const auto styled_count = box.cached_style_element_count();
    check(old_panel && old_inner && right_handle);
    check_false(box.retire_unbound_structural_subtree("root"));
    check_false(box.retire_unbound_structural_subtree("not-present"));
    check_equal(box.owned_element_count(), count);

    // The old raw Element pointer becomes stale after actual deletion.
    // Inspection must authenticate its ADDRESS against Box-owned unique_ptr
    // entries before reading any freed bytes, including owner_box_/parent_.
    const flexUI::Element *stale_panel_address = panel;
    check(box.retire_unbound_structural_subtree("scope::panel"));
    const auto stale_snapshot = box.inspect_subtree_borrowers(stale_panel_address);
    check(stale_snapshot.error == flexUI::BoxSubtreeBorrowErrorCode::InvalidRoot);
    check_equal(box.owned_element_count(), count - 3u);
    check_equal(host->child_count(), std::size_t{2});
    check_equal(host->child_at(0), left);
    check_equal(host->child_at(1), right);
    check_null(box.get_by_id("scope::panel"));
    check_null(box.get_by_id("scope::inner"));
    check_null(box.resolve_handle(old_panel));
    check_null(box.resolve_handle(old_inner));
    check_equal(box.resolve_handle(right_handle), right);
    check_equal(box.get_by_id("left"), left);
    check_equal(box.cached_style_element_count() <= styled_count, true);
    check_false(box.retire_unbound_structural_subtree("scope::panel"));

    // Reusing an ID must not resurrect an old handle even if the allocator
    // recycles the exact Element address.
    auto *next_panel = box.create("div", "scope::panel");
    host->append(next_panel);
    const auto reincarnated = box.handle_for(*next_panel);
    check(reincarnated);
    check(reincarnated.generation != old_panel.generation);
    check_null(box.resolve_handle(old_panel));
    check_equal(box.resolve_handle(reincarnated), next_panel);
    box.update();
  }

  it("refuses bound, widget-owned and script-handler subtrees") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *host = box.create("div", "host");
    auto *bound = box.create("div", "bound");
    auto *event = box.create("div", "event");
    root->append(host);
    host->append(bound);
    host->append(event);
    box.set_root(root);
    box.bindings().inputs().set_string("caption", "Ready");
    const auto bound_handle = box.bindings().targets().bind_text(*bound, "caption");
    check(bound_handle);
    const auto old_bound = box.handle_for(*bound);
    check_false(box.retire_unbound_structural_subtree("bound"));
    check_equal(box.resolve_handle(old_bound), bound);
    check(box.bindings().targets().unbind(bound_handle.id));
    check(box.retire_unbound_structural_subtree("bound"));
    check_null(box.resolve_handle(old_bound));
    check_equal(box.bindings().stats().binding_count, std::size_t{0});

    event->set_attribute("data-flexui-on-click", "save_document");
    check_false(box.retire_unbound_structural_subtree("event"));
    event->remove_attribute("data-flexui-on-click");
    // Native callback closures are also long-lived event ownership and
    // must be explicitly unregistered before deleting a whole subtree.
    event->on_click([] {});
    check_false(box.retire_unbound_structural_subtree("event"));
    check_equal(box.get_by_id("event"), event);
    event->on_click({});
    check(box.retire_unbound_structural_subtree("event"));

    auto *widget = box.create_widget<flexUI::LabelWidget>(
        "label", "widget", std::string("Widget"));
    host->append(widget);
    check_false(box.retire_unbound_structural_subtree("widget"));
    check_equal(box.get_by_id("widget"), widget);
    check_equal(host->child_count(), std::size_t{1});
  }

  it("blocks reentrant subtree deletion inside native event propagation") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *target = box.create("div", "target");
    auto *victim = box.create("div", "victim");
    root->append(target);
    root->append(victim);
    box.set_root(root);
    box.set_focus(target);
    const auto victim_handle = box.handle_for(*victim);
    bool observed = false;
    bool rejected = false;
    box.set_event_callback([&](flexUI::Element &element,
                               const flexUI::Event &) {
      if (&element == target) {
        observed = true;
        rejected = !box.retire_unbound_structural_subtree("victim");
      }
    });
    auto event = flexUI::Event::key_down(flexUI::KeyCode::A);
    box.dispatch_event(event);
    check(observed);
    check(rejected);
    check_equal(box.resolve_handle(victim_handle), victim);
    check(box.retire_unbound_structural_subtree("victim"));
    check_null(box.resolve_handle(victim_handle));
    check_equal(box.get_by_id("target"), target);
  }

  it("rejects detached indexed subtrees and active-widget projections") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *host = box.create("div", "host");
    root->append(host);
    box.set_root(root);

    // Box::create() registers an ID before its Element is necessarily
    // attached to the visible application tree.
    auto *detached = box.create("div", "detached");
    auto *detached_child = box.create("div", "detached::child");
    detached->append(detached_child);
    check_false(box.retire_unbound_structural_subtree("detached::child"));
    check_equal(box.get_by_id("detached::child"), detached_child);
    host->append(detached);

    box.register_active_widget(detached_child);
    check_false(box.retire_unbound_structural_subtree("detached::child"));
    check_equal(box.get_by_id("detached::child"), detached_child);
    box.unregister_active_widget(detached_child);

    const auto stale = box.handle_for(*detached_child);
    check(box.retire_unbound_structural_subtree("detached::child"));
    check_null(box.resolve_handle(stale));
    check_equal(box.get_by_id("detached"), detached);
    check_equal(detached->child_count(), std::size_t{0});
  }

  it("refuses cross-thread destruction before reading mutable Box state") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *victim = box.create("div", "victim");
    root->append(victim);
    box.set_root(root);
    const auto handle = box.handle_for(*victim);
    bool accepted = true;
    std::thread worker([&] {
      accepted = box.retire_unbound_structural_subtree("victim");
    });
    worker.join();
    check_false(accepted);
    check_equal(box.resolve_handle(handle), victim);
    check(box.retire_unbound_structural_subtree("victim"));
    check_null(box.resolve_handle(handle));
  }

  it("refuses retirement from Boxes with immutable compiled UI programs") {
    const auto compiled = flexUI::compile_ui_xml(
        "<ui name=\"Compiled\"><div id=\"root\"><div id=\"child\"/></div></ui>");
    check(static_cast<bool>(compiled));
    if (!compiled) return;
    flexUI::Box box(nullptr);
    const auto mounted = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program);
    check(static_cast<bool>(mounted));
    if (!mounted) return;
    const auto child = box.handle_for(*box.get_by_id("child"));
    check(child);
    check_false(box.retire_unbound_structural_subtree("child"));
    check_equal(box.resolve_handle(child), box.get_by_id("child"));
    check_equal(box.owned_element_count(), std::size_t{2});
  }

  it("protects live keyed-repeater container leases even when empty") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *host = box.create("div", "repeater-host");
    auto *outer = box.create("div", "outer");
    root->append(outer);
    outer->append(host);
    box.set_root(root);
    {
      flexUI::UiKeyedRepeater repeater(box, *host, 8u);
      check_false(box.retire_unbound_structural_subtree("repeater-host"));
      check_false(box.retire_unbound_structural_subtree("outer"));
      check_equal(box.get_by_id("repeater-host"), host);
      bool refused = false;
      try {
        flexUI::UiKeyedRepeater duplicate(box, *host, 8u);
      } catch (const std::invalid_argument &) {
        refused = true;
      }
      check(refused);
    }
    const auto retired = box.handle_for(*host);
    check(box.retire_unbound_structural_subtree("repeater-host"));
    check_null(box.resolve_handle(retired));
    check_equal(box.get_by_id("outer"), outer);
  }

  it("refuses retirement of a pending focus target during focus-out callbacks") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *old_focus = box.create("div", "old-focus");
    auto *incoming = box.create("div", "incoming");
    root->append(old_focus);
    root->append(incoming);
    box.set_root(root);
    const auto incoming_handle = box.handle_for(*incoming);
    box.set_focus(old_focus);

    bool observed = false;
    bool rejected = false;
    box.set_framework_event_callback([&](flexUI::Element &element,
                                         const flexUI::Event &event) {
      if (&element == old_focus &&
          event.type == flexUI::EventType::FocusOut) {
        observed = true;
        rejected = !box.retire_unbound_structural_subtree("incoming");
      }
    });

    // The incoming node is not focused until after focus_out returns. The
    // borrowed-pointer dispatch barrier, rather than state bits, protects it.
    box.set_focus(incoming);
    check(observed);
    check(rejected);
    check_equal(box.focused_element(), incoming);
    check_equal(box.resolve_handle(incoming_handle), incoming);
    box.set_focus(nullptr);
    check(box.retire_unbound_structural_subtree("incoming"));
    check_null(box.resolve_handle(incoming_handle));
  }

  it("blocks retirement from widget layout and frame callbacks") {
    struct ReentrantWidget final : flexUI::Widget {
      flexUI::Box *box = nullptr;
      bool *layout_called = nullptr;
      bool *layout_rejected = nullptr;
      bool *frame_called = nullptr;
      bool *frame_rejected = nullptr;

      void emit_render_commands(const flexUI::Element &,
                                flexUI::RenderCommandList &) override {}
      const char *type_name() const override { return "ReentrantWidget"; }

      void sync_host_semantics_for_layout(flexUI::Element &) override {
        *layout_called = true;
        *layout_rejected =
            !box->retire_unbound_structural_subtree("victim");
      }
      void update(float, flexUI::Element &) override {
        *frame_called = true;
        *frame_rejected =
            !box->retire_unbound_structural_subtree("victim");
      }
    };

    flexUI::Box box(nullptr);
    bool layout_called = false;
    bool layout_rejected = false;
    bool frame_called = false;
    bool frame_rejected = false;
    auto widget = std::make_unique<ReentrantWidget>();
    widget->box = &box;
    widget->layout_called = &layout_called;
    widget->layout_rejected = &layout_rejected;
    widget->frame_called = &frame_called;
    widget->frame_rejected = &frame_rejected;

    auto *root = box.create("div", "root");
    auto *host = box.create_with_widget("label", std::move(widget), "host");
    auto *victim = box.create("div", "victim");
    root->append(host);
    root->append(victim);
    box.set_root(root);
    const auto old_handle = box.handle_for(*victim);
    box.update();
    check(layout_called);
    check(layout_rejected);
    check_equal(box.resolve_handle(old_handle), victim);
    box.update_time(16.0f);
    check(frame_called);
    check(frame_rejected);
    check_equal(box.resolve_handle(old_handle), victim);
    check(box.retire_unbound_structural_subtree("victim"));
    check_null(box.resolve_handle(old_handle));
  }

  it("refuses focused, hovered or captured subtree retirement") {
    flexUI::Box box(nullptr);
    auto *root = box.create("div", "root");
    auto *focus = box.create("div", "focus");
    root->append(focus);
    box.set_root(root);
    box.set_focus(focus);
    check_false(box.retire_unbound_structural_subtree("focus"));
    check_equal(box.get_by_id("focus"), focus);
    box.set_focus(nullptr);
    box.set_mouse_capture(focus);
    check_false(box.retire_unbound_structural_subtree("focus"));
    box.release_mouse_capture(focus);
    check(box.retire_unbound_structural_subtree("focus"));
    check_null(box.get_by_id("focus"));
  }
}

spec("Scoped XML components are compiled into one owner/controller program") {
  it("maps each scoped template input to an explicit global typed source") {
    const auto base = flexUI::compile_ui_xml(
        "<ui name=\"Root\"><main id=\"root\"><section id=\"left\"/>"
        "<section id=\"right\"/></main></ui>");
    const auto component = flexUI::compile_ui_xml(R"xml(
      <ui name="Panel" xmlns:bind="urn:flexui:binding">
        <div id="panel">
          <label id="caption" bind:text="$caption">Initial</label>
          <div id="badge" bind:class_active="${enabled &amp;&amp; count &gt; 1}">
            Badge
          </div>
          <label id="shared" bind:text="$footer">Shared</label>
        </div>
      </ui>
    )xml");
    check(static_cast<bool>(base));
    check(static_cast<bool>(component));
    if (!base || !component) return;
    const auto registry = flexUI::WidgetRegistry::builtins();
    const auto combined = flexUI::compile_scoped_ui_components(
        *base.program,
        {{"sales", "left", component.program,
          {{"caption", "sales_caption"}, {"enabled", "sales_enabled"},
           {"count", "sales_count"}}},
         {"costs", "right", component.program,
          {{"caption", "costs_caption"}, {"enabled", "costs_enabled"},
           {"count", "costs_count"}}}},
        registry);
    check(static_cast<bool>(combined));
    if (!combined) return;
    check_equal(combined.program->bindings().size(), std::size_t{6});
    bool saw_sales_bool = false;
    bool saw_costs_bool = false;
    for (const auto &binding : combined.program->bindings()) {
      if (binding.element_id == "sales::caption")
        check_equal(binding.expression, "sales_caption");
      if (binding.element_id == "costs::caption")
        check_equal(binding.expression, "costs_caption");
      if (binding.element_id == "sales::badge") {
        saw_sales_bool = true;
        check_equal(binding.expression, "sales_enabled && sales_count > 1");
      }
      if (binding.element_id == "costs::badge") {
        saw_costs_bool = true;
        check_equal(binding.expression, "costs_enabled && costs_count > 1");
      }
      if (binding.element_id == "sales::shared" ||
          binding.element_id == "costs::shared")
        check_equal(binding.expression, "footer");
    }
    check(saw_sales_bool && saw_costs_bool);
    flexUI::Box box(nullptr);
    box.bindings().inputs().set_string("sales_caption", "Revenue");
    box.bindings().inputs().set_string("costs_caption", "Expenses");
    box.bindings().inputs().set_string("footer", "Common");
    box.bindings().inputs().set_bool("sales_enabled", true);
    box.bindings().inputs().set_bool("costs_enabled", true);
    box.bindings().inputs().set_number("sales_count", 5);
    box.bindings().inputs().set_number("costs_count", 0);
    const auto installed = flexUI::UiDocumentInstantiator::instantiate(
        box, *combined.program, registry);
    check(static_cast<bool>(installed));
    if (!installed) return;
    box.update();
    check_equal(box.get_by_id("sales::caption")->text(), "Revenue");
    check_equal(box.get_by_id("costs::caption")->text(), "Expenses");
    check_equal(box.get_by_id("sales::shared")->text(), "Common");
    check_equal(box.get_by_id("costs::shared")->text(), "Common");
    check(box.get_by_id("sales::badge")->has_class(flexUI::Symbol("active")));
    check_false(box.get_by_id("costs::badge")->has_class(flexUI::Symbol("active")));
    box.bindings().inputs().set_number("costs_count", 3);
    box.bindings().inputs().set_string("sales_caption", "Sales Updated");
    box.update();
    check(box.get_by_id("costs::badge")->has_class(flexUI::Symbol("active")));
    check_equal(box.get_by_id("sales::caption")->text(), "Sales Updated");
    check_equal(box.get_by_id("costs::caption")->text(), "Expenses");
  }

  it("rewrites whole MIR input tokens but never the built-in function") {
    const auto root = flexUI::compile_ui_xml(
        "<ui name=\"Root\"><main id=\"root\"><div id=\"host\"/></main></ui>");
    const auto panel = flexUI::compile_ui_xml(R"xml(
      <ui name="Panel" xmlns:bind="urn:flexui:binding">
        <div id="badge"
             bind:class_active="${max(count, counted) &gt; 1 &amp;&amp; count &gt; 0}"/>
      </ui>
    )xml");
    check(static_cast<bool>(root));
    check(static_cast<bool>(panel));
    if (!root || !panel) return;
    const auto compiled = flexUI::compile_scoped_ui_components(
        *root.program,
        {{"sales", "host", panel.program,
          {{"count", "sales_count"}, {"counted", "sales_counted"}}}},
        flexUI::WidgetRegistry::builtins());
    check(static_cast<bool>(compiled));
    if (!compiled) return;
    check_equal(compiled.program->bindings().size(), std::size_t{1});
    check_equal(compiled.program->bindings().front().expression,
                "max(sales_count, sales_counted) > 1 && sales_count > 0");
  }

  it("refuses duplicate unused or invalid source aliases before Box ownership") {
    const auto base = flexUI::compile_ui_xml(
        "<ui name=\"App\"><main id=\"root\"><div id=\"host\"/></main></ui>");
    const auto component = flexUI::compile_ui_xml(
        "<ui name=\"Panel\" xmlns:bind=\"urn:flexui:binding\">"
        "<label id=\"caption\" bind:text=\"$caption\">Initial</label></ui>");
    check(static_cast<bool>(base));
    check(static_cast<bool>(component));
    if (!base || !component) return;
    const auto registry = flexUI::WidgetRegistry::builtins();
    const auto validate = [&](std::vector<flexUI::UiScopedInputAlias> aliases) {
      return flexUI::compile_scoped_ui_components(
          *base.program, {{"scope", "host", component.program, std::move(aliases)}},
          registry);
    };
    const auto duplicate = validate({{"caption", "one"}, {"caption", "two"}});
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error.code == UiDocumentErrorCode::InvalidComponentScope);
    const auto unused = validate({{"missing", "one"}});
    check_false(static_cast<bool>(unused));
    check(unused.error.code == UiDocumentErrorCode::InvalidComponentScope);
    const auto invalid = validate({{"caption", "invalid-source"}});
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code == UiDocumentErrorCode::InvalidComponentScope);
    check_equal(base.program->definition().root.children.size(), std::size_t{1});
  }

  it("rewrites two identical local event and binding IDs without a second tree") {
    const auto app = flexUI::compile_ui_xml(R"xml(
      <ui name="Dashboard">
        <main id="root">
          <section id="left"/>
          <section id="right"/>
        </main>
      </ui>
    )xml");
    const auto component = flexUI::compile_ui_xml(R"xml(
      <ui name="Panel" xmlns:on="urn:flexui:event"
          xmlns:bind="urn:flexui:binding">
        <div id="panel" slot="frame">
          <button id="action" slot="action" on:click="save_document">Save</button>
          <label id="caption" bind:text="$caption">Initial</label>
        </div>
      </ui>
    )xml");
    check(static_cast<bool>(app));
    check(static_cast<bool>(component));
    const auto registry = flexUI::WidgetRegistry::builtins();
    const std::vector<flexUI::UiScopedComponent> instances = {
        {"sales", "left", component.program},
        {"costs", "right", component.program}};
    const auto combined = flexUI::compile_scoped_ui_components(
        *app.program, instances, registry);
    check(static_cast<bool>(combined));
    if (!combined) return;
    check_equal(combined.program->event_bindings().size(), std::size_t{2});
    check_equal(combined.program->bindings().size(), std::size_t{2});
    // The compiler contributes the ONLY scoped owner provenance. Source
    // declaration order matches the original host: both component trees can
    // use the same template IDs but must not share an owning node record.
    check_equal(combined.scoped_ownership.size(), std::size_t{2});
    const auto &sales_owner = combined.scoped_ownership[0];
    const auto &costs_owner = combined.scoped_ownership[1];
    check_equal(sales_owner.scope, "sales");
    check_equal(costs_owner.scope, "costs");
    check_equal(sales_owner.host_id, "left");
    check_equal(costs_owner.host_id, "right");
    check_equal(sales_owner.root_child_index, std::size_t{0});
    check_equal(costs_owner.root_child_index, std::size_t{0});
    check_equal(sales_owner.nodes.size(), std::size_t{3});
    check_equal(costs_owner.nodes.size(), std::size_t{3});
    check_equal(sales_owner.nodes[0].qualified_id, "sales::panel");
    check(sales_owner.nodes[0].parent_index ==
          flexUI::UiScopedOwnedNode::kNoParent);
    check_equal(sales_owner.nodes[1].qualified_id, "sales::action");
    check_equal(sales_owner.nodes[1].parent_index, std::size_t{0});
    check_equal(sales_owner.nodes[1].child_index, std::size_t{0});
    check(sales_owner.nodes[1].widget);
    check(sales_owner.nodes[1].has_compiled_events);
    check_false(sales_owner.nodes[1].has_compiled_bindings);
    check_equal(sales_owner.nodes[2].qualified_id, "sales::caption");
    check(sales_owner.nodes[2].has_compiled_bindings);
    check(sales_owner.nodes[2].widget);
    check_equal(costs_owner.nodes[1].qualified_id, "costs::action");
    check_equal(costs_owner.nodes[2].qualified_id, "costs::caption");
    check_not_null(combined.program->find_event_binding(
        "sales::action", flexUI::UiEventKind::Click));
    check_not_null(combined.program->find_event_binding(
        "costs::action", flexUI::UiEventKind::Click));
    check_null(combined.program->find_event_binding(
        "action", flexUI::UiEventKind::Click));
    flexUI::Box box(nullptr);
    box.bindings().inputs().set_string("caption", "Revenue");
    const auto installed = flexUI::UiDocumentInstantiator::instantiate(
        box, *combined.program, registry);
    check(static_cast<bool>(installed));
    if (!installed) return;
    check_equal(box.root(), installed.tree.root);
    auto* sales = box.get_by_id("sales::action");
    auto* costs = box.get_by_id("costs::action");
    check_not_null(sales);
    check_not_null(costs);
    check(sales != costs);
    check_null(box.get_by_id("action"));
    check_equal(*sales->attribute("data-slot"), "action");
    check_equal(*costs->attribute("data-slot"), "action");
    check_equal(installed.tree.slots_by_name.size(), std::size_t{0});
    check_equal(sales->parent_elem()->id(), "sales::panel");
    check_equal(costs->parent_elem()->id(), "costs::panel");
    box.update();
    check_equal(box.get_by_id("sales::caption")->text(), "Revenue");
    check_equal(box.get_by_id("costs::caption")->text(), "Revenue");
  }

  it("records eventless anonymous component roots and Widget owner locations") {
    const auto app = flexUI::compile_ui_xml(
        "<ui name=\"OwnerApp\"><main id=\"root\">"
        "<section id=\"host\"><div id=\"base\"/></section>"
        "</main></ui>");
    const auto component = flexUI::compile_ui_xml(
        "<ui name=\"Anonymous\"><div>"
        "<button id=\"send\">Send</button><span>Plain</span></div></ui>");
    check(static_cast<bool>(app));
    check(static_cast<bool>(component));
    if (!app || !component) return;
    const auto registry = flexUI::WidgetRegistry::builtins();
    const auto compiled = flexUI::compile_scoped_ui_components(
        *app.program, {{"eventless", "host", component.program}}, registry);
    check(static_cast<bool>(compiled));
    if (!compiled) return;
    check_equal(compiled.scoped_ownership.size(), std::size_t{1});
    const auto &scope = compiled.scoped_ownership[0];
    check_equal(scope.scope, "eventless");
    check_equal(scope.host_id, "host");
    check_equal(scope.root_child_index, std::size_t{1});
    check_equal(scope.nodes.size(), std::size_t{3});
    check(scope.nodes[0].qualified_id.empty());
    check_equal(scope.nodes[0].tag, "div");
    check(scope.nodes[0].parent_index == flexUI::UiScopedOwnedNode::kNoParent);
    check_equal(scope.nodes[0].child_index, std::size_t{1});
    check_false(scope.nodes[0].has_compiled_events);
    check_false(scope.nodes[0].has_compiled_bindings);
    check_false(scope.nodes[0].widget);
    check_equal(scope.nodes[1].qualified_id, "eventless::send");
    check_equal(scope.nodes[1].parent_index, std::size_t{0});
    check_equal(scope.nodes[1].child_index, std::size_t{0});
    check(scope.nodes[1].widget);
    check_false(scope.nodes[1].has_compiled_events);
    check(scope.nodes[2].qualified_id.empty());
    check_equal(scope.nodes[2].tag, "span");
    check_equal(scope.nodes[2].parent_index, std::size_t{0});
    check_equal(scope.nodes[2].child_index, std::size_t{1});

    // Eventless compiled components do not need manufactured script routes.
    check(compiled.program->event_bindings().empty());
    flexUI::Box box(nullptr);
    auto installed = flexUI::UiDocumentInstantiator::instantiate(
        box, *compiled.program, registry);
    check(static_cast<bool>(installed));
    if (!installed) return;
    auto *host = box.get_by_id("host");
    check_not_null(host);
    if (!host || host->children().size() < 2) return;
    const auto *anonymous = dynamic_cast<flexUI::Element *>(
        host->children()[scope.root_child_index]);
    check_not_null(anonymous);
    if (!anonymous) return;
    check(anonymous->id().empty());
    check_equal(anonymous->tag(), scope.nodes[0].tag);
    check_not_null(box.get_by_id("eventless::send"));
  }

  it("charges repeated components against one total node budget before copying") {
    const auto app = flexUI::compile_ui_xml(
        "<ui name=\"App\"><main id=\"root\"><div id=\"left\"/>"
        "<div id=\"right\"/></main></ui>");
    const auto component = flexUI::compile_ui_xml(
        "<ui name=\"Panel\"><div id=\"panel\"><span id=\"inner\"/>"
        "</div></ui>");
    check(static_cast<bool>(app));
    check(static_cast<bool>(component));
    flexUI::UiDocumentLimits limits;
    limits.max_nodes = 6;  // base=3, each component=2, total=7.
    const auto oversized = flexUI::compile_scoped_ui_components(
        *app.program, {{"sales", "left", component.program},
                       {"costs", "right", component.program}},
        flexUI::WidgetRegistry::builtins(), limits);
    check_false(static_cast<bool>(oversized));
    check(oversized.error.code == UiDocumentErrorCode::NodeLimitExceeded);
    check_equal(app.program->definition().root.children.size(),
                std::size_t{2});
    // The same input is accepted when the *aggregate* budget is sufficient.
    limits.max_nodes = 7;
    const auto admitted = flexUI::compile_scoped_ui_components(
        *app.program, {{"sales", "left", component.program},
                       {"costs", "right", component.program}},
        flexUI::WidgetRegistry::builtins(), limits);
    check(static_cast<bool>(admitted));
  }

  it("fails closed on duplicate scopes, unknown hosts, and widget hosts") {
    const auto base = flexUI::compile_ui_xml(
        "<ui name=\"Root\"><main id=\"root\"><div id=\"host\"/>"
        "<button id=\"not-a-host\">X</button></main></ui>");
    const auto component = flexUI::compile_ui_xml(
        "<ui name=\"Panel\"><button id=\"send\">Send</button></ui>");
    check(static_cast<bool>(base));
    check(static_cast<bool>(component));
    const auto registry = flexUI::WidgetRegistry::builtins();
    const auto duplicate = flexUI::compile_scoped_ui_components(
        *base.program,
        {{"first", "host", component.program},
         {"first", "host", component.program}}, registry);
    check_false(static_cast<bool>(duplicate));
    check(duplicate.error.code == UiDocumentErrorCode::InvalidComponentScope);
    const auto missing = flexUI::compile_scoped_ui_components(
        *base.program, {{"first", "unknown", component.program}}, registry);
    check_false(static_cast<bool>(missing));
    check(missing.error.code == UiDocumentErrorCode::MissingComponentHost);
    const auto widget = flexUI::compile_scoped_ui_components(
        *base.program, {{"first", "not-a-host", component.program}}, registry);
    check_false(static_cast<bool>(widget));
    check(widget.error.code == UiDocumentErrorCode::InvalidMountTarget);
    const auto invalid = flexUI::compile_scoped_ui_components(
        *base.program, {{"bad::scope", "host", component.program}}, registry);
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code == UiDocumentErrorCode::InvalidComponentScope);
    // Component IDs are scoped, never leaked to a separate live Box by
    // failed compilation; the pristine base program remains unchanged.
    check_equal(base.program->definition().root.children.size(), std::size_t{2});
    check_null(base.program->find_event_binding(
        "first::send", flexUI::UiEventKind::Click));
  }
}


spec("Scoped XML components preserve local label and accessibility references") {
  it("qualifies HTML-style local IDREF, fragment href and multi-target ARIA") {
    const auto app = flexUI::compile_ui_xml(
        "<ui name=\"App\"><main id=\"root\"><section id=\"left\"/>"
        "<section id=\"right\"/></main></ui>");
    const auto component = flexUI::compile_ui_xml(R"xml(
      <ui name="Fields" xmlns:attr="urn:flexui:attribute">
        <div id="panel" slot="frame">
          <label id="title" attr:for="field"
                 attr:aria-controls="field hint">Name</label>
          <input id="field" />
          <span id="hint">Help</span>
          <a id="link" attr:href="#field">Jump to field</a>
        </div>
      </ui>
    )xml");
    check(static_cast<bool>(app));
    check(static_cast<bool>(component));
    auto registry = flexUI::WidgetRegistry::builtins();
    const auto merged = flexUI::compile_scoped_ui_components(
        *app.program, {{"sales", "left", component.program},
                       {"costs", "right", component.program}},
        registry);
    check(static_cast<bool>(merged));
    if (!merged) return;
    flexUI::Box box(nullptr);
    const auto installed = flexUI::UiDocumentInstantiator::instantiate(
        box, *merged.program, registry);
    check(static_cast<bool>(installed));
    if (!installed) return;
    check_equal(*box.get_by_id("sales::title")->attribute("for"),
                "sales::field");
    check_equal(*box.get_by_id("sales::title")->attribute("aria-controls"),
                "sales::field sales::hint");
    check_equal(*box.get_by_id("costs::title")->attribute("for"),
                "costs::field");
    check_equal(*box.get_by_id("sales::link")->attribute("href"),
                "#sales::field");
    check_equal(*box.get_by_id("costs::link")->attribute("href"),
                "#costs::field");
  }


  it("qualifies plain XML IDREF and local links for each component instance") {
    const auto app = flexUI::compile_ui_xml(
        "<ui name=\"App\"><main id=\"root\"><div id=\"left\"/>"
        "<div id=\"right\"/></main></ui>");
    const auto component = flexUI::compile_ui_xml(R"xml(
      <ui name="Plain" xmlns:attr="urn:flexui:attribute">
        <div id="panel">
          <label id="plain-label" for="field" aria-labelledby="title hint"
                 aria-flowto="field hint">Name</label>
          <span id="title">Title</span>
          <span id="hint">Help</span>
          <input id="field" />
          <span id="detail" aria-details="hint"
                aria-errormessage="hint" headers="title hint"/>
          <a id="plain-link" href="#field">Local</a>
          <a id="external-link" href="https://example.com/#field">External</a>
        </div>
      </ui>
    )xml");
    check(static_cast<bool>(app));
    check(static_cast<bool>(component));
    if (!app || !component) return;
    const auto registry = flexUI::WidgetRegistry::builtins();
    const auto combined = flexUI::compile_scoped_ui_components(
        *app.program, {{"sales", "left", component.program},
                       {"costs", "right", component.program}}, registry);
    check(static_cast<bool>(combined));
    if (!combined) return;
    flexUI::Box box(nullptr);
    const auto mounted = flexUI::UiDocumentInstantiator::instantiate(
        box, *combined.program, registry);
    check(static_cast<bool>(mounted));
    if (!mounted) return;
    check_equal(*box.get_by_id("sales::plain-label")->attribute("for"),
                "sales::field");
    check_equal(*box.get_by_id("costs::plain-label")->attribute("for"),
                "costs::field");
    check_equal(*box.get_by_id("sales::plain-label")->attribute("aria-labelledby"),
                "sales::title sales::hint");
    check_equal(*box.get_by_id("costs::plain-label")->attribute("aria-flowto"),
                "costs::field costs::hint");
    check_equal(*box.get_by_id("sales::detail")->attribute("aria-details"),
                "sales::hint");
    check_equal(*box.get_by_id("costs::detail")->attribute("aria-errormessage"),
                "costs::hint");
    check_equal(*box.get_by_id("sales::detail")->attribute("headers"),
                "sales::title sales::hint");
    check_equal(*box.get_by_id("sales::plain-link")->attribute("href"),
                "#sales::field");
    check_equal(*box.get_by_id("costs::plain-link")->attribute("href"),
                "#costs::field");
    check_equal(*box.get_by_id("sales::external-link")->attribute("href"),
                "https://example.com/#field");
  }

  it("rejects conflicting plain/attr aliases and unresolved plain IDREFs") {
    const auto app = flexUI::compile_ui_xml(
        "<ui name=\"App\"><main id=\"root\"><div id=\"left\"/></main></ui>");
    const auto ambiguous = flexUI::compile_ui_xml(R"xml(
      <ui name="Ambiguous" xmlns:attr="urn:flexui:attribute">
        <div id="panel">
          <span id="field"/>
          <a id="link" href="#field" attr:href="#field">Link</a>
        </div>
      </ui>
    )xml");
    const auto missing = flexUI::compile_ui_xml(R"xml(
      <ui name="Missing"><label id="label" for="foreign">Label</label></ui>
    )xml");
    check(static_cast<bool>(app));
    check(static_cast<bool>(ambiguous));
    check(static_cast<bool>(missing));
    if (!app || !ambiguous || !missing) return;
    const auto registry = flexUI::WidgetRegistry::builtins();
    const auto alias_error = flexUI::compile_scoped_ui_components(
        *app.program, {{"sales", "left", ambiguous.program}}, registry);
    check_false(static_cast<bool>(alias_error));
    check(alias_error.error.code == UiDocumentErrorCode::InvalidProperty);
    const auto missing_error = flexUI::compile_scoped_ui_components(
        *app.program, {{"sales", "left", missing.program}}, registry);
    check_false(static_cast<bool>(missing_error));
    check(missing_error.error.code == UiDocumentErrorCode::InvalidComponentScope);
    check_equal(app.program->definition().root.children.size(), std::size_t{1});
  }

  it("rejects unresolved cross-instance references before mounting") {
    const auto app = flexUI::compile_ui_xml(
        "<ui name=\"App\"><main id=\"root\"><div id=\"left\"/>"
        "</main></ui>");
    const auto invalid = flexUI::compile_ui_xml(R"xml(
      <ui name="Wrong" xmlns:attr="urn:flexui:attribute">
        <div id="panel">
          <label id="label" attr:for="missing">Label</label>
        </div>
      </ui>
    )xml");
    check(static_cast<bool>(app));
    check(static_cast<bool>(invalid));
    const auto merged = flexUI::compile_scoped_ui_components(
        *app.program, {{"sales", "left", invalid.program}},
        flexUI::WidgetRegistry::builtins());
    check_false(static_cast<bool>(merged));
    check(merged.error.code == UiDocumentErrorCode::InvalidComponentScope);
    check_equal(app.program->definition().root.children.size(),
                std::size_t{1});
  }
}

spec("FlexUI M4b6f exact Box Element ownership membership") {
  it("rejects externally allocated children carrying a copied Box back-pointer") {
    flexUI::Box box(nullptr);
    auto *root = box.create("main", "root");
    auto *container = box.create("section", "container");
    root->append(container);
    box.set_root(root);
    auto original = box.inspect_subtree_borrowers(container);
    check(static_cast<bool>(original));
    check_equal(original.box_owned_elements, std::size_t{1});

    // Element::append propagates the Box back-pointer to an external
    // element, but no unique_ptr ownership is ever transferred.
    flexUI::Element foreign;
    check_equal(container->append(&foreign), &foreign);
    const auto invalid = box.inspect_subtree_borrowers(container);
    check_false(static_cast<bool>(invalid));
    check(invalid.error == flexUI::BoxSubtreeBorrowErrorCode::InconsistentTree);
    const auto direct = box.inspect_subtree_borrowers(&foreign);
    check_false(static_cast<bool>(direct));
    check(direct.error == flexUI::BoxSubtreeBorrowErrorCode::InvalidRoot);

    // The failed read must not detach, free, or invalidate sibling nodes.
    check_equal(container->child_count(), std::size_t{1});
    check(container->remove(&foreign));
    check_equal(container->child_count(), std::size_t{0});
    const auto recovered = box.inspect_subtree_borrowers(container);
    check(static_cast<bool>(recovered));
    check_equal(recovered.box_owned_elements, std::size_t{1});
  }
}

spec("FlexUI M4b6c built-in Widget owner audits") {
  it("counts actual form callbacks and hidden animation ownership without retiring") {
    flexUI::CheckboxWidget checkbox("Chosen");
    flexUI::RadioWidget radio;
    flexUI::SwitchWidget toggle;
    flexUI::SliderWidget slider;
    flexUI::ProgressBarWidget progress;
    check(checkbox.inspect_local_borrowers().audited);
    check(radio.inspect_local_borrowers().audited);
    check(toggle.inspect_local_borrowers().audited);
    check(slider.inspect_local_borrowers().audited);
    check(progress.inspect_local_borrowers().audited);
    check_equal(checkbox.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    checkbox.set_change_callback([](bool) {});
    radio.set_change_callback([](const std::string&, const std::string&) {});
    toggle.set_change_callback([](bool) {});
    slider.set_change_callback([](float) {});
    for (const flexUI::Widget* owner :
         {static_cast<flexUI::Widget*>(&checkbox), static_cast<flexUI::Widget*>(&radio),
          static_cast<flexUI::Widget*>(&toggle), static_cast<flexUI::Widget*>(&slider)}) {
      check_equal(owner->inspect_local_borrowers().native_callback_owners, std::size_t{1});
    }
    check_equal(progress.inspect_local_borrowers().native_callback_owners, std::size_t{0});

    checkbox.set_checked(true);
    radio.set_checked(true);
    toggle.set_checked(true);
    progress.set_indeterminate(true);
    check(checkbox.inspect_local_borrowers().interaction_or_callback_active);
    check(radio.inspect_local_borrowers().interaction_or_callback_active);
    check(toggle.inspect_local_borrowers().interaction_or_callback_active);
    check(progress.inspect_local_borrowers().interaction_or_callback_active);
    progress.set_indeterminate(false);
    check_false(progress.inspect_local_borrowers().interaction_or_callback_active);
    checkbox.set_change_callback({});
    radio.set_change_callback({});
    toggle.set_change_callback({});
    slider.set_change_callback({});
    check_equal(checkbox.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    check_equal(radio.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    check_equal(toggle.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    check_equal(slider.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    // Clearing callbacks does NOT silently retire a still-active animation.
    check(checkbox.inspect_local_borrowers().interaction_or_callback_active);
  }

  it("exposes open overlays, tab page borrows and accordion animation") {
    flexUI::SelectWidget select({"one", "two"});
    flexUI::DropdownWidget dropdown;
    flexUI::TabsWidget tabs;
    flexUI::AccordionWidget accordion;
    check(select.inspect_local_borrowers().audited);
    check(dropdown.inspect_local_borrowers().audited);
    check(tabs.inspect_local_borrowers().audited);
    check(accordion.inspect_local_borrowers().audited);
    select.set_change_callback([](int, const std::string&) {});
    dropdown.set_change_callback([](const std::string&) {});
    tabs.set_change_callback([](int, const std::string&) {});
    accordion.set_change_callback([](const std::string&, bool) {});
    check_equal(select.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    check_equal(dropdown.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    check_equal(tabs.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    check_equal(accordion.inspect_local_borrowers().native_callback_owners, std::size_t{1});

    select.set_expanded(true);
    // A pending open target alone is live, but no dropdown height is
    // retained until the native Widget update actually advances a frame.
    check(select.inspect_local_borrowers().interaction_or_callback_active);
    flexUI::Element select_host;
    select.update(16.0f, select_host);
    dropdown.open();
    flexUI::Element foreign_page;
    tabs.add_tab("First", "first", &foreign_page);
    accordion.add_section("Details", "details");
    accordion.expand("details");
    check(select.inspect_local_borrowers().interaction_or_callback_active);
    check(dropdown.inspect_local_borrowers().interaction_or_callback_active);
    check(tabs.inspect_local_borrowers().interaction_or_callback_active);
    check(accordion.inspect_local_borrowers().interaction_or_callback_active);
    // An Element* page borrowed by Tabs must NOT be discarded as harmless.
    tabs.clear_tabs();
    check_false(tabs.inspect_local_borrowers().interaction_or_callback_active);
    dropdown.close();
    check_false(dropdown.inspect_local_borrowers().interaction_or_callback_active);
    select.set_expanded(false);
    // A REAL opening frame has already produced nonzero current height.
    // Closing changes only the target; the still-unwinding animation is
    // a live Widget-local borrower even though the overlay is closed.
    check_false(select.is_expanded());
    check(select.inspect_local_borrowers().interaction_or_callback_active);
    select.update(200.0f, select_host);
    check_false(select.inspect_local_borrowers().interaction_or_callback_active);

    // Separate zero-frame case: opening and closing synchronously without
    // ANY update never creates retained animation height. Do not invent a
    // worker/animation borrower when both heights are genuinely zero.
    select.set_expanded(true);
    select.set_expanded(false);
    check_false(select.inspect_local_borrowers().interaction_or_callback_active);
  }

  it("reports only live scoped native Widget ownership in the one Box") {
    flexUI::Box box(nullptr);
    auto* root = box.create("main", "root");
    auto* target = box.create_widget<flexUI::CheckboxWidget>(
        "checkbox", "scope", std::string("One"), false);
    auto* sibling = box.create_widget<flexUI::ProgressBarWidget>(
        "progress", "sibling", 0.0f, false);
    root->append(target);
    root->append(sibling);
    box.set_root(root);
    auto* owned = static_cast<flexUI::CheckboxWidget*>(target->widget);
    const auto before = box.inspect_subtree_borrowers(target);
    check(static_cast<bool>(before));
    check_equal(before.widget_hosts, std::size_t{1});
    check_equal(before.unverified_widget_hosts, std::size_t{0});
    check_equal(before.native_widget_callback_owners, std::size_t{0});
    const auto other = box.inspect_subtree_borrowers(sibling);
    check(static_cast<bool>(other));
    check_equal(other.active_widget_local_borrowers, std::size_t{0});

    owned->set_change_callback([](bool) {});
    const auto callbacks = box.inspect_subtree_borrowers(target);
    check(static_cast<bool>(callbacks));
    check_equal(callbacks.native_widget_callback_owners, std::size_t{1});
    check_equal(box.inspect_subtree_borrowers(sibling).native_widget_callback_owners,
                std::size_t{0});
    owned->set_checked(true);
    const auto active = box.inspect_subtree_borrowers(target);
    check(static_cast<bool>(active));
    check_equal(active.active_widget_local_borrowers, std::size_t{1});
    check_equal(box.inspect_subtree_borrowers(sibling).active_widget_local_borrowers,
                std::size_t{0});
    // Existing compiled/Widget guards remain intact despite all audits.
    check_false(box.retire_bound_structural_subtree("scope"));
    check_not_null(box.get_by_id("scope"));
  }
}

spec("FlexUI M4b6d overlay, timer and simple native callback owner audits") {
  it("keeps active overlays and perpetual animation visible to scoped preflight") {
    flexUI::TooltipWidget tooltip("Hint");
    flexUI::ModalWidget modal("Heading");
    flexUI::ToastWidget toast("Done");
    flexUI::SpinnerWidget spinner;
    check(tooltip.inspect_local_borrowers().audited);
    check(modal.inspect_local_borrowers().audited);
    check(toast.inspect_local_borrowers().audited);
    check(spinner.inspect_local_borrowers().audited);
    check_false(tooltip.inspect_local_borrowers().interaction_or_callback_active);
    check_false(modal.inspect_local_borrowers().interaction_or_callback_active);
    check_false(toast.inspect_local_borrowers().interaction_or_callback_active);
    check(spinner.inspect_local_borrowers().interaction_or_callback_active);
    tooltip.show();
    modal.open();
    toast.show();
    check(tooltip.inspect_local_borrowers().interaction_or_callback_active);
    check(modal.inspect_local_borrowers().interaction_or_callback_active);
    check(toast.inspect_local_borrowers().interaction_or_callback_active);
    modal.set_close_callback([] {});
    toast.set_dismiss_callback([] {});
    check_equal(modal.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    check_equal(toast.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    modal.set_close_callback({});
    toast.set_dismiss_callback({});
    check_equal(modal.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    check_equal(toast.inspect_local_borrowers().native_callback_owners, std::size_t{0});
  }

  it("audits static presentation and six independent native UI callback types") {
    flexUI::DividerWidget divider;
    flexUI::BadgeWidget badge("New");
    flexUI::CardWidget card;
    check(divider.inspect_local_borrowers().audited);
    check(badge.inspect_local_borrowers().audited);
    check(card.inspect_local_borrowers().audited);
    check_false(divider.inspect_local_borrowers().interaction_or_callback_active);
    check_false(badge.inspect_local_borrowers().interaction_or_callback_active);
    check_false(card.inspect_local_borrowers().interaction_or_callback_active);
    check_equal(card.inspect_local_borrowers().native_callback_owners, std::size_t{0});

    flexUI::GroupButtonWidget group_button;
    flexUI::ToggleGroupWidget toggle_group;
    flexUI::BreadcrumbWidget breadcrumb;
    flexUI::PaginationWidget pagination;
    flexUI::StepperWidget stepper;
    check(group_button.inspect_local_borrowers().audited);
    check(toggle_group.inspect_local_borrowers().audited);
    check(breadcrumb.inspect_local_borrowers().audited);
    check(pagination.inspect_local_borrowers().audited);
    check(stepper.inspect_local_borrowers().audited);
    group_button.set_click_callback([] {});
    toggle_group.set_change_callback([](int, const std::string&) {});
    breadcrumb.set_click_callback([](int, const std::string&) {});
    pagination.set_page_change_callback([](int) {});
    stepper.set_change_callback([](int) {});
    for (const flexUI::Widget* item : {
         static_cast<flexUI::Widget*>(&group_button),
         static_cast<flexUI::Widget*>(&toggle_group),
         static_cast<flexUI::Widget*>(&breadcrumb),
         static_cast<flexUI::Widget*>(&pagination),
         static_cast<flexUI::Widget*>(&stepper)}) {
      check_equal(item->inspect_local_borrowers().native_callback_owners,
                  std::size_t{1});
    }
    group_button.set_click_callback({});
    toggle_group.set_change_callback({});
    breadcrumb.set_click_callback({});
    pagination.set_page_change_callback({});
    stepper.set_change_callback({});
    check_equal(group_button.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    check_equal(toggle_group.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    check_equal(breadcrumb.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    check_equal(pagination.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    check_equal(stepper.inspect_local_borrowers().native_callback_owners, std::size_t{0});
  }
}

spec("FlexUI M4b6e external-source Widget owner retirement preflight") {
  it("flags unretired image sources and native image load callbacks") {
    flexUI::ImageWidget image("assets/image.png");
    check(image.inspect_local_borrowers().audited);
    check(image.inspect_local_borrowers().interaction_or_callback_active);
    image.set_load_callback([](bool) {});
    check_equal(image.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    image.set_load_callback({});
    check_equal(image.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    // A loaded URL still requires backend image-resource lifetime proof.
    check(image.inspect_local_borrowers().interaction_or_callback_active);
    image.set_src("");
    check_false(image.inspect_local_borrowers().interaction_or_callback_active);

    flexUI::AvatarWidget avatar("Alice", "assets/alice.png");
    check(avatar.inspect_local_borrowers().audited);
    check(avatar.inspect_local_borrowers().interaction_or_callback_active);
    avatar.set_image_url("");
    check_false(avatar.inspect_local_borrowers().interaction_or_callback_active);
  }

  it("audits calendar, table, tree, color, and gradient callbacks independently") {
    flexUI::CalendarWidget calendar;
    flexUI::TableWidget table;
    flexUI::TreeWidget tree;
    flexUI::ColorPickerWidget picker;
    flexUI::GradientEditorWidget gradient;
    for (const flexUI::Widget* w : {
         static_cast<flexUI::Widget*>(&calendar),
         static_cast<flexUI::Widget*>(&table),
         static_cast<flexUI::Widget*>(&tree),
         static_cast<flexUI::Widget*>(&picker),
         static_cast<flexUI::Widget*>(&gradient)}) {
      check(w->inspect_local_borrowers().audited);
      check_equal(w->inspect_local_borrowers().native_callback_owners, std::size_t{0});
    }
    calendar.set_select_callback([](const flexUI::Date&) {});
    table.set_select_callback([](int) {});
    tree.set_select_callback([](flexUI::TreeNode*) {});
    picker.set_change_callback([](const flexUI::Color&) {});
    gradient.set_change_callback([] {});
    gradient.set_stop_select_callback([](int, const flexUI::Color&) {});
    check_equal(calendar.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    check_equal(table.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    check_equal(tree.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    check_equal(picker.inspect_local_borrowers().native_callback_owners, std::size_t{1});
    check_equal(gradient.inspect_local_borrowers().native_callback_owners, std::size_t{2});
    // TreeNode references are independently retained in the Widget graph
    // and cannot be treated as a resolved, borrower-free Element subtree.
    tree.add_node("root", "Tree root");
    check(tree.inspect_local_borrowers().interaction_or_callback_active);
    calendar.set_select_callback({});
    table.set_select_callback({});
    tree.set_select_callback({});
    picker.set_change_callback({});
    gradient.set_change_callback({});
    gradient.set_stop_select_callback({});
    check_equal(gradient.inspect_local_borrowers().native_callback_owners, std::size_t{0});
    check(tree.inspect_local_borrowers().interaction_or_callback_active);
    tree.clear();
    check_false(tree.inspect_local_borrowers().interaction_or_callback_active);
  }

  it("retains conservative default denial for Markdown's private projection lifecycle") {
    flexUI::MarkdownWidget markdown("# still mounted");
    const auto owner = markdown.inspect_local_borrowers();
    check_false(owner.audited);
    check_equal(owner.native_callback_owners, std::size_t{0});
    // Projection reconciliation and deferred retirement must first prove
    // their own callback-safe Box/Element destruction order.
  }
}
