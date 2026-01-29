/*
 * flexUI Designer - Widget Search Panel
 * 
 * Quick search to find and select widgets by ID or text.
 */

#pragma once

#include "designer.h"
#include <meta_editor/view/panel.h>
#include <string>
#include <vector>
#include <functional>

namespace flexui_designer {

class SearchPanel : public meta_editor::Panel {
public:
    SearchPanel();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float x, float y) override;
    
    void show() { visible_ = true; query_.clear(); results_.clear(); selected_index_ = 0; }
    void hide() { visible_ = false; }
    
    void set_widgets(const std::vector<DesignWidget>* widgets) { widgets_ = widgets; }
    void set_select_callback(std::function<void(const std::string&)> cb) { on_select_ = std::move(cb); }
    
    bool handle_text_input(const char* text);
    bool handle_key(int key);
    
    // Accessors for testing
    const std::vector<const DesignWidget*>& results() const { return results_; }
    int selected_index() const { return selected_index_; }
    const std::string& query() const { return query_; }

private:
    void update_results();
    
    const std::vector<DesignWidget>* widgets_ = nullptr;
    std::string query_;
    std::vector<const DesignWidget*> results_;
    int selected_index_ = 0;
    std::function<void(const std::string&)> on_select_;
};

} // namespace flexui_designer
