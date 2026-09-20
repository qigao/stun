#include <flexUI/widget_registry.h>

#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/widgets/avatar_widget.h>
#include <flexUI/widgets/badge_widget.h>
#include <flexUI/widgets/breadcrumb_widget.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/calendar_widget.h>
#include <flexUI/widgets/card_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/colorpicker_widget.h>
#include <flexUI/widgets/divider_widget.h>
#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/widgets/gradient_editor_widget.h>
#include <flexUI/widgets/group_button_widget.h>
#include <flexUI/widgets/image_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/widgets/modal_widget.h>
#include <flexUI/widgets/pagination_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/radio_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/spinner_widget.h>
#include <flexUI/widgets/stepper_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/table_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/tooltip_widget.h>
#include <flexUI/widgets/tree_widget.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace flexUI {

namespace {

WidgetRegistryError make_error(WidgetRegistryErrorCode code, std::string message) {
  return {code, std::move(message)};
}

const UiDocumentValue *find_property(const UiNodeDefinition &definition, std::string_view name) {
  const auto found = definition.properties.find(std::string(name));
  return found == definition.properties.end() ? nullptr : &found->second;
}

std::string string_property(const UiNodeDefinition &definition, std::string_view name,
                            std::string fallback = {}) {
  const auto *value = find_property(definition, name);
  if (!value) {
    return fallback;
  }
  const auto *text = std::get_if<std::string>(value);
  if (!text) {
    throw std::invalid_argument("widget '" + definition.id + "' property '" + std::string(name) +
                                "' requires a string value");
  }
  return *text;
}

std::string text_property(const UiNodeDefinition &definition, std::string fallback = {}) {
  if (find_property(definition, "text")) {
    return string_property(definition, "text");
  }
  return string_property(definition, "content", std::move(fallback));
}

bool bool_property(const UiNodeDefinition &definition, std::string_view name,
                   bool fallback = false) {
  const auto *value = find_property(definition, name);
  if (!value) {
    return fallback;
  }
  const auto *boolean = std::get_if<bool>(value);
  if (!boolean) {
    throw std::invalid_argument("widget '" + definition.id + "' property '" + std::string(name) +
                                "' requires a bool value");
  }
  return *boolean;
}

float float_property(const UiNodeDefinition &definition, std::string_view name, float fallback) {
  const auto *value = find_property(definition, name);
  if (!value) {
    return fallback;
  }
  const auto *number = std::get_if<float>(value);
  if (!number || !std::isfinite(*number)) {
    throw std::invalid_argument("widget '" + definition.id + "' property '" + std::string(name) +
                                "' requires a finite numeric value");
  }
  return *number;
}

int integer_property(const UiNodeDefinition &definition, std::string_view name, int fallback) {
  const float number = float_property(definition, name, static_cast<float>(fallback));
  const double converted = static_cast<double>(number);
  if (std::floor(converted) != converted ||
      converted < static_cast<double>(std::numeric_limits<int>::min()) ||
      converted > static_cast<double>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument("widget '" + definition.id + "' property '" + std::string(name) +
                                "' requires an integer value");
  }
  return static_cast<int>(converted);
}

std::vector<std::string> list_property(const UiNodeDefinition &definition, std::string_view name) {
  const auto source = string_property(definition, name);
  std::vector<std::string> values;
  std::size_t begin = 0;
  while (begin <= source.size()) {
    const auto end = source.find(',', begin);
    const auto token_end = end == std::string::npos ? source.size() : end;
    auto first = source.find_first_not_of(" \t\r\n", begin);
    if (first != std::string::npos && first < token_end) {
      const auto last = source.find_last_not_of(" \t\r\n", token_end - 1);
      values.push_back(source.substr(first, last - first + 1));
    }
    if (end == std::string::npos) {
      break;
    }
    begin = end + 1;
  }
  return values;
}

void require_registration(WidgetRegistryError error) {
  if (error) {
    throw std::logic_error("invalid built-in WidgetRegistry: " + error.message);
  }
}

template <typename WidgetT>
void register_default_widget(WidgetRegistry &registry, const char *tag) {
  require_registration(registry.register_widget(
      tag, [](const UiNodeDefinition &) { return std::make_unique<WidgetT>(); }));
}

} // namespace

WidgetRegistryError WidgetRegistry::register_element(std::string tag,
                                                     UiContentModel content) {
  if (tag.empty()) {
    return make_error(WidgetRegistryErrorCode::InvalidTag, "WidgetRegistry tag must not be empty");
  }
  if (!entries_
           .emplace(std::move(tag),
                    Entry{UiNodeDescriptor{UiNodeKind::Container, content}, {}})
           .second) {
    return make_error(WidgetRegistryErrorCode::DuplicateTag,
                      "WidgetRegistry tag is already registered");
  }
  return {};
}

