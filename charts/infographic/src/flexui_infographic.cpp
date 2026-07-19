#include <flexui_infographic.h>

#include <charts/ui/chart_frame.h>
#include <charts/ui/flex_node_plot_widget.h>
#include <flex.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <infographic_component.h>
#include <ir/unified_infographic.h>

#include <cmath>
#include <exception>
#include <memory>
#include <utility>

namespace flex::modules::infographic {
namespace {

struct SemanticShell {
  flexUI::Element* root = nullptr;
  flexUI::Element* surface = nullptr;
};

SemanticShell create_infographic_shell(
    flexUI::Box& box, const UnifiedInfographic& infographic,
    const InfographicViewOptions& options) {
  auto* root = box.create("figure");
  root->set_attribute("data-slot", "infographic");
  root->set_attribute("role", "figure");
  root->set_attribute("aria-label", options.accessible_label);
  if (!options.view_id.empty()) {
    root->set_attribute("data-infographic", options.view_id);
  }
  root->add_utilities(
      "flex flex-col relative w-full rounded-lg border bg-card "
      "text-card-foreground p-4 gap-4");

  if (infographic.title && !infographic.title->empty()) {
    auto* title = box.create("figcaption");
    title->set_attribute("data-slot", "infographic-title");
    title->set_text(*infographic.title);
    title->add_utilities("text-sm font-medium");
    root->append(title);
  }

  auto* surface = box.create("div");
  surface->set_attribute("data-slot", "infographic-surface");
  surface->set_attribute("role", "img");
  surface->set_attribute("aria-label", options.accessible_label);
  surface->add_utilities("relative w-full");
  root->append(surface);
  return {root, surface};
}

}  // namespace

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
    SemanticShell shell;
    if (infographic.category == TemplateCategory::Chart) {
      charts::ui::ChartFrameOptions frame_options;
      frame_options.title = infographic.get_title();
      frame_options.chart_id = options.view_id;
      frame_options.accessible_label = options.accessible_label;
      auto frame = charts::ui::create_chart_frame(box, frame_options);
      for (std::size_t index = 0; index < infographic.items.size(); ++index) {
        const auto& item = infographic.items[index];
        std::string color;
        if (!infographic.theme.palette.empty()) {
          color = infographic.theme.palette[
              index % infographic.theme.palette.size()];
        }
        charts::ui::create_chart_legend_item(
            box, frame, item->label, item->label, color);
      }
      shell = {frame.root, frame.surface};
    } else {
      shell = create_infographic_shell(box, infographic, options);
    }

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
    auto* plot = box.create_with_widget("div", std::move(widget));
    plot->set_attribute("data-slot", "infographic-plot");
    plot->set_attribute("data-renderer", "infographic");
    plot->add_utilities("relative w-full overflow-hidden");
    plot->set_layout_size(options.width, options.height);
    shell.surface->append(plot);
    return {shell.root, plot, {}};
  } catch (const std::exception& error) {
    return {nullptr, nullptr, error.what()};
  }
}

}  // namespace flex::modules::infographic
