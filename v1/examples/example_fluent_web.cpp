#include <nanogui/fluent_web_accordion.h>
#include <nanogui/fluent_web_button.h>
#include <nanogui/fluent_web_checkbox.h>
#include <nanogui/fluent_web_chip.h>
#include <nanogui/fluent_web_combo_box.h>
#include <nanogui/fluent_web_dialog.h>
#include <nanogui/fluent_web_data_viz.h>
#include <nanogui/fluent_web_list_view.h>
#include <nanogui/fluent_web_tooltip.h>
#include <nanogui/fluent_web_menu.h>
#include <nanogui/fluent_web_message_bar.h>
#include <nanogui/fluent_web_progress_bar.h>
#include <nanogui/fluent_web_radio.h>
#include <nanogui/fluent_web_segmented_control.h>
#include <nanogui/fluent_web_slider.h>
#include <nanogui/fluent_web_spinner.h>
#include <nanogui/fluent_web_switch.h>
#include <nanogui/fluent_web_text_field.h>
#include <nanogui/fluent_web_theme.h>
#include <nanogui/fluent_web_toast.h>
#include <nanogui/fluent_icons.h>
#include <nanogui.h>
#include <nanogui/layout.h>
#include <tuple>


#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#if defined(NANOGUI_USE_SDL3)
  #include <SDL3/SDL.h>
  #define KEY_ESCAPE SDLK_ESCAPE
  #define KEY_PRESS 1
#else
  #include <GLFW/glfw3.h>
  #define KEY_ESCAPE GLFW_KEY_ESCAPE
  #define KEY_PRESS GLFW_PRESS
#endif
using namespace nanogui;
namespace {
using BrandStops = std::array<std::array<float, 3>, 16>;

class FluentTooltipButton : public FluentWebButton {
public:
  using FluentWebButton::FluentWebButton;

  void set_tooltip_text(const std::string &text) { m_tooltip_text = text; }

protected:
  bool mouse_enter_event(const Vector2i &p, bool enter) override {
    if (enter) {
      if (!m_tooltip_text.empty()) {
        if (m_tooltip)
          m_tooltip->dismiss();
        m_tooltip = FluentWebTooltip::show(this, m_tooltip_text);
      }
    } else {
      if (m_tooltip) {
        m_tooltip->dismiss();
        m_tooltip = nullptr;
      }
    }
    return FluentWebButton::mouse_enter_event(p, enter);
  }

  bool mouse_button_event(const Vector2i &p, int button, bool down,
                          int modifiers) override {
    bool handled =
        FluentWebButton::mouse_button_event(p, button, down, modifiers);
    if (!down && m_tooltip) {
      m_tooltip->dismiss();
      m_tooltip = nullptr;
    }
    return handled;
  }

