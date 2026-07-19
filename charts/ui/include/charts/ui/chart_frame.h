#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace flexUI {
class Box;
class Element;
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

struct ChartTooltipState {
  bool active = false;
  std::string text;
  std::optional<float> x;
  std::optional<float> y;
};

ChartFrame create_chart_frame(flexUI::Box& box,
                              const ChartFrameOptions& options = {});

flexUI::Element* create_chart_legend_item(flexUI::Box& box,
                                          ChartFrame& frame,
                                          std::string_view series,
                                          std::string_view label,
                                          std::string_view css_color = {});

void set_chart_legend_highlighted(flexUI::Element& item, bool highlighted);
void set_chart_tooltip_state(ChartFrame& frame,
                             const ChartTooltipState& state);

}  // namespace charts::ui
