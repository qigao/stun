#include "flexui/widgets/button.h"
#include "flexui/document.h"
#include "nanovg.h"
#include "nanovg_css.h"
#include "nanovg_css_internal.h" // For NVGCSSComputedLayout

namespace flexui {

// Helper to get a computed style property as a string
static std::string get_computed_style(const FlexNode* node, const char* property) {
    if (!node || !node->document()) return "";
    NVGCSSRenderer* renderer = node->document()->renderer();
    NVGCSSElement* e = node->element();
    if (!renderer || !e) return "";

    char buffer[256];
    if (nvgcssGetComputedStyle(renderer, e, property, buffer, sizeof(buffer))) {
        return std::string(buffer);
    }
    return "";
}

void FlexButton::render(NVGcontext* vg) {
    if (!vg) return;

    NVGCSSElement* e = element();
    if (!e) return;

    // Get computed styles
    std::string font_size_str = get_computed_style(this, "font-size");
    std::string font_face_str = get_computed_style(this, "font-face");
    std::string color_str = get_computed_style(this, "color");
    
    if (font_size_str.empty() || color_str.empty()) return;

    float font_size = nvgcssParseLength(font_size_str.c_str(), 0.0f); // Context value 0 for now
    NVGcolor color = nvgcssParseColor(color_str.c_str());

    // Get computed layout box
    const NVGCSSComputedLayout& layout = e->computed;

    if (!text().empty()) {
        nvgSave(vg);
        nvgFontSize(vg, font_size);
        if (!font_face_str.empty()) {
            nvgFontFace(vg, font_face_str.c_str());
        }
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, color);

        float x = layout.x + layout.width * 0.5f;
        float y = layout.y + layout.height * 0.5f;

        nvgText(vg, x, y, text().c_str(), nullptr);
        nvgRestore(vg);
    }
}

void FlexButton::handleEvent(const SDL_Event &event) {
  // Simple click handling
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      if (event.button.button == SDL_BUTTON_LEFT) {
          if (m_on_click) {
              m_on_click();
          }
      }
  }
}

} // namespace flexui
