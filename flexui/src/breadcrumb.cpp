#include <flexui/breadcrumb.h>
#include <cssbox_internal.h>
namespace flexui {

Breadcrumb::Breadcrumb(cssboxRenderer* renderer, const std::string& id,
                       const std::vector<std::string>& items, const BreadcrumbStyle& style)
    : Widget(renderer, id, "breadcrumb"), items_(items), style_(style) {
}

void Breadcrumb::draw(NVGcontext* vg) {
    if (items_.empty()) return;

    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    float fontSize = cssFontSize(style_.fontSize);
    NVGcolor textColor = cssColor(style_.textColor);

    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    float xPos = x;
    float yCenter = y + h / 2;

    for (size_t i = 0; i < items_.size(); ++i) {
        bool isLast = (i == items_.size() - 1);
        NVGcolor color = isLast ? style_.activeColor : textColor;

        // Draw item text
        nvgFillColor(vg, color);
        nvgText(vg, xPos, yCenter, items_[i].c_str(), nullptr);

        float bounds[4];
        nvgTextBounds(vg, xPos, yCenter, items_[i].c_str(), nullptr, bounds);
        xPos = bounds[2] + style_.spacing;

        // Draw separator if not last
        if (!isLast) {
            nvgFillColor(vg, style_.separatorColor);
            nvgText(vg, xPos, yCenter, "/", nullptr);
            nvgTextBounds(vg, xPos, yCenter, "/", nullptr, bounds);
            xPos = bounds[2] + style_.spacing;
        }
    }
}

} // namespace flexui
