#pragma once

#include "flexui/node.h"
#include <functional>

namespace flexui {

class FlexToggle : public FlexNode {
public:
    using FlexNode::FlexNode; // Inherit constructor

    void setOn(bool on);
    bool isOn() const { return m_on; }

    void setOnChange(std::function<void(bool)> callback) { m_on_change = callback; }

    // Override handleEvent to handle clicks
    void handleEvent(const SDL_Event& event) override;

private:
    bool m_on = false;
    std::function<void(bool)> m_on_change;
};

} // namespace flexui
