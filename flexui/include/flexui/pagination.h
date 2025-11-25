#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>

namespace flexui {

struct PaginationStyle {
    NVGcolor buttonColor = nvgRGB(70, 70, 70);
    NVGcolor buttonHoverColor = nvgRGB(90, 90, 90);
    NVGcolor buttonActiveColor = nvgRGB(33, 150, 243);
    NVGcolor textColor = nvgRGB(255, 255, 255);
    float fontSize = 16.0f;
    float buttonWidth = 30.0f;
    float buttonHeight = 30.0f;
    float buttonSpacing = 5.0f;
    float borderRadius = 4.0f;
};

class Pagination : public Widget {
public:
    using PageChangeCallback = std::function<void(int)>;

    Pagination(NVGCSSRenderer* renderer, const std::string& id, int totalPages,
               int currentPage = 1, const PaginationStyle& style = PaginationStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float x, float y) override;

    void setTotalPages(int totalPages) { total_pages_ = totalPages; }
    void setCurrentPage(int currentPage);
    int getCurrentPage() const { return current_page_; }
    void setPageChangeCallback(PageChangeCallback cb) { page_change_callback_ = cb; }
    void setPaginationStyle(const PaginationStyle& style) { style_ = style; }

private:
    int total_pages_;
    int current_page_;
    PaginationStyle style_;
    PageChangeCallback page_change_callback_;
    int hover_page_ = -1;
};

} // namespace flexui
