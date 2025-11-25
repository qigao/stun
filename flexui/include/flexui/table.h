#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>
#include <vector>

namespace flexui {

struct TableStyle {
    float rowHeight = 40;
    float headerHeight = 45;
    NVGcolor headerBg = nvgRGB(245, 245, 245);
    NVGcolor rowBg = nvgRGB(255, 255, 255);
    NVGcolor altRowBg = nvgRGB(250, 250, 250);
    NVGcolor borderColor = nvgRGB(224, 224, 224);
    NVGcolor textColor = nvgRGB(51, 51, 51);
    float fontSize = 14;
};

class Table : public Widget {
public:
    Table(NVGCSSRenderer* renderer, const std::string& id,
          const std::vector<std::string>& headers,
          const std::vector<float>& columnWidths,
          const TableStyle& style = TableStyle());

    void draw(NVGcontext* vg) override;
    void addRow(const std::vector<std::string>& row);
    void clearRows();
    void setData(const std::vector<std::vector<std::string>>& data) { rows_ = data; }

private:
    std::vector<std::string> headers_;
    std::vector<float> columnWidths_;
    std::vector<std::vector<std::string>> rows_;
    TableStyle style_;
};

} // namespace flexui
