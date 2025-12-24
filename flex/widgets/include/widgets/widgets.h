/*
 * Flex Widgets - Main Header
 * Include this to get all widgets and registration functions
 */

#pragma once

#include "flex.h"
#include "widget_base.h"
#include "widget_theme.h"


namespace flex {
namespace widgets {

// ============================================================================
// Individual Widget Registration
// ============================================================================

void register_button(const WidgetTheme &theme = current_theme());
void register_label(const WidgetTheme &theme = current_theme());
void register_checkbox(const WidgetTheme &theme = current_theme());
void register_toggle(const WidgetTheme &theme = current_theme());
void register_slider(const WidgetTheme &theme = current_theme());
void register_progressbar(const WidgetTheme &theme = current_theme());
void register_textbox(const WidgetTheme &theme = current_theme());
void register_radiobutton(const WidgetTheme &theme = current_theme());
void register_combobox(const WidgetTheme &theme = current_theme());
void register_listbox(const WidgetTheme &theme = current_theme());

// ============================================================================
// Register All Widgets
// ============================================================================

inline void register_all(const WidgetTheme &theme = current_theme()) {
  set_theme(theme);

  register_button(theme);
  register_label(theme);
  register_checkbox(theme);
  register_toggle(theme);
  register_slider(theme);
  register_progressbar(theme);
  register_textbox(theme);
  register_radiobutton(theme);
  register_combobox(theme);
  register_listbox(theme);
}

// Convenience: register with preset themes
inline void register_all_fluent() { register_all(theme_fluent()); }
inline void register_all_dark() { register_all(theme_dark()); }
inline void register_all_light() { register_all(theme_light()); }

} // namespace widgets
} // namespace flex
