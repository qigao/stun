#include <flexUI.h>

#include <tinytest.hpp>

#include <cmath>
#include <limits>
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


spec("Scoped XML components are compiled into one owner/controller program") {
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
