#include <flexui/table.h>
#include <cssbox_internal.h>

namespace flexui {

Table::Table(cssboxRenderer* renderer, const std::string& id,
             const std::vector<std::string>& headers,
             const std::vector<float>& columnWidths, const TableStyle& style)
    : Widget(renderer, id, "table"), headers_(headers),
      columnWidths_(columnWidths), style_(style) {
}

void Table::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    // Get styles from CSS with fallbacks
    float fontSize = cssFontSize(style_.fontSize);
    const char* fontFamily = cssFontFamily("sans-serif");
    NVGcolor textColor = cssColor(style_.textColor);
    NVGcolor borderColor = cssBorderColor(style_.borderColor);
    
    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, fontFamily);

    float totalWidth = 0;
    for (float cw : columnWidths_) {
        totalWidth += cw;
    }

    // Draw header background (use CSS background if available)
    nvgBeginPath(vg);
    nvgRect(vg, x, y, totalWidth, style_.headerHeight);
    nvgFillColor(vg, cssBackground(style_.headerBg));
    nvgFill(vg);

    // Draw header text
    nvgFillColor(vg, textColor);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    float xPos = x + 10;
    for (size_t i = 0; i < headers_.size(); ++i) {
        nvgText(vg, xPos, y + style_.headerHeight / 2, headers_[i].c_str(), nullptr);
        xPos += columnWidths_[i];
    }

    // Draw header border
    nvgBeginPath(vg);
    nvgMoveTo(vg, x, y + style_.headerHeight);
    nvgLineTo(vg, x + totalWidth, y + style_.headerHeight);
    nvgStrokeColor(vg, borderColor);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Draw rows
    float yPos = y + style_.headerHeight;
    for (size_t r = 0; r < rows_.size(); ++r) {
        // Row background (selected > hover > alternate)
        NVGcolor bgColor;
        if (static_cast<int>(r) == selected_row_) {
            bgColor = style_.selectedRowBg;
        } else if (static_cast<int>(r) == hover_row_) {
            bgColor = style_.hoverRowBg;
        } else {
            bgColor = (r % 2 == 0) ? style_.rowBg : style_.altRowBg;
        }
        
        nvgBeginPath(vg);
        nvgRect(vg, x, yPos, totalWidth, style_.rowHeight);
        nvgFillColor(vg, bgColor);
        nvgFill(vg);

        // Row text (use CSS color)
        xPos = x + 10;
        nvgFillColor(vg, textColor);
        for (size_t c = 0; c < rows_[r].size() && c < columnWidths_.size(); ++c) {
            nvgText(vg, xPos, yPos + style_.rowHeight / 2, rows_[r][c].c_str(), nullptr);
            xPos += columnWidths_[c];
        }

        // Row border
        nvgBeginPath(vg);
        nvgMoveTo(vg, x, yPos + style_.rowHeight);
        nvgLineTo(vg, x + totalWidth, yPos + style_.rowHeight);
        nvgStrokeColor(vg, borderColor);
        nvgStrokeWidth(vg, 1);
        nvgStroke(vg);

        yPos += style_.rowHeight;
    }

    // Draw vertical lines
    xPos = x;
    for (float cw : columnWidths_) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, xPos, y);
        nvgLineTo(vg, xPos, yPos);
        nvgStrokeColor(vg, borderColor);
        nvgStrokeWidth(vg, 1);
        nvgStroke(vg);
        xPos += cw;
    }
    // Final right border
    nvgBeginPath(vg);
    nvgMoveTo(vg, xPos, y);
    nvgLineTo(vg, xPos, yPos);
    nvgStrokeColor(vg, borderColor);
    nvgStroke(vg);
}

void Table::addRow(const std::vector<std::string>& row) {
    rows_.push_back(row);
}

void Table::clearRows() {
    rows_.clear();
    selected_row_ = -1;
    hover_row_ = -1;
}

void Table::setSelectedRow(int row) {
    if (row >= -1 && row < static_cast<int>(rows_.size())) {
        selected_row_ = row;
    }
}

int Table::getRowAtPosition(float y) const {
    auto* el = element();
    float tableY = el->computed.y;
    float headerEnd = tableY + style_.headerHeight;
    
    if (y < headerEnd) return -1;  // Clicked on header
    
    int row = static_cast<int>((y - headerEnd) / style_.rowHeight);
    return (row >= 0 && row < static_cast<int>(rows_.size())) ? row : -1;
}

int Table::getColumnAtPosition(float x) const {
    auto* el = element();
    float tableX = el->computed.x;
    float xPos = tableX;
    
    for (size_t i = 0; i < columnWidths_.size(); ++i) {
        xPos += columnWidths_[i];
        if (x < xPos) {
            return static_cast<int>(i);
        }
    }
    
    return -1;
}

bool Table::handleMouseDown(float x, float y) {
    int row = getRowAtPosition(y);
    int col = getColumnAtPosition(x);
    
    if (row >= 0) {
        selected_row_ = row;
        if (selection_callback_) {
            selection_callback_(row, col);
        }
        return true;
    }
    
    return false;
}

bool Table::handleMouseMove(float x, float y) {
    int row = getRowAtPosition(y);
    hover_row_ = row;
    return row >= 0;
}

} // namespace flexui
