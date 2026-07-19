#include <flexchart/flexui_chart.h>

#include "chart_component_internal.h"

#include <charts/ui/chart_frame.h>
#include <charts/ui/flex_node_plot_widget.h>
#include <flex.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexchart/chart_component.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <vector>

namespace flex::modules::chart {
namespace {

std::vector<std::string> legend_series(
    const std::shared_ptr<flex::chart::AstChart>& chart) {
  std::vector<std::string> result;
  std::unordered_set<std::string> seen;
  for (const auto& mark : chart->marks) {
    std::string color_field;
    for (const auto& encoding : mark->encodings) {
      if (encoding->channel == "color") {
        color_field = encoding->field;
        break;
      }
    }
    if (color_field.empty()) {
      continue;
    }
    const auto records = flex::chart::get_records(
        flex::chart::find_dataset(chart, mark->data_ref));
    for (const auto& record : records) {
      std::string series = flex::chart::get_string_val(record.get(color_field));
      if (!series.empty() && seen.insert(series).second) {
        result.push_back(std::move(series));
      }
    }
  }
  return result;
}

}  // namespace

FlexUiChartResult create_flexui_chart(
    flexUI::Box& box, const flex::chart::AstChart& chart,
    const ChartViewOptions& options) {
  if (!std::isfinite(options.width) || !std::isfinite(options.height) ||
      options.width <= 0.0f || options.height <= 0.0f) {
    return {nullptr, nullptr,
            "FlexChart view dimensions must be finite and positive"};
  }
  if (options.accessible_label.empty()) {
    return {nullptr, nullptr,
            "FlexChart accessible label must not be empty"};
  }

  try {
    auto chart_copy = std::make_shared<flex::chart::AstChart>(chart);
    chart_copy->width = std::to_string(options.width) + "px";
    chart_copy->height = std::to_string(options.height) + "px";

    charts::ui::ChartFrameOptions frame_options;
    frame_options.title = chart.title;
    frame_options.chart_id = options.chart_id;
    frame_options.accessible_label = options.accessible_label;
    auto frame = charts::ui::create_chart_frame(box, frame_options);

    static const std::vector<std::string> palette = {
        "#3b82f6", "#10b981", "#f59e0b",
        "#ef4444", "#8b5cf6", "#06b6d4"};
    const auto series = legend_series(chart_copy);
    for (std::size_t index = 0; index < series.size(); ++index) {
      charts::ui::create_chart_legend_item(
          box, frame, series[index], series[index],
          palette[index % palette.size()]);
    }

    auto instance = flex::Instance::create(options.width, options.height);
    flex::chart::ChartComponentBuildOptions build_options;
    build_options.show_title = false;
    build_options.show_legend = false;
    auto* legacy_plot = flex::chart::ChartComponent::build(
        chart_copy, *instance, build_options);
    if (!legacy_plot) {
      return {nullptr, nullptr, "FlexChart mark renderer returned no plot"};
    }

    auto plot_widget = charts::ui::create_flex_node_plot_widget(
        instance, legacy_plot, options.width, options.height);
    auto* plot = box.create_with_widget("div", std::move(plot_widget));
    plot->set_attribute("data-slot", "chart-plot");
    plot->set_attribute("data-renderer", "flexchart");
    plot->add_utilities("relative w-full overflow-hidden");
    plot->set_layout_size(options.width, options.height);
    frame.surface->append(plot);
    return {frame.root, plot, {}};
  } catch (const std::exception& error) {
    return {nullptr, nullptr, error.what()};
  }
}

}  // namespace flex::modules::chart
