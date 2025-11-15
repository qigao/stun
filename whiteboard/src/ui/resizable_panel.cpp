/**
 * \file resizable_panel.cpp
 * \brief Implementation of ResizablePanel class.
 */

#include "whiteboard/ui/resizable_panel.h"
#include <algorithm>
#include <fmtlog.h>
#include <nanogui/layout.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

namespace whiteboard {

ResizablePanel::ResizablePanel(nanogui::Widget *parent, const std::string &title)
    : nanogui::Widget(parent), m_title(title), m_border_color(nanogui::Color(200, 200, 210, 255)),
      m_background_color(nanogui::Color(250, 250, 252, 255)), m_border_width(1.0f),
      m_min_size(nanogui::Vector2i(200, 150)), m_resizable(true), m_resizing(false),
      m_resize_edge(ResizeEdge::None) {

  // Create content widget that holds all child widgets
  m_content = new nanogui::Widget(this);
  m_content->set_layout(
      new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Fill, 10, 10));
}

void ResizablePanel::draw(NVGcontext *ctx) {
  // Draw background
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
  nvgFillColor(ctx, nvgRGBA(m_background_color.r() * 255, m_background_color.g() * 255,
                            m_background_color.b() * 255, m_background_color.w() * 255));
  nvgFill(ctx);

  // Draw border
  draw_border(ctx);

  // Draw title bar if title is set
  if (!m_title.empty()) {
    draw_title_bar(ctx);
  }

  // Update content position and size
  float content_y = m_title.empty() ? m_border_width : TITLE_BAR_HEIGHT;
  float content_height = m_size.y() - content_y - m_border_width;
  m_content->set_position(
      nanogui::Vector2i(static_cast<int>(m_border_width), static_cast<int>(content_y)));
  m_content->set_size(nanogui::Vector2i(static_cast<int>(m_size.x() - 2 * m_border_width),
                                        static_cast<int>(content_height)));

  // Draw children
  Widget::draw(ctx);
}

void ResizablePanel::draw_border(NVGcontext *ctx) {
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
  nvgStrokeColor(ctx, nvgRGBA(m_border_color.r() * 255, m_border_color.g() * 255,
                              m_border_color.b() * 255, m_border_color.w() * 255));
  nvgStrokeWidth(ctx, m_border_width);
  nvgStroke(ctx);
}

void ResizablePanel::draw_title_bar(NVGcontext *ctx) {
  float px = m_pos.x();
  float py = m_pos.y();
  float pw = m_size.x();

  // Draw title bar background
  nvgBeginPath(ctx);
  nvgRect(ctx, px, py, pw, TITLE_BAR_HEIGHT);
  nvgFillColor(ctx, nvgRGBA(240, 240, 245, 255));
  nvgFill(ctx);

  // Draw title bar bottom border
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, px, py + TITLE_BAR_HEIGHT);
  nvgLineTo(ctx, px + pw, py + TITLE_BAR_HEIGHT);
  nvgStrokeColor(ctx, nvgRGBA(m_border_color.r() * 255, m_border_color.g() * 255,
                              m_border_color.b() * 255, m_border_color.w() * 255));
  nvgStrokeWidth(ctx, m_border_width);
  nvgStroke(ctx);

  // Draw title text
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 14.0f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, px + 10, py + TITLE_BAR_HEIGHT / 2, m_title.c_str(), nullptr);
}

ResizablePanel::ResizeEdge ResizablePanel::get_resize_edge(const nanogui::Vector2i &p) const {
  if (!m_resizable) {
    return ResizeEdge::None;
  }

  // Convert screen coordinates to local coordinates
  nanogui::Vector2i local_p = p - absolute_position();
  int edge_flags = 0;

  // Check edges
  if (local_p.x() < RESIZE_HANDLE_SIZE) {
    edge_flags |= static_cast<int>(ResizeEdge::Left);
  } else if (local_p.x() > m_size.x() - RESIZE_HANDLE_SIZE) {
    edge_flags |= static_cast<int>(ResizeEdge::Right);
  }

  if (local_p.y() < RESIZE_HANDLE_SIZE) {
    edge_flags |= static_cast<int>(ResizeEdge::Top);
  } else if (local_p.y() > m_size.y() - RESIZE_HANDLE_SIZE) {
    edge_flags |= static_cast<int>(ResizeEdge::Bottom);
  }

  return static_cast<ResizeEdge>(edge_flags);
}

