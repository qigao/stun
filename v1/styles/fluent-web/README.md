# Fluent 2 Web Theme

This directory contains the Fluent 2 web theme for NanoGUI.  It provides a
`FluentWebTheme` class that exposes Fluent 2 design tokens (colors, typography,
spacing, elevation) while configuring the base `nanogui::Theme` values.

## Highlights

- Light and dark palettes derived from the Fluent 2 design token catalog.
- Brand-aware helpers so applications can override the accent color while
  keeping derived shades consistent.
- Accessors for typography and spacing tokens that match the 4 px Fluent scale.
- Elevation metadata for Fluent-style drop shadows.

## Usage

```cpp
#include <nanogui/fluent_web_theme.h>

auto *theme = new FluentWebTheme(screen->nvg_context(),
                                 FluentWebTheme::Mode::Light);
theme->set_brand_color(nanogui::Color(0.2f, 0.4f, 0.9f, 1.f)); // optional
screen->set_theme(theme);
```

Applications can query Fluent tokens when custom drawing is needed:

```cpp
const auto &subtitle = theme->typography(
    FluentWebTheme::TypographyToken::TitleSmall);
float padding = theme->spacing(FluentWebTheme::SpaceToken::Medium);
```

## Components

- `FluentWebButton` consumes the theme tokens to render Fluent 2 button
  appearances (`Primary`, `Secondary`, `Outline`, `Subtle`, `Transparent`,
  `Danger`).  Include `<nanogui/fluent_web_button.h>` and construct it like any
  other NanoGUI widget.
- `FluentWebCheckbox` provides Fluent styled checkboxes with proper colors,
  spacing, and focus treatment.  Include `<nanogui/fluent_web_checkbox.h>`.
- `FluentWebSwitch` renders Fluent toggle switches (track + thumb states, focus
  ring). Include `<nanogui/fluent_web_switch.h>`.
- `FluentWebRadio` draws Fluent radio buttons while supporting NanoGUI's radio
  grouping via `Button` flags. Include `<nanogui/fluent_web_radio.h>`.
- `FluentWebChip` supplies Fluent chip/pill buttons (assist, suggestion, filter
  variants). Include `<nanogui/fluent_web_chip.h>`.
- `FluentWebTextField` styles editable text inputs with Fluent tokens (hover,
  focus, invalid, disabled). Include `<nanogui/fluent_web_text_field.h>`.
- `FluentWebMessageBar` shows inline notifications with Fluent severities and
  optional action/close affordances. Include `<nanogui/fluent_web_message_bar.h>`.
- `FluentWebToast` provides transient bottom-edge notifications with optional
  actions. Include `<nanogui/fluent_web_toast.h>`.
- `FluentWebProgressBar` draws determinate/indeterminate bars with Fluent colors. Include `<nanogui/fluent_web_progress_bar.h>`.
- `FluentWebSpinner` adds circular activity indicators with Fluent animation. Include `<nanogui/fluent_web_spinner.h>`.
- `FluentWebSlider` restyles slider tracks/thumbs. Include `<nanogui/fluent_web_slider.h>`.
- `FluentWebComboBox` styles dropdown selectors and popup options. Include `<nanogui/fluent_web_combo_box.h>`.
- `FluentWebSegmentedControl` renders segmented buttons with Fluent typography/colors. Include `<nanogui/fluent_web_segmented_control.h>`.
- `FluentWebDialog` offers modal dialog surfaces with header, body, and action rows wired to Fluent tokens. Include `<nanogui/fluent_web_dialog.h>`.
- `FluentWebTooltip` / `FluentWebPopover` provide inline informational surfaces anchored to controls. Include `<nanogui/fluent_web_tooltip.h>`.
- `FluentWebMenu` and `FluentWebMenuBar` provide Fluent drop-down menus and menu bars with keyboard navigation. Include `<nanogui/fluent_web_menu.h>`.
- `FluentWebDataPalette`, `FluentWebBarChart`, `FluentWebLineChart`, and `FluentWebPieChart` provide Fluent-themed data visualization scaffolding. Include `<nanogui/fluent_web_data_viz.h>`.
- `FluentWebListView` / `FluentWebGridView` render Fluent-styled lists and grids with selection callbacks. Include `<nanogui/fluent_web_list_view.h>`.


