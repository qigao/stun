#include <tinytest.h>
#undef group

#include "test_support.h"
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/badge_widget.h>
#include <flexUI/widgets/breadcrumb_widget.h>
#include <flexUI/widgets/card_widget.h>
#include <flexUI/widgets/checkbox_widget.h>
#include <flexUI/widgets/divider_widget.h>
#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/widgets/dialog_widget.h>
#include <flexUI/widgets/group_button_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/menu_widget.h>
#include <flexUI/widgets/modal_widget.h>
#include <flexUI/widgets/notification_widget.h>
#include <flexUI/widgets/pagination_widget.h>
#include <flexUI/widgets/popover_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/radio_widget.h>
#include <flexUI/widgets/searchbox_widget.h>
#include <flexUI/widgets/select_widget.h>
#include <flexUI/widgets/sidebar_widget.h>
#include <flexUI/widgets/scrollview_widget.h>
#include <flexUI/widgets/spinner_widget.h>
#include <flexUI/widgets/switch_widget.h>
#include <flexUI/widgets/tabs_widget.h>
#include <flexUI/widgets/table_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <flexUI/widgets/toast_widget.h>
#include <flexUI/widgets/toolbar_widget.h>
#include <flexUI/widgets/tooltip_widget.h>
#include <flexUI/widgets/toggle_group_widget.h>
#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/widgets/avatar_widget.h>
#include <flexUI/widgets/calendar_widget.h>
#include <flexUI/widgets/datepicker_widget.h>
#include <flexUI/widgets/listview_widget.h>
#include <flexUI/widgets/slider_widget.h>
#include <flexUI/widgets/tree_widget.h>
#include <flex/runtime/renderer.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <iostream>

using namespace flexUI;
using json = nlohmann::json;

namespace {

thread_local std::string g_current_case_name;

struct DrawRectCall {
  float x;
  float y;
  float w;
  float h;
  flex::Color fill_color;
  flex::Color stroke_color;
  float stroke_width;
};

struct DrawCircleCall {
  float cx;
  float cy;
  float radius;
  flex::Color fill_color;
  flex::Color stroke_color;
  float stroke_width;
};

struct TextCall {
  std::string text;
  float x;
  float y;
  float size;
  flex::Color color;
};

struct PathCall {
  std::string path;
  flex::Color stroke_color;
  float stroke_width;
};

class RecordingRenderer final : public flex::Renderer {
public:
  void begin_frame(float width, float height, float) override {
    viewport_ = {0.0f, 0.0f, width, height};
    tx_ = 0.0f;
    ty_ = 0.0f;
    rects.clear();
    circles.clear();
    texts.clear();
    paths.clear();
  }

  void end_frame() override {}
  void set_retained_mode(bool enabled) override { retained_mode_ = enabled; }
  void save() override {}
  void restore() override {}
  void reset() override {
    tx_ = 0.0f;
    ty_ = 0.0f;
  }

  void set_transform(const flex::Transform& transform) override {
    tx_ = flex::tx(transform);
    ty_ = flex::ty(transform);
  }

  void translate(float x, float y) override {
    tx_ += x;
    ty_ += y;
  }

  void rotate(float) override {}
  void scale(float, float) override {}
  void clip_rect(float, float, float, float) override {}
  void reset_clip() override {}
  void set_global_alpha(float) override {}
  void set_shadow(const flex::Shadow&) override {}
  void clear_shadow() override {}
  void set_blur(const flex::BlurFilter&) override {}
  void clear_blur() override {}
  void fill_path(const std::string&, const flex::Paint&) override {}
  void stroke_path(const std::string& path, const flex::Paint& stroke,
                   float stroke_width) override {
    paths.push_back(
        {path, stroke.type == flex::Paint::Type::Solid ? stroke.color
                                                       : flex::Color{},
         stroke_width});
  }
  void draw_line(float, float, float, float, const flex::Paint&, float) override {}

  void draw_rect(float x, float y, float w, float h, float, const flex::Paint& fill,
                 const flex::Paint& stroke, float stroke_width) override {
    rects.push_back(
        {x + tx_, y + ty_, w, h,
         fill.type == flex::Paint::Type::Solid ? fill.color : flex::Color{},
         stroke.type == flex::Paint::Type::Solid ? stroke.color : flex::Color{},
         stroke_width});
  }

