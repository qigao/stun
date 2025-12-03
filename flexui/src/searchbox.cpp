#include <flexui/searchbox.h>
#include <SDL3/SDL.h>
#include <cmath>
#include <nanovg_css_internal.h>
namespace flexui {

SearchBox::SearchBox(NVGCSSRenderer* renderer, const std::string& id,
                     const std::string& placeholder, const SearchBoxStyle& style)
    : TextBox(renderer, id, placeholder), style_(style) {
}

void SearchBox::handleKeyPress(int key) {
    TextBox::handleKeyPress(key);

    if (key == SDLK_RETURN && search_callback_) {
        search_callback_(getInputText());
    }
}

void SearchBox::draw(NVGcontext* vg) {
    // Draw the base textbox
    TextBox::draw(vg);

    // Draw search icon on the left
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    float iconX = x + style_.iconPadding;
    float iconY = y + (h - style_.iconSize) / 2;

    drawSearchIcon(vg, iconX, iconY, style_.iconSize, style_.iconColor);
}

void SearchBox::drawSearchIcon(NVGcontext* vg, float x, float y, float size, const NVGcolor& color) {
    float cx = x + size * 0.4f;
    float cy = y + size * 0.4f;
    float r = size * 0.3f;

    // Draw circle (magnifying glass lens)
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r);
    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, 2.0f);
    nvgStroke(vg);

    // Draw handle
    float angle = 3.14159f / 4;  // 45 degrees
    float handleStart = r * 0.7f;
    float handleEnd = size * 0.9f;

    nvgBeginPath(vg);
    nvgMoveTo(vg, cx + handleStart * cos(angle), cy + handleStart * sin(angle));
    nvgLineTo(vg, cx + handleEnd * cos(angle), cy + handleEnd * sin(angle));
    nvgStrokeColor(vg, color);
    nvgStrokeWidth(vg, 2.0f);
    nvgStroke(vg);
}

} // namespace flexui
