/*
    nanogui/m3_data_table.h -- M3 Data Table

    Based on: https://m3.material.io/components/data-tables
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/m3_theme.h>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

class NANOGUI_EXPORT M3DataTable : public Widget {
public:
    struct Column {
        std::string header;
        int width;
        bool sortable;
        
        Column(const std::string &h, int w = 150, bool s = true)
            : header(h), width(w), sortable(s) {}
    };

    M3DataTable(Widget *parent);

    void set_columns(const std::vector<Column> &columns);
    void set_data(const std::vector<std::vector<std::string>> &data);
    
    void set_row_callback(const std::function<void(int)> &callback) { m_row_callback = callback; }
    void set_sort_callback(const std::function<void(int, bool)> &callback) { m_sort_callback = callback; }

    void set_selectable(bool selectable) { m_selectable = selectable; }
    void set_selected_row(int row);
    int selected_row() const { return m_selected_row; }

    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    Vector2i preferred_size(NVGcontext *ctx) const;

protected:
    M3Theme *m3_theme() const;
    int row_at_position(const Vector2i &p) const;
    int column_at_position(const Vector2i &p) const;

    std::vector<Column> m_columns;
    std::vector<std::vector<std::string>> m_data;
    std::function<void(int)> m_row_callback;
    std::function<void(int, bool)> m_sort_callback;
    
    bool m_selectable = true;
    int m_selected_row = -1;
    int m_hover_row = -1;
    int m_sort_column = -1;
    bool m_sort_ascending = true;
};

NAMESPACE_END(nanogui)
