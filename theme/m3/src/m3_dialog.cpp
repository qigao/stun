/*
    src/m3_dialog.cpp -- Material Design 3 Dialog implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    
    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_dialog.h>
#include <nanogui/opengl.h>
#include <nanogui/label.h>
#include <nanogui/vscrollpanel.h>
#include <nanogui/screen.h>
#include <algorithm>
#include <typeinfo>
#include <GLFW/glfw3.h>

NAMESPACE_BEGIN(nanogui)

M3Dialog::M3Dialog(Widget *parent, const std::string &title, DialogType type)
    : Window(parent, title), m_type(type) {
    // M3 dialogs have specific styling
    set_modal(true);
    
    // Set default size for basic dialogs (will be overridden by set_fixed_width if called)
    if (m_type == DialogType::BASIC) {
        set_fixed_width(400);
        set_fixed_height(200);  // Initial height, will adjust based on content
    }
}

void M3Dialog::show() {
    set_visible(true);
    
    // Ensure dialog is laid out before showing
    Screen *scr = screen();
    if (scr) {
        // Perform layout to calculate proper size
        perform_layout(scr->nvg_context());
        
        // Center basic dialogs (full-screen dialogs fill the screen)
        if (m_type == DialogType::BASIC) {
            center();
        }
        
        // Request screen redraw
        scr->perform_layout();
    }
    
    // Start animation for all dialog types
    animate_show();
    
    // Move focus to first focusable element for accessibility
    focus_first_element();
    
    if (m_on_show) {
        m_on_show();
    }
}

void M3Dialog::hide() {
    // Start animation for all dialog types
    if (m_visible) {
        animate_hide();
        // Don't set invisible immediately - let animation complete
        // The animation will hide the dialog when done
    }
    
    if (m_on_dismiss) {
        m_on_dismiss();
    }
}

void M3Dialog::animate_show() {
    M3Theme *theme = m3_theme();
    
    // Check if animations are disabled (accessibility preference)
    if (theme && !theme->animations_enabled()) {
        // Skip animation - show immediately
        m_animation_progress = 1.0f;
        m_animating = false;
        return;
    }
    
    m_animating = true;
    m_showing = true;
    m_animation_progress = 0.0f;
    m_animation_time = 0.0f;
    
    // Different durations for different dialog types
    if (m_type == DialogType::FULLSCREEN) {
        m_animation_duration = 0.3f;  // 300ms for full-screen slide
    } else {
        m_animation_duration = 0.2f;  // 200ms for basic dialog scale+fade
    }
}

void M3Dialog::animate_hide() {
    M3Theme *theme = m3_theme();
    
    // Check if animations are disabled (accessibility preference)
    if (theme && !theme->animations_enabled()) {
        // Skip animation - hide immediately
        m_animation_progress = 0.0f;
        m_animating = false;
        set_visible(false);
        return;
    }
    
    m_animating = true;
    m_showing = false;
    m_animation_progress = 1.0f;
    m_animation_time = 0.0f;
    
    // Different durations for different dialog types
    if (m_type == DialogType::FULLSCREEN) {
        m_animation_duration = 0.25f;  // 250ms for full-screen slide
    } else {
        m_animation_duration = 0.15f;  // 150ms for basic dialog scale+fade
    }
}

void M3Dialog::update_animation(float dt) {
    if (!m_animating) {
        return;
    }
    
    m_animation_time += dt;
    
    // Calculate linear progress (0.0 to 1.0)
    float linear_progress = std::min(1.0f, m_animation_time / m_animation_duration);
    
    // Apply easing curve
    float eased_progress = ease_in_out(linear_progress);
    
    if (m_showing) {
        // Show animation: progress from 0.0 to 1.0
        m_animation_progress = eased_progress;
    } else {
        // Hide animation: progress from 1.0 to 0.0
        m_animation_progress = 1.0f - eased_progress;
    }
    
    // Check if animation is complete
    if (linear_progress >= 1.0f) {
        m_animating = false;
        m_animation_progress = m_showing ? 1.0f : 0.0f;
        
        // If hiding, set invisible now that animation is complete
        if (!m_showing) {
            set_visible(false);
        }
    }
}

float M3Dialog::ease_in_out(float t) const {
    // Standard ease-in-out curve (cubic)
    // Accelerates from zero velocity and decelerates to zero velocity
    if (t < 0.5f) {
        return 4.0f * t * t * t;
    } else {
        float f = (2.0f * t - 2.0f);
        return 0.5f * f * f * f + 1.0f;
    }
}

void M3Dialog::add_action(const std::string &label, const std::function<void()> &callback) {
    Action action;
    action.label = label;
    action.callback = callback;
    action.enabled = true;
    m_actions.push_back(action);
}

void M3Dialog::set_content(Widget *content) {
    // Remove existing content if any
    if (m_content_area && m_content_area->parent() == this) {
        remove_child(m_content_area);
    }
    
    m_content_area = content;
    
    // Add new content as child if provided and not already a child
    if (m_content_area) {
        if (m_content_area->parent() == nullptr) {
            // Content has no parent, add it as child
            add_child(-1, m_content_area);
        } else if (m_content_area->parent() != this) {
            // Content has a different parent, this is an error case
            // For safety, we'll just store the reference but not reparent
            // The caller should ensure content is created with correct parent
        }
        // If parent is already this, it's already a child, do nothing
    }
}

void M3Dialog::set_content_text(const std::string &text) {
    M3Theme *theme = m3_theme();
    
    // Remove existing content if any
    if (m_content_area && m_content_area->parent() == this) {
        remove_child(m_content_area);
        m_content_area = nullptr;
    }
    
    // Create a label widget for simple text content with dialog as parent
    Label *label = new Label(this, text, "sans", 14);
    
    if (theme) {
        label->set_color(theme->on_surface());
    }
    
    // Enable text wrapping by setting a fixed width (will be adjusted in layout)
    // Use the dialog's width if available, otherwise use default
    int dialog_width = m_size.x() > 0 ? m_size.x() : 400;
    label->set_fixed_width(dialog_width - 32);  // Dialog width minus padding
    
    // Store reference to content area (already added as child in Label constructor)
    m_content_area = label;
}

float M3Dialog::calculate_action_area_height() const {
    if (m_actions.empty()) {
        return 0.0f;
    }

    const float button_height = 40.0f;
    const float button_spacing = 8.0f;
    const float padding = 24.0f;
    
    if (m_actions.size() <= 2) {
        // Horizontal layout: single row of buttons
        return padding + button_height + padding;
    } else {
        // Vertical layout: stacked buttons
        float total_height = padding;
        total_height += button_height * m_actions.size();
        total_height += button_spacing * (m_actions.size() - 1);
        total_height += padding;
        return total_height;
    }
}

void M3Dialog::layout_actions() {
    // Action buttons are laid out at the bottom of the dialog
    // Horizontal layout for â‰? actions, vertical for >2
    // All right-aligned with 8dp spacing
    
    if (m_actions.empty()) {
        return;
    }

    M3Theme *theme = m3_theme();
    NVGcontext *ctx = screen()->nvg_context();
    if (!theme || !ctx) {
        return;
    }

    const float button_height = 40.0f;
    const float button_spacing = 8.0f;
    const float padding = 24.0f;
    const float button_padding_h = 24.0f;
    
    float dialog_width = m_size.x();
    float dialog_height = m_size.y();
    
    // Set up font for text measurement
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans-bold");
    
    if (m_actions.size() <= 2) {
        // Horizontal layout - right-aligned with 8dp spacing
        float button_x = dialog_width - padding;
        float button_y = dialog_height - padding - button_height;
        
        // Layout from right to left
        for (auto it = m_actions.rbegin(); it != m_actions.rend(); ++it) {
            Action &action = const_cast<Action&>(*it);
            
            // Calculate button width based on text
            float text_width = nvgTextBounds(ctx, 0, 0, action.label.c_str(), nullptr, nullptr);
            float button_width = text_width + button_padding_h * 2;
            
            // Position button
            button_x -= button_width;
            action.position = Vector2f(button_x, button_y);
            action.size = Vector2f(button_width, button_height);
            
            // Add spacing for next button
            button_x -= button_spacing;
        }
    } else {
        // Vertical layout - right-aligned, stacked with 8dp spacing
        float button_y = dialog_height - padding - button_height;
        
        // Layout from bottom to top
        for (auto it = m_actions.rbegin(); it != m_actions.rend(); ++it) {
            Action &action = const_cast<Action&>(*it);
            
            // Calculate button width based on text
            float text_width = nvgTextBounds(ctx, 0, 0, action.label.c_str(), nullptr, nullptr);
            float button_width = text_width + button_padding_h * 2;
            
            // Position button (right-aligned)
            float button_x = dialog_width - padding - button_width;
            action.position = Vector2f(button_x, button_y);
            action.size = Vector2f(button_width, button_height);
            
            // Move up for next button
            button_y -= (button_height + button_spacing);
        }
    }
}

bool M3Dialog::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    // First, let the base class handle the event
    if (Window::mouse_button_event(p, button, down, modifiers)) {
        return true;
    }

    // Check if click is on close button (full-screen dialog only)
    if (button == 0 && m_type == DialogType::FULLSCREEN) {
        float x = m_pos.x();
        float y = m_pos.y();
        float button_x = x + m_close_button.position.x();
        float button_y = y + m_close_button.position.y();
        float button_w = m_close_button.size.x();
        float button_h = m_close_button.size.y();
        
        if (p.x() >= button_x && p.x() <= button_x + button_w &&
            p.y() >= button_y && p.y() <= button_y + button_h) {
            
            if (down) {
                m_close_button.pressed = true;
                return true;
            } else {
                if (m_close_button.pressed) {
                    // Close button clicked - hide dialog
                    hide();
                }
                m_close_button.pressed = false;
                return true;
            }
        }
        
        // Click outside close button - clear pressed state
        if (!down) {
            m_close_button.pressed = false;
        }
    }

    // Check if click is on an action button using pre-calculated positions
    if (button == 0 && !m_actions.empty()) {  // Left mouse button
        float x = m_pos.x();
        float y = m_pos.y();
        
        for (size_t i = 0; i < m_actions.size(); ++i) {
            const auto &action = m_actions[i];
            float button_x = x + action.position.x();
            float button_y = y + action.position.y();
            float button_w = action.size.x();
            float button_h = action.size.y();
            
            // Check if click is within button bounds
            if (p.x() >= button_x && p.x() <= button_x + button_w &&
                p.y() >= button_y && p.y() <= button_y + button_h) {
                
                if (down) {
                    // Button pressed
                    m_pressed_action = static_cast<int>(i);
                    return true;
                } else {
                    // Button released
                    if (m_pressed_action == static_cast<int>(i) && action.enabled && action.callback) {
                        // Invoke callback - callback is responsible for closing dialog if needed
                        action.callback();
                    }
                    m_pressed_action = -1;
                    return true;
                }
            }
        }
        
        // Click outside buttons - clear pressed state
        if (!down) {
            m_pressed_action = -1;
        }
    }

    // Check for scrim click (click outside dialog bounds)
    if (button == 0 && !down && m_dismissible) {  // Left mouse button release
        float x = m_pos.x();
        float y = m_pos.y();
        float w = m_size.x();
        float h = m_size.y();
        
        // Check if click is outside dialog bounds
        if (p.x() < x || p.x() > x + w || p.y() < y || p.y() > y + h) {
            // Click on scrim - close dialog
            hide();
            return true;
        }
    }

    return false;
}

bool M3Dialog::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    // First, let the base class handle the event
    if (Window::mouse_motion_event(p, rel, button, modifiers)) {
        return true;
    }

    // Track close button hover state (full-screen dialog only)
    bool prev_close_hovered = m_close_button.hovered;
    m_close_button.hovered = false;
    
    if (m_type == DialogType::FULLSCREEN) {
        float x = m_pos.x();
        float y = m_pos.y();
        float button_x = x + m_close_button.position.x();
        float button_y = y + m_close_button.position.y();
        float button_w = m_close_button.size.x();
        float button_h = m_close_button.size.y();
        
        if (p.x() >= button_x && p.x() <= button_x + button_w &&
            p.y() >= button_y && p.y() <= button_y + button_h) {
            m_close_button.hovered = true;
        }
    }

    // Track which action button is being hovered
    int prev_hovered = m_hovered_action;
    m_hovered_action = -1;
    
    if (!m_actions.empty()) {
        float x = m_pos.x();
        float y = m_pos.y();
        
        for (size_t i = 0; i < m_actions.size(); ++i) {
            const auto &action = m_actions[i];
            float button_x = x + action.position.x();
            float button_y = y + action.position.y();
            float button_w = action.size.x();
            float button_h = action.size.y();
            
            // Check if mouse is within button bounds
            if (p.x() >= button_x && p.x() <= button_x + button_w &&
                p.y() >= button_y && p.y() <= button_y + button_h) {
                m_hovered_action = static_cast<int>(i);
                break;
            }
        }
    }
    
    // Request redraw if hover state changed
    if (prev_hovered != m_hovered_action || prev_close_hovered != m_close_button.hovered) {
        return true;
    }

    return false;
}

void M3Dialog::perform_layout(NVGcontext *ctx) {
    // Handle full-screen dialog layout
    if (m_type == DialogType::FULLSCREEN) {
        // Set size to fill entire screen
        Screen *scr = screen();
        if (scr) {
            set_position(Vector2i(0, 0));
            set_size(Vector2i(scr->width(), scr->height()));
        }
    }
    
    // Call base class layout first
    Window::perform_layout(ctx);
    
    // Layout content area if present
    if (m_content_area) {
        const float header_height = 64.0f;  // Title area height
        const float padding_h = 16.0f;      // Horizontal padding for content
        const float padding_top = 16.0f;    // Top padding below header
        const float padding_bottom = 16.0f; // Bottom padding above actions
        
        float action_area_height = calculate_action_area_height();
        
        // Calculate available content height
        float available_height = m_size.y() - header_height - padding_top - padding_bottom - action_area_height;
        
        // Position content area
        float content_x = padding_h;
        float content_y = header_height + padding_top;
        float content_width = m_size.x() - (padding_h * 2);
        
        // Get the preferred size of the content
        Vector2i content_preferred = m_content_area->preferred_size(ctx);
        float content_preferred_height = content_preferred.y();
        
        // For now, don't auto-wrap in scroll panel as it causes issues
        // Users can manually create VScrollPanel if needed
        // Just position and size the content area
        
        // Check if content is already a VScrollPanel
        VScrollPanel *scroll_panel = dynamic_cast<VScrollPanel*>(m_content_area);
        if (scroll_panel) {
            // Already a scroll panel, position and size it to use available height
            scroll_panel->set_position(Vector2i(content_x, content_y));
            scroll_panel->set_fixed_size(Vector2i(content_width, available_height));
        } else {
            // Regular content - position and size based on preferred size
            // Clamp height to available space
            float content_height = std::min(content_preferred_height, available_height);
            m_content_area->set_position(Vector2i(content_x, content_y));
            m_content_area->set_fixed_size(Vector2i(content_width, content_height));
        }
        
        // Update label width if content is a label (for text wrapping)
        Label *label = dynamic_cast<Label*>(m_content_area);
        if (label) {
            label->set_fixed_width(content_width);
        }
        
        // Perform layout on content
        m_content_area->perform_layout(ctx);
    }
    
    // Layout action buttons
    layout_actions();
}

M3Theme *M3Dialog::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

bool M3Dialog::keyboard_event(int key, int scancode, int action, int modifiers) {
    // Handle Tab key for focus cycling (accessibility focus trap)
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        if (modifiers & GLFW_MOD_SHIFT) {
            // Shift+Tab: move to previous focusable element
            focus_previous_element();
        } else {
            // Tab: move to next focusable element
            focus_next_element();
        }
        return true;
    }
    
    // First, let the base class handle the event
    if (Window::keyboard_event(key, scancode, action, modifiers)) {
        return true;
    }

    // Handle Escape key to close dialog if dismissible
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS && m_dismissible) {
        hide();
        return true;
    }

    return false;
}

void M3Dialog::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) {
        Window::draw(ctx);
        return;
    }

    // Update animation state
    if (m_animating) {
        // Calculate delta time (approximate - using 60fps as baseline)
        float dt = 1.0f / 60.0f;
        update_animation(dt);
    }

    float x = m_pos.x();
    float y = m_pos.y();
    float w = m_size.x();
    float h = m_size.y();
    float corner_radius = theme->corner_radius(M3Theme::ShapeFamily::ExtraLarge);

    nvgSave(ctx);
    
    // Apply transforms based on dialog type and animation state
    if (m_type == DialogType::FULLSCREEN) {
        // Apply slide transform for full-screen dialog animation
        if (m_animation_progress < 1.0f) {
            // Slide from right: translate by (1.0 - progress) * width
            float offset = (1.0f - m_animation_progress) * w;
            nvgTranslate(ctx, offset, 0);
        }
    } else {
        // Apply scale and fade transform for basic dialog animation
        // Scale from 0.8 to 1.0 on show, 1.0 to 0.8 on hide
        float scale = 0.8f + (m_animation_progress * 0.2f);
        
        // Apply fade (opacity)
        nvgGlobalAlpha(ctx, m_animation_progress);
        
        // Calculate center point for scaling
        float center_x = x + w * 0.5f;
        float center_y = y + h * 0.5f;
        
        // Apply scale transform around center
        nvgTranslate(ctx, center_x, center_y);
        nvgScale(ctx, scale, scale);
        nvgTranslate(ctx, -center_x, -center_y);
    }

    // Draw scrim (background overlay) - only for basic dialogs
    if (m_modal && m_type == DialogType::BASIC) {
        nvgBeginPath(ctx);
        nvgRect(ctx, 0, 0, m_parent->width(), m_parent->height());
        nvgFillColor(ctx, Color(theme->scrim().r(), theme->scrim().g(), 
                               theme->scrim().b(), 0.32f));
        nvgFill(ctx);
    }

    // Draw shadow - only for basic dialogs
    if (m_type == DialogType::BASIC) {
        NVGpaint shadow = nvgBoxGradient(ctx, x, y + 4, w, h, corner_radius, 10.0f,
                                         nvgRGBAf(0, 0, 0, 0.25f),
                                         nvgRGBAf(0, 0, 0, 0));
        nvgBeginPath(ctx);
        nvgRect(ctx, x - 10, y - 10, w + 20, h + 30);
        nvgRoundedRect(ctx, x, y, w, h, corner_radius);
        nvgPathWinding(ctx, NVG_HOLE);
        nvgFillPaint(ctx, shadow);
        nvgFill(ctx);
    }

    // Draw dialog background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, corner_radius);
    
    // Use surface color for full-screen, surface-container-high for basic
    if (m_type == DialogType::FULLSCREEN) {
        nvgFillColor(ctx, theme->surface());
    } else {
        nvgFillColor(ctx, theme->surface());
    }
    nvgFill(ctx);

    // Draw elevation tint (only for basic dialogs)
    if (m_type == DialogType::BASIC) {
        Color tint = theme->elevation_tint(M3Theme::Elevation::Level3);
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, x, y, w, h, corner_radius);
        nvgFillColor(ctx, tint);
        nvgFill(ctx);
    }

    // Draw header
    float header_height = 64.0f;
    
    // For full-screen dialogs, draw app bar with close button
    if (m_type == DialogType::FULLSCREEN) {
        // Draw app bar background (same as dialog surface)
        nvgBeginPath(ctx);
        nvgRect(ctx, x, y, w, header_height);
        nvgFillColor(ctx, theme->surface());
        nvgFill(ctx);
        
        // Draw close button (X icon) on the left
        float button_size = 48.0f;
        float button_x = 4.0f;  // Relative to dialog
        float button_y = (header_height - button_size) * 0.5f;  // Relative to dialog
        
        // Store button position for hit testing (relative to dialog)
        m_close_button.position = Vector2f(button_x, button_y);
        m_close_button.size = Vector2f(button_size, button_size);
        
        // Absolute position for drawing
        float abs_button_x = x + button_x;
        float abs_button_y = y + button_y;
        
        // Draw state layer for hover/press
        if (m_close_button.pressed) {
            nvgBeginPath(ctx);
            nvgCircle(ctx, abs_button_x + button_size * 0.5f, abs_button_y + button_size * 0.5f, button_size * 0.5f);
            Color state = theme->state_layer(theme->on_surface(), 0.12f);
            nvgFillColor(ctx, state);
            nvgFill(ctx);
        } else if (m_close_button.hovered) {
            nvgBeginPath(ctx);
            nvgCircle(ctx, abs_button_x + button_size * 0.5f, abs_button_y + button_size * 0.5f, button_size * 0.5f);
            Color state = theme->state_layer(theme->on_surface(), 0.08f);
            nvgFillColor(ctx, state);
            nvgFill(ctx);
        }
        
        // Draw X icon (close icon)
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface());
        nvgText(ctx, abs_button_x + button_size * 0.5f, abs_button_y + button_size * 0.5f, 
                utf8(0xf00d).data(), nullptr);
        
        // Draw title to the right of close button
        nvgFontSize(ctx, 22);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface());
        nvgText(ctx, abs_button_x + button_size + 16.0f, y + header_height * 0.5f, m_title.c_str(), nullptr);
    } else {
        // Basic dialog header
        // Draw icon if present
        float icon_x = x + 24;
        float title_x = icon_x;
        
        if (m_icon) {
            nvgFontSize(ctx, 24);
            nvgFontFace(ctx, "icons");
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(ctx, theme->on_surface());
            nvgText(ctx, icon_x, y + header_height * 0.5f, utf8(m_icon).data(), nullptr);
            title_x += 40;
        }

        // Draw title
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface());
        nvgText(ctx, title_x, y + header_height * 0.5f, m_title.c_str(), nullptr);
    }

    // Draw action buttons using pre-calculated layout
    if (!m_actions.empty()) {
        nvgFontSize(ctx, 14);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        
        for (size_t i = 0; i < m_actions.size(); ++i) {
            const auto &action = m_actions[i];
            float button_x = x + action.position.x();
            float button_y = y + action.position.y();
            float button_w = action.size.x();
            float button_h = action.size.y();
            
            // Draw state layer for hover/press states (M3 text button style)
            if (action.enabled) {
                if (m_pressed_action == static_cast<int>(i)) {
                    // Pressed state: 12% opacity state layer
                    nvgBeginPath(ctx);
                    nvgRoundedRect(ctx, button_x, button_y, button_w, button_h, 20.0f);
                    Color state = theme->state_layer(theme->primary(), 0.12f);
                    nvgFillColor(ctx, state);
                    nvgFill(ctx);
                } else if (m_hovered_action == static_cast<int>(i)) {
                    // Hover state: 8% opacity state layer
                    nvgBeginPath(ctx);
                    nvgRoundedRect(ctx, button_x, button_y, button_w, button_h, 20.0f);
                    Color state = theme->state_layer(theme->primary(), 0.08f);
                    nvgFillColor(ctx, state);
                    nvgFill(ctx);
                }
            }
            
            // Draw button text (M3 text button style - no background fill)
            nvgFillColor(ctx, action.enabled ? theme->primary() : 
                         Color(theme->on_surface().r(), theme->on_surface().g(),
                              theme->on_surface().b(), 0.38f));
            nvgText(ctx, button_x + button_w * 0.5f,
                    button_y + button_h * 0.5f,
                    action.label.c_str(), nullptr);
        }
    }

    nvgRestore(ctx);

    // Draw children
    Widget::draw(ctx);
}

std::string M3Dialog::accessibility_description() const {
    // Return content text if it's a Label widget
    if (m_content_area) {
        Label *label = dynamic_cast<Label*>(m_content_area);
        if (label) {
            return std::string(label->caption());
        }
    }
    return "";
}

std::vector<Widget*> M3Dialog::get_focusable_widgets() const {
    std::vector<Widget*> focusable;
    
    // Helper function to recursively find focusable widgets
    std::function<void(Widget*)> find_focusable = [&](Widget* widget) {
        if (!widget || !widget->visible() || !widget->enabled()) {
            return;
        }
        
        // Check if widget is focusable (buttons, textboxes, etc.)
        // In NanoGUI, buttons and interactive widgets are typically focusable
        // We'll check for common interactive widget types
        const std::string& widget_id = typeid(*widget).name();
        
        // Add widget if it's likely focusable (contains "Button", "TextBox", "Slider", etc.)
        if (widget_id.find("Button") != std::string::npos ||
            widget_id.find("TextBox") != std::string::npos ||
            widget_id.find("Slider") != std::string::npos ||
            widget_id.find("CheckBox") != std::string::npos ||
            widget_id.find("ComboBox") != std::string::npos) {
            focusable.push_back(widget);
        }
        
        // Recursively check children
        for (auto child : widget->children()) {
            find_focusable(child);
        }
    };
    
    // Start from content area
    if (m_content_area) {
        find_focusable(m_content_area);
    }
    
    // Note: Action buttons are not child widgets, they're drawn directly
    // So we don't include them in the focusable list for now
    
    return focusable;
}

void M3Dialog::focus_first_element() {
    auto focusable = get_focusable_widgets();
    
    if (!focusable.empty()) {
        focusable[0]->request_focus();
        m_focused_widget_index = 0;
    } else {
        m_focused_widget_index = -1;
    }
}

void M3Dialog::focus_next_element() {
    auto focusable = get_focusable_widgets();
    
    if (focusable.empty()) {
        return;
    }
    
    // Move to next element, wrapping around to start
    m_focused_widget_index = (m_focused_widget_index + 1) % static_cast<int>(focusable.size());
    focusable[m_focused_widget_index]->request_focus();
}

void M3Dialog::focus_previous_element() {
    auto focusable = get_focusable_widgets();
    
    if (focusable.empty()) {
        return;
    }
    
    // Move to previous element, wrapping around to end
    m_focused_widget_index--;
    if (m_focused_widget_index < 0) {
        m_focused_widget_index = static_cast<int>(focusable.size()) - 1;
    }
    focusable[m_focused_widget_index]->request_focus();
}

NAMESPACE_END(nanogui)
