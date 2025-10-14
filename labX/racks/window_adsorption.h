#pragma once
#include <iostream>
#include <nanogui.h>
#include <set>
#include <unordered_map>
#include <vector>

using namespace nanogui;

class WindowAdsorption {
public:
  WindowAdsorption(int snapDistance = 20) : m_snapDistance(snapDistance) {}

  void registerWindow(Window *window) { m_windows.push_back(window); }

  // Get all windows that should move together with the given window
  std::vector<Window *> getConnectedWindows(Window *window) {
    std::set<Window *> connected;
    std::vector<Window *> toCheck = {window};

    while (!toCheck.empty()) {
      Window *current = toCheck.back();
      toCheck.pop_back();

      if (connected.find(current) != connected.end())
        continue;

      connected.insert(current);

      // Check connections
      auto it = m_connections.find(current);
      if (it != m_connections.end()) {
        for (Window *neighbor : it->second) {
          if (connected.find(neighbor) == connected.end()) {
            toCheck.push_back(neighbor);
          }
        }
      }
    }

    return std::vector<Window *>(connected.begin(), connected.end());
  }

  // Move a group of connected windows together
  void moveConnectedWindows(Window *draggedWindow, const Vector2i &delta) {
    auto connectedWindows = getConnectedWindows(draggedWindow);

    for (Window *win : connectedWindows) {
      Vector2i currentPos = win->position();
      win->set_position(currentPos + delta);
    }
  }

  void updateWindowPosition(Window *movingWindow, int screenWidth, int screenHeight) {
    if (!movingWindow)
      return;

    Vector2i pos = movingWindow->position();
    Vector2i size = movingWindow->size();

    int x = pos.x();
    int y = pos.y();
    int w = size.x();
    int h = size.y();

    int originalX = x;
    int originalY = y;

    // Snap to screen edges
    if (std::abs(x) < m_snapDistance)
      x = 0;
    if (std::abs(y) < m_snapDistance)
      y = 0;
    if (std::abs(x + w - screenWidth) < m_snapDistance)
      x = screenWidth - w;
    if (std::abs(y + h - screenHeight) < m_snapDistance)
      y = screenHeight - h;

    // Snap to other windows
    for (auto *otherWindow : m_windows) {
      if (otherWindow == movingWindow || !otherWindow->visible())
        continue;

      Vector2i otherPos = otherWindow->position();
      Vector2i otherSize = otherWindow->size();

      int ox = otherPos.x();
      int oy = otherPos.y();
      int ow = otherSize.x();
      int oh = otherSize.y();

      // Check if windows overlap vertically
      bool verticalOverlap = !(y + h < oy || y > oy + oh);

      if (verticalOverlap) {
        // Snap right edge to left edge of other window
        if (std::abs((x + w) - ox) < m_snapDistance) {
          x = ox - w;
        }
        // Snap left edge to right edge of other window
        else if (std::abs(x - (ox + ow)) < m_snapDistance) {
          x = ox + ow;
        }
      }

      // Check if windows overlap horizontally
      bool horizontalOverlap = !(x + w < ox || x > ox + ow);

      if (horizontalOverlap) {
        // Snap bottom edge to top edge of other window
        if (std::abs((y + h) - oy) < m_snapDistance) {
          y = oy - h;
        }
        // Snap top edge to bottom edge of other window
        else if (std::abs(y - (oy + oh)) < m_snapDistance) {
          y = oy + oh;
        }
      }
    }

    // Update connections based on proximity
    updateConnections(movingWindow);

    if (x != originalX || y != originalY) {
      movingWindow->set_position(Vector2i(x, y));
    }
  }

  void setSnapDistance(int distance) { m_snapDistance = distance; }

  // Disconnect a window from all its connections
  void disconnectWindow(Window *window) {
    if (!window)
      return;

    // Remove this window from all other windows' connection lists
    auto it = m_connections.find(window);
    if (it != m_connections.end()) {
      for (Window *connectedWindow : it->second) {
        auto otherIt = m_connections.find(connectedWindow);
        if (otherIt != m_connections.end()) {
          otherIt->second.erase(window);
        }
      }
      // Clear this window's connections
      it->second.clear();
    }
  }

private:
  void updateConnections(Window *window) {
    if (!window)
      return;

    Vector2i pos = window->position();
    Vector2i size = window->size();

    int x = pos.x();
    int y = pos.y();
    int w = size.x();
    int h = size.y();

    // Clear existing connections for this window
    m_connections[window].clear();

    // Check proximity to other windows
    for (auto *otherWindow : m_windows) {
      if (otherWindow == window || !otherWindow->visible())
        continue;

      Vector2i otherPos = otherWindow->position();
      Vector2i otherSize = otherWindow->size();

      int ox = otherPos.x();
      int oy = otherPos.y();
      int ow = otherSize.x();
      int oh = otherSize.y();

      bool isConnected = false;

      // Check if windows are touching or very close
      // Right edge to left edge
      if (std::abs((x + w) - ox) <= 2) {
        // Check vertical overlap
        if (!(y + h < oy || y > oy + oh)) {
          isConnected = true;
        }
      }
      // Left edge to right edge
      else if (std::abs(x - (ox + ow)) <= 2) {
        if (!(y + h < oy || y > oy + oh)) {
          isConnected = true;
        }
      }
      // Bottom edge to top edge
      else if (std::abs((y + h) - oy) <= 2) {
        if (!(x + w < ox || x > ox + ow)) {
          isConnected = true;
        }
      }
      // Top edge to bottom edge
      else if (std::abs(y - (oy + oh)) <= 2) {
        if (!(x + w < ox || x > ox + ow)) {
          isConnected = true;
        }
      }

      if (isConnected) {
        m_connections[window].insert(otherWindow);
        m_connections[otherWindow].insert(window);
      }
    }
  }

  std::vector<Window *> m_windows;
  int m_snapDistance;
  std::unordered_map<Window *, std::set<Window *>> m_connections;
};