WidgetRegistryError WidgetRegistry::register_widget(std::string tag, Factory factory,
                                                    UiContentModel content) {
  if (tag.empty()) {
    return make_error(WidgetRegistryErrorCode::InvalidTag, "WidgetRegistry tag must not be empty");
  }
  if (!factory) {
    return make_error(WidgetRegistryErrorCode::InvalidFactory,
                      "WidgetRegistry factory must not be empty");
  }
  if (!entries_
           .emplace(std::move(tag),
                    Entry{UiNodeDescriptor{UiNodeKind::Widget, content},
                          std::move(factory)})
           .second) {
    return make_error(WidgetRegistryErrorCode::DuplicateTag,
                      "WidgetRegistry tag is already registered");
  }
  return {};
}

bool WidgetRegistry::contains(std::string_view tag) const {
  return entries_.find(std::string(tag)) != entries_.end();
}

const UiNodeDescriptor *WidgetRegistry::descriptor(std::string_view tag) const {
  const auto found = entries_.find(std::string(tag));
  return found == entries_.end() ? nullptr : &found->second.descriptor;
}

UiDocumentError WidgetRegistry::validate(
    const UiDocumentDefinition &definition) const {
  const auto validate_node =
      [this](const auto &self, const UiNodeDefinition &node) -> UiDocumentError {
    const auto found = entries_.find(node.tag);
    if (found == entries_.end()) {
      return {UiDocumentErrorCode::UnknownElementTag,
              "unknown registered UI tag: " + node.tag,
              node.source.line, node.source.column};
    }

    const auto content = found->second.descriptor.content;
    if ((content == UiContentModel::Empty || content == UiContentModel::Text) &&
        !node.children.empty()) {
      return {UiDocumentErrorCode::InvalidNode,
              "UI tag '" + node.tag + "' does not accept child elements",
              node.source.line, node.source.column};
    }
    if (content == UiContentModel::SingleChild && node.children.size() != 1) {
      return {UiDocumentErrorCode::InvalidNode,
              "UI tag '" + node.tag + "' requires exactly one child element",
              node.source.line, node.source.column};
    }

    for (const auto &child : node.children) {
      auto error = self(self, child);
      if (error) {
        return error;
      }
    }
    return {};
  };

  return validate_node(validate_node, definition.root);
}

UiDocumentError WidgetRegistry::validate(const CompiledUiProgram &program) const {
  return validate(program.definition());
}

WidgetCreationResult WidgetRegistry::create(const UiNodeDefinition &definition) const {
  const auto found = entries_.find(definition.tag);
  if (found == entries_.end()) {
    return {{},
            make_error(WidgetRegistryErrorCode::UnknownTag,
                       "unknown registered UI tag: " + definition.tag)};
  }
  if (!found->second.factory) {
    return {};
  }
  try {
    auto widget = found->second.factory(definition);
    if (!widget) {
      return {{},
              make_error(WidgetRegistryErrorCode::FactoryFailed,
                         "widget factory returned no widget for tag: " + definition.tag)};
    }
    return {std::move(widget), {}};
  } catch (const std::exception &exception) {
    return {
        {},
        make_error(WidgetRegistryErrorCode::FactoryFailed,
                   "widget factory failed for tag '" + definition.tag + "': " + exception.what())};
  } catch (...) {
    return {{},
            make_error(WidgetRegistryErrorCode::FactoryFailed, "widget factory failed for tag '" +
                                                                   definition.tag +
                                                                   "' with an unknown exception")};
  }
}

