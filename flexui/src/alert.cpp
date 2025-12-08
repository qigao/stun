#include <flexui/alert.h>
#include <cssbox_internal.h>
namespace flexui {

Alert::Alert(cssboxRenderer* renderer, const std::string& id, const std::string& message,
             AlertType type)
    : Widget(renderer, id, "alert"), message_(message), type_(type) {
    // Set text_content for layout engine to calculate intrinsic size
    element()->text_content = message;
    updateStyleForType();
}

void Alert::updateStyleForType() {
    switch (type_) {
        case AlertType::Success:
            style_.bgColor = nvgRGB(237, 247, 237);
            style_.textColor = nvgRGB(30, 70, 32);
            style_.borderColor = nvgRGB(129, 199, 132);
            break;
        case AlertType::Warning:
            style_.bgColor = nvgRGB(255, 244, 229);
            style_.textColor = nvgRGB(102, 60, 0);
            style_.borderColor = nvgRGB(255, 183, 77);
            break;
        case AlertType::Error:
            style_.bgColor = nvgRGB(253, 237, 237);
            style_.textColor = nvgRGB(95, 33, 32);
            style_.borderColor = nvgRGB(239, 83, 80);
            break;
        default: // Info
            style_.bgColor = nvgRGB(229, 246, 253);
            style_.textColor = nvgRGB(1, 67, 97);
            style_.borderColor = nvgRGB(144, 202, 249);
            break;
    }
}

void Alert::setType(AlertType type) {
    type_ = type;
    updateStyleForType();
}

void Alert::draw(NVGcontext* vg) {
    // REMOVED: cssbox renders background, border, and text_content
    // Style with CSS: .alert { background: #e5f6fd; border: 1px solid #90caf9; }
}

} // namespace flexui
