#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>
#include <string>
#include <vector>

namespace flexui {

struct DropdownStyle {
    float itemHeight = 35;
    float borderRadius = 4;
    NVGcolor bgColor = nvgRGB(255, 255, 255);
    NVGcolor bgColorHover = nvgRGB(245, 245, 245);
    NVGcolor borderColor = nvgRGB(204, 204, 204);
    NVGcolor textColor = nvgRGB(51, 51, 51);
    NVGcolor arrowColor = nvgRGB(100, 100, 100);
    float fontSize = 14;
    float padding = 10;
};

class Dropdown : public Widget {
public:
    using ChangeCallback = std::function<void(int, const std::string&)>;

    Dropdown(NVGCSSRenderer* renderer, const std::string& id,
             const std::vector<std::string>& items,
             const DropdownStyle& style = DropdownStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float x, float y) override;
    bool handleMouseMove(float x, float y) override;

    void setItems(const std::vector<std::string>& items) { items_ = items; }
    void setSelectedIndex(int index);
    int getSelectedIndex() const { return selected_index_; }
    std::string getSelectedItem() const;

    void setChangeCallback(ChangeCallback cb) { change_callback_ = cb; }
    void setDropdownStyle(const DropdownStyle& style) { style_ = style; }

    bool isOpen() const { return open_; }
    void close() { open_ = false; }

private:
    std::vector<std::string> items_;
    int selected_index_ = 0;
    int hover_index_ = -1;
    bool open_ = false;
    DropdownStyle style_;
    ChangeCallback change_callback_;
};

} // namespace flexui