  ~FluentTooltipButton() override {
    if (m_tooltip)
      m_tooltip->dismiss();
  }

private:
  FluentWebTooltip *m_tooltip = nullptr;
  std::string m_tooltip_text;
};

FluentWebTheme::BrandRamp make_brand_ramp(const BrandStops &stops) {
  FluentWebTheme::BrandRamp ramp;
  for (size_t i = 0; i < stops.size(); ++i)
    ramp.stops[i] = Color(stops[i][0], stops[i][1], stops[i][2], 1.f);
  return ramp;
}

const BrandStops kBrandWebStops = {{
    {0.02352941f, 0.09019608f, 0.14117647f}, // 10
    {0.03137255f, 0.13725490f, 0.21960784f}, // 20
    {0.03921569f, 0.18039216f, 0.29019608f}, // 30
    {0.04705882f, 0.23137255f, 0.36862745f}, // 40
    {0.05490196f, 0.27843137f, 0.45882353f}, // 50
    {0.05882353f, 0.32941176f, 0.54901961f}, // 60
    {0.06666667f, 0.36862745f, 0.63921569f}, // 70
    {0.05882353f, 0.42352941f, 0.74117647f}, // 80
    {0.15686275f, 0.52549020f, 0.87058824f}, // 90
    {0.27843137f, 0.61960784f, 0.96078431f}, // 100
    {0.38431373f, 0.67058824f, 0.96078431f}, // 110
    {0.46666667f, 0.71764706f, 0.96862745f}, // 120
    {0.58823529f, 0.77647059f, 0.98039216f}, // 130
    {0.70588235f, 0.83921569f, 0.98039216f}, // 140
    {0.81176471f, 0.89411765f, 0.98039216f}, // 150
    {0.92156863f, 0.95294118f, 0.98823529f}  // 160
}};

const BrandStops kBrandTeamsStops = {{
    {0.16862745f, 0.16862745f, 0.25098039f}, // 10
    {0.18431373f, 0.18431373f, 0.29019608f}, // 20
    {0.20000000f, 0.20000000f, 0.34117647f}, // 30
    {0.21960784f, 0.22352941f, 0.40000000f}, // 40
    {0.23921569f, 0.24313725f, 0.47058824f}, // 50
    {0.26666667f, 0.27843137f, 0.56862745f}, // 60
    {0.30980392f, 0.32156863f, 0.69803922f}, // 70
    {0.35686275f, 0.37254902f, 0.78039216f}, // 80
    {0.45882353f, 0.47450980f, 0.92156863f}, // 90
    {0.49803922f, 0.52156863f, 0.96078431f}, // 100
    {0.57254902f, 0.60000000f, 0.96862745f}, // 110
    {0.66666667f, 0.69411765f, 0.98039216f}, // 120
    {0.71372549f, 0.73725490f, 0.98039216f}, // 130
    {0.77254902f, 0.79607843f, 0.98039216f}, // 140
    {0.86274510f, 0.87843137f, 0.98039216f}, // 150
    {0.90980392f, 0.92156863f, 0.98039216f}  // 160
}};

const BrandStops kBrandOfficeStops = {{
    {0.16078431f, 0.07450980f, 0.04313725f}, // 10
    {0.30196078f, 0.14117647f, 0.08235294f}, // 20
    {0.47450980f, 0.12549020f, 0.00000000f}, // 30
    {0.60000000f, 0.28235294f, 0.16862745f}, // 40
    {0.64705882f, 0.17254902f, 0.00000000f}, // 50
    {0.76470588f, 0.20392157f, 0.00000000f}, // 60
    {0.87843137f, 0.41568627f, 0.24705882f}, // 70
    {0.84705882f, 0.23137255f, 0.00392157f}, // 80
    {0.86666667f, 0.30980392f, 0.10588235f}, // 90
    {0.99607843f, 0.47450980f, 0.28235294f}, // 100
    {1.00000000f, 0.52549020f, 0.35294118f}, // 110
    {1.00000000f, 0.60000000f, 0.45098039f}, // 120
    {0.90980392f, 0.50980392f, 0.36470588f}, // 130
    {1.00000000f, 0.70588235f, 0.59607843f}, // 140
    {0.95686275f, 0.74509804f, 0.66666667f}, // 150
    {0.97647059f, 0.86274510f, 0.81960784f}  // 160
}};
} // namespace
class FluentWebDemo : public Screen {
public:
  FluentWebDemo() : Screen(Vector2i(960, 620), "Fluent 2 Web Demo") {
    m_theme = new FluentWebTheme(nvg_context(), FluentWebTheme::Mode::Light);
    set_theme(m_theme);
    m_toast = new FluentWebToast(this);
    build_ui();
    perform_layout();
    update_token_labels();
  }
  bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (Screen::keyboard_event(key, scancode, action, modifiers))
      return true;
    if (key == KEY_ESCAPE && action == KEY_PRESS) {
      set_visible(false);
    }
    return false;
  }

  bool resize_event(const Vector2i &size) override {
    bool handled = Screen::resize_event(size);
    if (m_menu_bar)
      m_menu_bar->set_fixed_width(size.x());
    return handled;
  }

