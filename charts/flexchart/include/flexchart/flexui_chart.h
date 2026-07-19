#pragma once

#include <string>

namespace flexUI {
class Box;
class Element;
}

namespace flex::chart {
struct AstChart;
}

namespace flex::modules::chart {

struct ChartViewOptions {
  float width = 800.0f;
  float height = 600.0f;
  std::string chart_id;
  std::string accessible_label = "Chart";
};

struct FlexUiChartResult {
  flexUI::Element* root = nullptr;
  flexUI::Element* plot = nullptr;
  std::string error;

  explicit operator bool() const noexcept { return root != nullptr; }
};

/**
 * Build a Box-owned chart shell and adapt the existing mark renderer tree to
 * flexUI render commands. The AST is copied and never mutated.
 */
FlexUiChartResult create_flexui_chart(
    flexUI::Box& box,
    const flex::chart::AstChart& chart,
    const ChartViewOptions& options = {});

}  // namespace flex::modules::chart
