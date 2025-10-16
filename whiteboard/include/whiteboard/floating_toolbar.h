/**
 * \file floating_toolbar.h
 * \brief Floating toolbar that appears near selection for quick actions
 */

#pragma once

#include <functional>
#include <nanogui/button.h>
#include <nanogui/layout.h>
#include <nanogui/widget.h>


namespace whiteboard {

/**
 * \class FloatingToolbar
 * \brief A floating toolbar that appears near selected objects with quick actions
 */
class FloatingToolbar : public nanogui::Widget {
public:
  FloatingToolbar(nanogui::Widget *parent)
      : nanogui::Widget(parent), m_background_color(40, 40, 40, 240), m_corner_radius(8.0f),
        m_current_alpha(0.0f), m_target_alpha(0.0f) {
    set_layout(
        new nanogui::BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Middle, 5, 8));
    set_visible(false);
  }

  /**
   * Show toolbar at specified position with relevant actions
   */
  void show_at(const nanogui::Vector2i &pos, bool can_group, bool can_ungroup, int num_selected) {
    // Clear existing buttons
    while (!children().empty()) {
      remove_child(children().back());
    }

    // Add relevant buttons based on selection state
    if (can_group && num_selected > 1) {
      auto *group_btn = new nanogui::Button(this, "", FA_OBJECT_GROUP);
      group_btn->set_tooltip("Group (Ctrl+G)");
      group_btn->set_font_size(18);
      group_btn->set_fixed_size(nanogui::Vector2i(36, 36));
      group_btn->set_callback([this]() {
        if (m_group_callback)
          m_group_callback();
      });
    }

    if (can_ungroup) {
      auto *ungroup_btn = new nanogui::Button(this, "", FA_OBJECT_UNGROUP);
      ungroup_btn->set_tooltip("Ungroup (Ctrl+Shift+G)");
      ungroup_btn->set_font_size(18);
      ungroup_btn->set_fixed_size(nanogui::Vector2i(36, 36));
      ungroup_btn->set_callback([this]() {
        if (m_ungroup_callback)
          m_ungroup_callback();
      });
    }

    // Alignment buttons (only show for multiple selections)
    if (num_selected > 1) {
      auto *align_left_btn = new nanogui::Button(this, "", FA_ALIGN_LEFT);
      align_left_btn->set_tooltip("Align Left");
      align_left_btn->set_font_size(18);
      align_left_btn->set_fixed_size(nanogui::Vector2i(36, 36));
      align_left_btn->set_callback([this]() {
        if (m_align_left_callback)
          m_align_left_callback();
      });

      auto *align_right_btn = new nanogui::Button(this, "", FA_ALIGN_RIGHT);
      align_right_btn->set_tooltip("Align Right");
      align_right_btn->set_font_size(18);
      align_right_btn->set_fixed_size(nanogui::Vector2i(36, 36));
      align_right_btn->set_callback([this]() {
        if (m_align_right_callback)
          m_align_right_callback();
      });

      auto *align_top_btn = new nanogui::Button(this, "", FA_ARROW_UP);
      align_top_btn->set_tooltip("Align Top");
      align_top_btn->set_font_size(18);
      align_top_btn->set_fixed_size(nanogui::Vector2i(36, 36));
      align_top_btn->set_callback([this]() {
        if (m_align_top_callback)
          m_align_top_callback();
      });

      auto *align_bottom_btn = new nanogui::Button(this, "", FA_ARROW_DOWN);
      align_bottom_btn->set_tooltip("Align Bottom");
      align_bottom_btn->set_font_size(18);
      align_bottom_btn->set_fixed_size(nanogui::Vector2i(36, 36));
      align_bottom_btn->set_callback([this]() {
        if (m_align_bottom_callback)
          m_align_bottom_callback();
      });
    }

    // Duplicate button
    auto *duplicate_btn = new nanogui::Button(this, "", FA_COPY);
    duplicate_btn->set_tooltip("Duplicate (Ctrl+D)");
    duplicate_btn->set_font_size(18);
    duplicate_btn->set_fixed_size(nanogui::Vector2i(36, 36));
    duplicate_btn->set_callback([this]() {
      if (m_duplicate_callback)
        m_duplicate_callback();
    });

    // Bring forward button
    auto *forward_btn = new nanogui::Button(this, "", FA_CHEVRON_UP);
    forward_btn->set_tooltip("Bring Forward (Ctrl+])");
    forward_btn->set_font_size(18);
    forward_btn->set_fixed_size(nanogui::Vector2i(36, 36));
    forward_btn->set_callback([this]() {
      if (m_bring_forward_callback)
        m_bring_forward_callback();
    });

    // Send backward button
    auto *backward_btn = new nanogui::Button(this, "", FA_CHEVRON_DOWN);
    backward_btn->set_tooltip("Send Backward (Ctrl+[)");
    backward_btn->set_font_size(18);
    backward_btn->set_fixed_size(nanogui::Vector2i(36, 36));
    backward_btn->set_callback([this]() {
      if (m_send_backward_callback)
        m_send_backward_callback();
    });

    // Delete button (with red color)
    auto *delete_btn = new nanogui::Button(this, "", FA_TRASH);
    delete_btn->set_tooltip("Delete (Del)");
    delete_btn->set_font_size(18);
    delete_btn->set_fixed_size(nanogui::Vector2i(36, 36));
    delete_btn->set_background_color(nanogui::Color(180, 50, 50, 255));
    delete_btn->set_callback([this]() {
      if (m_delete_callback)
        m_delete_callback();
    });

    // Layout first to get actual size
    if (screen()) {
      NVGcontext *ctx = screen()->nvg_context();

      // Calculate preferred size based on children
      nanogui::Vector2i pref = preferred_size(ctx);
      set_size(pref);

      // Now perform layout with the correct size
      perform_layout(ctx);
    }

    // Position toolbar based on actual size
    set_position(calculate_position(pos));
    set_visible(true);
    m_target_alpha = 1.0f;
  }