  void draw_circle(float cx, float cy, float radius, const flex::Paint& fill,
                   const flex::Paint& stroke, float stroke_width) override {
    circles.push_back(
        {cx + tx_, cy + ty_, radius,
         fill.type == flex::Paint::Type::Solid ? fill.color : flex::Color{},
         stroke.type == flex::Paint::Type::Solid ? stroke.color : flex::Color{},
         stroke_width});
  }
  void draw_ellipse(float, float, float, float, const flex::Paint&,
                    const flex::Paint&, float) override {}
  void draw_text(const std::string& text, float x, float y, const std::string&,
                 float size, bool, const flex::Color& color) override {
    texts.push_back({text, x + tx_, y + ty_, size, color});
  }
  void draw_image(const std::string&, float, float, float, float) override {}
  void draw_svg(const std::string&, float, float, float, float) override {}
  void draw_svg_data(const std::string&, float, float, float, float) override {}
  void clear(const flex::Color&) override {}
  flex::Bounds viewport() const override { return viewport_; }
  flex::RendererCapabilities capabilities() const override { return capabilities_; }
  bool supports_retained_mode() const override { return false; }
  void remove_cached(flex::PaintHandle) override {}
  flex::PaintHandle push_rect(float, float, float, float, float, const flex::Paint&,
                              const flex::Paint&, float, const flex::Transform&,
                              float) override {
    return nullptr;
  }
  flex::PaintHandle push_circle(float, float, float, const flex::Paint&,
                                const flex::Paint&, float,
                                const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_ellipse(float, float, float, float, const flex::Paint&,
                                 const flex::Paint&, float,
                                 const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_polygon(int, float, const flex::Paint&,
                                 const flex::Paint&, float,
                                 const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_path(const std::string&, const flex::Paint&,
                              const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return nullptr;
  }
  flex::PaintHandle push_star(int, float, float, const flex::Paint&,
                              const flex::Paint&, float,
                              const flex::Transform&, float) override {
    return nullptr;
  }
  void update_transform(flex::PaintHandle, const flex::Transform&) override {}

  std::vector<DrawRectCall> rects;
  std::vector<DrawCircleCall> circles;
  std::vector<TextCall> texts;
  std::vector<PathCall> paths;

private:
  bool retained_mode_ = false;
  float tx_ = 0.0f;
  float ty_ = 0.0f;
  flex::Bounds viewport_{};
  flex::RendererCapabilities capabilities_{};
};

bool color_matches(const flex::Color& actual, const json& expected) {
  return approx_eq(actual.r, expected[0].get<float>(), 0.001f) &&
         approx_eq(actual.g, expected[1].get<float>(), 0.001f) &&
         approx_eq(actual.b, expected[2].get<float>(), 0.001f) &&
         approx_eq(actual.a, expected[3].get<float>(), 0.001f);
}

ToastWidget::Type parse_toast_type(const std::string& value) {
  if (value == "success") {
    return ToastWidget::Type::Success;
  }
  if (value == "error") {
    return ToastWidget::Type::Error;
  }
  if (value == "warning") {
    return ToastWidget::Type::Warning;
  }
  if (value == "info") {
    return ToastWidget::Type::Info;
  }
  return ToastWidget::Type::Default;
}

TooltipWidget::Position parse_tooltip_position(const std::string& value) {
  if (value == "bottom") {
    return TooltipWidget::Position::Bottom;
  }
  if (value == "left") {
    return TooltipWidget::Position::Left;
  }
  if (value == "right") {
    return TooltipWidget::Position::Right;
  }
  return TooltipWidget::Position::Top;
}

PopoverWidget::Position parse_popover_position(const std::string& value) {
  if (value == "top") {
    return PopoverWidget::Position::Top;
  }
  if (value == "left") {
    return PopoverWidget::Position::Left;
  }
  if (value == "right") {
    return PopoverWidget::Position::Right;
  }
  return PopoverWidget::Position::Bottom;
}

NotificationWidget::Type parse_notification_type(const std::string& value) {
  if (value == "success") {
    return NotificationWidget::Type::Success;
  }
  if (value == "warning") {
    return NotificationWidget::Type::Warning;
  }
  if (value == "error") {
    return NotificationWidget::Type::Error;
  }
  return NotificationWidget::Type::Info;
}

NotificationWidget::Position parse_notification_position(
    const std::string& value) {
  if (value == "top-left") {
    return NotificationWidget::Position::TopLeft;
  }
  if (value == "bottom-right") {
    return NotificationWidget::Position::BottomRight;
  }
  if (value == "bottom-left") {
    return NotificationWidget::Position::BottomLeft;
  }
  return NotificationWidget::Position::TopRight;
}

KeyCode parse_key_code(const std::string& value) {
  if (value == "enter") {
    return KeyCode::Enter;
  }
  if (value == "escape") {
    return KeyCode::Escape;
  }
  if (value == "tab") {
    return KeyCode::Tab;
  }
  if (value == "backspace") {
    return KeyCode::Backspace;
  }
  if (value == "delete") {
    return KeyCode::Delete;
  }
  if (value == "up") {
    return KeyCode::Up;
  }
  if (value == "down") {
    return KeyCode::Down;
  }
  if (value == "left") {
    return KeyCode::Left;
  }
  if (value == "right") {
    return KeyCode::Right;
  }
  if (value == "home") {
    return KeyCode::Home;
  }
  if (value == "end") {
    return KeyCode::End;
  }
  return KeyCode::Unknown;
}

std::string fixture_dir() {
  return std::string(FLEXUI_TEST_SOURCE_DIR) + "/shadcn/manifests";
}

std::string env_filter(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr ? std::string(value) : std::string{};
}

bool env_flag_enabled(const char* name) {
  const std::string value = env_filter(name);
  return !value.empty() && value != "0" && value != "false" &&
         value != "FALSE";
}

bool matches_optional_filter(const std::string& value, const std::string& filter) {
  return filter.empty() || value.find(filter) != std::string::npos;
}

std::vector<json> load_manifests() {
  namespace fs = std::filesystem;
  const fs::path dir(fixture_dir());
  const std::string manifest_filter = env_filter("FLEXUI_SHADCN_MANIFEST_FILTER");
  if (!fs::exists(dir) || !fs::is_directory(dir)) {
    throw std::runtime_error("unable to open shadcn manifest directory");
  }

  std::vector<fs::path> paths;
  for (const auto& entry : fs::directory_iterator(dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto name = entry.path().filename().string();
    if (entry.path().extension() == ".json" &&
        name.find("_manifest.json") != std::string::npos &&
        matches_optional_filter(name, manifest_filter)) {
      paths.push_back(entry.path());
    }
  }
  std::sort(paths.begin(), paths.end());

  std::vector<json> manifests;
  manifests.reserve(paths.size());
  for (const auto& path : paths) {
    std::ifstream input(path);
    if (!input) {
      throw std::runtime_error("unable to open shadcn manifest: " +
                               path.string());
    }
    json manifest;
    input >> manifest;
    manifests.push_back(std::move(manifest));
  }
  return manifests;
}

std::vector<json> flatten_cases(const std::vector<json>& manifests) {
  const std::string case_filter = env_filter("FLEXUI_SHADCN_CASE_FILTER");
  std::vector<json> cases;
  for (const auto& manifest : manifests) {
    for (const auto& spec : manifest.at("cases")) {
      const std::string name = spec.at("name").get<std::string>();
      if (matches_optional_filter(name, case_filter)) {
        cases.push_back(spec);
      }
    }
  }
  return cases;
}

void require_condition(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void require_color(const Color& actual, const json& expected) {
  require_condition(expected.is_array(),
                    "color expectation must be an array");
  require_condition(expected.size() == 4,
                    "color expectation must contain 4 channels");
  const float expected_r = expected[0].get<float>();
  const float expected_g = expected[1].get<float>();
  const float expected_b = expected[2].get<float>();
  const float expected_a = expected[3].get<float>();
  if (!approx_eq(actual.r, expected_r, 0.001f) ||
      !approx_eq(actual.g, expected_g, 0.001f) ||
      !approx_eq(actual.b, expected_b, 0.001f) ||
      !approx_eq(actual.a, expected_a, 0.001f)) {
    std::ostringstream oss;
    oss << "color expectation failed";
    if (!g_current_case_name.empty()) {
      oss << " in case '" << g_current_case_name << "'";
    }
    oss << "; actual=(" << actual.r << "," << actual.g << "," << actual.b
        << "," << actual.a << ") expected=(" << expected_r << ","
        << expected_g << "," << expected_b << "," << expected_a << ")";
    throw std::runtime_error(oss.str());
  }
}

void require_variable_color(const ComputedStyle& style, const char* name,
                            const json& expected) {
  require_color(style.get_variable_color(Symbol(name), {}), expected);
}

Element* build_target(Box& box, const json& spec) {
  const std::string widget = spec.at("widget").get<std::string>();
  const std::string tag = spec.value("tag", "div");
  const std::string id = spec.at("id").get<std::string>();
  const json widget_props = spec.value("widget_props", json::object());

  if (widget == "button") {
    auto* elem = box.create_widget<ButtonWidget>(tag, id,
                                                widget_props.value("text", ""));
    auto* button = static_cast<ButtonWidget*>(elem->widget);
    if (widget_props.contains("text")) {
      button->set_text(widget_props.at("text").get<std::string>());
    }
    if (widget_props.contains("disabled")) {
      button->set_disabled(widget_props.at("disabled").get<bool>());
    }
    if (widget_props.contains("loading")) {
      button->set_loading(widget_props.at("loading").get<bool>());
    }
    if (widget_props.contains("variant")) {
      const auto variant = widget_props.at("variant").get<std::string>();
      if (variant == "secondary") {
        button->set_variant(ButtonWidget::Variant::Secondary);
      } else if (variant == "outline") {
        button->set_variant(ButtonWidget::Variant::Outline);
      } else if (variant == "ghost") {
        button->set_variant(ButtonWidget::Variant::Ghost);
      } else if (variant == "destructive") {
        button->set_variant(ButtonWidget::Variant::Destructive);
      } else {
        button->set_variant(ButtonWidget::Variant::Default);
      }
    }
    if (widget_props.contains("size")) {
      const auto size = widget_props.at("size").get<std::string>();
      if (size == "sm") {
        button->set_size(ButtonWidget::Size::Small);
      } else if (size == "lg") {
        button->set_size(ButtonWidget::Size::Large);
      } else if (size == "icon") {
        button->set_size(ButtonWidget::Size::Icon);
      } else {
        button->set_size(ButtonWidget::Size::Default);
      }
    }
    return elem;
  }

  if (widget == "badge") {
    auto* elem = box.create_widget<BadgeWidget>(tag, id,
                                                widget_props.value("text", ""));
    auto* badge = static_cast<BadgeWidget*>(elem->widget);
    if (widget_props.contains("count")) {
      badge->set_count(widget_props.at("count").get<int>());
    }
    if (widget_props.contains("dot")) {
      badge->set_dot(widget_props.at("dot").get<bool>());
    }
    if (widget_props.contains("visible") &&
        !widget_props.at("visible").get<bool>()) {
      badge->hide();
    }
    return elem;
  }

  if (widget == "card") {
    auto* elem = box.create_widget<CardWidget>(tag, id);
    auto* card = static_cast<CardWidget*>(elem->widget);
    if (widget_props.contains("title")) {
      card->set_title(widget_props.at("title").get<std::string>());
    }
    if (widget_props.contains("subtitle")) {
      card->set_subtitle(widget_props.at("subtitle").get<std::string>());
    }
    if (widget_props.contains("has_header")) {
      card->set_has_header(widget_props.at("has_header").get<bool>());
    }
    if (widget_props.contains("has_footer")) {
      card->set_has_footer(widget_props.at("has_footer").get<bool>());
    }
    if (widget_props.contains("header_height")) {
      card->set_header_height(widget_props.at("header_height").get<float>());
    }
    if (widget_props.contains("footer_height")) {
      card->set_footer_height(widget_props.at("footer_height").get<float>());
    }
    return elem;
  }

  if (widget == "group-button") {
    auto* elem = box.create_widget<GroupButtonWidget>(tag, id);
    auto* button = static_cast<GroupButtonWidget*>(elem->widget);
    if (widget_props.contains("text")) {
      button->set_label(widget_props.at("text").get<std::string>());
    }
    return elem;
  }

  if (widget == "avatar") {
    auto* elem = box.create_widget<AvatarWidget>(
        tag, id, widget_props.value("name", std::string("")),
        widget_props.value("image_url", std::string("")));
    auto* avatar = static_cast<AvatarWidget*>(elem->widget);
    if (widget_props.contains("shape")) {
      const auto shape = widget_props.at("shape").get<std::string>();
      if (shape == "square") {
        avatar->set_avatar_shape(AvatarWidget::AvatarShape::Square);
      } else if (shape == "rounded") {
        avatar->set_avatar_shape(AvatarWidget::AvatarShape::Rounded);
      } else {
        avatar->set_avatar_shape(AvatarWidget::AvatarShape::Circle);
      }
    }
    if (widget_props.contains("status")) {
      const auto status = widget_props.at("status").get<std::string>();
      if (status == "online") {
        avatar->set_status(AvatarWidget::Status::Online);
      } else if (status == "offline") {
        avatar->set_status(AvatarWidget::Status::Offline);
      } else if (status == "away") {
        avatar->set_status(AvatarWidget::Status::Away);
      } else if (status == "busy") {
        avatar->set_status(AvatarWidget::Status::Busy);
      }
    }
    return elem;
  }

  if (widget == "calendar") {
    auto* elem = box.create_widget<CalendarWidget>(tag, id);
    auto* calendar = static_cast<CalendarWidget*>(elem->widget);
    if (widget_props.contains("selected")) {
      const auto& selected = widget_props.at("selected");
      calendar->set_selected_date(
          {selected.at("year").get<int>(), selected.at("month").get<int>(),
           selected.at("day").get<int>()});
    }
    if (widget_props.contains("view")) {
      const auto& view = widget_props.at("view");
      calendar->set_view_date(
          {view.at("year").get<int>(), view.at("month").get<int>(),
           view.value("day", 1)});
    }
    return elem;
  }

  if (widget == "input") {
    const std::string placeholder = widget_props.value("placeholder", "");
    const bool password = widget_props.value("password", false);
    auto* elem = box.create_widget<InputWidget>(tag, id, placeholder, password);
    auto* input = static_cast<InputWidget*>(elem->widget);
    input->set_placeholder(placeholder);
    if (widget_props.contains("text")) {
      input->set_text(widget_props.at("text").get<std::string>());
    }
    if (widget_props.contains("readonly")) {
      input->set_readonly(widget_props.at("readonly").get<bool>());
    }
    if (widget_props.contains("disabled")) {
      input->set_disabled(widget_props.at("disabled").get<bool>());
    }
    return elem;
  }

  if (widget == "searchbox") {
    auto* elem = box.create_widget<SearchBoxWidget>(
        tag, id, widget_props.value("placeholder", std::string("Search...")));
    auto* search = static_cast<SearchBoxWidget*>(elem->widget);
    for (const auto& suggestion :
         widget_props.value("suggestions", json::array())) {
      search->add_suggestion(suggestion.at("id").get<std::string>(),
                             suggestion.at("text").get<std::string>(),
                             suggestion.value("description", std::string("")));
    }
    if (widget_props.contains("text")) {
      search->set_text(widget_props.at("text").get<std::string>());
    }
    if (widget_props.contains("focused")) {
      search->set_focused(widget_props.at("focused").get<bool>());
    }
    if (widget_props.contains("open")) {
      search->set_dropdown_open(widget_props.at("open").get<bool>());
    } else if (widget_props.contains("dropdown_open")) {
      search->set_dropdown_open(widget_props.at("dropdown_open").get<bool>());
    }
    return elem;
  }

  if (widget == "select") {
    const auto options =
        widget_props.value("options", std::vector<std::string>{});
    const int selected_index = widget_props.value("selected_index", -1);
    auto* elem =
        box.create_widget<SelectWidget>(tag, id, options, selected_index);
    auto* select = static_cast<SelectWidget*>(elem->widget);
    select->set_selected_index(selected_index);
    if (widget_props.contains("expanded")) {
      select->set_expanded(widget_props.at("expanded").get<bool>());
    }
    return elem;
  }

  if (widget == "checkbox") {
    const std::string label = widget_props.value("label", "");
    const bool checked = widget_props.value("checked", false);
    auto* elem = box.create_widget<CheckboxWidget>(tag, id, label, checked);
    auto* checkbox = static_cast<CheckboxWidget*>(elem->widget);
    checkbox->set_checked(checked);
    if (widget_props.contains("disabled")) {
      checkbox->set_disabled(widget_props.at("disabled").get<bool>());
    }
    return elem;
  }

  if (widget == "switch") {
    const std::string label = widget_props.value("label", "");
    const bool checked = widget_props.value("checked", false);
    auto* elem = box.create_widget<SwitchWidget>(tag, id, label, checked);
    auto* toggle = static_cast<SwitchWidget*>(elem->widget);
    toggle->set_checked(checked);
    if (widget_props.contains("disabled")) {
      toggle->set_disabled(widget_props.at("disabled").get<bool>());
    }
    return elem;
  }

  if (widget == "tabs") {
    auto* elem = box.create_widget<TabsWidget>(tag, id);
    auto* tabs = static_cast<TabsWidget*>(elem->widget);
    for (const auto& tab : widget_props.at("tabs")) {
      tabs->add_tab(tab.at("label").get<std::string>(),
                    tab.at("id").get<std::string>(), nullptr,
                    tab.value("disabled", false));
    }
    if (widget_props.contains("active_id")) {
      tabs->set_active_id(widget_props.at("active_id").get<std::string>());
    } else if (widget_props.contains("active_index")) {
      tabs->set_active_index(widget_props.at("active_index").get<int>());
    }
    return elem;
  }

  if (widget == "breadcrumb") {
    auto* elem = box.create_widget<BreadcrumbWidget>(tag, id);
    auto* breadcrumb = static_cast<BreadcrumbWidget*>(elem->widget);
    std::vector<BreadcrumbWidget::Item> items;
    for (const auto& item : widget_props.value("items", json::array())) {
      items.push_back({item.at("id").get<std::string>(),
                       item.at("label").get<std::string>()});
    }
    breadcrumb->set_items(items);
    if (widget_props.contains("separator")) {
      breadcrumb->set_separator(widget_props.at("separator").get<std::string>());
    }
    return elem;
  }

  if (widget == "modal") {
    const std::string title = widget_props.value("title", "");
    auto* elem = box.create_widget<ModalWidget>(tag, id, title);
    auto* modal = static_cast<ModalWidget*>(elem->widget);
    const std::string side = widget_props.value("side", std::string("center"));
    if (side == "left") {
      modal->set_side(ModalWidget::Side::Left);
    } else if (side == "right") {
      modal->set_side(ModalWidget::Side::Right);
    } else if (side == "top") {
      modal->set_side(ModalWidget::Side::Top);
    } else if (side == "bottom") {
      modal->set_side(ModalWidget::Side::Bottom);
    }
    if (widget_props.contains("show_close_button")) {
      modal->set_show_close_button(
          widget_props.at("show_close_button").get<bool>());
    }
    if (widget_props.contains("close_on_overlay")) {
      modal->set_close_on_overlay(
          widget_props.at("close_on_overlay").get<bool>());
    }
    if (widget_props.value("open", false)) {
      modal->open();
    } else {
      modal->close();
    }
    return elem;
  }

  if (widget == "dialog") {
    const std::string title = widget_props.value("title", std::string("Dialog"));
    const auto type_name = widget_props.value("type", std::string("info"));
    DialogWidget::Type type = DialogWidget::Type::Info;
    if (type_name == "confirm") {
      type = DialogWidget::Type::Confirm;
    } else if (type_name == "warning") {
      type = DialogWidget::Type::Warning;
    } else if (type_name == "error") {
      type = DialogWidget::Type::Error;
    } else if (type_name == "custom") {
      type = DialogWidget::Type::Custom;
    }
    auto* elem = box.create_widget<DialogWidget>(tag, id, title, type);
    auto* dialog = static_cast<DialogWidget*>(elem->widget);
    if (widget_props.contains("message")) {
      dialog->set_message(widget_props.at("message").get<std::string>());
    }
    if (widget_props.contains("width") || widget_props.contains("height")) {
      dialog->set_size(widget_props.value("width", 0.0f),
                       widget_props.value("height", 0.0f));
    }
    if (widget_props.value("alert_role", false)) {
      dialog->set_alert_role(true);
    }
    if (widget_props.contains("buttons")) {
      std::vector<DialogWidget::Button> buttons;
      for (const auto& button : widget_props.at("buttons")) {
        buttons.push_back({button.at("id").get<std::string>(),
                           button.at("label").get<std::string>(),
                           button.value("primary", false)});
      }
      dialog->set_buttons(std::move(buttons));
    }
    if (widget_props.value("open", false)) {
      dialog->show();
    } else {
      dialog->hide();
    }
    return elem;
  }

  if (widget == "popover") {
    const std::string content = widget_props.value("content", "");
    auto* elem = box.create_widget<PopoverWidget>(tag, id, content);
    auto* popover = static_cast<PopoverWidget*>(elem->widget);
    if (widget_props.contains("title")) {
      popover->set_title(widget_props.at("title").get<std::string>());
    }
    if (widget_props.contains("position")) {
      popover->set_position(parse_popover_position(
          widget_props.at("position").get<std::string>()));
    }
    if (widget_props.contains("width") || widget_props.contains("height")) {
      popover->set_size(widget_props.value("width", 0.0f),
                        widget_props.value("height", 0.0f));
    }
    if ((widget_props.contains("show") && widget_props.at("show").get<bool>()) ||
        (widget_props.contains("open") && widget_props.at("open").get<bool>())) {
      popover->show();
    }
    return elem;
  }

  if (widget == "datepicker") {
    Date initial{2024, 1, 1};
    if (widget_props.contains("date")) {
      const auto& date = widget_props.at("date");
      initial = {date.at("year").get<int>(), date.at("month").get<int>(),
                 date.at("day").get<int>()};
    }
    auto* elem = box.create_widget<DatePickerWidget>(tag, id, initial);
    auto* datepicker = static_cast<DatePickerWidget*>(elem->widget);
    if (widget_props.value("open", false)) {
      datepicker->set_open(true);
    }
    return elem;
  }

  if (widget == "progress") {
    auto* elem = box.create_widget<ProgressBarWidget>(
        tag, id, widget_props.value("value", 0.0f),
        widget_props.value("indeterminate", false));
    auto* progress = static_cast<ProgressBarWidget*>(elem->widget);
    progress->set_value(widget_props.value("value", 0.0f));
    progress->set_indeterminate(widget_props.value("indeterminate", false));
    return elem;
  }

  if (widget == "slider") {
    auto* elem = box.create_widget<SliderWidget>(
        tag, id, widget_props.value("min", 0.0f),
        widget_props.value("max", 100.0f), widget_props.value("value", 50.0f),
        widget_props.value("step", 0.0f));
    auto* slider = static_cast<SliderWidget*>(elem->widget);
    slider->set_value(widget_props.value("value", 50.0f));
    slider->set_disabled(widget_props.value("disabled", false));
    return elem;
  }

  if (widget == "spinner") {
    auto variant = SpinnerWidget::Variant::Ring;
    const std::string variant_name =
        widget_props.value("variant", std::string("ring"));
    if (variant_name == "dots") {
      variant = SpinnerWidget::Variant::Dots;
    } else if (variant_name == "bars") {
      variant = SpinnerWidget::Variant::Bars;
    }
    auto* elem = box.create_widget<SpinnerWidget>(tag, id, variant);
    auto* spinner = static_cast<SpinnerWidget*>(elem->widget);
    if (widget_props.contains("spinning") &&
        !widget_props.at("spinning").get<bool>()) {
      spinner->stop();
    }
    return elem;
  }

  if (widget == "scrollview") {
    const auto h_policy_name =
        widget_props.value("h_policy", std::string("auto"));
    const auto v_policy_name =
        widget_props.value("v_policy", std::string("auto"));
    auto parse_policy = [](const std::string& name) {
      if (name == "always") {
        return ScrollViewWidget::Policy::Always;
      }
      if (name == "never") {
        return ScrollViewWidget::Policy::Never;
      }
      return ScrollViewWidget::Policy::Auto;
    };
    auto* elem = box.create_widget<ScrollViewWidget>(
        tag, id, parse_policy(h_policy_name), parse_policy(v_policy_name));
    auto* content = box.create("div", id + "-content");
    elem->append(content);
    return elem;
  }

  if (widget == "listview") {
    auto* elem = box.create_widget<ListViewWidget>(tag, id);
    auto* list = static_cast<ListViewWidget*>(elem->widget);
    list->set_multi_select(widget_props.value("multi_select", false));
    for (const auto& item : widget_props.value("items", json::array())) {
      list->add_item(item.at("id").get<std::string>(),
                     item.at("text").get<std::string>(),
                     item.value("secondary", std::string("")));
    }
    if (widget_props.contains("selected_index")) {
      const int index = widget_props.at("selected_index").get<int>();
      if (index >= 0 &&
          index < static_cast<int>(list->items().size())) {
        list->items()[static_cast<size_t>(index)].selected = true;
      }
    }
    if (widget_props.contains("selected_indices")) {
      for (const auto& index_value : widget_props.at("selected_indices")) {
        const int index = index_value.get<int>();
        if (index >= 0 &&
            index < static_cast<int>(list->items().size())) {
          list->items()[static_cast<size_t>(index)].selected = true;
        }
      }
    }
    return elem;
  }

  if (widget == "dropdown") {
    const std::string placeholder = widget_props.value("placeholder", "Select...");
    auto* elem = box.create_widget<DropdownWidget>(tag, id, placeholder);
    auto* dropdown = static_cast<DropdownWidget*>(elem->widget);
    for (const auto& option : widget_props.value("options", json::array())) {
      dropdown->add_option(option.at("label").get<std::string>(),
                           option.at("value").get<std::string>(),
                           option.value("disabled", false));
    }
    if (widget_props.contains("selected_value")) {
      dropdown->set_selected_value(
          widget_props.at("selected_value").get<std::string>());
    } else if (widget_props.contains("selected_index")) {
      dropdown->set_selected_index(widget_props.at("selected_index").get<int>());
    }
    if (widget_props.value("open", false)) {
      dropdown->open();
    }
    return elem;
  }

  if (widget == "menu") {
    auto* elem = box.create_widget<MenuWidget>(tag, id);
    auto* menu = static_cast<MenuWidget*>(elem->widget);
    for (const auto& item : widget_props.value("items", json::array())) {
      if (item.value("separator", false)) {
        menu->add_separator();
        continue;
      }
      if (item.contains("children")) {
        std::vector<MenuWidget::MenuItem> children;
        for (const auto& child : item.at("children")) {
          children.push_back({child.at("id").get<std::string>(),
                              child.at("label").get<std::string>(),
                              child.value("shortcut", std::string("")),
                              child.value("enabled", true),
                              child.value("separator", false),
                              child.value("checked", false),
                              {},
                              nullptr});
        }
        menu->add_submenu(item.at("id").get<std::string>(),
                          item.at("label").get<std::string>(),
                          std::move(children));
      } else {
        menu->add_item(item.at("id").get<std::string>(),
                       item.at("label").get<std::string>(), nullptr,
                       item.value("shortcut", std::string("")));
      }
    }
    if (widget_props.value("show", false)) {
      menu->show(widget_props.value("x", 0.0f), widget_props.value("y", 0.0f));
    }
    return elem;
  }

  if (widget == "tooltip") {
    const std::string text = widget_props.value("text", "");
    auto* elem = box.create_widget<TooltipWidget>(tag, id, text);
    auto* tooltip = static_cast<TooltipWidget*>(elem->widget);
    tooltip->set_position(
        parse_tooltip_position(widget_props.value("position", "top")));
    if (widget_props.value("show", false)) {
      tooltip->show();
    } else if (widget_props.value("hide", false)) {
      tooltip->hide();
    }
    return elem;
  }

  if (widget == "notification") {
    auto* elem = box.create_widget<NotificationWidget>(
        tag, id,
        parse_notification_position(
            widget_props.value("position", std::string("top-right"))));
    auto* notifications = static_cast<NotificationWidget*>(elem->widget);
    for (const auto& notification :
         widget_props.value("notifications", json::array())) {
      notifications->notify(
          notification.at("title").get<std::string>(),
          notification.at("message").get<std::string>(),
          parse_notification_type(
              notification.value("type", std::string("info"))),
          notification.value("duration", 5000.0f));
    }
    return elem;
  }

  if (widget == "toolbar") {
    auto* elem = box.create_widget<ToolbarWidget>(tag, id);
    auto* toolbar = static_cast<ToolbarWidget*>(elem->widget);
    for (const auto& item : widget_props.value("items", json::array())) {
      const std::string type = item.value("type", std::string("button"));
      if (type == "separator") {
        toolbar->add_separator();
        continue;
      }
      if (type == "toggle") {
        toolbar->add_toggle(item.at("id").get<std::string>(),
                            item.at("icon").get<std::string>(),
                            item.value("toggled", false),
                            item.value("tooltip", std::string("")));
        if (item.contains("enabled")) {
          toolbar->set_enabled(item.at("id").get<std::string>(),
                               item.at("enabled").get<bool>());
        }
        continue;
      }
      if (type == "dropdown") {
        std::vector<std::pair<std::string, std::string>> dropdown_items;
        for (const auto& dropdown_item :
             item.value("items", json::array())) {
          dropdown_items.emplace_back(
              dropdown_item.at("id").get<std::string>(),
              dropdown_item.at("label").get<std::string>());
        }
        toolbar->add_dropdown(item.at("id").get<std::string>(),
                              item.at("icon").get<std::string>(),
                              std::move(dropdown_items),
                              item.value("tooltip", std::string("")));
        if (item.contains("enabled")) {
          toolbar->set_enabled(item.at("id").get<std::string>(),
                               item.at("enabled").get<bool>());
        }
        continue;
      }
      toolbar->add_button(item.at("id").get<std::string>(),
                          item.at("icon").get<std::string>(), [] {},
                          item.value("tooltip", std::string("")));
      if (item.contains("enabled")) {
        toolbar->set_enabled(item.at("id").get<std::string>(),
                             item.at("enabled").get<bool>());
      }
    }
    if (widget_props.contains("open_dropdown")) {
      toolbar->set_open_dropdown(
          widget_props.at("open_dropdown").get<std::string>());
    }
    return elem;
  }

  if (widget == "toast") {
    const std::string message = widget_props.value("message", "");
    const auto type =
        parse_toast_type(widget_props.value("type", std::string("default")));
    auto* elem = box.create_widget<ToastWidget>(tag, id, message, type);
    auto* toast = static_cast<ToastWidget*>(elem->widget);
    if (widget_props.contains("duration")) {
      toast->set_duration(widget_props.at("duration").get<float>());
    }
    if (widget_props.value("show", false)) {
      toast->show();
    } else if (widget_props.value("hide", false)) {
      toast->hide();
    }
    return elem;
  }

  if (widget == "accordion") {
    auto* elem = box.create_widget<AccordionWidget>(tag, id);
    auto* accordion = static_cast<AccordionWidget*>(elem->widget);
    accordion->set_allow_multiple(widget_props.value("allow_multiple", false));
    for (const auto& section : widget_props.value("sections", json::array())) {
      accordion->add_section(section.at("title").get<std::string>(),
                             section.at("id").get<std::string>(),
                             section.value("content_height", 100.0f));
    }
    for (const auto& expanded_id :
         widget_props.value("expanded_ids", json::array())) {
      accordion->expand(expanded_id.get<std::string>());
    }
    if (widget_props.contains("expanded_id")) {
      accordion->expand(widget_props.at("expanded_id").get<std::string>());
    }
    return elem;
  }

  if (widget == "divider") {
    auto* elem = box.create_widget<DividerWidget>(
        tag, id,
        widget_props.value("orientation", std::string("horizontal")) ==
                "vertical"
            ? DividerWidget::Orientation::Vertical
            : DividerWidget::Orientation::Horizontal);
    auto* divider = static_cast<DividerWidget*>(elem->widget);
    if (widget_props.contains("label")) {
      divider->set_label(widget_props.at("label").get<std::string>());
    }
    return elem;
  }

  if (widget == "toggle-group") {
    std::vector<ToggleGroupWidget::Option> options;
    for (const auto& option : widget_props.value("options", json::array())) {
      options.push_back(
          {option.at("id").get<std::string>(),
           option.at("label").get<std::string>()});
    }
    auto* elem = box.create_widget<ToggleGroupWidget>(tag, id, options);
    auto* toggle = static_cast<ToggleGroupWidget*>(elem->widget);
    toggle->set_multi_select(widget_props.value("multi_select", false));
    if (widget_props.contains("selected_indices")) {
      toggle->set_selected_indices(
          widget_props.at("selected_indices").get<std::vector<int>>());
    } else if (widget_props.contains("selected_index")) {
      toggle->set_selected_index(widget_props.at("selected_index").get<int>());
    }
    return elem;
  }

  if (widget == "radio") {
    const std::string label = widget_props.value("label", "");
    const std::string value = widget_props.value("value", "");
    const std::string group = widget_props.value("group", "");
    const bool checked = widget_props.value("checked", false);
    auto* elem =
        box.create_widget<RadioWidget>(tag, id, label, value, group, checked);
    auto* radio = static_cast<RadioWidget*>(elem->widget);
    radio->set_checked(checked);
    if (widget_props.contains("disabled")) {
      radio->set_disabled(widget_props.at("disabled").get<bool>());
    }
    return elem;
  }

  if (widget == "pagination") {
    const int total_pages = widget_props.value("total_pages", 1);
    const int current_page = widget_props.value("current_page", 1);
    auto* elem =
        box.create_widget<PaginationWidget>(tag, id, total_pages, current_page);
    auto* pagination = static_cast<PaginationWidget*>(elem->widget);
    if (widget_props.contains("visible_pages")) {
      pagination->set_visible_pages(
          widget_props.at("visible_pages").get<int>());
    }
    if (widget_props.contains("show_prev_next")) {
      pagination->set_show_prev_next(
          widget_props.at("show_prev_next").get<bool>());
    }
    return elem;
  }

  if (widget == "sidebar") {
    auto* elem = box.create_widget<SidebarWidget>(tag, id);
    auto* sidebar = static_cast<SidebarWidget*>(elem->widget);
    for (const auto& section : widget_props.value("sections", json::array())) {
      sidebar->add_section(section.value("title", std::string("")));
      for (const auto& item : section.value("items", json::array())) {
        sidebar->add_item(item.at("id").get<std::string>(),
                          item.value("icon", std::string("")),
                          item.at("label").get<std::string>(),
                          item.value("badge", std::string("")));
      }
    }
    if (widget_props.contains("selected_id")) {
      sidebar->select(widget_props.at("selected_id").get<std::string>());
    }
    if (widget_props.contains("collapsed")) {
      sidebar->set_collapsed(widget_props.at("collapsed").get<bool>());
    }
    return elem;
  }

  if (widget == "textarea") {
    auto* elem = box.create_widget<TextAreaWidget>(
        tag, id, widget_props.value("text", std::string("")),
        widget_props.value("placeholder", std::string("")));
    auto* textarea = static_cast<TextAreaWidget*>(elem->widget);
    textarea->set_text(widget_props.value("text", std::string("")));
    textarea->set_placeholder(
        widget_props.value("placeholder", std::string("")));
    if (widget_props.contains("cursor_position")) {
      textarea->set_cursor_position(
          widget_props.at("cursor_position").get<int>());
    }
    if (widget_props.contains("readonly")) {
      textarea->set_readonly(widget_props.at("readonly").get<bool>());
    }
    if (widget_props.contains("disabled")) {
      textarea->set_disabled(widget_props.at("disabled").get<bool>());
    }
    return elem;
  }

  if (widget == "table") {
    auto* elem = box.create_widget<TableWidget>(tag, id);
    auto* table = static_cast<TableWidget*>(elem->widget);
    for (const auto& column : widget_props.value("columns", json::array())) {
      table->add_column(column.at("header").get<std::string>(),
                        column.value("width", 100.0f));
    }
    for (const auto& row : widget_props.value("rows", json::array())) {
      table->add_row(row.get<std::vector<std::string>>());
    }
    if (widget_props.contains("selected_row")) {
      table->set_selected_row(widget_props.at("selected_row").get<int>());
    }
    return elem;
  }

  if (widget == "tree") {
    auto* elem = box.create_widget<TreeWidget>(tag, id);
    auto* tree = static_cast<TreeWidget*>(elem->widget);
    std::unordered_map<std::string, TreeNode*> nodes;
    for (const auto& node : widget_props.value("nodes", json::array())) {
      TreeNode* parent = nullptr;
      if (node.contains("parent")) {
        const auto it = nodes.find(node.at("parent").get<std::string>());
        if (it != nodes.end()) {
          parent = it->second;
        }
      }
      auto added = tree->add_node(node.at("id").get<std::string>(),
                                  node.at("label").get<std::string>(), parent);
      nodes[added->id] = added.get();
    }
    for (const auto& expanded_id :
         widget_props.value("expanded_ids", json::array())) {
      tree->expand(expanded_id.get<std::string>());
    }
    if (widget_props.contains("selected_id")) {
      tree->set_selected(widget_props.at("selected_id").get<std::string>());
    }
    return elem;
  }

  throw std::runtime_error("unsupported widget in shadcn smoke manifest");
}

void apply_case_state(Element& elem, const json& spec) {
  for (const auto& cls : spec.value("class_tokens", json::array())) {
    elem.add_class(cls.get<std::string>().c_str());
  }

  if (spec.contains("attributes")) {
    for (auto it = spec["attributes"].begin(); it != spec["attributes"].end(); ++it) {
      elem.set_attribute(it.key(), it.value().get<std::string>());
    }
  }

  for (const auto& state : spec.value("states", json::array())) {
    elem.set_state(state.get<std::string>().c_str(), true);
  }
}

void replay_case_events(Element& elem, const json& spec) {
  if (!elem.widget) {
    return;
  }

  for (const auto& event_spec : spec.value("events", json::array())) {
    const std::string type = event_spec.at("type").get<std::string>();
    if (type == "widget_call") {
      const std::string action = event_spec.value("action", std::string(""));
      bool handled = false;

      if (auto* tooltip = dynamic_cast<TooltipWidget*>(elem.widget)) {
        if (action == "show") {
          tooltip->show();
          handled = true;
        } else if (action == "hide") {
          tooltip->hide();
          handled = true;
        }
      } else if (auto* toast = dynamic_cast<ToastWidget*>(elem.widget)) {
        if (action == "show") {
          toast->show();
          handled = true;
        } else if (action == "hide") {
          toast->hide();
          handled = true;
        }
      } else if (auto* modal = dynamic_cast<ModalWidget*>(elem.widget)) {
        if (action == "open" || action == "show") {
          modal->open();
          handled = true;
        } else if (action == "close" || action == "hide") {
          modal->close();
          handled = true;
        }
      } else if (auto* dialog = dynamic_cast<DialogWidget*>(elem.widget)) {
        if (action == "open" || action == "show") {
          dialog->show();
          handled = true;
        } else if (action == "close" || action == "hide") {
          dialog->hide();
          handled = true;
        }
      } else if (auto* popover = dynamic_cast<PopoverWidget*>(elem.widget)) {
        if (action == "show") {
          popover->show();
          handled = true;
        } else if (action == "hide") {
          popover->hide();
          handled = true;
        }
      } else if (auto* badge = dynamic_cast<BadgeWidget*>(elem.widget)) {
        if (action == "show") {
          badge->show();
          handled = true;
        } else if (action == "hide") {
          badge->hide();
          handled = true;
        } else if (action == "set-dot") {
          badge->set_dot(event_spec.value("value", true));
          handled = true;
        } else if (action == "set-count") {
          badge->set_count(event_spec.value("value", 0));
          handled = true;
        } else if (action == "set-text") {
          badge->set_text(event_spec.value("value", std::string("")));
          handled = true;
        }
      } else if (auto* avatar = dynamic_cast<AvatarWidget*>(elem.widget)) {
        if (action == "set-status") {
          const std::string value = event_spec.value("value", std::string("none"));
          if (value == "online") {
            avatar->set_status(AvatarWidget::Status::Online);
          } else if (value == "offline") {
            avatar->set_status(AvatarWidget::Status::Offline);
          } else if (value == "away") {
            avatar->set_status(AvatarWidget::Status::Away);
          } else if (value == "busy") {
            avatar->set_status(AvatarWidget::Status::Busy);
          } else {
            avatar->set_status(AvatarWidget::Status::None);
          }
          handled = true;
        } else if (action == "set-shape") {
          const std::string value = event_spec.value("value", std::string("circle"));
          if (value == "square") {
            avatar->set_avatar_shape(AvatarWidget::AvatarShape::Square);
          } else if (value == "rounded") {
            avatar->set_avatar_shape(AvatarWidget::AvatarShape::Rounded);
          } else {
            avatar->set_avatar_shape(AvatarWidget::AvatarShape::Circle);
          }
          handled = true;
        } else if (action == "set-image") {
          avatar->set_image_url(event_spec.value("value", std::string("")));
          handled = true;
        } else if (action == "set-name") {
          avatar->set_name(event_spec.value("value", std::string("")));
          handled = true;
        }
      } else if (auto* progress = dynamic_cast<ProgressBarWidget*>(elem.widget)) {
        if (action == "set-value") {
          progress->set_value(event_spec.value("value", 0.0f));
          handled = true;
        } else if (action == "set-indeterminate") {
          progress->set_indeterminate(event_spec.value("value", true));
          handled = true;
        }
      } else if (auto* button = dynamic_cast<ButtonWidget*>(elem.widget)) {
        if (action == "set-loading") {
          button->set_loading(event_spec.value("value", true));
          handled = true;
        } else if (action == "set-disabled") {
          button->set_disabled(event_spec.value("value", true));
          handled = true;
        }
      }

      if (!handled) {
        throw std::runtime_error("unsupported shadcn conformance widget_call");
      }
      elem.mark_paint_dirty();
      if (elem.owner_box_) {
        elem.owner_box_->update();
      }
      continue;
    }

    Event event{};
    if (type == "mouse_down") {
      event = Event::mouse_down(event_spec.value("x", 0.0f),
                                event_spec.value("y", 0.0f));
    } else if (type == "mouse_move") {
      event = Event::mouse_move(event_spec.value("x", 0.0f),
                                event_spec.value("y", 0.0f));
    } else if (type == "mouse_up") {
      event = Event::mouse_up(event_spec.value("x", 0.0f),
                              event_spec.value("y", 0.0f));
    } else if (type == "mouse_wheel") {
      event = Event::mouse_wheel(event_spec.value("x", 0.0f),
                                 event_spec.value("y", 0.0f),
                                 event_spec.value("dx", 0.0f),
                                 event_spec.value("dy", 0.0f));
    } else if (type == "text_input") {
      event = Event::text_input(event_spec.value("text", std::string("")));
    } else if (type == "key_down") {
      event = Event::key_down(
          parse_key_code(event_spec.value("key", std::string(""))));
    } else {
      throw std::runtime_error("unsupported shadcn conformance event");
    }
    elem.widget->handle_event(event, elem);
    elem.mark_paint_dirty();
    if (elem.owner_box_) {
      elem.owner_box_->update();
    }
  }
}

void require_computed_expectations(const Element& elem, const json& expected) {
  const auto* style = elem.computed_style;
  require_condition(style != nullptr, "computed_style is null");
  if (!style) {
    return;
  }

  if (!expected.contains("computed")) {
    return;
  }

  const auto& computed = expected.at("computed");
  if (computed.contains("outline_width")) {
    require_condition(
        approx_eq(style->outline_width, computed.at("outline_width").get<float>(),
                  0.001f),
        "outline_width expectation failed");
  }
  if (computed.contains("outline_offset")) {
    require_condition(
        approx_eq(style->outline_offset,
                  computed.at("outline_offset").get<float>(), 0.001f),
        "outline_offset expectation failed");
  }
  if (computed.contains("ring_width")) {
    require_condition(
        approx_eq(style->ring_width, computed.at("ring_width").get<float>(),
                  0.001f),
        "ring_width expectation failed");
  }
  if (computed.contains("ring_offset")) {
    require_condition(
        approx_eq(style->ring_offset, computed.at("ring_offset").get<float>(),
                  0.001f),
        "ring_offset expectation failed");
  }
  if (computed.contains("outline_color")) {
    require_color(style->outline_color, computed.at("outline_color"));
  }
  if (computed.contains("ring_color")) {
    require_color(style->ring_color, computed.at("ring_color"));
  }
  if (computed.contains("ring_offset_color")) {
    require_color(style->ring_offset_color, computed.at("ring_offset_color"));
  }
  if (computed.contains("border_color")) {
    require_color(style->border_color, computed.at("border_color"));
  }
  if (computed.contains("background_color")) {
    require_color(style->background_color, computed.at("background_color"));
  }
  if (computed.contains("text_color")) {
    require_color(style->text_color, computed.at("text_color"));
  }
}

void require_variable_expectations(const Element& elem, const json& expected) {
  if (!expected.contains("variables")) {
    return;
  }
  const auto* style = elem.computed_style;
  require_condition(style != nullptr, "computed_style is null for variables");
  if (!style) {
    return;
  }

  for (auto it = expected["variables"].begin(); it != expected["variables"].end(); ++it) {
    require_variable_color(*style, it.key().c_str(), it.value());
  }
}

void require_attribute_expectations(const Element& elem, const json& expected) {
  if (!expected.contains("attributes")) {
    if (!expected.contains("missing_attributes")) {
      return;
    }
  }

  if (expected.contains("attributes")) {
    for (auto it = expected["attributes"].begin(); it != expected["attributes"].end(); ++it) {
      const std::string* value = elem.attribute(it.key());
      require_condition(value != nullptr,
                        "missing attribute: " + it.key());
      if (value) {
        require_condition(
            *value == it.value().get<std::string>(),
            "attribute expectation failed for: " + it.key());
      }
    }
  }

  for (const auto& key : expected.value("missing_attributes", json::array())) {
    const std::string name = key.get<std::string>();
    require_condition(elem.attribute(name) == nullptr,
                      "attribute should be absent: " + name);
  }
}

std::string describe_rects(const RecordingRenderer& backend) {
  std::ostringstream oss;
  for (size_t i = 0; i < backend.rects.size(); ++i) {
    const auto& rect = backend.rects[i];
    if (i > 0) {
      oss << " | ";
    }
    oss << "#" << i << " x=" << rect.x << " y=" << rect.y << " w=" << rect.w
        << " h=" << rect.h << " fill=(" << rect.fill_color.r << ","
        << rect.fill_color.g << "," << rect.fill_color.b << ","
        << rect.fill_color.a << ") stroke=(" << rect.stroke_color.r << ","
        << rect.stroke_color.g << "," << rect.stroke_color.b << ","
        << rect.stroke_color.a << ") stroke_width=" << rect.stroke_width;
  }
  return oss.str();
}

std::string describe_texts(const RecordingRenderer& backend) {
  std::ostringstream oss;
  for (size_t i = 0; i < backend.texts.size(); ++i) {
    const auto& text = backend.texts[i];
    if (i > 0) {
      oss << " | ";
    }
    oss << "#" << i << " text='" << text.text << "' x=" << text.x
        << " y=" << text.y << " size=" << text.size << " color=("
        << text.color.r << "," << text.color.g << "," << text.color.b << ","
        << text.color.a << ")";
  }
  return oss.str();
}

std::string describe_paths(const RecordingRenderer& backend) {
  std::ostringstream oss;
  for (size_t i = 0; i < backend.paths.size(); ++i) {
    const auto& path = backend.paths[i];
    if (i > 0) {
      oss << " | ";
    }
    oss << "#" << i << " path='" << path.path << "' stroke=("
        << path.stroke_color.r << "," << path.stroke_color.g << ","
        << path.stroke_color.b << "," << path.stroke_color.a
        << ") stroke_width=" << path.stroke_width;
  }
  return oss.str();
}

void require_render_expectations(const std::string& case_name,
                                 const RecordingRenderer& backend,
                                 const json& expected) {
  if (!expected.contains("render")) {
    return;
  }

  const auto& render = expected.at("render");
  if (render.contains("min_rect_count")) {
    const size_t min_rect_count = render.at("min_rect_count").get<size_t>();
    if (backend.rects.size() < min_rect_count) {
      throw std::runtime_error("render min_rect_count failed in case '" +
                               case_name + "'; actual=" +
                               std::to_string(backend.rects.size()) +
                               " expected>=" + std::to_string(min_rect_count));
    }
  }
  if (render.contains("max_rect_count")) {
    const size_t max_rect_count = render.at("max_rect_count").get<size_t>();
    if (backend.rects.size() > max_rect_count) {
      throw std::runtime_error("render max_rect_count failed in case '" +
                               case_name + "'; actual=" +
                               std::to_string(backend.rects.size()) +
                               " expected<=" + std::to_string(max_rect_count));
    }
  }
  if (render.contains("min_text_count")) {
    const size_t min_text_count = render.at("min_text_count").get<size_t>();
    if (backend.texts.size() < min_text_count) {
      throw std::runtime_error("render min_text_count failed in case '" +
                               case_name + "'; actual=" +
                               std::to_string(backend.texts.size()) +
                               " expected>=" + std::to_string(min_text_count));
    }
  }
  if (render.contains("max_text_count")) {
    const size_t max_text_count = render.at("max_text_count").get<size_t>();
    if (backend.texts.size() > max_text_count) {
      throw std::runtime_error("render max_text_count failed in case '" +
                               case_name + "'; actual=" +
                               std::to_string(backend.texts.size()) +
                               " expected<=" + std::to_string(max_text_count));
    }
  }
  if (render.contains("min_path_count")) {
    const size_t min_path_count = render.at("min_path_count").get<size_t>();
    if (backend.paths.size() < min_path_count) {
      throw std::runtime_error("render min_path_count failed in case '" +
                               case_name + "'; actual=" +
                               std::to_string(backend.paths.size()) +
                               " expected>=" + std::to_string(min_path_count));
    }
  }
  if (render.contains("max_path_count")) {
    const size_t max_path_count = render.at("max_path_count").get<size_t>();
    if (backend.paths.size() > max_path_count) {
      throw std::runtime_error("render max_path_count failed in case '" +
                               case_name + "'; actual=" +
                               std::to_string(backend.paths.size()) +
                               " expected<=" + std::to_string(max_path_count));
    }
  }
  if (render.contains("min_circle_count")) {
    const size_t min_circle_count = render.at("min_circle_count").get<size_t>();
    if (backend.circles.size() < min_circle_count) {
      throw std::runtime_error("render min_circle_count failed in case '" +
                               case_name + "'; actual=" +
                               std::to_string(backend.circles.size()) +
                               " expected>=" + std::to_string(min_circle_count));
    }
  }
  if (render.contains("max_circle_count")) {
    const size_t max_circle_count = render.at("max_circle_count").get<size_t>();
    if (backend.circles.size() > max_circle_count) {
      throw std::runtime_error("render max_circle_count failed in case '" +
                               case_name + "'; actual=" +
                               std::to_string(backend.circles.size()) +
                               " expected<=" + std::to_string(max_circle_count));
    }
  }
  for (const auto& text : render.value("texts", json::array())) {
    const auto match = std::find_if(
        backend.texts.begin(), backend.texts.end(), [&](const TextCall& call) {
          if (text.is_string()) {
            return call.text == text.get<std::string>();
          }
          if (!text.is_object()) {
            return false;
          }
          if (text.contains("text") &&
              call.text != text.at("text").get<std::string>()) {
            return false;
          }
          if (text.contains("color") &&
              !color_matches(call.color, text.at("color"))) {
            return false;
          }
          if (text.contains("min_size") &&
              call.size < text.at("min_size").get<float>()) {
            return false;
          }
          return true;
        });
    if (match == backend.texts.end()) {
      throw std::runtime_error("render text expectation failed in case '" +
                               case_name + "'");
    }
  }
  for (const auto& rect_expect : render.value("rects", json::array())) {
    const auto match = std::find_if(
        backend.rects.begin(), backend.rects.end(), [&](const DrawRectCall& call) {
          if (rect_expect.contains("fill_color") &&
              !color_matches(call.fill_color, rect_expect.at("fill_color"))) {
            return false;
          }
          if (rect_expect.contains("stroke_color") &&
              !color_matches(call.stroke_color, rect_expect.at("stroke_color"))) {
            return false;
          }
          if (rect_expect.contains("stroke_width") &&
              !approx_eq(call.stroke_width,
                         rect_expect.at("stroke_width").get<float>(), 0.001f)) {
            return false;
          }
          if (rect_expect.contains("min_x") &&
              call.x < rect_expect.at("min_x").get<float>()) {
            return false;
          }
          if (rect_expect.contains("max_x") &&
              call.x > rect_expect.at("max_x").get<float>()) {
            return false;
          }
          if (rect_expect.contains("min_y") &&
              call.y < rect_expect.at("min_y").get<float>()) {
            return false;
          }
          if (rect_expect.contains("max_y") &&
              call.y > rect_expect.at("max_y").get<float>()) {
            return false;
          }
          if (rect_expect.contains("min_w") &&
              call.w < rect_expect.at("min_w").get<float>()) {
            return false;
          }
          if (rect_expect.contains("min_h") &&
              call.h < rect_expect.at("min_h").get<float>()) {
            return false;
          }
          return true;
        });
    if (match == backend.rects.end()) {
      throw std::runtime_error("render rect expectation failed in case '" +
                               case_name + "'; actual rects: " +
                               describe_rects(backend));
    }
  }
  for (const auto& circle_expect : render.value("circles", json::array())) {
    const auto match = std::find_if(
        backend.circles.begin(), backend.circles.end(),
        [&](const DrawCircleCall& call) {
          if (circle_expect.contains("fill_color") &&
              !color_matches(call.fill_color, circle_expect.at("fill_color"))) {
            return false;
          }
          if (circle_expect.contains("stroke_color") &&
              !color_matches(call.stroke_color,
                             circle_expect.at("stroke_color"))) {
            return false;
          }
          if (circle_expect.contains("stroke_width") &&
              !approx_eq(call.stroke_width,
                         circle_expect.at("stroke_width").get<float>(), 0.001f)) {
            return false;
          }
          if (circle_expect.contains("min_radius") &&
              call.radius < circle_expect.at("min_radius").get<float>()) {
            return false;
          }
          if (circle_expect.contains("max_radius") &&
              call.radius > circle_expect.at("max_radius").get<float>()) {
            return false;
          }
          return true;
        });
    if (match == backend.circles.end()) {
      throw std::runtime_error("render circle expectation failed in case '" +
                               case_name + "'");
    }
  }
  for (const auto& path_expect : render.value("paths", json::array())) {
    const auto match = std::find_if(
        backend.paths.begin(), backend.paths.end(), [&](const PathCall& call) {
          if (path_expect.contains("path") &&
              call.path != path_expect.at("path").get<std::string>()) {
            return false;
          }
          if (path_expect.contains("path_contains") &&
              call.path.find(path_expect.at("path_contains").get<std::string>()) ==
                  std::string::npos) {
            return false;
          }
          if (path_expect.contains("stroke_color") &&
              !color_matches(call.stroke_color, path_expect.at("stroke_color"))) {
            return false;
          }
          if (path_expect.contains("stroke_width") &&
              !approx_eq(call.stroke_width,
                         path_expect.at("stroke_width").get<float>(), 0.001f)) {
            return false;
          }
          return true;
        });
    if (match == backend.paths.end()) {
      throw std::runtime_error("render path expectation failed in case '" +
                               case_name + "'; actual paths: " +
                               describe_paths(backend));
    }
  }
}

} // namespace

spec("shadcn smoke corpus manifest is well formed") {
  const auto manifests = load_manifests();
  const bool filtered = !env_filter("FLEXUI_SHADCN_MANIFEST_FILTER").empty();
  check(manifests.size() >= (filtered ? 1 : 3));

  std::set<std::string> names;
  for (const auto& manifest : manifests) {
    check(manifest.at("schema_version").get<int>() == 1);
    check(manifest.contains("suite"));
    check(manifest.contains("cases"));
    check(manifest.at("cases").is_array());
    check(manifest.at("cases").size() >= 1);

    for (const auto& spec : manifest.at("cases")) {
      check(spec.contains("name"));
      check(spec.contains("widget"));
      check(spec.contains("required_features"));
      check(spec.contains("expect"));
      const std::string name = spec.at("name").get<std::string>();
      check(names.insert(name).second);
    }
  }
}

spec("shadcn smoke corpus drives real flexUI create compute and state bridge checks") {
  it("runs all smoke cases") {
    const auto manifests = load_manifests();
    const auto cases = flatten_cases(manifests);
    require_condition(!cases.empty(),
                      "no shadcn conformance cases matched current filters");

    for (const auto& spec : cases) {
      const std::string case_name = spec.at("name").get<std::string>();
      try {
        g_current_case_name = case_name;
        if (env_flag_enabled("FLEXUI_SHADCN_TRACE")) {
          std::cerr << "[shadcn] running case: " << case_name << std::endl;
        }
        RecordingRenderer backend;
        const bool needs_renderer =
            spec.at("expect").contains("render") || spec.contains("events");
        Box box(needs_renderer ? &backend : nullptr);
        auto* root = box.create("div", "root");
        auto* target = build_target(box, spec);
        root->append(target);
        box.set_root(root);
        box.set_viewport(400.0f, 200.0f);
        box.load_css(spec.at("css").get<std::string>());
        apply_case_state(*target, spec);
        box.update();
        for (const auto delta_ms : spec.value("pre_update_time_ms", json::array())) {
          box.update_time(delta_ms.get<float>());
          box.update();
        }
        replay_case_events(*target, spec);
        for (const auto delta_ms : spec.value("update_time_ms", json::array())) {
          box.update_time(delta_ms.get<float>());
          box.update();
        }
        require_computed_expectations(*target, spec.at("expect"));
        require_variable_expectations(*target, spec.at("expect"));
        require_attribute_expectations(*target, spec.at("expect"));
        require_render_expectations(case_name, backend, spec.at("expect"));
      } catch (const std::exception& ex) {
        std::cerr << "[shadcn] case failed: " << case_name << " :: " << ex.what()
                  << std::endl;
        throw std::runtime_error("shadcn conformance case failed: " + case_name +
                                 " :: " + ex.what());
      }
    }
    g_current_case_name.clear();
  }
}
