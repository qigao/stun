#pragma once

#include <nanogui/widget.h>
#include <vector>
#include <string>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Data Table
 * 
 * Displays data in rows and columns with sorting and selection.
 * Follows Fluent Design 3 specifications.
 */
class NANOGUI_EXPORT FluentDataTable : public Widget {
public:
    struct Column {
        std::string header;
        int width;
        bool sortable;
        
        Column(const std::string &h, int w = 100, bool s = true)
            : header(h), width(w), sortable(s) {}
    };
    
    FluentDataTable(Widget *parent);
    
    /// Add column
    void add_column(const std::string &header, int width = 100, bool sortable = true);
    
    /// Add row
    void add_row(const std::vector<std::string> &cells);
    
    /// Clear all rows
    void clear_rows();
    
    /// Enable row selection
    bool selectable() const { return m_selectable; }
    void set_selectable(bool selectable) { m_selectable = selectable; }
    
    /// Selected row indices
    std::vector<int> selected_rows() const { return m_selected_rows; }
    
    /// Selection callback
    std::function<void(const std::vector<int>&)> callback() const { return m_callback; }
    void set_callback(const std::function<void(const std::vector<int>&)> &callback) { 
        m_callback = callback; 
    }
    
    Vector2i preferred_size_impl(NVGcontext *ctx) const override;
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
    void draw(NVGcontext *ctx) override;
    
protected:
    void sort_by_column(int column_index);
    
    std::vector<Column> m_columns;
    std::vector<std::vector<std::string>> m_rows;
    std::vector<int> m_selected_rows;
    bool m_selectable;
    int m_sort_column;
    bool m_sort_ascending;
    std::function<void(const std::vector<int>&)> m_callback;
};

NAMESPACE_END(nanogui)
