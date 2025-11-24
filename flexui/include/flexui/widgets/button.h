#pragma once

#include "flexui/node.h"
#include <functional>

// Forward declare NVGcontext
struct NVGcontext;

namespace flexui {

class FlexButton : public FlexNode {
public:
  using FlexNode::FlexNode; // Inherit constructor

  // Callback for button click
  using ClickCallback = std::function<void()>;

  void setOnClick(ClickCallback callback) {
      m_on_click = std::move(callback);
  }

  void handleEvent(const SDL_Event &event) override;
  void render(NVGcontext* vg) override;

private:
  ClickCallback m_on_click;
};

} // namespace flexui
