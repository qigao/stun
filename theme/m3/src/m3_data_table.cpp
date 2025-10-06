/*
    src/m3_data_table.cpp -- M3 Data Table implementation
*/

#include <nanogui/m3_data_table.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

M3DataTable::M3DataTable(Widget *parent)
    : Widget(parent) {
}

M3Theme *M3DataTable::m3_theme() const {
    return dynamic_cast<M3Theme*>(const_cast<Theme*>(m_theme.get()));
}

void M3DataTable::set_columns(const std::vector<Column> &columns) {
    m_columns = columns;
}

void M3DataTable::set_data(const std::vector<std::vector<std::string>> &data) {
    m_data = data;
}

void M3DataTable::set_selected_row(int row) {
    if (row >= -1 && row < static_cast<int>(m_data.size())) {
        m_selected_row = row;
    }
}

int M3DataTable::row_at_position(const Vector2i &p) const {
    Vector2i local = p - m_pos;
    int header_height = 56;
    int row_height = 52;
    
    if (local.y() < header_height) return -1;
    
    int row = (local.y() - header_height) / row_height;
    return (row >= 0 && row < static_cast<int>(m_data.size())) ? row : -1;
}

int M3DataTable::column_at_position(const Vector2i &p) const {
    Vector2i local = p - m_pos;
    
    if (local.y() >= 56) return -1; // Not in header
    
    int x_offset = 0;
    for (size_t i = 0; i < m_columns.size(); ++i) {
        if (local.x() >= x_offset && local.x() < x_offset + m_columns[i].width) {
            return static_cast<int>(i);
        }
        x_offset += m_columns[i].width;
    }
    return -1;
}

bool M3DataTable::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
    Widget::mouse_button_event(p, button, down, modifiers);
    
    if (!m_enabled || button != NANOGUI_MOUSE_BUTTON_LEFT || !down) return false;
    
    // Check for header click (sorting)
    int col = column_at_position(p);
    if (col >= 0 && m_columns[col].sortable) {
        if (m_sort_column == col) {
            m_sort_ascending = !m_sort_ascending;
        } else {
            m_sort_column = col;
            m_sort_ascending = true;
        }
        if (m_sort_callback) {
            m_sort_callback(col, m_sort_ascending);
        }
        return true;
    }
    
    // Check for row click
    int row = row_at_position(p);
    if (row >= 0 && m_selectable) {
        m_selected_row = row;
        if (m_row_callback) {
            m_row_callback(row);
        }
        return true;
    }
    
    return false;
}

Vector2i M3DataTable::preferred_size(NVGcontext *) const {
    int width = 0;
    for (const auto &col : m_columns) {
        width += col.width;
    }
    int height = 56 + static_cast<int>(m_data.size()) * 52;
    return Vector2i(width, height);
}

void M3DataTable::draw(NVGcontext *ctx) {
    M3Theme *theme = m3_theme();
    if (!theme) { Widget::draw(ctx); return; }

    float x = m_pos.x(), y = m_pos.y(), w = m_size.x();
    int header_height = 56;
    int row_height = 52;

    nvgSave(ctx);

    // Background
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, m_size.y());
    nvgFillColor(ctx, theme->surface());
    nvgFill(ctx);

    // Header background
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, header_height);
    nvgFillColor(ctx, theme->surface_variant());
    nvgFill(ctx);

    // Header text
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, theme->on_surface());

    float col_x = x;
    for (size_t i = 0; i < m_columns.size(); ++i) {
        const auto &col = m_columns[i];
        
        // Column header
        nvgText(ctx, col_x + 16, y + header_height * 0.5f, col.header.c_str(), nullptr);
        
        // Sort indicator
        if (m_sort_column == static_cast<int>(i)) {
            nvgFontSize(ctx, 16);
            nvgFontFace(ctx, "icons");
            int icon = m_sort_ascending ? 0xf0d8 : 0xf0d7; // Up/down arrow
            nvgText(ctx, col_x + col.width - 32, y + header_height * 0.5f, 
                   utf8(icon).data(), nullptr);
            nvgFontSize(ctx, 14);
            nvgFontFace(ctx, "sans-bold");
        }
        
        // Column divider
        if (i < m_columns.size() - 1) {
            nvgBeginPath(ctx);
            nvgRect(ctx, col_x + col.width, y, 1, header_height);
            nvgFillColor(ctx, theme->outline());
            nvgFill(ctx);
        }
        
        col_x += col.width;
    }

    // Horizontal divider after header
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y + header_height, w, 1);
    nvgFillColor(ctx, theme->outline());
    nvgFill(ctx);

    // Data rows
    nvgFontSize(ctx, 14);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    for (size_t row = 0; row < m_data.size(); ++row) {
        float row_y = y + header_height + row * row_height;
        bool is_selected = (static_cast<int>(row) == m_selected_row);
        bool is_hovered = (static_cast<int>(row) == m_hover_row);

        // Row background
        if (is_selected) {
            nvgBeginPath(ctx);
            nvgRect(ctx, x, row_y, w, row_height);
            nvgFillColor(ctx, theme->secondary_container());
            nvgFill(ctx);
        } else if (is_hovered && m_selectable) {
            nvgBeginPath(ctx);
            nvgRect(ctx, x, row_y, w, row_height);
            nvgFillColor(ctx, theme->state_layer(theme->on_surface(), 0.08f));
            nvgFill(ctx);
        }

        // Row data
        col_x = x;
        for (size_t col = 0; col < m_columns.size() && col < m_data[row].size(); ++col) {
            Color text_color = is_selected ? theme->on_secondary_container() : theme->on_surface();
            nvgFillColor(ctx, text_color);
            
            // Truncate text if too long
            const std::string &text = m_data[row][col];
            // float max_width = m_columns[col].width - 32;
            
            nvgText(ctx, col_x + 16, row_y + row_height * 0.5f, text.c_str(), nullptr);
            
            col_x += m_columns[col].width;
        }

        // Row divider
        nvgBeginPath(ctx);
        nvgRect(ctx, x, row_y + row_height, w, 1);
        nvgFillColor(ctx, theme->outline());
        nvgFill(ctx);
    }

    nvgRestore(ctx);
    Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
