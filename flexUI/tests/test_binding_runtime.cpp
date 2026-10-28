#include <flexUI/binding_runtime.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/textarea_widget.h>

#include <nlohmann/json.hpp>
#include <tinytest.hpp>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

nlohmann::json utility_fixture() {
  return nlohmann::json{{"tokens",
                         {{"flex",
                           {{"kind", "decl"},
                            {"decls", {{"display", "flex"}}}}},
                          {"hidden",
                           {{"kind", "decl"},
                            {"decls", {{"display", "none"}}}}}}}};
}

struct PreboundRehashResult {
  std::string initial_value;
  std::string updated_value;
  bool unrelated_inputs_skipped = false;
  std::uint64_t initial_evaluations = 0;
  std::uint64_t final_evaluations = 0;
};

#if defined(_MSC_VER)
__declspec(noinline)
#endif
PreboundRehashResult run_prebound_rehash_scenario() {
  auto box = std::make_unique<flexUI::Box>(nullptr);
  auto* root = box->create("div", "root");
  box->set_root(root);
  auto& runtime = box->bindings();

  runtime.inputs().set_number("progress", 2.0);
  runtime.targets().bind_custom_property(*root, "--value", "progress * 3", "");
  if (!runtime.update()) {
    throw std::runtime_error("initial binding update was skipped");
  }

  PreboundRehashResult result;
  const auto* initial_value = root->custom_property("--value");
  if (!initial_value) {
    throw std::runtime_error("initial binding value was not applied");
  }
  result.initial_value = *initial_value;
  result.initial_evaluations = runtime.stats().evaluation_count;

  for (int i = 0; i < 512; ++i) {
    runtime.inputs().set_number("unrelated-" + std::to_string(i), i);
  }
  result.unrelated_inputs_skipped = !runtime.update() &&
      runtime.stats().evaluation_count == result.initial_evaluations;

  runtime.inputs().set_number("progress", 5.0);
  if (!runtime.update()) {
    throw std::runtime_error("changed binding input was skipped");
  }
  const auto* updated_value = root->custom_property("--value");
  if (!updated_value) {
    throw std::runtime_error("updated binding value was not applied");
  }
  result.updated_value = *updated_value;
  result.final_evaluations = runtime.stats().evaluation_count;
  return result;
}

}  // namespace

spec("Element exposes HTML-style class and custom property APIs") {
  it("tokenizes and replaces complete class attributes") {
    flexUI::Element element;
    element.add_classes(
        " flex items-center  data-[state=open]:bg-accent rounded-md ");

    check(element.has_class(flex::Symbol("flex")));
    check(element.has_class(flex::Symbol("items-center")));
    check(element.has_class(flex::Symbol("data-[state=open]:bg-accent")));
    check_equal(element.class_names().size(), 4);

    element.set_attribute("class", "grid gap-4 grid-cols-2");
    check_false(element.has_class(flex::Symbol("flex")));
    check(element.has_class(flex::Symbol("grid")));
    check(element.has_attribute("class"));
    check_equal(*element.attribute("class"), "gap-4 grid grid-cols-2");

    element.toggle_class("hidden", true);
    check(element.has_class(flex::Symbol("hidden")));
    check(element.replace_class("hidden", "block"));
    check_false(element.has_class(flex::Symbol("hidden")));
    check(element.has_class(flex::Symbol("block")));

    element.remove_attribute("class");
    check(element.class_names().empty());
    check_false(element.has_attribute("class"));
  }

  it("keeps inline custom properties outside computed style state") {
    flexUI::Element element;
    element.set_custom_property("--progress", "42%");
    check_equal(*element.custom_property("--progress"), "42%");
    element.remove_custom_property("--progress");
    check_null(element.custom_property("--progress"));

    check_throws_as(element.set_custom_property("progress", "42%"),
                    std::invalid_argument);
  }
}

