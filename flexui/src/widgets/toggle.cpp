#include "flexui/widgets/toggle.h"
#include "flexui/document.h"

namespace flexui {

void FlexToggle::setOn(bool on) {
    if (m_on == on) return;
    m_on = on;

    if (document()) {
        document()->setClass(id(), "on", m_on);
    }

    if (m_on_change) {
        m_on_change(m_on);
    }
}

void FlexToggle::handleEvent(const SDL_Event &event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            setOn(!m_on);
        }
    }
}

} // namespace flexui
