/*
 * flexUI Designer - Search Panel Implementation
 */

#include "flexui_designer/search_panel.h"
#include <algorithm>
#include <cctype>

namespace flexui_designer {

namespace {

// Convert string to lowercase for case-insensitive matching
std::string to_lower(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

} // anonymous namespace

SearchPanel::SearchPanel() {
    set_size(300, 400);
    set_draggable(false);
}

void SearchPanel::update_results() {
    results_.clear();
    if (query_.empty() || !widgets_) return;
    
    std::string lower_query = to_lower(query_);
    for (const auto& w : *widgets_) {
        // Match against ID (case-insensitive)
        if (to_lower(w.id).find(lower_query) != std::string::npos) {
            results_.push_back(&w);
            continue;
        }
        // Match against text content (case-insensitive)
        if (to_lower(w.text).find(lower_query) != std::string::npos) {
            results_.push_back(&w);
        }
    }
    
    // Reset selection if out of bounds
    if (selected_index_ >= (int)results_.size()) {
        selected_index_ = results_.empty() ? 0 : (int)results_.size() - 1;
    }
}

bool SearchPanel::handle_text_input(const char* text) {
    if (!visible_) return false;
    
    query_ += text;
    update_results();
    selected_index_ = 0;
    return true;
}

bool SearchPanel::handle_key(int key) {
    if (!visible_) return false;
    
    // Backspace - delete last character
    if (key == '\b' || key == 0x08) {
        if (!query_.empty()) {
            query_.pop_back();
            update_results();
        }
        return true;
    }
    
    // Up arrow - navigate up
    if (key == 0x26) {
        if (selected_index_ > 0) {
            selected_index_--;
        }
        return true;
    }
    
    // Down arrow - navigate down
    if (key == 0x28) {
        if (!results_.empty() && selected_index_ < (int)results_.size() - 1) {
            selected_index_++;
        }
        return true;
    }
    
    // Enter - select current result
    if (key == '\r' || key == '\n') {
        if (!results_.empty() && selected_index_ >= 0 && selected_index_ < (int)results_.size()) {
            if (on_select_) {
                on_select_(results_[selected_index_]->id);
            }
            hide();
        }
        return true;
    }
    
    // Escape - close panel
    if (key == 0x1B) {
        hide();
        return true;
    }
    
    return false;
}

void SearchPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;
    
    renderer.save();
    renderer.translate(x_, y_);

    render_background(renderer);
    
    const float PADDING = 10.0f;
    const float INPUT_HEIGHT = 32.0f;
    const float ENTRY_HEIGHT = 28.0f;
    
    // Header
    flex::Color header_text{0.9f, 0.9f, 0.9f, 1.0f};
    renderer.draw_text("Search Widgets", PADDING, 12, "sans", 13, true, header_text);
    
    // Search input box
    float input_y = 35;
    flex::Paint input_bg = flex::Paint::solid(flex::Color{0.12f, 0.12f, 0.14f, 1.0f});
    flex::Paint input_border = flex::Paint::solid(flex::Color{0.35f, 0.55f, 0.9f, 1.0f});
    renderer.draw_rect(PADDING, input_y, width_ - 2 * PADDING, INPUT_HEIGHT, 4, input_bg, input_border, 1.5f);
    
    // Query text with cursor
    flex::Color query_color{0.95f, 0.95f, 0.95f, 1.0f};
    std::string display_query = query_ + "|";  // Simple cursor
    if (query_.empty()) {
        flex::Color placeholder{0.5f, 0.5f, 0.5f, 1.0f};
        renderer.draw_text("Type to search...", PADDING + 8, input_y + 10, "sans", 12, false, placeholder);
    } else {
        renderer.draw_text(display_query, PADDING + 8, input_y + 10, "sans", 12, false, query_color);
    }
    
    // Results list
    float list_y = input_y + INPUT_HEIGHT + 10;
    float list_h = height_ - list_y - PADDING;
    
    if (results_.empty()) {
        // No results message
        flex::Color dim{0.5f, 0.5f, 0.5f, 1.0f};
        if (!query_.empty()) {
            renderer.draw_text("No results found", PADDING, list_y + 8, "sans", 11, false, dim);
        } else {
            renderer.draw_text("Start typing to search", PADDING, list_y + 8, "sans", 11, false, dim);
        }
        renderer.restore();
        return;
    }
    
    // Render result entries
    float entry_y = list_y;
    int max_visible = (int)(list_h / ENTRY_HEIGHT);
    
    for (int i = 0; i < (int)results_.size() && i < max_visible; ++i) {
        const DesignWidget* w = results_[i];
        bool is_selected = (i == selected_index_);
        
        // Highlight selected entry
        if (is_selected) {
            flex::Paint highlight = flex::Paint::solid(flex::Color{0.25f, 0.5f, 0.9f, 0.4f});
            renderer.draw_rect(5, entry_y, width_ - 10, ENTRY_HEIGHT - 2, 3, highlight, flex::Paint::none(), 0);
        }
        
        // Widget ID
        flex::Color id_color = is_selected ? flex::Color{0.4f, 0.7f, 1.0f, 1.0f} : flex::Color{0.85f, 0.85f, 0.85f, 1.0f};
        renderer.draw_text(w->id, PADDING + 4, entry_y + 8, "sans", 11, is_selected, id_color);
        
        // Widget text (if different from ID and not empty)
        if (!w->text.empty() && w->text != w->id) {
            flex::Color text_color{0.6f, 0.6f, 0.6f, 1.0f};
            std::string preview = w->text;
            if (preview.length() > 20) {
                preview = preview.substr(0, 17) + "...";
            }
            renderer.draw_text(preview, PADDING + 120, entry_y + 8, "sans", 10, false, text_color);
        }
        
        entry_y += ENTRY_HEIGHT;
    }
    
    // Show count if more results than visible
    if ((int)results_.size() > max_visible) {
        flex::Color count_color{0.5f, 0.5f, 0.5f, 1.0f};
        std::string count_text = "+" + std::to_string(results_.size() - max_visible) + " more";
        renderer.draw_text(count_text, PADDING, entry_y + 5, "sans", 10, false, count_color);
    }

    renderer.restore();
}

bool SearchPanel::handle_click(float screen_x, float screen_y) {
    if (!visible_ || !contains(screen_x, screen_y)) return false;
    
    const float INPUT_HEIGHT = 32.0f;
    const float ENTRY_HEIGHT = 28.0f;
    float list_y = y_ + 35 + INPUT_HEIGHT + 10;
    
    // Check if click is in results area
    float local_y = screen_y - list_y;
    if (local_y < 0) return true;  // Clicked in header/input area
    
    int clicked_index = (int)(local_y / ENTRY_HEIGHT);
    if (clicked_index >= 0 && clicked_index < (int)results_.size()) {
        selected_index_ = clicked_index;
        if (on_select_) {
            on_select_(results_[selected_index_]->id);
        }
        hide();
        return true;
    }
    
    return true;
}

} // namespace flexui_designer
