#include <charts/ui/chart_frame.h>

#include <flexUI/box.h>
#include <flexUI/element.h>

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

ChartFrame create_chart_frame(flexUI::Box& box,
                              const ChartFrameOptions& options) {
  if (options.accessible_label.empty()) {
    throw std::invalid_argument("chart accessible label must not be empty");
  }

  ChartFrame frame;
  frame.root = box.create("figure");
  frame.root->set_attribute("data-slot", "chart");
  frame.root->set_attribute("role", "figure");
  if (!options.chart_id.empty()) {
    frame.root->set_attribute("data-chart", options.chart_id);
  }
  frame.root->add_utilities(
      "flex flex-col relative w-full box-border rounded-lg border bg-card "
      "text-card-foreground p-4 gap-4");

  if (!options.title.empty()) {
    frame.title = box.create("figcaption");
    frame.title->set_attribute("data-slot", "chart-title");
    frame.title->set_text(options.title);
    frame.title->add_utilities("text-sm font-medium");
    frame.title->set_custom_property("--overflow-wrap", "anywhere");
    frame.root->append(frame.title);
  }

  frame.surface = box.create("div");
  frame.surface->set_attribute("data-slot", "chart-surface");
  frame.surface->set_attribute("role", "img");
  frame.surface->set_attribute("aria-label", options.accessible_label);
  frame.surface->add_utilities("relative w-full");
  frame.root->append(frame.surface);

  if (options.create_legend) {
    frame.legend = box.create("div");
    frame.legend->set_attribute("data-slot", "chart-legend");
    frame.legend->set_attribute("role", "list");
    frame.legend->add_utilities(
        "flex items-center justify-center gap-4 text-sm");
    frame.root->append(frame.legend);
  }

  if (options.create_tooltip) {
    frame.tooltip = box.create("div");
    frame.tooltip->set_attribute("data-slot", "chart-tooltip");
    frame.tooltip->set_attribute("role", "status");
    frame.tooltip->set_attribute("data-state", "inactive");
    frame.tooltip->set_attribute("aria-hidden", "true");
    frame.tooltip->add_utilities(
        "absolute hidden rounded-md border bg-popover "
        "text-popover-foreground p-2 shadow-md text-sm");
    frame.root->append(frame.tooltip);
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