  /**
   * Hide the toolbar with fade out
   */
  void hide() {
    m_target_alpha = 0.0f;
    // Will be hidden when alpha reaches 0
  }

  // Callback setters
  void set_group_callback(std::function<void()> cb) { m_group_callback = cb; }
  void set_ungroup_callback(std::function<void()> cb) { m_ungroup_callback = cb; }
  void set_duplicate_callback(std::function<void()> cb) { m_duplicate_callback = cb; }
  void set_delete_callback(std::function<void()> cb) { m_delete_callback = cb; }
  void set_bring_forward_callback(std::function<void()> cb) { m_bring_forward_callback = cb; }
  void set_send_backward_callback(std::function<void()> cb) { m_send_backward_callback = cb; }
  void set_align_left_callback(std::function<void()> cb) { m_align_left_callback = cb; }
  void set_align_right_callback(std::function<void()> cb) { m_align_right_callback = cb; }
  void set_align_top_callback(std::function<void()> cb) { m_align_top_callback = cb; }
  void set_align_bottom_callback(std::function<void()> cb) { m_align_bottom_callback = cb; }

  virtual void draw(NVGcontext *ctx) override {
    // Animate alpha
    m_current_alpha += (m_target_alpha - m_current_alpha) * 0.3f;

    // Hide when fully faded out
    if (m_current_alpha < 0.01f && m_target_alpha < 0.01f) {
      set_visible(false);
      return;
    }

    if (!visible())
      return;

    nvgSave(ctx);

    // Apply alpha to background
    int alpha = static_cast<int>(m_background_color.a() * m_current_alpha);

    // Draw rounded background with shadow
    NVGpaint shadow_paint =
        nvgBoxGradient(ctx, m_pos.x(), m_pos.y() + 2, m_size.x(), m_size.y(), m_corner_radius, 10,
                       nvgRGBA(0, 0, 0, 80 * m_current_alpha), nvgRGBA(0, 0, 0, 0));

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, m_pos.x() - 5, m_pos.y() - 5, m_size.x() + 10, m_size.y() + 10,
                   m_corner_radius + 2);
    nvgFillPaint(ctx, shadow_paint);
    nvgFill(ctx);

    // Draw background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), m_corner_radius);
    nvgFillColor(ctx, nvgRGBA(m_background_color.r(), m_background_color.g(),
                              m_background_color.b(), alpha));
    nvgFill(ctx);

    // Draw subtle border
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), m_corner_radius);
    nvgStrokeColor(ctx, nvgRGBA(100, 100, 100, 200 * m_current_alpha));
    nvgStrokeWidth(ctx, 1.0f);
    nvgStroke(ctx);

    nvgRestore(ctx);

    // Draw children with alpha
    Widget::draw(ctx);
  }

private:
  /**
   * Calculate smart position to avoid screen edges
   */
  nanogui::Vector2i calculate_position(const nanogui::Vector2i &mouse_pos) {
    nanogui::Vector2i pos = mouse_pos + nanogui::Vector2i(20, 20);

    if (!screen())
      return pos;

    // Use actual toolbar size (after layout)
    int toolbar_width = m_size.x();
    int toolbar_height = m_size.y();

    // Adjust if too close to right edge
    if (pos.x() + toolbar_width > screen()->width()) {
      pos.x() = mouse_pos.x() - toolbar_width - 20;
    }

    // Adjust if too close to bottom edge
    if (pos.y() + toolbar_height > screen()->height()) {
      pos.y() = mouse_pos.y() - toolbar_height - 20;
    }

    // Ensure not off left edge
    if (pos.x() < 10) {
      pos.x() = 10;
    }

    // Ensure not off top edge
    if (pos.y() < 10) {
      pos.y() = 10;
    }

    return pos;
  }

  std::function<void()> m_group_callback;
  std::function<void()> m_ungroup_callback;
  std::function<void()> m_duplicate_callback;
  std::function<void()> m_delete_callback;
  std::function<void()> m_bring_forward_callback;
  std::function<void()> m_send_backward_callback;
  std::function<void()> m_align_left_callback;
  std::function<void()> m_align_right_callback;
  std::function<void()> m_align_top_callback;
  std::function<void()> m_align_bottom_callback;

  nanogui::Color m_background_color;
  float m_corner_radius;
  float m_current_alpha;
  float m_target_alpha;
};

} // namespace whiteboard