spec("UiBindingRuntime projects typed C++ inputs through MIR") {
  it("updates classes attributes text and CSS variables") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* label = box.create("label", "status");
    root->append(label);
    box.set_root(root);
    box.set_viewport(320.0f, 120.0f);
    box.load_css(R"(
      #status { width: var(--progress); }
      #status.active[data-state=open] { height: 40px; }
    )");

    auto& runtime = box.bindings();
    runtime.inputs().set_number("progress", 0.25);
    runtime.inputs().set_bool("enabled", true);
    runtime.inputs().set_string("title", "Ready");

    const auto class_binding =
        runtime.targets().bind_class(*label, "active", "enabled && progress > 0");
    runtime.targets().bind_class(*label, "has-progress", "progress > 0");
    check(class_binding);
    check(class_binding.uses_jit);
    runtime.targets().bind_attribute(*label, "data-state", "enabled", "open",
                                     "closed");
    runtime.targets().bind_text(*label, "title");
    runtime.targets().bind_custom_property(*label, "--progress",
                                           "progress * 200", "px");

    box.update();
    check(label->has_class(flex::Symbol("active")));
    check(label->has_class(flex::Symbol("has-progress")));
    check_equal(*label->attribute("data-state"), "open");
    check_equal(label->text(), "Ready");
    check_equal(*label->custom_property("--progress"), "50px");
    check_within(label->computed_style->width, 50.0f, 0.001f);
    check_within(label->computed_style->height, 40.0f, 0.001f);

    const auto first_stats = runtime.stats();
    check_equal(first_stats.binding_count, 5);
    box.update();
    const auto unchanged_stats = runtime.stats();
    check(unchanged_stats.update_count == first_stats.update_count + 1);
    check(unchanged_stats.evaluation_count == first_stats.evaluation_count);

    runtime.inputs().set_number("progress", 0.5);
    runtime.inputs().set_bool("enabled", false);
    runtime.inputs().set_string("title", "Paused");
    box.update();
    check_false(label->has_class(flex::Symbol("active")));
    check(label->has_class(flex::Symbol("has-progress")));
    check_equal(*label->attribute("data-state"), "closed");
    check_equal(label->text(), "Paused");
    check_equal(*label->custom_property("--progress"), "100px");
    check_within(label->computed_style->width, 100.0f, 0.001f);
    check(runtime.stats().evaluation_count > unchanged_stats.evaluation_count);
  }

  it("keeps prebound MIR inputs valid when the data map rehashes") {
    const auto result = run_prebound_rehash_scenario();
    check_equal(result.initial_value, "6");
    check(result.unrelated_inputs_skipped);
    check_equal(result.updated_value, "15");
    check(result.final_evaluations == result.initial_evaluations + 1);
  }

  it("restores targets when a binding is removed") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    root->add_class("base");
    root->set_text("original");
    box.set_root(root);

    auto& runtime = box.bindings();
    runtime.inputs().set_bool("visible", true);
    runtime.inputs().set_string("text", "bound");
    const auto class_binding =
        runtime.targets().bind_class(*root, "visible", "visible");
    const auto text_binding = runtime.targets().bind_text(*root, "text");
    box.update();
    check(root->has_class(flex::Symbol("visible")));
    check_equal(root->text(), "bound");

    check(runtime.targets().unbind(class_binding.id));
    check(runtime.targets().unbind(text_binding.id));
    check_false(root->has_class(flex::Symbol("visible")));
    check(root->has_class(flex::Symbol("base")));
    check_equal(root->text(), "original");
  }

  it("binds a complete Tailwind class string with exclusive ownership") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    root->set_classes("base rounded-md");
    box.set_root(root);
    box.enable_utility_jit(utility_fixture());

    auto& runtime = box.bindings();
    runtime.inputs().set_string("classes", "flex items-center gap-4");
    const auto classes_binding =
        runtime.targets().bind_classes(*root, "classes");
    box.update();
    check(root->has_class(flex::Symbol("flex")));
    check(root->has_class(flex::Symbol("gap-4")));
    check_false(root->has_class(flex::Symbol("base")));
    check(root->computed_style->display == flexUI::Display::Flex);

    runtime.inputs().set_string("classes", "hidden");
    box.update();
    check(root->computed_style->display == flexUI::Display::None);

    check_throws_as(runtime.targets().bind_class(*root, "active", "1"),
                    std::invalid_argument);

    check(runtime.targets().unbind(classes_binding.id));
    check(root->has_class(flex::Symbol("base")));
    check(root->has_class(flex::Symbol("rounded-md")));
    check_false(root->has_class(flex::Symbol("flex")));
  }

  it("binds explicit utilities without creating a second target owner") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);

    auto& runtime = box.bindings();
    runtime.inputs().set_bool("enabled", true);
    const auto utility_binding =
        runtime.targets().bind_utility(*root, "flex", "enabled");
    box.update();
    check(root->utility_names().count("flex") == 1);
    check(root->computed_style->display == flexUI::Display::Flex);

    runtime.inputs().set_bool("enabled", false);
    box.update();
    check(root->utility_names().count("flex") == 0);
    check(root->computed_style->display == flexUI::Display::Block);
    check_throws_as(runtime.targets().bind_classes(*root, "missing-input"),
                    std::invalid_argument);
    check_throws_as(runtime.targets().bind_utility(
                        *root, "not-a-utility", "enabled"),
                    std::invalid_argument);
    check_true(runtime.targets().unbind(utility_binding.id));
  }

  it("inherits bound custom properties through the CSS cascade") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* child = box.create("div", "child");
    root->append(child);
    root->set_custom_property("--bound-width", "12px");
    box.set_root(root);
    box.set_viewport(320.0f, 120.0f);
    box.load_css("#child { width: var(--bound-width, 7px); }");

    auto& runtime = box.bindings();
    runtime.inputs().set_number("width", 25.0);
    const auto binding = runtime.targets().bind_custom_property(
        *root, "--bound-width", "width", "px");
    box.update();
    check_within(child->computed_style->width, 25.0f, 0.001f);

    runtime.inputs().set_number("width", 50.0);
    box.update();
    check_within(child->computed_style->width, 50.0f, 0.001f);

    check(runtime.targets().unbind(binding.id));
    box.update();
    check_within(child->computed_style->width, 12.0f, 0.001f);

    root->remove_custom_property("--bound-width");
    box.update();
    check_within(child->computed_style->width, 7.0f, 0.001f);
  }

  it("binds editable widget values in both directions") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* input_element =
        box.create_widget<flexUI::InputWidget>("input", "name");
    auto* textarea_element =
        box.create_widget<flexUI::TextAreaWidget>("textarea", "notes");
    root->append(input_element);
    root->append(textarea_element);
    box.set_root(root);
    box.set_viewport(320.0f, 160.0f);
    box.load_css(R"(
      #root { display: flex; flex-direction: column; }
      #name, #notes { width: 300px; height: 48px; }
    )");

    auto* input = static_cast<flexUI::InputWidget*>(input_element->widget);
    auto* textarea =
        static_cast<flexUI::TextAreaWidget*>(textarea_element->widget);
    input->set_text("input-original");
    textarea->set_text("textarea-original");

    int input_callback_count = 0;
    input->set_change_callback(
        [&](const std::string&) { ++input_callback_count; });

    auto& runtime = box.bindings();
    runtime.inputs().set_string("name", "Alice");
    runtime.inputs().set_string("notes", "First line");
    const auto name_binding =
        runtime.targets().bind_value(*input_element, "name");
    const auto notes_binding =
        runtime.targets().bind_value(*textarea_element, "notes");
    box.update();

    check_equal(input->text(), "Alice");
    check_equal(textarea->text(), "First line");
    check_equal(input_callback_count, 0);

    check(input->handle_event(flexUI::Event::text_input("!"),
                              *input_element));
    check(textarea->handle_event(flexUI::Event::text_input("!"),
                                 *textarea_element));
    check_equal(runtime.inputs().string("name"), input->text());
    check_equal(runtime.inputs().string("notes"), textarea->text());
    check_equal(input_callback_count, 1);

    runtime.inputs().set_string("name", "External");
    box.update();
    check_equal(input->text(), "External");
    check_equal(input_callback_count, 1);

    check(runtime.targets().unbind(name_binding.id));
    check(runtime.targets().unbind(notes_binding.id));
    check_equal(input->text(), "input-original");
    check_equal(textarea->text(), "textarea-original");

    check(input->handle_event(flexUI::Event::text_input("?"),
                              *input_element));
    check_equal(runtime.inputs().string("name"), "External");
  }

  it("keeps copied Widget observer callbacks inert after unbind") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* input_element =
        box.create_widget<flexUI::InputWidget>("input", "sale");
    auto* textarea_element =
        box.create_widget<flexUI::TextAreaWidget>("textarea", "sales");
    root->append(input_element);
    root->append(textarea_element);
    box.set_root(root);
    auto* input = static_cast<flexUI::InputWidget*>(input_element->widget);
    auto* textarea =
        static_cast<flexUI::TextAreaWidget*>(textarea_element->widget);

    auto& bindings = box.bindings();
    bindings.inputs().set_string("sale_value", "original");
    bindings.inputs().set_string("sales_value", "sibling");
    // Register first: the Widget's notification snapshot will contain both
    // this witness and the Binding callback, even after unbind removes it.
    flexUI::UiBindingId sale_id = 0;
    bool unbound_during_edit = false;
    bool notification_frame_visible = false;
    const auto witness = input->add_edit_observer([&](const std::string&) {
      const auto active = box.inspect_subtree_borrowers(input_element);
      notification_frame_visible = static_cast<bool>(active) &&
                                   active.widget_observer_notifications_active;
      unbound_during_edit = bindings.targets().unbind(sale_id);
    });
    const auto sale = bindings.targets().bind_value(*input_element, "sale_value");
    const auto sales =
        bindings.targets().bind_value(*textarea_element, "sales_value");
    sale_id = sale.id;
    check(sale && sales);
    const auto before = box.inspect_subtree_borrowers(input_element);
    check(static_cast<bool>(before));
    check_equal(before.binding_targets, std::size_t{1});
    check_equal(before.value_observers, std::size_t{1});
    check_equal(before.blocked_value_observers, std::size_t{0});
    check_equal(before.unverified_widget_hosts, std::size_t{0});
    check_equal(before.native_widget_callback_owners, std::size_t{0});
    check_equal(before.active_widget_local_borrowers, std::size_t{0});
    check_equal(before.widget_edit_observers, std::size_t{2});
    check_equal(before.unowned_widget_edit_observers, std::size_t{1});
    check_false(before.widget_observer_notifications_active);
    check_false(before.uninspectable_edit_widget);
    check_false(before.binding_mutation_active);
    check_false(before.invalid_binding_ownership);

    check(input->handle_event(flexUI::Event::text_input("!"), *input_element));
    check(unbound_during_edit);
    check(notification_frame_visible);
    // The copied Binding callback was retired before its turn. It must not
    // write to the shared application input or use a destroyed Binding owner.
    check_equal(bindings.inputs().string("sale_value"), "original");
    check_equal(bindings.stats().binding_count, std::size_t{1});
    check(input->remove_edit_observer(witness));
    const auto after = box.inspect_subtree_borrowers(input_element);
    check(static_cast<bool>(after));
    check_equal(after.value_observers, std::size_t{0});
    check_equal(after.widget_edit_observers, std::size_t{0});
    check_equal(after.unowned_widget_edit_observers, std::size_t{0});
    const auto sibling = box.inspect_subtree_borrowers(textarea_element);
    check(static_cast<bool>(sibling));
    check_equal(sibling.value_observers, std::size_t{1});
    check_equal(sibling.blocked_value_observers, std::size_t{0});
    check_equal(sibling.widget_edit_observers, std::size_t{1});
    check_equal(sibling.unowned_widget_edit_observers, std::size_t{0});
    check_equal(sibling.unverified_widget_hosts, std::size_t{0});
    check_equal(sibling.native_widget_callback_owners, std::size_t{0});

    // M4b4's observer proof grants NO raw Box retirement of a Widget.
    check_false(box.retire_bound_structural_subtree("sales"));
    check_equal(bindings.stats().binding_count, std::size_t{1});
    bool textarea_frame_visible = false;
    const auto textarea_witness = textarea->add_edit_observer(
        [&](const std::string&) {
          const auto active = box.inspect_subtree_borrowers(textarea_element);
          textarea_frame_visible = static_cast<bool>(active) &&
              active.widget_observer_notifications_active &&
              active.unowned_widget_edit_observers == std::size_t{1};
        });
    check(textarea->handle_event(flexUI::Event::text_input("?"),
                                 *textarea_element));
    check(textarea_frame_visible);
    check(textarea->remove_edit_observer(textarea_witness));
    check_equal(bindings.inputs().string("sales_value"), textarea->text());
    input->set_change_callback([](const std::string&) {});
    const auto callback_held = box.inspect_subtree_borrowers(input_element);
    check(static_cast<bool>(callback_held));
    check_equal(callback_held.native_widget_callback_owners, std::size_t{1});
    input->set_change_callback({});
    const auto callback_released = box.inspect_subtree_borrowers(input_element);
    check_equal(callback_released.native_widget_callback_owners, std::size_t{0});
    check(bindings.targets().unbind(sales.id));
  }

  it("rolls back a rejected duplicate value binding without a stale callback") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* field = box.create_widget<flexUI::InputWidget>("input", "sale");
    root->append(field);
    box.set_root(root);
    auto* input = static_cast<flexUI::InputWidget*>(field->widget);
    auto& runtime = box.bindings();
    runtime.inputs().set_string("original", "First");
    runtime.inputs().set_string("rejected", "Unchanged");
    const auto first = runtime.targets().bind_value(*field, "original");
    check(first);
    check_throws_as(runtime.targets().bind_value(*field, "rejected"),
                    std::invalid_argument);
    const auto owned = runtime.inspect_target_observers(field);
    check_equal(owned.binding_count, std::size_t{1});
    check_equal(owned.value_observers, std::size_t{1});
    check_equal(owned.blocked_value_observers, std::size_t{0});
    check(runtime.targets().unbind(first.id));
    check(input->handle_event(flexUI::Event::text_input("!"), *field));
    // An accidentally retained rejected observer would mutate this input.
    check_equal(runtime.inputs().string("rejected"), "Unchanged");
  }

  it("refuses Binding-owned unbind while its callback is executing") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    auto* input_element =
        box.create_widget<flexUI::InputWidget>("input", "sale");
    root->append(input_element);
    box.set_root(root);
    auto* input = static_cast<flexUI::InputWidget*>(input_element->widget);

    std::unique_ptr<flexUI::UiBindingRuntime> runtime;
    flexUI::UiBindingId bound_id = 0;
    bool armed = false;
    bool saw_busy = false;
    bool refused_reentrant_unbind = false;
    runtime = std::make_unique<flexUI::UiBindingRuntime>([&] {
      if (!armed) return;
      const auto checked = runtime->inspect_target_observers(input_element);
      saw_busy = checked.value_observers == std::size_t{1} &&
                 checked.blocked_value_observers == std::size_t{1} &&
                 !checked.owned_observers_retirable();
      refused_reentrant_unbind = !runtime->targets().unbind(bound_id);
    });
    runtime->inputs().set_string("sale_value", "first");
    const auto binding =
        runtime->targets().bind_value(*input_element, "sale_value");
    bound_id = binding.id;
    armed = true;
    check(input->handle_event(flexUI::Event::text_input("!"), *input_element));
    check(saw_busy);
    check(refused_reentrant_unbind);
    check_equal(runtime->inputs().string("sale_value"), input->text());
    const auto idle = runtime->inspect_target_observers(input_element);
    check_equal(idle.blocked_value_observers, std::size_t{0});
    check(idle.owned_observers_retirable());
    check(runtime->targets().unbind(bound_id));
  }

  it("rejects invalid input lifetimes values and binding definitions") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    auto& runtime = box.bindings();
    runtime.inputs().set_bool("enabled", true);

    check_throws_with(runtime.targets().bind_text(*root, "missing"),
                      "UI binding references a missing input: missing");
    check_throws_as(runtime.targets().bind_class(*root, "active", "enabled && ("),
                    std::invalid_argument);
    check_throws_as(
        runtime.targets().bind_custom_property(*root, "width", "enabled", "px"),
        std::invalid_argument);
    check_throws_as(runtime.inputs().set_number(
                        "invalid", std::numeric_limits<double>::infinity()),
                    std::invalid_argument);
    runtime.inputs().set_string("value", "text");
    check_throws_as(runtime.targets().bind_value(*root, "value"),
                    std::invalid_argument);
    check_throws_as(runtime.inputs().number("value"), std::invalid_argument);
    check_throws_as(runtime.inputs().string("missing-value"),
                    std::invalid_argument);

    const auto binding =
        runtime.targets().bind_class(*root, "active", "enabled");
    check_throws_as(runtime.targets().bind_class(*root, "active", "enabled"),
                    std::invalid_argument);
    check_throws_as(runtime.inputs().erase("enabled"), std::invalid_argument);
    check(runtime.targets().unbind(binding.id));
    check(runtime.inputs().erase("enabled"));
    check_false(runtime.inputs().contains("enabled"));
  }
}

