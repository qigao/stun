#include <charts/ui/chart_frame.h>

#include <flexUI/box.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/render_command.h>
#include <flexUI/render_manager.h>
#include <tinytest.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>

using charts::ui::ChartFrameOptions;
using charts::ui::ChartTooltipState;

namespace {

struct ThemeSnapshot {
  flexUI::Color card;
  flexUI::Color text;
  flexUI::Color tooltip;
  uint64_t command_signature = 0;
  size_t command_count = 0;
  float root_width = 0.0f;
  float surface_width = 0.0f;
};

bool color_near(const flexUI::Color& lhs, const flexUI::Color& rhs) {
  constexpr float kTolerance = 0.001f;
  return std::abs(lhs.r - rhs.r) <= kTolerance &&
         std::abs(lhs.g - rhs.g) <= kTolerance &&
         std::abs(lhs.b - rhs.b) <= kTolerance &&
         std::abs(lhs.a - rhs.a) <= kTolerance;
}

ThemeSnapshot render_theme_snapshot(flexUI::ThemeMode mode,
                                    bool system_dark = false,
                                    float viewport_width = 320.0f) {
  flexUI::BoxOptions box_options;
  box_options.theme = mode;
  flexUI::Box box(nullptr, box_options);
  box.load_css(
      "[data-slot=chart] { width: 100%; height: 240px; } "
      "[data-slot=chart-surface] { height: 120px; }");

  ChartFrameOptions frame_options;
  frame_options.title = "Quarterly revenue with a deliberately long title";
  frame_options.accessible_label = "Quarterly revenue";
  auto frame = charts::ui::create_chart_frame(box, frame_options);
  charts::ui::create_chart_legend_item(
      box, frame, "desktop", "Desktop", "#3b82f6");
  ChartTooltipState tooltip;
  tooltip.active = true;
  tooltip.text = "Desktop: 186";
  tooltip.x = 24.0f;
  tooltip.y = 32.0f;
  charts::ui::set_chart_tooltip_state(frame, tooltip);

  box.set_root(frame.root);
  box.set_viewport(viewport_width, 240.0f);
  flexUI::MediaEnvironment media;
  media.prefers_dark_scheme = system_dark;
  box.set_media_environment(media);
  box.update();

  flexUI::RenderFrame render_frame;
  render_frame.root = frame.root;
  render_frame.viewport = {viewport_width, 240.0f, 1.0f};
  flexUI::RenderManager render_manager(nullptr);
  auto commands = render_manager.build_frame_commands(
      render_frame, flex::RendererCapabilities{});
  return {
      frame.root->computed_style->background_color,
      frame.root->computed_style->text_color,
      frame.tooltip->computed_style->background_color,
      commands.signature(),
      commands.commands().size(),
      frame.root->width(),
      frame.surface->width(),
  };
}

}  // namespace

spec("ChartFrame provides a Box-owned utility styled semantic shell") {
  it("creates stable slots without global ids") {
    flexUI::Box box(nullptr);
    ChartFrameOptions options;
    options.title = "Revenue";
    options.chart_id = "revenue";
    options.accessible_label = "Quarterly revenue";
    auto first = charts::ui::create_chart_frame(box, options);
    auto second = charts::ui::create_chart_frame(box, options);

    auto* page = box.create("main");
    page->append(first.root);
    page->append(second.root);
    box.set_root(page);
    box.update();

    check(first.root != second.root);
    check(first.root->id().empty());
    check(second.root->id().empty());
    check(*first.root->attribute("data-slot") == "chart");
    check(*first.surface->attribute("aria-label") == "Quarterly revenue");
    check(first.root->computed_style->display == flexUI::Display::Flex);
    check_true(first.root->computed_style->border_width[0] > 0.0f);
  }

  it("keeps tooltip state aria and utility styling synchronized") {
    flexUI::Box box(nullptr);
    auto frame = charts::ui::create_chart_frame(box);
    box.set_root(frame.root);
    box.update();

    check(*frame.tooltip->attribute("data-state") == "inactive");
    check(*frame.tooltip->attribute("aria-hidden") == "true");
    check(frame.tooltip->computed_style->display == flexUI::Display::None);

    ChartTooltipState active;
    active.active = true;
    active.text = "Desktop: 186";
    active.x = 24.0f;
    active.y = 32.0f;
    charts::ui::set_chart_tooltip_state(frame, active);
    box.update();

    check(*frame.tooltip->attribute("data-state") == "active");
    check(*frame.tooltip->attribute("aria-hidden") == "false");
    check(frame.tooltip->computed_style->display == flexUI::Display::Block);
    check(frame.tooltip->computed_style->get_variable(flex::Symbol("--tooltip-x")) ==
          "24px");
  }

  it("creates legend items and validates required state") {
    flexUI::Box box(nullptr);
    auto frame = charts::ui::create_chart_frame(box);
    auto* item = charts::ui::create_chart_legend_item(
        box, frame, "desktop", "Desktop", "#3b82f6");
    charts::ui::set_chart_legend_highlighted(*item, true);
    box.set_root(frame.root);
    box.update();

    check(*item->attribute("data-series") == "desktop");
    check(*item->attribute("data-state") == "highlighted");
    check(item->utility_names().count("font-medium") == 1);

    ChartFrameOptions without_legend;
    without_legend.create_legend = false;
    auto no_legend = charts::ui::create_chart_frame(box, without_legend);
    check_throws_as(charts::ui::create_chart_legend_item(
                        box, no_legend, "desktop", "Desktop"),
                    std::logic_error);
  }

  it("keeps deterministic light dark and system render snapshots") {
    const auto light = render_theme_snapshot(flexUI::ThemeMode::Light);
    const auto dark = render_theme_snapshot(flexUI::ThemeMode::Dark);
    const auto system_light =
        render_theme_snapshot(flexUI::ThemeMode::System, false);
    const auto system_dark =
        render_theme_snapshot(flexUI::ThemeMode::System, true);

    check_true(light.command_count > 0);
    check_size_eq(light.command_count, dark.command_count);
    check_false(color_near(light.card, dark.card));
    check_false(color_near(light.text, dark.text));
    check_false(color_near(light.tooltip, dark.tooltip));
    check(light.command_signature != dark.command_signature);
    check_true(color_near(system_light.card, light.card));
    check_true(color_near(system_light.text, light.text));
    check_true(color_near(system_dark.card, dark.card));
    check_true(color_near(system_dark.tooltip, dark.tooltip));
    check(system_light.command_signature == light.command_signature);
    check(system_dark.command_signature == dark.command_signature);
  }

  it("reflows deterministic render snapshots when the viewport changes") {
    const auto narrow =
        render_theme_snapshot(flexUI::ThemeMode::Light, false, 320.0f);
    const auto wide =
        render_theme_snapshot(flexUI::ThemeMode::Light, false, 640.0f);
    const auto wide_repeat =
        render_theme_snapshot(flexUI::ThemeMode::Light, false, 640.0f);

    check_float_eq(narrow.root_width, 320.0f, 0.001f);
    check_float_eq(wide.root_width, 640.0f, 0.001f);
    check_true(wide.surface_width > narrow.surface_width);
    check(narrow.command_signature != wide.command_signature);
    check(wide.command_signature == wide_repeat.command_signature);
    check_size_eq(wide.command_count, wide_repeat.command_count);
  }
}
