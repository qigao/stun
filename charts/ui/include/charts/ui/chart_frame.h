#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <string_view>

namespace flexUI {
class Box;
class Element;
class Widget;
}

namespace charts::ui {

struct ChartFrameOptions {
  std::string title;
  std::string chart_id;
  std::string accessible_label = "Chart";
  bool create_legend = true;
  bool create_tooltip = true;
};

struct ChartFrame {
  flexUI::Element* root = nullptr;
  flexUI::Element* title = nullptr;
  flexUI::Element* surface = nullptr;
  flexUI::Element* legend = nullptr;
  flexUI::Element* tooltip = nullptr;
};

/// Domain data for one legend item; visual tree originates from the XML
/// legend-item component template, not hand-written Element nodes.
struct ChartLegendEntry {
  std::string series;
  std::string label;
  std::string css_color;
};

struct ChartFrameComposition {
  ChartFrame frame;
  flexUI::Element* plot = nullptr;

  explicit operator bool() const noexcept {
    return frame.root != nullptr && plot != nullptr;
  }
};

struct ChartTooltipState {
  bool active = false;
  std::string text;
  std::optional<float> x;
  std::optional<float> y;
};

ChartFrame create_chart_frame(flexUI::Box& box,
                              const ChartFrameOptions& options = {});

/// Adopt an already-prepared native plot and all XML-defined legend items in
/// ONE UiDocument detached transaction. No Box Elements are created until
/// the entire chart shell, plot factory and legend forest pass admission.
ChartFrameComposition create_chart_frame_with_plot(
    flexUI::Box& box, const ChartFrameOptions& options,
    const std::vector<ChartLegendEntry>& legend_entries,
    std::unique_ptr<flexUI::Widget> prepared_plot,
    float plot_width, float plot_height,
    std::string_view renderer);

flexUI::Element* create_chart_legend_item(flexUI::Box& box,
                                          ChartFrame& frame,
                                          std::string_view series,
                                          std::string_view label,
                                          std::string_view css_color = {});

void set_chart_legend_highlighted(flexUI::Element& item, bool highlighted);
void set_chart_tooltip_state(ChartFrame& frame,
                             const ChartTooltipState& state);

}  // namespace charts::ui