void ResizablePanel::update_cursor(ResizeEdge edge) {
  // Note: NanoGUI doesn't have built-in cursor changing API
  // This is a placeholder for future cursor support
  // You would need to use GLFW directly to change cursors
  (void)edge; // Suppress unused parameter warning
}

bool ResizablePanel::mouse_button_event(const nanogui::Vector2i &p, int button, bool down,
                                        int modifiers) {
  logi("馃敺 ResizablePanel '{}' CLICKED at ({},{}) button={} down={}", 
       m_title, p.x(), p.y(), button, down);
  
  // First, let children handle the event
  if (Widget::mouse_button_event(p, button, down, modifiers)) {
    return true;
  }

  if (button == 0 && m_resizable) { // Left mouse button
    if (down) {
      ResizeEdge edge = get_resize_edge(p);
      if (edge != ResizeEdge::None) {
        logi("鉁� RESIZE STARTED! Edge={}, pos=({},{}), size=({},{})",
             static_cast<int>(edge), p.x(), p.y(), m_size.x(), m_size.y());
        m_resizing = true;
        m_resize_edge = edge;
        m_resize_start_pos = p;
        m_resize_start_size = m_size;
        return true;
      }
    } else {
      if (m_resizing) {
        logi("鉁� RESIZE ENDED");
        m_resizing = false;
        m_resize_edge = ResizeEdge::None;
        return true;
      }
    }
  }

  return false;

  return false;
}

bool ResizablePanel::mouse_motion_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel,
                                        int button, int modifiers) {
  if (m_resizable && !m_resizing) {
    ResizeEdge edge = get_resize_edge(p);
    update_cursor(edge);
  }

  return Widget::mouse_motion_event(p, rel, button, modifiers);
}

bool ResizablePanel::mouse_drag_event(const nanogui::Vector2i &p, const nanogui::Vector2i &rel,
                                      int button, int modifiers) {
  if (m_resizing && button == 0) { // Left mouse button
    nanogui::Vector2i delta = p - m_resize_start_pos;
    nanogui::Vector2i new_size = m_resize_start_size;
    nanogui::Vector2i new_pos = m_pos;
    nanogui::Vector2i start_pos = m_pos; // Store original position

    // Apply resize based on edge
    int edge_int = static_cast<int>(m_resize_edge);

    if (edge_int & static_cast<int>(ResizeEdge::Left)) {
      int size_change = -delta.x();
      new_size.x() = m_resize_start_size.x() + size_change;
      new_pos.x() = start_pos.x() - size_change;
    } else if (edge_int & static_cast<int>(ResizeEdge::Right)) {
      new_size.x() = m_resize_start_size.x() + delta.x();
    }

    if (edge_int & static_cast<int>(ResizeEdge::Top)) {
      int size_change = -delta.y();
      new_size.y() = m_resize_start_size.y() + size_change;
      new_pos.y() = start_pos.y() - size_change;
    } else if (edge_int & static_cast<int>(ResizeEdge::Bottom)) {
      new_size.y() = m_resize_start_size.y() + delta.y();
    }

    // Apply minimum size constraints
    if (new_size.x() < m_min_size.x()) {
      if (edge_int & static_cast<int>(ResizeEdge::Left)) {
        int excess = m_min_size.x() - new_size.x();
        new_pos.x() = start_pos.x() + excess;
      }
      new_size.x() = m_min_size.x();
    }

    if (new_size.y() < m_min_size.y()) {
      if (edge_int & static_cast<int>(ResizeEdge::Top)) {
        int excess = m_min_size.y() - new_size.y();
        new_pos.y() = start_pos.y() + excess;
      }
      new_size.y() = m_min_size.y();
    }

    // Update position and size
    set_position(new_pos);
    set_size(new_size);

    // Request layout update
    if (parent()) {
      parent()->perform_layout(screen()->nvg_context());
    }

    // Request redraw
    if (screen()) {
      screen()->redraw();
    }

    return true;
  }

  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

} // namespace whiteboard
