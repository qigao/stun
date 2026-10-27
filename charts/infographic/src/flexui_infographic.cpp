#include <flexui_infographic.h>

#include <charts/ui/chart_frame.h>
#include <charts/ui/flex_node_plot_widget.h>
#include <charts/ui/infographic_frame.h>
#include <flex.h>
#include <flexUI/box.h>
#include <infographic_component.h>
#include <ir/unified_infographic.h>

#include <cmath>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace flex::modules::infographic {

FlexUiInfographicResult create_flexui_infographic(
    flexUI::Box& box, const UnifiedInfographic& infographic,
    const InfographicViewOptions& options) {
  if (!std::isfinite(options.width) || !std::isfinite(options.height) ||
      options.width <= 0.0f || options.height <= 0.0f) {
    return {nullptr, nullptr,
            "Infographic view dimensions must be finite and positive"};
  }
  if (options.accessible_label.empty()) {
    return {nullptr, nullptr,
            "Infographic accessible label must not be empty"};
  }

  try {
    // Domain IR + native Flex plot are prepared before any UI is published.
    // A failed layout/mark builder must never leave a partial Box-owned shell.
    auto instance = flex::Instance::create(options.width, options.height);
    InfographicComponentBuildOptions build_options;
    build_options.show_background = false;
    build_options.show_title = false;
    auto* legacy_plot = InfographicComponent::build(
        infographic, *instance, options.width, options.height, build_options);
    if (!legacy_plot) {
      return {nullptr, nullptr,
              "Infographic layout renderer returned no plot"};
    }
    auto widget = charts::ui::create_flex_node_plot_widget(
        instance, legacy_plot, options.width, options.height, false);

    if (infographic.category == TemplateCategory::Chart) {
      charts::ui::ChartFrameOptions frame_options;
      frame_options.title = infographic.get_title();
      frame_options.chart_id = options.view_id;
      frame_options.accessible_label = options.accessible_label;

      std::vector<charts::ui::ChartLegendEntry> legends;
      legends.reserve(infographic.items.size());
      for (std::size_t index = 0; index < infographic.items.size(); ++index) {
        const auto& item = infographic.items[index];
        if (!item) {
          throw std::invalid_argument("infographic legend item must not be null");
        }
        std::string color;
        if (!infographic.theme.palette.empty()) {
          color = infographic.theme.palette[
              index % infographic.theme.palette.size()];
        }
        legends.push_back({item->label, item->label, std::move(color)});
      }
      const auto frame = charts::ui::create_chart_frame_with_plot(
          box, frame_options, legends, std::move(widget),
          options.width, options.height, "infographic");
      return {frame.frame.root, frame.plot, {}};
    }

    charts::ui::InfographicFrameOptions frame_options;
    frame_options.title = infographic.title.value_or("");
    frame_options.view_id = options.view_id;
    frame_options.accessible_label = options.accessible_label;
    frame_options.width = options.width;
    frame_options.height = options.height;
    const auto frame = charts::ui::create_infographic_frame_with_plot(
        box, frame_options, std::move(widget));
    return {frame.root, frame.plot, {}};
  } catch (const std::exception& error) {
    return {nullptr, nullptr, error.what()};
  }
}

}  // namespace flex::modules::infographic