private:
  void build_ui() {
    m_menu_bar = new FluentWebMenuBar(this);
    m_menu_bar->set_position(Vector2i(0, 0));
    m_menu_bar->set_fixed_height(48);
    m_menu_bar->set_fixed_width(m_size.x());

    auto *file_menu = m_menu_bar->add_menu("File");
    file_menu->add_item("New project", [] {
      std::cout << "Menu: File > New project" << std::endl;
    }, 0, "Ctrl+N");
    file_menu->add_item("Open...", [] {
      std::cout << "Menu: File > Open" << std::endl;
    }, 0, "Ctrl+O");
    file_menu->add_separator();
    file_menu->add_item("Exit", [this] {
      set_visible(false);
    }, 0, "Alt+F4");

    auto *view_menu = m_menu_bar->add_menu("View");
    view_menu->add_item("Toggle theme", [this] {
      set_mode(m_theme->mode() == FluentWebTheme::Mode::Light ? FluentWebTheme::Mode::Dark
                                                              : FluentWebTheme::Mode::Light);
      std::cout << "Menu: View > Toggle theme" << std::endl;
    }, 0, "Ctrl+Shift+L");
    view_menu->add_item("Focus toast", [this] {
      if (m_toast) {
        m_toast->set_visible(true);
        m_toast->request_focus();
      }
    });

    auto *help_menu = m_menu_bar->add_menu("Help");
    help_menu->add_item("Fluent design docs", [] {
      std::cout << "Menu: Help > Fluent design docs" << std::endl;
    }, 0, "F1");

    Window *controls = new Window(this, "Fluent 2 Controls");
    controls->set_position(Vector2i(32, 72));
    controls->set_layout(new GroupLayout(12, 16, 16, 16));
    new Label(controls, "Brand presets", "sans-bold");
    Widget *accent_row = new Widget(controls);
    accent_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));
    auto add_brand_button = [accent_row, this](const std::string &label,
                                               const BrandStops &stops,
                                               const char *log_message) {
      auto *btn = new Button(accent_row, label);
      btn->set_callback([this, label, stops, log_message] {
        set_brand(make_brand_ramp(stops), label);
        std::cout << log_message << std::endl;
      });
      return btn;
    };
    add_brand_button("Microsoft 365", kBrandWebStops, "Brand ramp set to Microsoft 365");
    add_brand_button("Teams", kBrandTeamsStops, "Brand ramp set to Teams");
    add_brand_button("Office", kBrandOfficeStops, "Brand ramp set to Office");
    new Label(controls, "Mode", "sans-bold");
    Widget *mode_row = new Widget(controls);
    mode_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));
    auto *light = new Button(mode_row, "Light");
    light->set_callback([this] {
      set_mode(FluentWebTheme::Mode::Light);
      std::cout << "Switched to Fluent 2 Light" << std::endl;
    });
    auto *dark = new Button(mode_row, "Dark");
    dark->set_callback([this] {
      set_mode(FluentWebTheme::Mode::Dark);
      std::cout << "Switched to Fluent 2 Dark" << std::endl;
    });
    new Label(controls, "Buttons", "sans-bold");
    auto *primary = new FluentWebButton(controls, "Primary", FluentWebButton::Appearance::Primary);
    primary->set_callback([] { std::cout << "Primary action pressed" << std::endl; });
    auto *secondary =
        new FluentWebButton(controls, "Secondary", FluentWebButton::Appearance::Secondary);
    secondary->set_flags(Button::Flags::ToggleButton);
    new FluentWebButton(controls, "Outline", FluentWebButton::Appearance::Outline);
    new FluentWebButton(controls, "Subtle", FluentWebButton::Appearance::Subtle);
    auto *dialog_demo = new FluentWebButton(controls, "Show dialog",
                                            FluentWebButton::Appearance::Primary);
    dialog_demo->set_callback([this] {
      auto *dialog = new FluentWebDialog(this, "Upload complete");
      dialog->set_subtitle("We saved your changes to the cloud.");
      dialog->set_body_text(
          "Would you like to keep working while we run a quick health check?");
      dialog->add_action("Cancel", FluentWebButton::Appearance::Subtle,
                         [dialog] { dialog->dismiss(); });
      dialog->add_action("Keep working", FluentWebButton::Appearance::Primary,
                         [dialog] { dialog->dismiss(); });
      dialog->show();
    });
    auto *tooltip_btn = new FluentTooltipButton(controls, "Hover for tooltip",
                                                FluentWebButton::Appearance::Subtle);
    tooltip_btn->set_tooltip_text("Shows secondary guidance when hovering.");
    auto *popover_button =
        new FluentWebButton(controls, "Show popover", FluentWebButton::Appearance::Transparent);
    popover_button->set_callback([this, popover_button] {
      auto *popover = new FluentWebPopover(screen(), popover_button->window());
      auto *layout = new GroupLayout(12, 10, 6, 0);
      popover->set_layout(layout);
      new Label(popover, "Quick actions", "sans-bold", 16);
      auto *details = new FluentWebButton(popover, "View details",
                                          FluentWebButton::Appearance::Transparent);
      details->set_callback([popover] {
        popover->set_visible(false);
        popover->dispose();
      });
      Vector2i anchor_pos = popover_button->window()
                                ? popover_button->absolute_position() -
                                      popover_button->window()->position()
                                : popover_button->absolute_position();
      anchor_pos.x() += popover_button->width();
      anchor_pos.y() += popover_button->height() / 2;
      popover->set_anchor_pos(anchor_pos);
      popover->set_anchor_offset(popover_button->height() / 2);
      popover->set_side(Popup::Right);
      popover->set_visible(true);
      if (auto *scr = screen())
        scr->move_window_to_front(popover);
    });
    auto *menu_button = new FluentWebButton(controls, "Quick menu",
                                            FluentWebButton::Appearance::Transparent);
    menu_button->set_callback([this, menu_button] {
      if (!m_context_menu) {
        Widget *parent = screen() ? static_cast<Widget *>(screen()) : static_cast<Widget *>(this);
        m_context_menu = new FluentWebMenu(parent, window());
        m_context_menu->set_theme(m_theme);
        m_context_menu->add_item("Rename project", [] {
          std::cout << "Context menu: Rename project" << std::endl;
        }, 0, "F2");
        m_context_menu->add_item("Duplicate", [] {
          std::cout << "Context menu: Duplicate" << std::endl;
        }, 0, "Ctrl+D");
        m_context_menu->add_separator();
        m_context_menu->add_item("Archive", [] {
          std::cout << "Context menu: Archive" << std::endl;
        });
      }
      m_context_menu->show_for_anchor(menu_button);
    });
    new Label(controls, "List view", "sans-bold");
    m_list_view = new FluentWebListView(controls);
    m_list_view->set_selection_mode(FluentWebListView::SelectionMode::Multiple);
    m_list_view->set_fixed_height(200);
    const std::vector<std::tuple<std::string, std::string, std::string, int>> list_rows = {
        {"Project Alpha", "Planning sprint aligned", "2 min ago", FLUENT_ICON_CHECKMARK},
        {"Marketing sync", "Syncs Tue/Thu", "Today", FLUENT_ICON_INFO},
        {"Research backlog", "", "Draft", FLUENT_ICON_WARNING},
        {"Archive", "Completed milestones", "", FLUENT_ICON_CHEVRON_RIGHT}
    };
    m_list_view->set_item_provider([list_rows](int index, FluentWebListItem *item) {
      if (index >= static_cast<int>(list_rows.size()))
        return;
      const auto &[primary, secondary, meta, icon] = list_rows[index];
      item->set_primary_text(primary);
      item->set_secondary_text(secondary);
      item->set_meta_text(meta);
      item->set_layout(secondary.empty() ? FluentWebListItem::Layout::OneLine
                                         : FluentWebListItem::Layout::TwoLine);
      item->set_leading_icon(icon);
    });
    m_list_view->set_item_count(static_cast<int>(list_rows.size()));
    m_list_view->set_selection_callback([](const std::vector<int> &indices) {
      std::cout << "List selection:";
      for (int idx : indices)
        std::cout << " " << idx;
      std::cout << std::endl;
    });
    m_list_view->set_activation_callback([](int index) {
      std::cout << "List activated: " << index << std::endl;
    });

    new Label(controls, "Grid view", "sans-bold");
    m_grid_view = new FluentWebGridView(controls);
    m_grid_view->set_columns(2);
    m_grid_view->set_selection_mode(FluentWebListView::SelectionMode::Multiple);
    m_grid_view->set_fixed_height(240);
    const std::vector<std::tuple<std::string, std::string, int>> grid_cards = {
        {"Design specs", "Updated 2 days ago", FLUENT_ICON_INFO},
        {"Product roadmap", "v3 milestone", FLUENT_ICON_CHEVRON_RIGHT},
        {"Launch assets", "12 files", FLUENT_ICON_CHECKMARK},
        {"Beta feedback", "48 responses", FLUENT_ICON_WARNING}
    };
    m_grid_view->set_item_provider([grid_cards](int index, FluentWebGridItem *item) {
      if (index >= static_cast<int>(grid_cards.size()))
        return;
      const auto &[title, subtitle, icon] = grid_cards[index];
      item->set_title(title);
      item->set_subtitle(subtitle);
      item->set_icon(icon);
    });
    m_grid_view->set_item_count(static_cast<int>(grid_cards.size()));
    m_grid_view->set_selection_callback([](const std::vector<int> &indices) {
      std::cout << "Grid selection:";
      for (int idx : indices)
        std::cout << " " << idx;
      std::cout << std::endl;
    });
    m_grid_view->set_activation_callback([](int index) {
      std::cout << "Grid activated: " << index << std::endl;
    });

    new Label(controls, "Options", "sans-bold");
    auto *option_a = new FluentWebCheckbox(controls, "Enable notifications");
    option_a->set_checked(true);
    auto *option_b = new FluentWebCheckbox(controls, "Sync over metered connections");
    option_b->set_callback([](bool value) {
      std::cout << "Metered sync toggled: " << (value ? "on" : "off") << std::endl;
    });
    auto *option_disabled = new FluentWebCheckbox(controls, "Auto install updates");
    option_disabled->set_enabled(false);
    new Label(controls, "Inputs", "sans-bold");
    auto *text_main = new FluentWebTextField(controls, "", "Project name");
    text_main->set_editable(true);
    text_main->set_callback([](const std::string &value) {
      std::cout << "Text committed: " << value << std::endl;
      return true;
    });
    auto *text_disabled = new FluentWebTextField(controls, "", "readonly");
    text_disabled->set_editable(false);
    text_disabled->set_enabled(false);
    new Label(controls, "Toggles", "sans-bold");
    auto *switch_wifi = new FluentWebSwitch(controls, "Wi-Fi");
    switch_wifi->set_checked(true);
    auto *switch_airplane = new FluentWebSwitch(controls, "Airplane mode");
    switch_airplane->set_callback([](bool value) {
      std::cout << "Airplane mode toggled: " << (value ? "on" : "off") << std::endl;
    });
    auto *switch_disabled = new FluentWebSwitch(controls, "Bluetooth");
    switch_disabled->set_enabled(false);
    new Label(controls, "Radio group", "sans-bold");
    std::vector<Button *> workspaces;
    auto *radio_a = new FluentWebRadio(controls, "Workspace A", true);
    auto *radio_b = new FluentWebRadio(controls, "Workspace B");
    auto *radio_c = new FluentWebRadio(controls, "Workspace C (offline)");
    radio_c->set_enabled(false);
    workspaces.push_back(radio_a);
    workspaces.push_back(radio_b);
    workspaces.push_back(radio_c);
    radio_a->set_button_group(workspaces);
    radio_b->set_button_group(workspaces);
    radio_c->set_button_group(workspaces);
    new Label(controls, "Activity", "sans-bold");
    Widget *spinner_row = new Widget(controls);
    spinner_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));
    new FluentWebSpinner(spinner_row, FluentWebSpinner::Size::Small);
    new FluentWebSpinner(spinner_row, FluentWebSpinner::Size::Medium);
    auto *large_spinner = new FluentWebSpinner(spinner_row, FluentWebSpinner::Size::Large);
    large_spinner->set_tooltip("Background sync");
    new Label(controls, "Progress", "sans-bold");
    m_progress_bar = new FluentWebProgressBar(controls);
    m_progress_bar->set_value(0.4f);
    auto *progress_indeterminate = new FluentWebProgressBar(controls);
    progress_indeterminate->set_indeterminate(true);
    Widget *slider_row = new Widget(controls);
    slider_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 12));
    new Label(slider_row, "Adjust progress");
    auto *progress_slider = new FluentWebSlider(slider_row);
    progress_slider->set_value(0.4f);
    progress_slider->set_callback([this](float value) {
      if (m_progress_bar)
        m_progress_bar->set_value(value);
      std::cout << "Progress: " << static_cast<int>(value * 100.f) << "%" << std::endl;
    });
    new Label(controls, "Legacy widgets", "sans-bold");
    Widget *input_row = new Widget(controls);
    input_row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 8));
    auto *textbox = new TextBox(input_row);
    textbox->set_editable(true);
    textbox->set_value("Fluent 2");
    textbox->set_placeholder("Type here...");
    auto *check = new CheckBox(controls, "Enable rich surfaces");
    check->set_checked(true);
    auto *legacy_progress = new ProgressBar(controls);
    legacy_progress->set_value(0.4f);
    Window *tokens = new Window(this, "Design Tokens");
    tokens->set_position(Vector2i(560, 48));
    tokens->set_layout(new GroupLayout(12, 16, 16, 16));
    m_mode_label = new Label(tokens, "", "sans-bold", 18);
    m_brand_label = new Label(tokens, "", "sans", 14);
    m_spacing_label = new Label(tokens, "", "sans", 14);
    m_typography_label = new Label(tokens, "", "sans", 14);
    Widget *type_samples = new Widget(tokens);
    type_samples->set_layout(new GroupLayout(4, 10, 10, 10));
    new Label(type_samples, "Display style", "sans-bold", 22);
    new Label(type_samples, "Fluent 2 embraces spacious layouts with clear hierarchy.", "sans", 13);
  }
  void update_token_labels() {
    std::ostringstream oss_mode;
    oss_mode << "Mode: " << (m_theme->mode() == FluentWebTheme::Mode::Light ? "Light" : "Dark");
    m_mode_label->set_caption(oss_mode.str());
    const Color &brand_primary =
        m_theme->color(FluentWebTheme::ColorToken::colorBrandBackground);
    const Color &brand_pressed =
        m_theme->color(FluentWebTheme::ColorToken::colorBrandBackgroundPressed);
    const Color &compound =
        m_theme->color(FluentWebTheme::ColorToken::colorCompoundBrandBackground);
    std::ostringstream oss_brand;
    oss_brand << std::fixed << std::setprecision(2) << "Brand preset: " << m_brand_name
              << "\n  stop 80 rgba(" << brand_primary.r() << ", " << brand_primary.g() << ", "
              << brand_primary.b() << ")\n  stop 40 rgba(" << brand_pressed.r() << ", "
              << brand_pressed.g() << ", " << brand_pressed.b() << ")\n  compound rgba("
              << compound.r() << ", " << compound.g() << ", " << compound.b() << ")";
    m_brand_label->set_caption(oss_brand.str());
    std::ostringstream oss_spacing;
    oss_spacing << "Spacing (xs..lg): " << m_theme->spacing(FluentWebTheme::SpaceToken::XS) << ", "
                << m_theme->spacing(FluentWebTheme::SpaceToken::S) << ", "
                << m_theme->spacing(FluentWebTheme::SpaceToken::M) << ", "
                << m_theme->spacing(FluentWebTheme::SpaceToken::L);
    m_spacing_label->set_caption(oss_spacing.str());
    const auto &body = m_theme->typography(FluentWebTheme::TypographyToken::Body1);
    std::ostringstream oss_type;
    oss_type << "Body: " << body.font_size << " / " << body.line_height << " weight "
             << body.font_weight;
    m_typography_label->set_caption(oss_type.str());
    redraw();
  }
  void set_mode(FluentWebTheme::Mode mode) {
    m_theme->apply(mode);
    if (m_menu_bar)
      m_menu_bar->set_theme(m_theme);
    if (m_context_menu)
      m_context_menu->set_theme(m_theme);
    if (m_list_view)
      m_list_view->set_theme(m_theme);
    if (m_grid_view)
      m_grid_view->set_theme(m_theme);
    if (m_bar_chart)
      m_bar_chart->set_theme(m_theme);
    if (m_line_chart)
      m_line_chart->set_theme(m_theme);
    if (m_pie_chart)
      m_pie_chart->set_theme(m_theme);
    update_token_labels();
  }
  void set_brand(const FluentWebTheme::BrandRamp &ramp, const std::string &brand_name) {
    m_theme->set_brand_variants(ramp);
    m_brand_name = brand_name;
    if (m_menu_bar)
      m_menu_bar->set_theme(m_theme);
    if (m_context_menu)
      m_context_menu->set_theme(m_theme);
    if (m_list_view)
      m_list_view->set_theme(m_theme);
    if (m_grid_view)
      m_grid_view->set_theme(m_theme);
    if (m_bar_chart)
      m_bar_chart->set_theme(m_theme);
    if (m_line_chart)
      m_line_chart->set_theme(m_theme);
    if (m_pie_chart)
      m_pie_chart->set_theme(m_theme);
    update_token_labels();
  }
  FluentWebTheme *m_theme = nullptr;
  Label *m_mode_label = nullptr;
  Label *m_brand_label = nullptr;
  Label *m_spacing_label = nullptr;
  Label *m_typography_label = nullptr;
  FluentWebMenuBar *m_menu_bar = nullptr;
  FluentWebMenu *m_context_menu = nullptr;
  FluentWebListView *m_list_view = nullptr;
  FluentWebGridView *m_grid_view = nullptr;
  FluentWebProgressBar *m_progress_bar = nullptr;
  FluentWebToast *m_toast = nullptr;
  FluentWebBarChart *m_bar_chart = nullptr;
  FluentWebLineChart *m_line_chart = nullptr;
  FluentWebPieChart *m_pie_chart = nullptr;
  std::string m_brand_name = "Microsoft 365";
};
int main(int argc, char **argv) {
  try {
    nanogui::init();
    {
      ref<FluentWebDemo> app = new FluentWebDemo();
      app->set_visible(true);
      while (app->process_events()) {
        app->draw_all();
      }
    }
    // nanogui::mainloop(); // Commented out as it seems redundant with the explicit event loop
    nanogui::shutdown();
  } catch (const std::exception &e) {
    std::cerr << "Fluent 2 web demo error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}





