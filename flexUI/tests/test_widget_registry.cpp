#include <flexUI.h>

#include <tinytest.hpp>

#include <memory>
#include <stdexcept>

namespace {

class ThrowingBindWidget final : public flexUI::Widget {
public:
  void emit_render_commands(const flexUI::Element &, flexUI::RenderCommandList &) override {}
  const char *type_name() const override { return "ThrowingBindWidget"; }

private:
  void sync_host_semantics() override { throw std::runtime_error("intentional bind failure"); }
};

} // namespace

spec("FlexUI WidgetRegistry creates typed XML widget trees") {
  it("rejects invalid duplicate and empty factory registrations") {
    flexUI::WidgetRegistry registry;
    check(registry.register_element("div").code == flexUI::WidgetRegistryErrorCode::None);
    check(registry.register_element("div").code == flexUI::WidgetRegistryErrorCode::DuplicateTag);
    check(registry.register_element("").code == flexUI::WidgetRegistryErrorCode::InvalidTag);
    check(registry.register_widget("button", {}).code ==
          flexUI::WidgetRegistryErrorCode::InvalidFactory);
  }

  it("exposes explicit container and widget descriptors") {
    auto registry = flexUI::WidgetRegistry::builtins();
    const auto *container = registry.descriptor("div");
    const auto *widget = registry.descriptor("button");

    check_not_null(container);
    check_not_null(widget);
    if (container && widget) {
      check(container->kind == flexUI::UiNodeKind::Container);
      check(container->content == flexUI::UiContentModel::Children);
      check(widget->kind == flexUI::UiNodeKind::Widget);
      check(widget->content == flexUI::UiContentModel::Children);
    }
  }

  it("validates content models before invoking widget factories") {
    int factory_calls = 0;
    flexUI::WidgetRegistry registry;
    check(registry.register_element("root").code ==
          flexUI::WidgetRegistryErrorCode::None);
    check(registry
              .register_widget(
                  "leaf",
                  [&factory_calls](const flexUI::UiNodeDefinition &) {
                    ++factory_calls;
                    return std::make_unique<flexUI::ButtonWidget>();
                  },
                  flexUI::UiContentModel::Empty)
              .code == flexUI::WidgetRegistryErrorCode::None);

    const auto compiled = flexUI::compile_ui_xml(R"(
      <ui name="Schema">
        <root id="root">
          <leaf id="parent">
            <leaf id="child"/>
          </leaf>
        </root>
      </ui>
    )");
    check(static_cast<bool>(compiled));

    const auto error = registry.validate(*compiled.program);
    check(static_cast<bool>(error));
    check(error.code == flexUI::UiDocumentErrorCode::InvalidNode);
    check(error.line > 0);
    check(error.column > 0);
    check_equal(factory_calls, 0);

    flexUI::Box box(nullptr);
    const auto instantiated =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program, registry);
    check_false(static_cast<bool>(instantiated));
    check(instantiated.error.code == flexUI::UiDocumentErrorCode::InvalidNode);
    check_null(box.root());
    check_equal(factory_calls, 0);
  }

  it("constructs concrete built-in widget types from XML tags") {
    const auto compiled = flexUI::compile_ui_xml(R"(
      <ui name="Controls">
        <div id="root">
          <button id="save" text="Save" disabled="true" />
          <input id="name" placeholder="Name" value="Ada" readonly="true" />
          <checkbox id="enabled" label="Enabled" checked="true" />
          <progress id="work" value="25" indeterminate="false" />
          <select id="choice" options="One, Two, Three" selected_index="1" />
        </div>
      </ui>
    )");
    check(static_cast<bool>(compiled));

    auto registry = flexUI::WidgetRegistry::builtins();
    flexUI::Box box(nullptr);
    const auto instantiated =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program, registry);
    check(static_cast<bool>(instantiated));

    auto *button = dynamic_cast<flexUI::ButtonWidget *>(box.get_by_id("save")->widget);
    check_not_null(button);
    check_equal(button->text(), "Save");
    check_true(button->is_disabled());
    check_not_null(button->label_element());
    check_true(button->label_element()->is_widget_owned());

    auto *input = dynamic_cast<flexUI::InputWidget *>(box.get_by_id("name")->widget);
    check_not_null(input);
    check_equal(input->text(), "Ada");
    check_equal(input->placeholder(), "Name");
    check_true(input->is_readonly());

    auto *checkbox = dynamic_cast<flexUI::CheckboxWidget *>(box.get_by_id("enabled")->widget);
    check_not_null(checkbox);
    check_true(checkbox->is_checked());
    check_equal(checkbox->label(), "Enabled");

    auto *progress = dynamic_cast<flexUI::ProgressBarWidget *>(box.get_by_id("work")->widget);
    check_not_null(progress);
    check_within(progress->value(), 25.0F, 0.001F);

    auto *select = dynamic_cast<flexUI::SelectWidget *>(box.get_by_id("choice")->widget);
    check_not_null(select);
    check_equal(select->options().size(), 3);
    check_equal(select->selected_value(), "Two");
  }

  it("rejects unknown tags without changing the target Box") {
    const auto compiled =
        flexUI::compile_ui_xml("<ui name=\"Unknown\"><custom id=\"subject\"/></ui>");
    check(static_cast<bool>(compiled));

    auto registry = flexUI::WidgetRegistry::builtins();
    const auto schema_error = registry.validate(*compiled.program);
    check(static_cast<bool>(schema_error));
    check(schema_error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);
    check(schema_error.line > 0);
    check(schema_error.column > 0);

    flexUI::Box box(nullptr);
    const auto result =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program, registry);
    check_false(static_cast<bool>(result));
    check(result.error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);
    check_null(box.root());
    check_null(box.get_by_id("subject"));
  }

  it("converts factory failures into structured errors") {
    flexUI::WidgetRegistry registry;
    check(registry
              .register_widget(
                  "broken",
                  [](const flexUI::UiNodeDefinition &) -> std::unique_ptr<flexUI::Widget> {
                    throw std::runtime_error("factory rejected input");
                  })
              .code == flexUI::WidgetRegistryErrorCode::None);

    flexUI::UiDocumentDefinition definition;
    definition.name = "Broken";
    definition.root.tag = "broken";
    definition.root.id = "subject";
    const auto compiled = flexUI::compile_ui_definition(definition);
    check(static_cast<bool>(compiled));

    flexUI::Box box(nullptr);
    const auto result =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program, registry);
    check_false(static_cast<bool>(result));
    check(result.error.code == flexUI::UiDocumentErrorCode::WidgetFactoryFailed);
    check_null(box.root());
    check_null(box.get_by_id("subject"));
  }

  it("rolls back elements widgets and bindings when widget binding fails") {
    flexUI::WidgetRegistry failing_registry;
    check(failing_registry
              .register_widget("throwing",
                               [](const flexUI::UiNodeDefinition &) {
                                 return std::make_unique<ThrowingBindWidget>();
                               })
              .code == flexUI::WidgetRegistryErrorCode::None);

    flexUI::UiDocumentDefinition failing_definition;
    failing_definition.name = "Failing";
    failing_definition.root.tag = "throwing";
    failing_definition.root.id = "failed-root";
    const auto failing_program = flexUI::compile_ui_definition(std::move(failing_definition));
    check(static_cast<bool>(failing_program));

    flexUI::Box box(nullptr);
    const auto failed = flexUI::UiDocumentInstantiator::instantiate(box, *failing_program.program,
                                                                    failing_registry);
    check_false(static_cast<bool>(failed));
    check(failed.error.code == flexUI::UiDocumentErrorCode::BuildFailed);
    check_null(box.root());
    check_null(box.get_by_id("failed-root"));

    auto builtins = flexUI::WidgetRegistry::builtins();
    const auto valid_program =
        flexUI::compile_ui_xml("<ui name=\"Recovered\"><button id=\"ok\" text=\"OK\"/></ui>");
    check(static_cast<bool>(valid_program));
    const auto recovered =
        flexUI::UiDocumentInstantiator::instantiate(box, *valid_program.program, builtins);
    check(static_cast<bool>(recovered));
    check_not_null(dynamic_cast<flexUI::ButtonWidget *>(box.get_by_id("ok")->widget));
  }

  it("keeps the no-registry overload compatible with generic elements") {
    const auto compiled =
        flexUI::compile_ui_xml("<ui name=\"Legacy\"><button id=\"plain\" text=\"Plain\"/></ui>");
    check(static_cast<bool>(compiled));

    flexUI::Box box(nullptr);
    const auto result = flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program);
    check(static_cast<bool>(result));
    check_null(box.get_by_id("plain")->widget);
    check_equal(box.get_by_id("plain")->text(), "Plain");
  }
}
