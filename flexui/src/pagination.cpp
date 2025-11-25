#include <flexui/pagination.h>
#include <nanovg_css_internal.h>
#include <algorithm>

namespace flexui {

Pagination::Pagination(NVGCSSRenderer* renderer, const std::string& id, int totalPages,
                       int currentPage, const PaginationStyle& style)
    : Widget(renderer, id, "pagination"), total_pages_(totalPages),
      current_page_(currentPage), style_(style) {
}

void Pagination::setCurrentPage(int currentPage) {
    if (currentPage >= 1 && currentPage <= total_pages_) {
        current_page_ = currentPage;
        if (page_change_callback_) {
            page_change_callback_(current_page_);
        }
    }
}

void Pagination::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    float currentX = x;
    float buttonY = y + (h - style_.buttonHeight) / 2;

    // Draw previous button
    nvgBeginPath(vg);
    nvgRoundedRect(vg, currentX, buttonY, style_.buttonWidth, style_.buttonHeight, style_.borderRadius);
    NVGcolor prevColor = (current_page_ > 1) ?
        (hover_page_ == -2 ? style_.buttonHoverColor : style_.buttonColor) :
        nvgRGBA(100, 100, 100, 255);
    nvgFillColor(vg, prevColor);
    nvgFill(vg);

    nvgFillColor(vg, style_.textColor);
    nvgFontSize(vg, style_.fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, currentX + style_.buttonWidth / 2, buttonY + style_.buttonHeight / 2, "<", nullptr);
    currentX += style_.buttonWidth + style_.buttonSpacing;

    // Draw page numbers
    for (int i = 1; i <= total_pages_; ++i) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, currentX, buttonY, style_.buttonWidth, style_.buttonHeight, style_.borderRadius);

        NVGcolor buttonColor;
        if (i == current_page_) {
            buttonColor = style_.buttonActiveColor;
        } else if (i == hover_page_) {
            buttonColor = style_.buttonHoverColor;
        } else {
            buttonColor = style_.buttonColor;
        }
        nvgFillColor(vg, buttonColor);
        nvgFill(vg);

        nvgFillColor(vg, style_.textColor);
        nvgFontSize(vg, style_.fontSize);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        std::string pageNum = std::to_string(i);
        nvgText(vg, currentX + style_.buttonWidth / 2, buttonY + style_.buttonHeight / 2, pageNum.c_str(), nullptr);
        currentX += style_.buttonWidth + style_.buttonSpacing;
    }

    // Draw next button
    nvgBeginPath(vg);
    nvgRoundedRect(vg, currentX, buttonY, style_.buttonWidth, style_.buttonHeight, style_.borderRadius);
    NVGcolor nextColor = (current_page_ < total_pages_) ?
        (hover_page_ == -3 ? style_.buttonHoverColor : style_.buttonColor) :
        nvgRGBA(100, 100, 100, 255);
    nvgFillColor(vg, nextColor);
    nvgFill(vg);

    nvgFillColor(vg, style_.textColor);
    nvgFontSize(vg, style_.fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, currentX + style_.buttonWidth / 2, buttonY + style_.buttonHeight / 2, ">", nullptr);
}

bool Pagination::handleMouseDown(float mx, float my) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float h = el->computed.height;

    float currentX = x;
    float buttonY = y + (h - style_.buttonHeight) / 2;

    // Handle previous button click
    if (mx >= currentX && mx <= currentX + style_.buttonWidth &&
        my >= buttonY && my <= buttonY + style_.buttonHeight) {
        if (current_page_ > 1) {
            setCurrentPage(current_page_ - 1);
            return true;
        }
    }
    currentX += style_.buttonWidth + style_.buttonSpacing;

    // Handle page number clicks
    for (int i = 1; i <= total_pages_; ++i) {
        if (mx >= currentX && mx <= currentX + style_.buttonWidth &&
            my >= buttonY && my <= buttonY + style_.buttonHeight) {
            if (current_page_ != i) {
                setCurrentPage(i);
            }
            return true;
        }
        currentX += style_.buttonWidth + style_.buttonSpacing;
    }

    // Handle next button click
    if (mx >= currentX && mx <= currentX + style_.buttonWidth &&
        my >= buttonY && my <= buttonY + style_.buttonHeight) {
        if (current_page_ < total_pages_) {
            setCurrentPage(current_page_ + 1);
            return true;
        }
    }
    return false;
}

} // namespace flexui
