#pragma once

#include "flexui/node.h"
#include <functional>

namespace flexui {

// We can reuse FlexCheckboxBinding or simpler Props, but let's stick to node properties.
// Actually, the factory uses FlexCheckboxProps.
// But here we are defining the Node class.

class FlexCheckbox : public FlexNode {
public:
    using FlexNode::FlexNode; // Inherit constructor

    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }
    
    void setOnChange(std::function<void(bool)> callback) { m_on_change = callback; }

    // Override handleEvent to handle clicks
    void handleEvent(const SDL_Event& event) override;

    // Optional: update method if needed, but not for now.

private:
    bool m_checked = false;
    std::function<void(bool)> m_on_change;
};

} // namespace flexui
