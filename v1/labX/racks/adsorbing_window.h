#pragma once
#include "window_adsorption.h"
#include <iostream>
#include <nanogui.h>
#include <nanogui/opengl.h>

using namespace nanogui;

class AdsorbingWindow : public Window {
public:
  AdsorbingWindow(Widget *parent, const std::string &title = "Untitled")
      : Window(parent, title), m_adsorption(nullptr), m_lastDragPos(0, 0), m_isDraggingGroup(false) {}

  void setAdsorptionManager(WindowAdsorption *adsorption) { m_adsorption = adsorption; }

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override {
    if (button == 0 && down) {
      m_lastDragPos = position();
      m_isDraggingGroup = false;
    } else if (button == 0 && !down) {
      m_isDraggingGroup = false;
    }
    return Window::mouse_button_event(p, button, down, modifiers);
  }

  bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                        int modifiers) override {
    // Check if Shift key is held - this breaks the group
    bool breakGroup = (modifiers & GLFW_MOD_SHIFT);

    if (breakGroup && m_adsorption && m_drag) {
      m_adsorption->disconnectWindow(this);
      m_isDraggingGroup = false;
    }

    // Check if we should move as a group (only if not breaking)
    if (!breakGroup && m_adsorption && m_drag) {
      auto connectedWindows = m_adsorption->getConnectedWindows(this);
      if (connectedWindows.size() > 1) {
        m_isDraggingGroup = true;

        // Store position before parent drag
        Vector2i posBefore = position();

        // Let parent handle the drag for this window
        bool handled = Window::mouse_drag_event(p, rel, button, modifiers);

        // Calculate how much this window moved
        Vector2i posAfter = position();
        Vector2i delta = posAfter - posBefore;

        if (delta.x() != 0 || delta.y() != 0) {
          // Move all OTHER connected windows by the same delta
          for (Window *win : connectedWindows) {
            if (win != this) {
              Vector2i winPos = win->position();
              win->set_position(winPos + delta);
            }
          }
        }

        return handled;
      }
    }

    // Normal single window drag
    bool handled = Window::mouse_drag_event(p, rel, button, modifiers);

    // If this window is being dragged and we have an adsorption manager
    if (handled && m_adsorption && m_drag) {
      // Get screen size from parent
      Screen *scr = screen();
      if (scr) {
        Vector2i screenSize = scr->size();
        m_adsorption->updateWindowPosition(this, screenSize.x(), screenSize.y());
      }
    }

    return handled;
  }

private:
  WindowAdsorption *m_adsorption;
  Vector2i m_lastDragPos;
  bool m_isDraggingGroup;
};
