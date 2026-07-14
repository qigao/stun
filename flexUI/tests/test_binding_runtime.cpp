#include <flexUI/binding_runtime.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/textarea_widget.h>

#include <nlohmann/json.hpp>
#include <tinytest.h>

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
    check_size_eq(element.class_names().size(), 4);

    element.set_attribute("class", "grid gap-4 grid-cols-2");
    check_false(element.has_class(flex::Symbol("flex")));
    check(element.has_class(flex::Symbol("grid")));
    check(element.has_attribute("class"));
    check_string_eq(*element.attribute("class"), "gap-4 grid grid-cols-2");

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
    check_string_eq(*element.custom_property("--progress"), "42%");
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
    check_string_eq(*label->attribute("data-state"), "open");
    check_string_eq(label->text(), "Ready");
    check_string_eq(*label->custom_property("--progress"), "50px");
    check_float_eq(label->computed_style->width, 50.0f, 0.001f);
    check_float_eq(label->computed_style->height, 40.0f, 0.001f);

    const auto first_stats = runtime.stats();
    check_size_eq(first_stats.binding_count, 5);
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
    check_string_eq(*label->attribute("data-state"), "closed");
    check_string_eq(label->text(), "Paused");
    check_string_eq(*label->custom_property("--progress"), "100px");
    check_float_eq(label->computed_style->width, 100.0f, 0.001f);
    check(runtime.stats().evaluation_count > unchanged_stats.evaluation_count);
  }

  it("keeps prebound MIR inputs valid when the data map rehashes") {
    const auto result = run_prebound_rehash_scenario();
    check_string_eq(result.initial_value, "6");
    check(result.unrelated_inputs_skipped);
    check_string_eq(result.updated_value, "15");
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
    check_string_eq(root->text(), "bound");

    check(runtime.targets().unbind(class_binding.id));
    check(runtime.targets().unbind(text_binding.id));
    check_false(root->has_class(flex::Symbol("visible")));
    check(root->has_class(flex::Symbol("base")));
    check_string_eq(root->text(), "original");
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
    check_float_eq(child->computed_style->width, 25.0f, 0.001f);

    runtime.inputs().set_number("width", 50.0);
    box.update();
    check_float_eq(child->computed_style->width, 50.0f, 0.001f);

    check(runtime.targets().unbind(binding.id));
    box.update();
    check_float_eq(child->computed_style->width, 12.0f, 0.001f);

    root->remove_custom_property("--bound-width");
    box.update();
    check_float_eq(child->computed_style->width, 7.0f, 0.001f);
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

    check_string_eq(input->text(), "Alice");
    check_string_eq(textarea->text(), "First line");
    check_size_eq(input_callback_count, 0);

    check(input->handle_event(flexUI::Event::text_input("!"),
                              *input_element));
    check(textarea->handle_event(flexUI::Event::text_input("!"),
                                 *textarea_element));
    check_string_eq(runtime.inputs().string("name"), input->text());
    check_string_eq(runtime.inputs().string("notes"), textarea->text());
    check_size_eq(input_callback_count, 1);

    runtime.inputs().set_string("name", "External");
    box.update();
    check_string_eq(input->text(), "External");
    check_size_eq(input_callback_count, 1);

    check(runtime.targets().unbind(name_binding.id));
    check(runtime.targets().unbind(notes_binding.id));
    check_string_eq(input->text(), "input-original");
    check_string_eq(textarea->text(), "textarea-original");

    check(input->handle_event(flexUI::Event::text_input("?"),
                              *input_element));
    check_string_eq(runtime.inputs().string("name"), "External");
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
