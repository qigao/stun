#pragma once

#include <nanogui/popup.h>
#include <nanogui/vector.h>
#include <string>

NAMESPACE_BEGIN(nanogui)

class Label;
class FluentWebTheme;

/**
 * Fluent-styled popover surface using NanoGUI's Popup.
 *
 * Provides Fluent spacing + colors and supports hosting
 * arbitrary child widgets.
 */
class NANOGUI_EXPORT FluentWebPopover : public Popup {
public:
  FluentWebPopover(Widget *parent, Window *parent_window = nullptr);

  /// Updates layout metrics based on the active Fluent theme.
  void set_theme(Theme *theme) override;
  void draw(NVGcontext *ctx) override;

protected:
  void refresh_tokens(const FluentWebTheme *fluent);
  void update_spacing(const FluentWebTheme *fluent);

  Color m_background;
  Color m_border;
};

/**
 * Lightweight tooltip with Fluent 2 tokens.
 *
 * Usage:
 * ```
 * auto *tooltip = FluentWebTooltip::show(button, "This is a tooltip");
 * ```
 * Call `tooltip->dismiss()` when the tooltip should hide (e.g. on mouse leave).
 */
class NANOGUI_EXPORT FluentWebTooltip : public FluentWebPopover {
public:
  FluentWebTooltip(Widget *parent, Window *parent_window, const std::string &text);

  /// Updates the tooltip's text.
  void set_text(const std::string &text);
  const std::string &text() const { return m_text; }

  /// Positions and shows a tooltip anchored to the given widget.
  static FluentWebTooltip *show(Widget *anchor, const std::string &text);

  /// Hides and deletes the tooltip.
  void dismiss();

protected:
  void configure_anchor_for(Widget *anchor);

  Label *m_label;
  std::string m_text;
};

NAMESPACE_END(nanogui)
