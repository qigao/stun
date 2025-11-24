#include "flexui/widgets/checkbox.h"
#include "flexui/document.h"
#include <fmtlog.h>

namespace flexui {

void FlexCheckbox::setChecked(bool checked) {
    if (m_checked == checked) return;
    m_checked = checked;

    if (document()) {
        document()->setClass(id(), "checkbox-checked", m_checked);
        // Force layout/restyle if needed?
        // Usually setClass triggers restyle in next frame or immediately.
        // FlexDocument::setClass calls nvgcssAddClass which flags dirty?
        // nvgcss usually doesn't auto-recompute layout on class change unless told to.
        // But FlexDocument::setClass calls nvgcssAddClass directly on element.
    }

    if (m_on_change) {
        m_on_change(m_checked);
    }
}

void FlexCheckbox::handleEvent(const SDL_Event &event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            setChecked(!m_checked);
        }
    }
}

} // namespace flexui
