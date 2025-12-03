#pragma once

#include <flexui/textbox.h>
#include <nanovg.h>
#include <functional>

namespace flexui {

struct SearchBoxStyle {
    NVGcolor iconColor = nvgRGB(158, 158, 158);
    float iconSize = 18;
    float iconPadding = 8;
};

class SearchBox : public TextBox {
public:
    using SearchCallback = std::function<void(const std::string&)>;

    SearchBox(NVGCSSRenderer* renderer, const std::string& id,
              const std::string& placeholder = "Search...",
              const SearchBoxStyle& style = SearchBoxStyle());

    void draw(NVGcontext* vg) override;
    void handleKeyPress(int key) override;

    void setSearchCallback(SearchCallback cb) { search_callback_ = cb; }
    void setSearchBoxStyle(const SearchBoxStyle& style) { style_ = style; }

private:
    SearchBoxStyle style_;
    SearchCallback search_callback_;

    void drawSearchIcon(NVGcontext* vg, float x, float y, float size, const NVGcolor& color);
};

} // namespace flexui
