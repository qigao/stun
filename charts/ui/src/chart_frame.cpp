#include <charts/ui/chart_frame.h>

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/ui_document.h>
#include <flexUI/ui_xml.h>
#include <flexUI/widget.h>
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

// The native Chart and Infographic DSLs own data, marks and plot geometry.
// ChartFrame/legend markup is authored as XML, compiled into native typed UI
// definitions and published once through the regular Box transaction.
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

const flexUI::CompiledUiProgram& chart_legend_item_template() {
  static auto compiled = [] {
    const auto result = flexUI::compile_ui_xml(kChartLegendItemXml);
    if (!result) {
      throw std::logic_error("invalid embedded XML ChartLegendItem: " +
                             result.error.message);
    }
    return result.program;
  }();
  return *compiled;
}

std::string_view slot_name(const flexUI::UiNodeDefinition& node) {
  const auto it = node.properties.find("slot");
  if (it == node.properties.end()) return {};
  const auto* name = std::get_if<std::string>(&it->second);
  return name ? std::string_view(*name) : std::string_view{};
}

flexUI::Element* optional_slot(
    const flexUI::UiDocumentTree& tree, std::string_view slot) {
  // No allocations after UiDocument commits ownership. This matters for the
  // component transaction's all-or-nothing guarantee.
  for (const auto& entry : tree.slots_by_name) {
    if (std::string_view(entry.first) == slot) return entry.second;
  }
  return nullptr;
}

flexUI::UiDocumentDefinition specialize_chart_frame(
    const ChartFrameOptions& options, bool with_plot,
    const std::vector<ChartLegendEntry>& legends,
    float plot_width, float plot_height, std::string_view renderer) {
  if (options.accessible_label.empty()) {
    throw std::invalid_argument("chart accessible label must not be empty");
  }
  if (!options.create_legend && !legends.empty()) {
    throw std::invalid_argument("chart legend entries require a legend slot");
  }
  if (with_plot) {
    if (!std::isfinite(plot_width) || !std::isfinite(plot_height) ||
        plot_width <= 0.0f || plot_height <= 0.0f || renderer.empty()) {
      throw std::invalid_argument(
          "native plot requires positive finite size and renderer");
    }
  }

  auto definition = chart_frame_template().definition();
  if (!options.chart_id.empty()) {
    definition.root.properties["attr.data-chart"] = options.chart_id;
  }
  auto& children = definition.root.children;
  for (auto& child : children) {
    const auto name = slot_name(child);
    if (name == "chart-title") {
      child.properties["text"] = options.title;
    } else if (name == "chart-surface") {
      child.properties["attr.aria-label"] = options.accessible_label;
      if (!with_plot) {
        // A shell-only caller does not create a placeholder native plot.
        child.children.clear();
      } else {
        if (child.children.size() != 1 ||
            slot_name(child.children.front()) != "chart-plot") {
          throw std::logic_error("ChartFrame XML plot slot is missing");
        }
        auto& plot = child.children.front();
        plot.properties["attr.data-renderer"] = std::string(renderer);
        plot.properties["layout_width"] = plot_width;
        plot.properties["layout_height"] = plot_height;
      }
    } else if (name == "chart-legend") {
      child.children.reserve(legends.size());
      for (const auto& legend : legends) {
        if (legend.series.empty()) {
          throw std::invalid_argument("chart legend series must not be empty");
        }
        auto entry = chart_legend_item_template().definition().root;
        if (entry.children.size() != 2) {
          throw std::logic_error("ChartLegendItem XML schema is incomplete");
        }
        entry.properties["attr.data-series"] = legend.series;
        if (!legend.css_color.empty()) {
          entry.children.front().properties["var.series-color"] = legend.css_color;
        }
        entry.children.back().properties["text"] = legend.label;
        child.children.push_back(std::move(entry));
      }
    }
  }
  children.erase(std::remove_if(children.begin(), children.end(),
        [&](const flexUI::UiNodeDefinition& child) {
          const auto name = slot_name(child);
          return (name == "chart-title" && options.title.empty()) ||
                 (name == "chart-legend" && !options.create_legend) ||
                 (name == "chart-tooltip" && !options.create_tooltip);
        }), children.end());
  return definition;
}

ChartFrameComposition instantiate_chart_frame(
    flexUI::Box& box, flexUI::UiDocumentDefinition definition,
    const flexUI::WidgetRegistry& registry,
    const ChartFrameOptions& options, bool with_plot) {
  auto compiled = flexUI::compile_ui_definition(std::move(definition));
  if (!compiled) {
    throw std::invalid_argument("ChartFrame XML specialization failed: " +
                                compiled.error.message);
  }
  flexUI::UiMountOptions mount;
  mount.mode = flexUI::UiMountMode::DetachedFragment;
  mount.id_policy = flexUI::UiIdPolicy::LocalOnly;
  mount.required_slots = {"chart", "chart-surface"};
  if (!options.title.empty()) mount.required_slots.emplace_back("chart-title");
  if (options.create_legend) mount.required_slots.emplace_back("chart-legend");
  if (options.create_tooltip) mount.required_slots.emplace_back("chart-tooltip");
  if (with_plot) mount.required_slots.emplace_back("chart-plot");

  auto installed = flexUI::UiDocumentInstantiator::instantiate(
      box, *compiled.program, registry, mount);
  if (!installed) {
    throw std::invalid_argument("ChartFrame XML instantiation failed: " +
                                installed.error.message);
  }

  ChartFrameComposition result;
  result.frame.root = installed.tree.root;
  result.frame.title = optional_slot(installed.tree, "chart-title");
  result.frame.surface = optional_slot(installed.tree, "chart-surface");
  result.frame.legend = optional_slot(installed.tree, "chart-legend");
  result.frame.tooltip = optional_slot(installed.tree, "chart-tooltip");
  result.plot = optional_slot(installed.tree, "chart-plot");
  // Mandatory slot presence is checked before creating any Box nodes.
  return result;
}

ChartFrame create_chart_frame(flexUI::Box& box,
                              const ChartFrameOptions& options) {
  static const auto registry = flexUI::WidgetRegistry::builtins();
  return instantiate_chart_frame(
      box, specialize_chart_frame(options, false, {}, 0.0f, 0.0f, {}),
      registry, options, false).frame;
}

ChartFrameComposition create_chart_frame_with_plot(
    flexUI::Box& box, const ChartFrameOptions& options,
    const std::vector<ChartLegendEntry>& legend_entries,
    std::unique_ptr<flexUI::Widget> prepared_plot,
    float plot_width, float plot_height, std::string_view renderer) {
  if (!prepared_plot) {
    throw std::invalid_argument("native chart plot widget must not be null");
  }
  // Preflight and lower the *entire* component, including arbitrary legend
  // entries, before moving ownership of the prepared Plot into a factory.
  auto definition = specialize_chart_frame(
      options, true, legend_entries, plot_width, plot_height, renderer);
  auto registry = flexUI::WidgetRegistry::builtins();
  auto pending = std::make_shared<std::unique_ptr<flexUI::Widget>>(
      std::move(prepared_plot));
  const auto registered = registry.register_widget(
      "chart-plot",
      [pending](const flexUI::UiNodeDefinition&)
          -> std::unique_ptr<flexUI::Widget> {
        if (!*pending) {
          throw std::logic_error("native chart plot factory invoked twice");
        }
        return std::move(*pending);
      }, flexUI::UiContentModel::Empty);
  if (registered) {
    throw std::logic_error("failed to register native Chart Plot widget: " +
                           registered.message);
  }
  return instantiate_chart_frame(
      box, std::move(definition), registry, options, true);
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
