#include <flexui/table.h>
#include <nanovg_css_internal.h>

namespace flexui {

Table::Table(NVGCSSRenderer* renderer, const std::string& id,
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

    nvgFontSize(vg, style_.fontSize);
    nvgFontFace(vg, "sans-serif");

    float totalWidth = 0;
    for (float cw : columnWidths_) {
        totalWidth += cw;
    }

    // Draw header background
    nvgBeginPath(vg);
    nvgRect(vg, x, y, totalWidth, style_.headerHeight);
    nvgFillColor(vg, style_.headerBg);
    nvgFill(vg);

    // Draw header text
    nvgFillColor(vg, style_.textColor);
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
    nvgStrokeColor(vg, style_.borderColor);
    nvgStrokeWidth(vg, 1);
    nvgStroke(vg);

    // Draw rows
    float yPos = y + style_.headerHeight;
    for (size_t r = 0; r < rows_.size(); ++r) {
        // Alternate row background
        NVGcolor bgColor = (r % 2 == 0) ? style_.rowBg : style_.altRowBg;
        nvgBeginPath(vg);
        nvgRect(vg, x, yPos, totalWidth, style_.rowHeight);
        nvgFillColor(vg, bgColor);
        nvgFill(vg);

        // Row text
        xPos = x + 10;
        nvgFillColor(vg, style_.textColor);
        for (size_t c = 0; c < rows_[r].size() && c < columnWidths_.size(); ++c) {
            nvgText(vg, xPos, yPos + style_.rowHeight / 2, rows_[r][c].c_str(), nullptr);
            xPos += columnWidths_[c];
        }

        // Row border
        nvgBeginPath(vg);
        nvgMoveTo(vg, x, yPos + style_.rowHeight);
        nvgLineTo(vg, x + totalWidth, yPos + style_.rowHeight);
        nvgStrokeColor(vg, style_.borderColor);
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
        nvgStrokeColor(vg, style_.borderColor);
        nvgStrokeWidth(vg, 1);
        nvgStroke(vg);
        xPos += cw;
    }
    // Final right border
    nvgBeginPath(vg);
    nvgMoveTo(vg, xPos, y);
    nvgLineTo(vg, xPos, yPos);
    nvgStrokeColor(vg, style_.borderColor);
    nvgStroke(vg);
}

void Table::addRow(const std::vector<std::string>& row) {
    rows_.push_back(row);
}

void Table::clearRows() {
    rows_.clear();
}

} // namespace flexui
