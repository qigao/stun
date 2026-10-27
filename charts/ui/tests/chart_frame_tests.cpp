#include <charts/ui/chart_frame.h>
#include <charts/ui/infographic_frame.h>

#include <flexUI/box.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/render_command.h>
#include <flexUI/render_manager.h>
#include <flexUI/widget.h>
#include <tinytest.hpp>

#include <cmath>
#include <cstdint>
#include <stdexcept>

using charts::ui::ChartFrameOptions;
using charts::ui::ChartTooltipState;

namespace {


class ProbePlotWidget final : public flexUI::Widget {
 public:
  void emit_render_commands(const flexUI::Element&,
                            flexUI::RenderCommandList&) override {}
  const char* type_name() const override { return "ProbePlotWidget"; }
};

class RejectingPlotWidget final : public flexUI::Widget {
 public:
  void emit_render_commands(const flexUI::Element&,
                            flexUI::RenderCommandList&) override {}
  const char* type_name() const override { return "RejectingPlotWidget"; }

 private:
  void sync_host_semantics() override {
    throw std::runtime_error("intentional chart plot bind failure");
  }
};

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
    check_true(light.command_count == dark.command_count);
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

  it("keeps XML ChartFrame preparation fail-closed before Box ownership") {
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "page");
    box.set_root(page);
    const auto before = box.owned_element_count();

    ChartFrameOptions empty_label;
    empty_label.accessible_label.clear();
    check_throws_as(charts::ui::create_chart_frame(box, empty_label),
                    std::invalid_argument);
    check_equal(box.owned_element_count(), before);
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);

    ChartFrameOptions excessive_id;
    excessive_id.chart_id.assign(70 * 1024, 'x');
    check_throws_as(charts::ui::create_chart_frame(box, excessive_id),
                    std::invalid_argument);
    check_equal(box.owned_element_count(), before);
    check_equal(page->child_count(), 0U);
  }

  it("projects XML-defined CSS custom properties before element publication") {
    flexUI::Box box(nullptr);
    ChartFrameOptions opts;
    opts.title = "A long descriptive revenue title";
    const auto frame = charts::ui::create_chart_frame(box, opts);
    check_not_null(frame.title);
    check_null(box.root());
    box.set_root(frame.root);
    box.update();
    check(frame.title->computed_style->get_variable(
              flex::Symbol("--overflow-wrap")) == "anywhere");
    check_equal(*frame.title->attribute("data-slot"), "chart-title");
  }

  it("reflows deterministic render snapshots when the viewport changes") {
    const auto narrow =
        render_theme_snapshot(flexUI::ThemeMode::Light, false, 320.0f);
    const auto wide =
        render_theme_snapshot(flexUI::ThemeMode::Light, false, 640.0f);
    const auto wide_repeat =
        render_theme_snapshot(flexUI::ThemeMode::Light, false, 640.0f);

    check_true(std::abs(narrow.root_width - 320.0f) <= 0.001f);
    check_true(std::abs(wide.root_width - 640.0f) <= 0.001f);
    check_true(wide.surface_width > narrow.surface_width);
    check(narrow.command_signature != wide.command_signature);
    check(wide.command_signature == wide_repeat.command_signature);
    check_true(wide.command_count == wide_repeat.command_count);
  }
}


