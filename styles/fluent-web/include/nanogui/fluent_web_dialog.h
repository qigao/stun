#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nanogui/fluent_web_button.h>
#include <nanogui/window.h>

NAMESPACE_BEGIN(nanogui)

class Label;

/**
 * Fluent 2 dialog surface.
 *
 * Provides header, body, and action regions that use Fluent 2 web tokens.
 * Dialogs are modal by default and expose helpers to add primary / secondary
 * actions.
 */
class NANOGUI_EXPORT FluentWebDialog : public Window {
public:
  explicit FluentWebDialog(Widget *parent, const std::string &title = "Dialog");

  /// Sets optional subtitle text shown under the title.
  void set_subtitle(const std::string &text);
  const std::string &subtitle() const { return m_subtitle; }

  /// Makes dialog dismissible (true by default) – controls whether the dismiss button is shown.
  void set_dismissible(bool value);
  bool dismissible() const { return m_dismissible; }

  /// Sets a callback invoked when the dialog is dismissed (after it hides and disposes itself).
  void set_on_dismiss(std::function<void()> callback) { m_on_dismiss = std::move(callback); }

  /// Returns the container that holds custom content widgets.
  Widget *content_container() const { return m_content_container; }

  /// Clears previous actions and removes the buttons from the action row.
  void clear_actions();

  /// Adds an action button to the action row and returns it for additional configuration.
  FluentWebButton *add_action(const std::string &caption,
                              FluentWebButton::Appearance appearance,
                              std::function<void()> callback = {});

  /// Sets or updates the optional body text label within the content area.
  void set_body_text(const std::string &text);

  /// Shows the dialog (sets visible, requests focus, centers within the parent).
  void show();
  /// Dismisses the dialog, triggering the on-dismiss callback and disposing the widget.
  void dismiss();

  /// Updates Fluent specific styling when the theme changes.
  void set_theme(Theme *theme) override;

protected:
  void refresh_chrome();
  void update_metrics();

  Label *m_title_label;
  Label *m_subtitle_label;
  Label *m_body_label;
  Widget *m_header;
  Widget *m_content_container;
  Widget *m_action_row;

  bool m_dismissible;
  std::string m_subtitle;
  std::function<void()> m_on_dismiss;
  std::vector<Widget *> m_action_buttons;
};

NAMESPACE_END(nanogui)

