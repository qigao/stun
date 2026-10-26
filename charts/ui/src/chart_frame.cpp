#include <charts/ui/chart_frame.h>

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/ui_document.h>
#include <flexUI/ui_xml.h>
#include <flexUI/widget_registry.h>
#include "chart_frame_xml.inc"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace charts::ui {
namespace {

std::string pixel_value(float value) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument("chart tooltip coordinates must be finite");
  }
  std::ostringstream result;
  result << value << "px";
  return result.str();
}

}  // namespace

// The UI shell is a normal XML component. The chart grammar continues to
// own data/marks/layout; this template owns only semantics, CSS and slots.
// Parse once, specialize the immutable definition copy, then commit via the
// same detached UiDocument pipeline as every other registered UI component.
const flexUI::CompiledUiProgram& chart_frame_template() {
  static auto compiled = [] {
    const auto result = flexUI::compile_ui_xml(kChartFrameXml);
    if (!result) {
      throw std::logic_error("invalid embedded XML ChartFrame: " +
                             result.error.message);
    }
    return result.program;
  }();
  return *compiled;
}

std::string_view slot_name(const flexUI::UiNodeDefinition& node) {
  const auto it = node.properties.find("slot");
  if (it == node.properties.end()) return {};
  const auto *name = std::get_if<std::string>(&it->second);
  return name ? std::string_view(*name) : std::string_view{};
}

flexUI::Element* optional_slot(
    const flexUI::UiDocumentTree& tree, std::string_view slot) {
  const auto found = tree.slots_by_name.find(std::string(slot));
  return found == tree.slots_by_name.end() ? nullptr : found->second;
}

ChartFrame create_chart_frame(flexUI::Box& box,
                              const ChartFrameOptions& options) {
  if (options.accessible_label.empty()) {
    throw std::invalid_argument("chart accessible label must not be empty");
  }

  auto definition = chart_frame_template().definition();
  if (!options.chart_id.empty()) {
    definition.root.properties["attr.data-chart"] = options.chart_id;
  }
  auto &children = definition.root.children;
  for (auto &child : children) {
    const auto name = slot_name(child);
    if (name == "chart-title") {
      child.properties["text"] = options.title;
    } else if (name == "chart-surface") {
      child.properties["attr.aria-label"] = options.accessible_label;
    }
  }
  children.erase(std::remove_if(children.begin(), children.end(),
        [&](const flexUI::UiNodeDefinition& child) {
          const auto name = slot_name(child);
          return (name == "chart-title" && options.title.empty()) ||
                 (name == "chart-legend" && !options.create_legend) ||
                 (name == "chart-tooltip" && !options.create_tooltip);
        }), children.end());

  auto compiled = flexUI::compile_ui_definition(std::move(definition));
  if (!compiled) {
    throw std::invalid_argument("ChartFrame XML specialization failed: " +
                                compiled.error.message);
  }
  auto registry = flexUI::WidgetRegistry::builtins();
  flexUI::UiMountOptions mount;
  mount.mode = flexUI::UiMountMode::DetachedFragment;
  mount.id_policy = flexUI::UiIdPolicy::LocalOnly;
  auto installed = flexUI::UiDocumentInstantiator::instantiate(
      box, *compiled.program, registry, mount);
  if (!installed) {
    throw std::invalid_argument("ChartFrame XML instantiation failed: " +
                                installed.error.message);
  }

  ChartFrame frame;
  frame.root = installed.tree.root;
  frame.title = optional_slot(installed.tree, "chart-title");
  frame.surface = optional_slot(installed.tree, "chart-surface");
  frame.legend = optional_slot(installed.tree, "chart-legend");
  frame.tooltip = optional_slot(installed.tree, "chart-tooltip");
  if (!frame.root || !frame.surface) {
    throw std::logic_error("ChartFrame missing mandatory XML slots");
  }
  return frame;
}

flexUI::Element* create_chart_legend_item(flexUI::Box& box,
                                          ChartFrame& frame,
                                          std::string_view series,
                                          std::string_view label,
                                          std::string_view css_color) {
  if (!frame.legend) {
    throw std::logic_error("chart frame has no legend container");
  }
  if (series.empty()) {
    throw std::invalid_argument("chart legend series must not be empty");
  }

  auto* item = box.create("span");
  item->set_attribute("data-slot", "chart-legend-item");
  item->set_attribute("data-series", std::string(series));
  item->set_attribute("data-state", "normal");
  item->set_attribute("role", "listitem");
  item->add_utilities("flex items-center gap-2");

  auto* indicator = box.create("span");
  indicator->set_attribute("data-slot", "chart-legend-indicator");
  indicator->set_attribute("aria-hidden", "true");
  indicator->add_utilities("h-2 w-2 rounded-full bg-muted");
  if (!css_color.empty()) {
    indicator->set_custom_property("--series-color", std::string(css_color));
  }
  item->append(indicator);

  auto* text = box.create("span");
  text->set_attribute("data-slot", "chart-legend-label");
  text->set_text(std::string(label));
  item->append(text);
  frame.legend->append(item);
  return item;
}

void set_chart_legend_highlighted(flexUI::Element& item, bool highlighted) {
  item.set_attribute("data-state", highlighted ? "highlighted" : "normal");
  item.toggle_utility("font-medium", highlighted);
  item.toggle_utility("opacity-50", !highlighted);
}

void set_chart_tooltip_state(ChartFrame& frame,
                             const ChartTooltipState& state) {
  if (!frame.tooltip) {
    throw std::logic_error("chart frame has no tooltip container");
  }
  if (state.x.has_value() != state.y.has_value()) {
    throw std::invalid_argument(
        "chart tooltip coordinates must be provided as an x/y pair");
  }

  frame.tooltip->set_text(state.text);
  frame.tooltip->set_attribute("data-state",
                               state.active ? "active" : "inactive");
  frame.tooltip->set_attribute("aria-hidden", state.active ? "false" : "true");
  frame.tooltip->toggle_utility("hidden", !state.active);
  if (state.x) {
    frame.tooltip->set_custom_property("--tooltip-x", pixel_value(*state.x));
    frame.tooltip->set_custom_property("--tooltip-y", pixel_value(*state.y));
  }
}

}  // namespace charts::ui
