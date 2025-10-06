/*
    nanogui/m3_dialog.h -- Material Design 3 Dialog

    Implements M3 modal dialog with proper styling.

    Based on: https://m3.material.io/components/dialogs

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/m3_theme.h>
#include <nanogui/window.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Dialog m3_dialog.h nanogui/m3_dialog.h
 *
 * \brief Material Design 3 Dialog
 *
 * Dialogs provide important prompts in a user flow.
 */
class NANOGUI_EXPORT M3Dialog : public Window {
public:
  /// Dialog type enum
  enum class DialogType {
    BASIC,      ///< Standard centered dialog
    FULLSCREEN  ///< Full-screen dialog with app bar
  };

  /// Action button structure
  struct Action {
    std::string label;
    std::function<void()> callback;
    bool enabled = true;
    // Layout information (calculated in layout_actions)
    Vector2f position;
    Vector2f size;
  };

  /**
   * \brief Construct an M3 dialog
   *
   * \param parent Parent widget (usually Screen)
   * \param title Dialog title
   * \param type Dialog type (BASIC or FULLSCREEN)
   */
  M3Dialog(Widget *parent, const std::string &title = "Dialog", 
           DialogType type = DialogType::BASIC);

  /// Set dialog icon
  void set_icon(int icon) { m_icon = icon; }

  /// Get dialog icon
  int icon() const { return m_icon; }

  /// Add action button
  void add_action(const std::string &label, const std::function<void()> &callback);

  /// Clear all actions
  void clear_actions() { m_actions.clear(); }

  /// Get actions
  const std::vector<Action> &actions() const { return m_actions; }

  /// Set content widget
  void set_content(Widget *content);

  /// Set content as simple text (creates a Label widget)
  void set_content_text(const std::string &text);

  /// Get content widget
  Widget *content() const { return m_content_area; }

  /// Set callback invoked when dialog is shown
  void set_on_show(const std::function<void()> &callback) { m_on_show = callback; }

  /// Set callback invoked when dialog is dismissed
  void set_on_dismiss(const std::function<void()> &callback) { m_on_dismiss = callback; }

  /// Set whether dialog can be dismissed by clicking scrim or pressing Escape
  void set_dismissible(bool dismissible) { m_dismissible = dismissible; }

  /// Get whether dialog is dismissible
  bool dismissible() const { return m_dismissible; }

  /// Show the dialog and invoke onShow callback
  void show();

  /// Hide the dialog and invoke onDismiss callback
  void hide();

  /// Perform layout
  void perform_layout(NVGcontext *ctx) override;

  /// Draw the dialog
  void draw(NVGcontext *ctx) override;

  /// Handle mouse button events
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

  /// Handle mouse motion events
  bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

  /// Handle keyboard events
  bool keyboard_event(int key, int scancode, int action, int modifiers) override;

  // Accessibility support
  
  /// Get accessibility role (always "dialog")
  virtual const char* accessibility_role() const { return "dialog"; }
  
  /// Check if dialog is modal
  bool is_modal() const { return true; }
  
  /// Get accessibility label (returns title)
  virtual std::string accessibility_label() const { return std::string(title()); }
  
  /// Get accessibility description (returns content text if available)
  virtual std::string accessibility_description() const;
  
  // Focus management for accessibility
  
  /// Get all focusable widgets within the dialog
  std::vector<Widget*> get_focusable_widgets() const;
  
  /// Move focus to the first focusable element
  void focus_first_element();
  
  /// Move focus to the next focusable element (for Tab key)
  void focus_next_element();
  
  /// Move focus to the previous focusable element (for Shift+Tab)
  void focus_previous_element();

protected:
  /// Get M3 theme
  M3Theme *m3_theme() const;

  /// Layout action buttons
  void layout_actions();

  /// Calculate action area height
  float calculate_action_area_height() const;
  
  /// Start show animation for full-screen dialog
  void animate_show();
  
  /// Start hide animation for full-screen dialog
  void animate_hide();
  
  /// Update animation state (called from draw)
  void update_animation(float dt);
  
  /// Apply easing curve to animation progress
  float ease_in_out(float t) const;

  DialogType m_type;  // Dialog type (BASIC or FULLSCREEN)
  int m_icon = 0;
  std::vector<Action> m_actions;
  Widget *m_content_area = nullptr;  // Content widget (can be any widget, including VScrollPanel)
  std::function<void()> m_on_show;
  std::function<void()> m_on_dismiss;
  bool m_dismissible = true;  // Whether dialog can be dismissed by scrim click or Escape key
  
  // Full-screen dialog app bar
  struct AppBarButton {
    Vector2f position;
    Vector2f size;
    bool hovered = false;
    bool pressed = false;
  };
  AppBarButton m_close_button;  // Close button for full-screen dialog
  
  // Interaction state tracking
  int m_hovered_action = -1;  // Index of hovered action button (-1 = none)
  int m_pressed_action = -1;  // Index of pressed action button (-1 = none)
  
  // Animation state for full-screen dialog slide animations
  float m_animation_progress = 1.0f;  // 0.0 = hidden, 1.0 = fully visible
  bool m_animating = false;           // Whether animation is in progress
  bool m_showing = false;             // True for show animation, false for hide
  float m_animation_duration = 0.3f;  // Animation duration in seconds (300ms show, 250ms hide)
  float m_animation_time = 0.0f;      // Current animation time
  
  // Focus management for accessibility
  int m_focused_widget_index = -1;    // Index of currently focused widget in focusable list
};

NAMESPACE_END(nanogui)
