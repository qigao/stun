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
    NVGcolor selectedRowBg = nvgRGBA(33, 150, 243, 50);  // Light blue
    NVGcolor hoverRowBg = nvgRGBA(33, 150, 243, 20);     // Very light blue
    NVGcolor borderColor = nvgRGB(224, 224, 224);
    NVGcolor textColor = nvgRGB(51, 51, 51);
    float fontSize = 14;
};

class Table : public Widget {
public:
    using SelectionCallback = std::function<void(int row, int col)>;
    
    Table(cssboxRenderer* renderer, const std::string& id,
          const std::vector<std::string>& headers,
          const std::vector<float>& columnWidths,
          const TableStyle& style = TableStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float x, float y) override;
    bool handleMouseMove(float x, float y) override;
    
    void addRow(const std::vector<std::string>& row);
    void clearRows();
    void setData(const std::vector<std::vector<std::string>>& data) { rows_ = data; }
    
    // Selection
    void setSelectedRow(int row);
    int getSelectedRow() const { return selected_row_; }
    void clearSelection() { selected_row_ = -1; }
    
    void setSelectionCallback(SelectionCallback cb) { selection_callback_ = cb; }

private:
    int getRowAtPosition(float y) const;
    int getColumnAtPosition(float x) const;
    
    std::vector<std::string> headers_;
    std::vector<float> columnWidths_;
    std::vector<std::vector<std::string>> rows_;
    TableStyle style_;
    
    int selected_row_ = -1;
    int hover_row_ = -1;
    SelectionCallback selection_callback_;
};

} // namespace flexui
