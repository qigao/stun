/**
 * \file resizable_panel.h
 * \brief Base class for resizable panels with borders.
 */

#pragma once

#include <nanogui/widget.h>
#include <nanovg.h>

namespace whiteboard {

/**
 * \class ResizablePanel
 * \brief A widget with a visible border frame that can be resized by dragging edges.
 *
 * Features:
 * - Clear border frame with customizable color and width
 * - Resizable by dragging any edge or corner
 * - Minimum size constraints
 * - Optional title bar
 */
class ResizablePanel : public nanogui::Widget {
public:
  enum class ResizeEdge {
    None = 0,
    Left = 1,
    Right = 2,
    Top = 4,
    Bottom = 8,
    TopLeft = Top | Left,
    TopRight = Top | Right,
    BottomLeft = Bottom | Left,
    BottomRight = Bottom | Right
  };

  /**
   * \brief Constructor.
   * \param parent Parent widget
   * \param title Optional title for the panel
   */
  ResizablePanel(nanogui::Widget *parent, const std::string &title = "");

  /**
   * \brief Set the border color.
   */
  void set_border_color(const nanogui::Color &color) { m_border_color = color; }

  /**
   * \brief Set the border width.
   */
  void set_border_width(float width) { m_border_width = width; }

  /**
   * \brief Set the background color.
   */
  void set_background_color(const nanogui::Color &color) { m_background_color = color; }

  /**
   * \brief Set minimum size for the panel.
   */
  void set_min_size(const nanogui::Vector2i &size) { m_min_size = size; }

  /**
   * \brief Set whether the panel is resizable.
   */
  void set_resizable(bool resizable) { m_resizable = resizable; }

  /**
   * \brief Set the title.
   */
  void set_title(const std::string &title) { m_title = title; }

  /**
   * \brief Get the content area (inside the border).
   */
  nanogui::Widget *content_widget() { return m_content; }

  // Overrides
  void draw(NVGcontext *ctx) override;
  bool mouse_button_event(const nanogui::Vector2i &p, int button, bool down,
                          int modifiers) override;
  bool mouse_motion_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel, int button,
                          int modifiers) override;
  bool mouse_drag_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel, int button,
                        int modifiers) override;

protected:
  /**
   * \brief Determine which edge/corner is at the given position.
   */
  ResizeEdge get_resize_edge(const nanogui::Vector2i &p) const;

  /**
   * \brief Update cursor based on resize edge.
   */
  void update_cursor(ResizeEdge edge);

  /**
   * \brief Draw the border frame.
   */
  void draw_border(NVGcontext *ctx);

  /**
   * \brief Draw the title bar if title is set.
   */
  void draw_title_bar(NVGcontext *ctx);

  std::string m_title;
  nanogui::Color m_border_color;
  nanogui::Color m_background_color;
  float m_border_width;
  nanogui::Vector2i m_min_size;
  bool m_resizable;

  // Resize state
  bool m_resizing;
  ResizeEdge m_resize_edge;
  nanogui::Vector2i m_resize_start_pos;
  nanogui::Vector2i m_resize_start_size;

  // Content widget (child widgets go here)
  nanogui::Widget *m_content;

  // Title bar height
  static constexpr float TITLE_BAR_HEIGHT = 30.0f;
  static constexpr float RESIZE_HANDLE_SIZE = 8.0f;
};

// Bitwise operators for ResizeEdge
inline ResizablePanel::ResizeEdge operator|(ResizablePanel::ResizeEdge a,
                                             ResizablePanel::ResizeEdge b) {
  return static_cast<ResizablePanel::ResizeEdge>(static_cast<int>(a) | static_cast<int>(b));
}

inline ResizablePanel::ResizeEdge operator&(ResizablePanel::ResizeEdge a,
                                             ResizablePanel::ResizeEdge b) {
  return static_cast<ResizablePanel::ResizeEdge>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool operator!(ResizablePanel::ResizeEdge e) {
  return e == ResizablePanel::ResizeEdge::None;
}

} // namespace whiteboard
