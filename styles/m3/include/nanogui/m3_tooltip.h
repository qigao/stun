/*
    nanogui/m3_tooltip.h -- Material Design 3 Tooltip

    Implements M3 tooltip with proper styling.

    Based on: https://m3.material.io/components/tooltips

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>

NAMESPACE_BEGIN(nanogui)

/**
 * \class M3Tooltip m3_tooltip.h nanogui/m3_tooltip.h
 *
 * \brief Material Design 3 Tooltip
 *
 * Tooltips display informative text when users hover over an element.
 */
class NANOGUI_EXPORT M3Tooltip : public Widget {
public:
    /// Tooltip position relative to target
    enum class Position {
        TOP,
        BOTTOM,
        LEFT,
        RIGHT,
        AUTO
    };

    /**
     * \brief Construct an M3 tooltip
     *
     * \param parent Parent widget
     * \param text Tooltip text
     */
    M3Tooltip(Widget *parent, const std::string &text = "");

    /// Destructor - ensures proper cleanup with tooltip manager
    virtual ~M3Tooltip();

    /// Set tooltip text
    void set_text(const std::string &text) { m_text = text; }

    /// Get tooltip text
    const std::string &text() const { return m_text; }

    /// Set tooltip position
    void set_position(Position pos) { m_position = pos; }

    /// Get tooltip position
    Position position() const { return m_position; }

    /// Set target widget
    void set_target(Widget *target) { m_target = target; }

    /// Get target widget
    Widget *target() const { return m_target; }

    /// Draw the tooltip
    virtual void draw(NVGcontext *ctx) override;

    /// Preferred size
    virtual Vector2i preferred_size(NVGcontext *ctx) const;

    /// Position tooltip relative to target widget
    void position_relative_to_target();

    /// Show tooltip with fade-in animation
    void animate_show();

    /// Hide tooltip with fade-out animation
    void animate_hide();

    /// Show tooltip after a delay (default 500ms)
    void show_delayed(int delay_ms = 500);

    /// Show tooltip immediately without delay
    void show_immediate();

    /// Hide tooltip immediately without animation
    void hide_immediate(bool notify_manager = true);

    /// Get current opacity
    float opacity() const { return m_opacity; }

    /// Check if currently animating
    bool is_animating() const { return m_animating; }

    /// Set whether tooltip is persistent (doesn't auto-hide)
    void set_persistent(bool persistent) { m_persistent = persistent; }

    /// Check if tooltip is persistent
    bool is_persistent() const { return m_persistent; }
    
    // Accessibility support
    
    /// Get accessibility role (always "tooltip")
    virtual const char* accessibility_role() const { return "tooltip"; }
    
    /// Get accessibility description (returns tooltip text)
    virtual std::string accessibility_description() const { return m_text; }
    
    /// Announce tooltip to screen readers (platform-specific implementation would go here)
    virtual void announce_to_screen_reader() const;

protected:
    /// Get M3 theme
    M3Theme *m3_theme() const;

    /// Update animation state (called each frame)
    void update_animation(float dt);

    std::string m_text;
    Position m_position = Position::AUTO;
    Widget *m_target = nullptr;
    float m_opacity = 0.0f;
    bool m_animating = false;
    bool m_persistent = false;
    
    // Timing state
    float m_show_timer = 0.0f;
    int m_show_delay = 0;
    bool m_pending_show = false;
    
    // Animation state
    float m_animation_time = 0.0f;
    float m_animation_duration = 0.0f;
    bool m_fading_in = false;
    bool m_fading_out = false;
};

/**
 * \class M3RichTooltip m3_tooltip.h nanogui/m3_tooltip.h
 *
 * \brief Material Design 3 Rich Tooltip
 *
 * Rich tooltips provide more detailed information with optional title, supporting text,
 * icon, and action buttons. They remain visible until explicitly dismissed or an action
 * is clicked.
 */
class NANOGUI_EXPORT M3RichTooltip : public M3Tooltip {
public:
    /// Action button for rich tooltip
    struct Action {
        std::string label;
        std::function<void()> callback;
    };

    /**
     * \brief Construct an M3 rich tooltip
     *
     * \param parent Parent widget
     */
    M3RichTooltip(Widget *parent);

    /// Set tooltip title
    void set_title(const std::string &title) { m_title = title; }

    /// Get tooltip title
    const std::string &title() const { return m_title; }

    /// Set supporting text
    void set_supporting_text(const std::string &text) { m_supporting_text = text; }

    /// Get supporting text
    const std::string &supporting_text() const { return m_supporting_text; }

    /// Set icon (FontAwesome icon code)
    void set_icon(int icon) { m_icon = icon; }

    /// Get icon
    int icon() const { return m_icon; }

    /// Add an action button
    void add_action(const std::string &label, const std::function<void()> &callback);

    /// Clear all action buttons
    void clear_actions();

    /// Get actions
    const std::vector<Action> &actions() const { return m_actions; }

    /// Draw the rich tooltip
    void draw(NVGcontext *ctx) override;

    /// Preferred size (larger than basic tooltip)
    Vector2i preferred_size(NVGcontext *ctx) const override;

    /// Handle mouse button events for action clicks
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

    /// Handle mouse motion events for action hover
    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) override;

protected:
    std::string m_title;
    std::string m_supporting_text;
    int m_icon = 0;
    std::vector<Action> m_actions;
    
    // Track hovered action for visual feedback
    int m_hovered_action = -1;
};

NAMESPACE_END(nanogui)

