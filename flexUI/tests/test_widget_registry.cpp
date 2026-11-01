#include <flexUI.h>
#include <flexUI/native_canvas_widget.h>

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


spec("FlexUI registry preflight rejects invalid content before construction") {
  it("classifies built-in leaves text widgets composites and structural containers") {
    const auto registry = flexUI::WidgetRegistry::builtins();
    for (const auto *tag : {"input", "image", "slider", "progress", "select", "spinner", "divider"}) {
      const auto *schema = registry.descriptor(tag);
      check_not_null(schema);
      if (schema) {
        check(schema->kind == flexUI::UiNodeKind::Widget);
        check(schema->content == flexUI::UiContentModel::Empty);
      }
    }
    for (const auto *tag : {"label", "textarea", "badge", "checkbox", "radio", "switch", "markdown"}) {
      const auto *schema = registry.descriptor(tag);
      check_not_null(schema);
      if (schema) check(schema->content == flexUI::UiContentModel::Text);
    }
    check(registry.descriptor("button")->content == flexUI::UiContentModel::TextAndChildren);
    check(registry.descriptor("div")->kind == flexUI::UiNodeKind::Container);
    check(registry.descriptor("div")->content == flexUI::UiContentModel::TextAndChildren);
    check_null(registry.descriptor("unregistered"));
  }

  it("rejects an unknown later sibling before calling an earlier factory") {
    int calls = 0;
    flexUI::WidgetRegistry registry;
    check_false(static_cast<bool>(registry.register_element("root")));
    check_false(static_cast<bool>(registry.register_widget("probe", [&](const flexUI::UiNodeDefinition &) {
      ++calls;
      return std::make_unique<flexUI::ButtonWidget>();
    }, flexUI::UiContentModel::Empty)));
    auto parsed = flexUI::compile_ui_xml("<ui name=\"Order\"><root id=\"root\"><probe id=\"first\"/><missing id=\"last\"/></root></ui>");
    check(static_cast<bool>(parsed));
    if (!parsed) return;
    flexUI::Box box(nullptr);
    for (const bool compiled : {false, true}) {
      const auto result = compiled
          ? flexUI::UiDocumentInstantiator::instantiate(box, *parsed.program, registry)
          : flexUI::UiDocumentInstantiator::instantiate(box, parsed.program->definition(), registry);
      check_false(static_cast<bool>(result));
      check(result.error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);
      check_equal(calls, 0);
      check_null(box.root());
      check_null(box.get_by_id("first"));
      check_null(box.get_by_id("last"));
    }
  }

  it("rejects leaf children and text before allocating a typed tree") {
    const auto registry = flexUI::WidgetRegistry::builtins();
    for (const auto *xml : {
        "<ui name=\"Leaf\"><input id=\"root\"><label id=\"child\"/></input></ui>",
        "<ui name=\"Leaf\"><image id=\"root\" text=\"\"/></ui>",
        "<ui name=\"Leaf\"><label id=\"root\"><button id=\"child\"/></label></ui>"}) {
      const auto parsed = flexUI::compile_ui_xml(xml);
      check(static_cast<bool>(parsed));
      if (!parsed) continue;
      flexUI::Box box(nullptr);
      const auto result = flexUI::UiDocumentInstantiator::instantiate(box, *parsed.program, registry);
      check_false(static_cast<bool>(result));
      check_null(box.root());
      check_null(box.get_by_id("root"));
    }
  }

  it("reports exact node positions and bounded traversal errors") {
    const auto registry = flexUI::WidgetRegistry::builtins();
    const auto parsed = flexUI::compile_ui_xml("<ui name=\"Position\">\n  <div id=\"root\">\n    <unknown id=\"bad\"/>\n  </div>\n</ui>");
    check(static_cast<bool>(parsed));
    if (!parsed) return;
    auto error = registry.validate(*parsed.program);
    check(error.code == flexUI::UiDocumentErrorCode::UnknownElementTag);
    check_equal(error.line, 3);
    check_equal(error.column, 6);
    flexUI::UiDocumentLimits limits;
    limits.max_nodes = 1;
    error = registry.validate(*parsed.program, limits);
    check(error.code == flexUI::UiDocumentErrorCode::NodeLimitExceeded);
    limits.max_nodes = 2;
    limits.max_depth = 1;
    error = registry.validate(*parsed.program, limits);
    check(error.code == flexUI::UiDocumentErrorCode::DepthLimitExceeded);
  }

  it("checks direct factory content and preserves duplicate descriptors") {
    int calls = 0;
    flexUI::WidgetRegistry registry;
    const auto factory = [&](const flexUI::UiNodeDefinition &) {
      ++calls;
      return std::make_unique<flexUI::ButtonWidget>();
    };
    check_false(static_cast<bool>(registry.register_widget("leaf", factory, flexUI::UiContentModel::Empty)));
    check(registry.register_widget("leaf", factory, flexUI::UiContentModel::Text).code == flexUI::WidgetRegistryErrorCode::DuplicateTag);
    check(registry.descriptor("leaf")->content == flexUI::UiContentModel::Empty);
    flexUI::UiNodeDefinition invalid;
    invalid.tag = "leaf";
    invalid.id = "subject";
    invalid.properties["text"] = std::string("invalid");
    const auto result = registry.create(invalid);
    check(result.error.code == flexUI::WidgetRegistryErrorCode::InvalidContent);
    check_equal(calls, 0);
    check(registry.register_element("bad", static_cast<flexUI::UiContentModel>(99)).code == flexUI::WidgetRegistryErrorCode::InvalidDescriptor);
    check_false(registry.contains("bad"));
  }
}


