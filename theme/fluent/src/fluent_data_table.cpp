#include <nanogui/fluent_data_table.h>
#include <nanogui/fluent_theme.h>
#include <nanogui/opengl.h>
#include <algorithm>

NAMESPACE_BEGIN(nanogui)

FluentDataTable::FluentDataTable(Widget *parent)
    : Widget(parent), m_selectable(true), m_sort_column(-1), m_sort_ascending(true) {
}

void FluentDataTable::add_column(const std::string &header, int width, bool sortable) {
    m_columns.emplace_back(header, width, sortable);
}

void FluentDataTable::add_row(const std::vector<std::string> &cells) {
    m_rows.push_back(cells);
}

void FluentDataTable::clear_rows() {
    m_rows.clear();
    m_selected_rows.clear();
}

void FluentDataTable::sort_by_column(int column_index) {
    if (column_index < 0 || column_index >= (int)m_columns.size())
        return;
    
    if (!m_columns[column_index].sortable)
        return;
    
    // Toggle sort direction if same column
    if (m_sort_column == column_index) {
        m_sort_ascending = !m_sort_ascending;
    } else {
        m_sort_column = column_index;
        m_sort_ascending = true;
    }
    
    // Sort rows
    std::sort(m_rows.begin(), m_rows.end(), 
        [this, column_index](const std::vector<std::string> &a, const std::vector<std::string> &b) {
            if (column_index >= (int)a.size() || column_index >= (int)b.size())
                return false;
            
            if (m_sort_ascending)
                return a[column_index] < b[column_index];
            else
                return a[column_index] > b[column_index];
        });
}

Vector2i FluentDataTable::preferred_size_impl(NVGcontext *ctx) const {
    int total_width = 0;
    for (const auto &col : m_columns) {
        total_width += col.width;
    }
    
    int height = 56 + (m_rows.size() * 52); // Header + rows
    
    return Vector2i(total_width, height);
}

bool FluentDataTable::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!down || button != GLFW_MOUSE_BUTTON_1)
        return false;
    
    // Check header clicks for sorting
    if (p.y() < m_pos.y() + 56) {
        int x_offset = m_pos.x();
        for (size_t i = 0; i < m_columns.size(); ++i) {
            if (p.x() >= x_offset && p.x() < x_offset + m_columns[i].width) {
                sort_by_column(i);
                return true;
            }
            x_offset += m_columns[i].width;
        }
        return false;
    }
    
    // Check row clicks for selection
    if (m_selectable) {
        int row_index = (p.y() - m_pos.y() - 56) / 52;
        if (row_index >= 0 && row_index < (int)m_rows.size()) {
            auto it = std::find(m_selected_rows.begin(), m_selected_rows.end(), row_index);
            if (it != m_selected_rows.end()) {
                m_selected_rows.erase(it);
            } else {
                if (!(modifiers & GLFW_MOD_CONTROL)) {
                    m_selected_rows.clear();
                }
                m_selected_rows.push_back(row_index);
            }
            
            if (m_callback)
                m_callback(m_selected_rows);
            
            return true;
        }
    }
    
    return false;
}

void FluentDataTable::draw(NVGcontext *ctx) {
    Widget::draw(ctx);
    
    auto theme = dynamic_cast<FluentTheme*>(m_theme.get());
    if (!theme) return;
    
    // Background
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    // Header
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), 56);
    nvgFillColor(ctx, theme->surface_color());
    nvgFill(ctx);
    
    // Column headers
    int x_offset = m_pos.x();
    for (size_t i = 0; i < m_columns.size(); ++i) {
        const auto &col = m_columns[i];
        
        nvgFontSize(ctx, 14.0f);
        nvgFontFace(ctx, "sans-bold");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, theme->on_surface_color());
        nvgText(ctx, x_offset + 16, m_pos.y() + 28, col.header.c_str(), nullptr);
        
        // Sort indicator
        if (m_sort_column == (int)i) {
            const char *arrow = m_sort_ascending ? "▲" : "▼";
            nvgFontSize(ctx, 10.0f);
            nvgText(ctx, x_offset + col.width - 24, m_pos.y() + 28, arrow, nullptr);
        }
        
        x_offset += col.width;
    }
    
    // Rows
    int y_offset = m_pos.y() + 56;
    for (size_t row_idx = 0; row_idx < m_rows.size(); ++row_idx) {
        const auto &row = m_rows[row_idx];
        
        // Row background (if selected)
        bool is_selected = std::find(m_selected_rows.begin(), m_selected_rows.end(), 
                                     row_idx) != m_selected_rows.end();
        if (is_selected) {
            nvgBeginPath(ctx);
            nvgRect(ctx, m_pos.x(), y_offset, m_size.x(), 52);
            nvgFillColor(ctx, Color(0.9f, 0.9f, 1.0f, 1.0f));
            nvgFill(ctx);
        }
        
        // Cells
        x_offset = m_pos.x();
        for (size_t col_idx = 0; col_idx < m_columns.size() && col_idx < row.size(); ++col_idx) {
            nvgFontSize(ctx, 14.0f);
            nvgFontFace(ctx, "sans");
            nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
            nvgFillColor(ctx, is_selected ? Color(0.1f, 0.1f, 0.2f, 1.0f) : theme->on_surface_color());
            nvgText(ctx, x_offset + 16, y_offset + 26, row[col_idx].c_str(), nullptr);
            
            x_offset += m_columns[col_idx].width;
        }
        
        // Row divider
        nvgBeginPath(ctx);
        nvgRect(ctx, m_pos.x(), y_offset + 51, m_size.x(), 1);
        nvgFillColor(ctx, Color(0.7f, 0.7f, 0.7f, 1.0f));
        nvgFill(ctx);
        
        y_offset += 52;
    }
}

NAMESPACE_END(nanogui)
