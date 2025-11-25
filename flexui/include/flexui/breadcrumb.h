#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>
#include <vector>
#include <functional>

namespace flexui {

struct BreadcrumbStyle {
    NVGcolor textColor = nvgRGB(51, 51, 51);
    NVGcolor activeColor = nvgRGB(25, 118, 210);
    NVGcolor separatorColor = nvgRGB(158, 158, 158);
    float fontSize = 14;
    float spacing = 8;
};

class Breadcrumb : public Widget {
public:
    using ItemCallback = std::function<void(int)>;

    Breadcrumb(NVGCSSRenderer* renderer, const std::string& id,
               const std::vector<std::string>& items,
               const BreadcrumbStyle& style = BreadcrumbStyle());

    void draw(NVGcontext* vg) override;
    void setItems(const std::vector<std::string>& items) { items_ = items; }
    const std::vector<std::string>& getItems() const { return items_; }
    void setItemCallback(ItemCallback callback) { item_callback_ = callback; }
    void setBreadcrumbStyle(const BreadcrumbStyle& style) { style_ = style; }

private:
    std::vector<std::string> items_;
    BreadcrumbStyle style_;
    ItemCallback item_callback_;
};

} // namespace flexui