spec("FlexUI custom registration defaults remain compatible") {
  it("allows text and children unless a custom tag explicitly restricts them") {
    flexUI::WidgetRegistry registry;
    check_false(static_cast<bool>(registry.register_element("custom")));
    flexUI::UiDocumentDefinition definition;
    definition.root.tag = "custom";
    definition.root.properties["text"] = std::string("Caption");
    definition.root.children.emplace_back();
    definition.root.children.back().tag = "custom";
    check_false(static_cast<bool>(registry.validate(definition)));
    check_false(static_cast<bool>(registry.register_element("children", flexUI::UiContentModel::Children)));
    definition.root.tag = "children";
    check(registry.validate(definition).code == flexUI::UiDocumentErrorCode::InvalidProperty);
  }
}


spec("FlexUI anonymous nodes stay outside the named index") {
  it("constructs multiple anonymous widgets and preserves named handles in both typed overloads") {
    const auto compiled = flexUI::compile_ui_xml(
      "<ui name=\"Anonymous\"><main><button text=\"First\"/><button text=\"Second\"/><label id=\"title\" text=\"Named\"/></main></ui>");
    check(static_cast<bool>(compiled));
    if (!compiled) return;
    const auto registry = flexUI::WidgetRegistry::builtins();
    for (const bool use_program : {false, true}) {
      flexUI::Box box(nullptr);
      const auto result = use_program
        ? flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program, registry)
        : flexUI::UiDocumentInstantiator::instantiate(box, compiled.program->definition(), registry);
      check(static_cast<bool>(result));
      if (!result) continue;
      check_equal(box.root()->child_count(), 3);
      for (std::size_t index = 0; index < 2; ++index) {
        auto *child = box.root()->child_at(index);
        check_true(child->id().empty());
        check_false(static_cast<bool>(box.handle_for(*child)));
        auto *button = dynamic_cast<flexUI::ButtonWidget *>(child->widget);
        check_not_null(button);
        if (button) check_equal(button->text(), index == 0 ? "First" : "Second");
      }
      check_equal(result.tree.elements_by_id.size(), 1);
      check_null(box.get_by_id(""));
      check_true(box.root()->id().empty());
      check_false(static_cast<bool>(box.handle_for(*box.root())));
      auto *title = box.get_by_id("title");
      check_not_null(title);
      check_not_null(dynamic_cast<flexUI::LabelWidget *>(title->widget));
      const auto handle = box.handle_for(*title);
      check(static_cast<bool>(handle));
      check_equal(box.resolve_handle(handle), title);
      check_equal(title->text(), "Named");
    }
  }
}