WidgetRegistry WidgetRegistry::builtins() {
  WidgetRegistry registry;
  static constexpr const char *kElementTags[] = {
      "div", "span",  "main", "section",  "article", "header", "footer",
      "nav", "aside", "form", "fieldset", "legend",  "p",      "a",
      "ul",  "ol",    "li",   "window",   "view",    "box",    "content"};
  for (const auto *tag : kElementTags) {
    require_registration(registry.register_element(tag));
  }

  require_registration(registry.register_widget("button", [](const UiNodeDefinition &node) {
    auto widget = std::make_unique<ButtonWidget>(text_property(node));
    widget->set_disabled(bool_property(node, "disabled"));
    widget->set_loading(bool_property(node, "loading"));
    return widget;
  }));
  require_registration(registry.register_widget("label", [](const UiNodeDefinition &node) {
    return std::make_unique<LabelWidget>(text_property(node));
  }));
  require_registration(registry.register_widget("input", [](const UiNodeDefinition &node) {
    auto widget = std::make_unique<InputWidget>(string_property(node, "placeholder"),
                                                bool_property(node, "password"));
    widget->set_text(string_property(node, "value"));
    widget->set_readonly(bool_property(node, "readonly"));
    widget->set_disabled(bool_property(node, "disabled"));
    return widget;
  }));
  require_registration(registry.register_widget("checkbox", [](const UiNodeDefinition &node) {
    auto widget = std::make_unique<CheckboxWidget>(
        string_property(node, "label", text_property(node)), bool_property(node, "checked"));
    widget->set_disabled(bool_property(node, "disabled"));
    return widget;
  }));
  require_registration(registry.register_widget("radio", [](const UiNodeDefinition &node) {
    auto widget = std::make_unique<RadioWidget>(
        string_property(node, "label", text_property(node)), string_property(node, "value"),
        string_property(node, "group"), bool_property(node, "checked"));
    widget->set_disabled(bool_property(node, "disabled"));
    return widget;
  }));
  require_registration(registry.register_widget("switch", [](const UiNodeDefinition &node) {
    auto widget = std::make_unique<SwitchWidget>(
        string_property(node, "label", text_property(node)), bool_property(node, "checked"));
    widget->set_disabled(bool_property(node, "disabled"));
    return widget;
  }));
  require_registration(registry.register_widget("slider", [](const UiNodeDefinition &node) {
    auto widget = std::make_unique<SliderWidget>(
        float_property(node, "min", 0.0F), float_property(node, "max", 100.0F),
        float_property(node, "value", 50.0F), float_property(node, "step", 0.0F));
    widget->set_disabled(bool_property(node, "disabled"));
    return widget;
  }));
  require_registration(registry.register_widget("progress", [](const UiNodeDefinition &node) {
    return std::make_unique<ProgressBarWidget>(float_property(node, "value", 0.0F),
                                               bool_property(node, "indeterminate"));
  }));
  require_registration(registry.register_widget("textarea", [](const UiNodeDefinition &node) {
    auto widget = std::make_unique<TextAreaWidget>(
        string_property(node, "value", text_property(node)), string_property(node, "placeholder"));
    widget->set_readonly(bool_property(node, "readonly"));
    widget->set_disabled(bool_property(node, "disabled"));
    return widget;
  }));
  require_registration(registry.register_widget("select", [](const UiNodeDefinition &node) {
    auto widget = std::make_unique<SelectWidget>(list_property(node, "options"),
                                                 integer_property(node, "selected_index", -1));
    widget->set_disabled(bool_property(node, "disabled"));
    return widget;
  }));
  require_registration(registry.register_widget("tooltip", [](const UiNodeDefinition &node) {
    return std::make_unique<TooltipWidget>(text_property(node));
  }));
  require_registration(registry.register_widget("modal", [](const UiNodeDefinition &node) {
    return std::make_unique<ModalWidget>(string_property(node, "title"));
  }));
  register_default_widget<TabsWidget>(registry, "tabs");
  require_registration(registry.register_widget("image", [](const UiNodeDefinition &node) {
    return std::make_unique<ImageWidget>(string_property(node, "src"));
  }));
  require_registration(registry.register_widget("badge", [](const UiNodeDefinition &node) {
    return std::make_unique<BadgeWidget>(text_property(node));
  }));
  register_default_widget<SpinnerWidget>(registry, "spinner");
  register_default_widget<DividerWidget>(registry, "divider");
  require_registration(registry.register_widget("toast", [](const UiNodeDefinition &node) {
    return std::make_unique<ToastWidget>(string_property(node, "message", text_property(node)));
  }));
  require_registration(registry.register_widget("dropdown", [](const UiNodeDefinition &node) {
    return std::make_unique<DropdownWidget>(string_property(node, "placeholder", "Select..."));
  }));
  register_default_widget<AccordionWidget>(registry, "accordion");
  register_default_widget<CalendarWidget>(registry, "calendar");
  register_default_widget<TableWidget>(registry, "table");
  register_default_widget<TreeWidget>(registry, "tree");
  register_default_widget<ColorPickerWidget>(registry, "colorpicker");
  register_default_widget<GroupButtonWidget>(registry, "group-button");
  register_default_widget<ToggleGroupWidget>(registry, "toggle-group");
  require_registration(registry.register_widget("avatar", [](const UiNodeDefinition &node) {
    return std::make_unique<AvatarWidget>(string_property(node, "name"),
                                          string_property(node, "image_url"));
  }));
  register_default_widget<CardWidget>(registry, "card");
  register_default_widget<BreadcrumbWidget>(registry, "breadcrumb");
  require_registration(registry.register_widget("pagination", [](const UiNodeDefinition &node) {
    return std::make_unique<PaginationWidget>(integer_property(node, "total_pages", 1),
                                              integer_property(node, "current_page", 1));
  }));
  require_registration(registry.register_widget("stepper", [](const UiNodeDefinition &node) {
    return std::make_unique<StepperWidget>(
        integer_property(node, "value", 0), integer_property(node, "min", 0),
        integer_property(node, "max", 100), integer_property(node, "step", 1));
  }));
  register_default_widget<GradientEditorWidget>(registry, "gradient-editor");
  require_registration(registry.register_widget("markdown", [](const UiNodeDefinition &node) {
    return std::make_unique<MarkdownWidget>(string_property(node, "markdown", text_property(node)));
  }));
  return registry;
}

} // namespace flexUI