spec("ChartFrame and native Plot share one XML publication transaction") {
  it("installs plot, legend and shell as one Box-owned typed component") {
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "page");
    box.set_root(page);
    const auto before = box.owned_element_count();
    ChartFrameOptions options;
    options.title = "Sales";
    options.chart_id = "sales";
    options.accessible_label = "Sales chart";
    const std::vector<charts::ui::ChartLegendEntry> series = {
        {"desktop", "Desktop", "#3b82f6"},
        {"mobile", "Mobile", "#10b981"}};

    auto assembled = charts::ui::create_chart_frame_with_plot(
        box, options, series, std::make_unique<ProbePlotWidget>(),
        420.0f, 180.0f, "native-probe");

    check(static_cast<bool>(assembled));
    check_null(box.get_by_id("chart-plot"));
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);
    check(assembled.plot->parent_elem() == assembled.frame.surface);
    check_not_null(assembled.plot->widget);
    check_equal(*assembled.plot->attribute("data-slot"), "chart-plot");
    check_equal(*assembled.plot->attribute("data-renderer"), "native-probe");
    check_true(std::abs(assembled.plot->layout_width() - 420.0f) <= 0.001f);
    check_true(std::abs(assembled.plot->layout_height() - 180.0f) <= 0.001f);
    check(assembled.frame.legend->child_count() == 2U);
    auto* first = assembled.frame.legend->child_at(0);
    check_equal(*first->attribute("data-series"), "desktop");
    check_equal(*first->attribute("data-slot"), "chart-legend-item");
    check_equal(*first->child_at(1)->attribute("data-slot"), "chart-legend-label");
    check_equal(first->child_at(1)->text(), "Desktop");
    check_true(box.owned_element_count() > before);

    page->append(assembled.frame.root);
    box.update();
    check(assembled.frame.root->computed_style->display == flexUI::Display::Flex);
    check(first->child_at(0)->computed_style->get_variable(
          flex::Symbol("--series-color")) == "#3b82f6");
    charts::ui::set_chart_legend_highlighted(*first, true);
    box.update();
    check_equal(*first->attribute("data-state"), "highlighted");
    check(first->utility_names().count("font-medium") == 1);
  }

  it("does not publish a shell if the prepared native plot rejects binding") {
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "page");
    box.set_root(page);
    const auto before = box.owned_element_count();
    ChartFrameOptions options;
    options.title = "Must not appear";
    std::vector<charts::ui::ChartLegendEntry> series = {
        {"desktop", "Desktop", "#3b82f6"}};

    check_throws_as(
        charts::ui::create_chart_frame_with_plot(
            box, options, series, std::make_unique<RejectingPlotWidget>(),
            300.0f, 150.0f, "native-probe"),
        std::invalid_argument);
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);
    check_equal(box.owned_element_count(), before);
    check_null(box.get_by_id("chart-plot"));

    // The same Box remains usable; a failed plot factory never poisons the
    // subsequent XML fragment or its component-local slot index.
    const auto recovered = charts::ui::create_chart_frame_with_plot(
        box, options, series, std::make_unique<ProbePlotWidget>(),
        300.0f, 150.0f, "native-probe");
    check(static_cast<bool>(recovered));
    check_equal(box.root(), page);
  }

  it("rejects invalid plot and legend arguments without altering Box") {
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "page");
    box.set_root(page);
    const auto before = box.owned_element_count();
    const ChartFrameOptions valid;
    check_throws_as(charts::ui::create_chart_frame_with_plot(
                        box, valid, {}, {}, 300.0f, 150.0f, "native-probe"),
                    std::invalid_argument);
    check_throws_as(charts::ui::create_chart_frame_with_plot(
                        box, valid, {}, std::make_unique<ProbePlotWidget>(),
                        -1.0f, 150.0f, "native-probe"), std::invalid_argument);
    check_throws_as(charts::ui::create_chart_frame_with_plot(
                        box, valid, {{"", "No ID", ""}},
                        std::make_unique<ProbePlotWidget>(),
                        300.0f, 150.0f, "native-probe"), std::invalid_argument);
    auto no_legend = valid;
    no_legend.create_legend = false;
    check_throws_as(charts::ui::create_chart_frame_with_plot(
                        box, no_legend, {{"series", "Label", ""}},
                        std::make_unique<ProbePlotWidget>(),
                        300.0f, 150.0f, "native-probe"), std::invalid_argument);
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);
    check_equal(box.owned_element_count(), before);
  }
}