spec("FlexUI linked native canvas widget uses the one XML and Box event path") {
  it("registers a native canvas and captures drag events through EventDispatcher") {
    struct Counters {
      int paints = 0;
      int moves = 0;
      int releases = 0;
    };
    auto counters = std::make_shared<Counters>();
    auto registry = flexUI::WidgetRegistry::builtins();
    const auto registered = registry.register_widget(
        "native-canvas",
        [counters](const flexUI::UiNodeDefinition &) -> std::unique_ptr<flexUI::Widget> {
          flexUI::NativeCanvasWidget::Callbacks callbacks;
          callbacks.paint = [counters](const flexUI::Element &elem,
                                       flexUI::RenderCommandList &commands) {
            ++counters->paints;
            commands.draw_rect(0.0F, 0.0F, elem.width(), elem.height(), 0.0F,
                               flexUI::Paint::solid(flexUI::Color{0.3F, 0.5F, 0.7F, 1.0F}),
                               flexUI::Paint::none(), 0.0F);
          };
          callbacks.input = [counters](flexUI::NativeCanvasWidget &widget,
                                       const flexUI::Event &event,
                                       flexUI::Element &elem) {
            if (event.type == flexUI::EventType::MouseDown) {
              widget.request_pointer_capture();
              return true;
            }
            if (event.type == flexUI::EventType::MouseMove) {
              ++counters->moves;
              elem.mark_paint_dirty();
              return true;
            }
            if (event.type == flexUI::EventType::MouseUp) {
              ++counters->releases;
              widget.release_pointer_capture();
              return true;
            }
            return false;
          };
          callbacks.measure = [](const flexUI::Element &, float, float,
                                 float &w, float &h) {
            w = 120.0F;
            h = 80.0F;
            return true;
          };
          return std::make_unique<flexUI::NativeCanvasWidget>(std::move(callbacks));
        },
        flexUI::UiContentModel::Empty);
    check_false(static_cast<bool>(registered));
    check(registry.descriptor("native-canvas")->kind == flexUI::UiNodeKind::Widget);

    const auto compiled = flexUI::compile_ui_xml(
        "<ui name=\"NativeCanvas\"><div id=\"root\"><native-canvas id=\"canvas\"/></div></ui>");
    check(static_cast<bool>(compiled));
    if (!compiled) return;
    flexUI::Box box(nullptr);
    const auto mounted =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program, registry);
    check(static_cast<bool>(mounted));
    if (!mounted) return;
    auto *root = box.get_by_id("root");
    auto *canvas = box.get_by_id("canvas");
    check_not_null(root);
    check_not_null(canvas);
    if (!root || !canvas) return;
    root->set_layout_bounds(0.0F, 0.0F, 300.0F, 150.0F);
    canvas->set_layout_bounds(0.0F, 0.0F, 100.0F, 100.0F);

    auto *native = dynamic_cast<flexUI::NativeCanvasWidget *>(canvas->widget);
    check_not_null(native);
    if (!native) return;
    check_false(native->needs_frame_update(*canvas));
    float intrinsic_w = 0.0F, intrinsic_h = 0.0F;
    check_true(native->measure_intrinsic_size(*canvas, 300.0F, 150.0F,
                                               intrinsic_w, intrinsic_h));
    check_equal(intrinsic_w, 120.0F);
    check_equal(intrinsic_h, 80.0F);

    auto down = flexUI::Event::mouse_down(20.0F, 20.0F);
    box.dispatch_event(down);
    check_true(down.handled);
    check(box.capturing_element() == canvas);

    auto move = flexUI::Event::mouse_move(180.0F, 20.0F);
    box.dispatch_event(move);
    check_true(move.handled);
    check(move.target == canvas);
    check_equal(counters->moves, 1);

    auto up = flexUI::Event::mouse_up(180.0F, 20.0F);
    box.dispatch_event(up);
    check_true(up.handled);
    check_null(box.capturing_element());
    check_equal(counters->releases, 1);

    flexUI::RenderCommandList commands(flex::RendererCapabilities{});
    native->emit_render_commands(*canvas, commands);
    check_equal(counters->paints, 1);

    // FocusOut must clear both the widget request AND the host's capture
    // pointer before any new event. Otherwise the old widget still receives
    // the next mouse move after a document/tool switch.
    auto second_down = flexUI::Event::mouse_down(20.0F, 20.0F);
    box.dispatch_event(second_down);
    check(box.capturing_element() == canvas);
    box.set_focus(nullptr);
    check_false(native->wants_mouse_capture());
    check_null(box.capturing_element());
  }

  it("rejects a missing native paint callback before publishing the tree") {
    auto registry = flexUI::WidgetRegistry::builtins();
    const auto registered = registry.register_widget(
        "invalid-native", [](const flexUI::UiNodeDefinition &) {
          return std::make_unique<flexUI::NativeCanvasWidget>(
              flexUI::NativeCanvasWidget::Callbacks{});
        },
        flexUI::UiContentModel::Empty);
    check_false(static_cast<bool>(registered));
    const auto compiled = flexUI::compile_ui_xml(
        "<ui name=\"BadCanvas\"><invalid-native id=\"native\"/></ui>");
    check(static_cast<bool>(compiled));
    if (!compiled) return;
    flexUI::Box box(nullptr);
    const auto result =
        flexUI::UiDocumentInstantiator::instantiate(box, *compiled.program, registry);
    check_false(static_cast<bool>(result));
    check(result.error.code == flexUI::UiDocumentErrorCode::WidgetFactoryFailed);
    check_null(box.root());
  }
}