spec("UiBindingRuntime projects numeric values through StylePropertyRegistry") {
  it("applies and restores a descriptor-owned style override") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    box.load_css("#root { opacity: 0.8; }");
    box.update();

    auto& runtime = box.bindings();
    runtime.inputs().set_number("progress", 0.4);
    const auto binding =
        runtime.targets().bind_style_number(*root, "opacity", "progress * 0.5");
    box.update();
    check(std::fabs(root->computed_style->opacity - 0.2f) < 0.001f);

    runtime.inputs().set_number("progress", 0.8);
    box.update();
    check(std::fabs(root->computed_style->opacity - 0.4f) < 0.001f);

    check(runtime.targets().unbind(binding.id));
    box.update();
    check(std::fabs(root->computed_style->opacity - 0.8f) < 0.001f);
  }

  it("rejects unknown and nonnumeric registered properties") {
    flexUI::Box box(nullptr);
    auto* root = box.create("div", "root");
    box.set_root(root);
    auto& runtime = box.bindings();
    runtime.inputs().set_number("value", 1.0);

    check_throws_as(
        runtime.targets().bind_style_number(*root, "missing", "value"),
        std::invalid_argument);
    check_throws_as(
        runtime.targets().bind_style_number(*root, "visibility", "value"),
        std::invalid_argument);
  }
}