spec("Infographic native Plot uses the same atomic XML component pipeline") {
  it("builds a real Box-owned native infographic with accessible XML slots") {
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "page");
    box.set_root(page);
    charts::ui::InfographicFrameOptions options;
    options.title = "Data journey";
    options.view_id = "roadmap-7";
    options.accessible_label = "Process infographic";
    options.width = 540.0f;
    options.height = 380.0f;

    const auto result = charts::ui::create_infographic_frame_with_plot(
        box, options, std::make_unique<ProbePlotWidget>());
    check(static_cast<bool>(result));
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);
    check_equal(*result.root->attribute("data-slot"), "infographic");
    check_equal(*result.root->attribute("data-infographic"), "roadmap-7");
    check_equal(*result.root->attribute("aria-label"), "Process infographic");
    check_not_null(result.title);
    check_equal(result.title->text(), "Data journey");
    check_equal(*result.surface->attribute("data-slot"), "infographic-surface");
    check_equal(*result.surface->attribute("aria-label"), "Process infographic");
    check_equal(*result.plot->attribute("data-slot"), "infographic-plot");
    check_equal(*result.plot->attribute("data-renderer"), "infographic");
    check(result.plot->parent_elem() == result.surface);
    check_not_null(result.plot->widget);
    check_true(std::abs(result.plot->layout_width() - 540.0f) <= 0.001f);
    check_true(std::abs(result.plot->layout_height() - 380.0f) <= 0.001f);
    check_null(box.get_by_id("infographic"));

    page->append(result.root);
    box.update();
    check(result.root->computed_style->display == flexUI::Display::Flex);
    check_true(result.root->computed_style->border_width[0] > 0.0f);
  }

  it("supports multiple id-less infographic instances and optional captions") {
    flexUI::Box box(nullptr);
    charts::ui::InfographicFrameOptions options;
    options.title.clear();
    const auto first = charts::ui::create_infographic_frame_with_plot(
        box, options, std::make_unique<ProbePlotWidget>());
    const auto second = charts::ui::create_infographic_frame_with_plot(
        box, options, std::make_unique<ProbePlotWidget>());
    check(static_cast<bool>(first));
    check(static_cast<bool>(second));
    check_null(first.title);
    check_null(second.title);
    check(first.root != second.root);
    check(first.plot != second.plot);
    auto* page = box.create("main");
    page->append(first.root);
    page->append(second.root);
    box.set_root(page);
    box.update();
    check_equal(page->child_count(), 2U);
  }

  it("rolls back the complete infographic on native Plot binding failure") {
    flexUI::Box box(nullptr);
    auto* page = box.create("main", "page");
    box.set_root(page);
    const auto before = box.owned_element_count();
    charts::ui::InfographicFrameOptions options;
    options.title = "Should never publish";

    check_throws_as(charts::ui::create_infographic_frame_with_plot(
                        box, options, std::make_unique<RejectingPlotWidget>()),
                    std::invalid_argument);
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);
    check_equal(box.owned_element_count(), before);
    check_null(box.get_by_id("infographic-plot"));

    // Failed widget binding did not corrupt the Box ownership transaction.
    const auto recovered = charts::ui::create_infographic_frame_with_plot(
        box, options, std::make_unique<ProbePlotWidget>());
    check(static_cast<bool>(recovered));
    check_equal(box.root(), page);
  }

  it("rejects invalid dimensions, missing accessibility and empty widget") {
    flexUI::Box box(nullptr);
    auto* page = box.create("main");
    box.set_root(page);
    const auto before = box.owned_element_count();
    charts::ui::InfographicFrameOptions options;
    check_throws_as(charts::ui::create_infographic_frame_with_plot(
                        box, options, {}), std::invalid_argument);
    options.width = -2.0f;
    check_throws_as(charts::ui::create_infographic_frame_with_plot(
                        box, options, std::make_unique<ProbePlotWidget>()),
                    std::invalid_argument);
    options.width = 600.0f;
    options.accessible_label.clear();
    check_throws_as(charts::ui::create_infographic_frame_with_plot(
                        box, options, std::make_unique<ProbePlotWidget>()),
                    std::invalid_argument);
    check_equal(box.root(), page);
    check_equal(page->child_count(), 0U);
    check_equal(box.owned_element_count(), before);
  }
}
