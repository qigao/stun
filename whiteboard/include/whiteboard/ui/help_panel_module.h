#pragma once

#include <nanogui/widget.h>
#include <string>
#include <vector>

namespace whiteboard {

class WhiteboardDocument;

class HelpPanelModule : public nanogui::Widget {
public:
    struct ShortcutItem {
        std::string category;
        std::string description;
        std::string keys;
    };
    
    HelpPanelModule(nanogui::Widget* parent, WhiteboardDocument* document);
    
    void show();
    void hide();
    bool is_visible() const { return m_is_visible; }
    
    nanogui::Vector2i preferred_size_impl(NVGcontext*) const override;
    bool mouse_button_event(const nanogui::Vector2i& p, int button, bool down, int modifiers) override;
    bool keyboard_event(int key, int scancode, int action, int modifiers) override;
    void draw(NVGcontext* ctx) override;
    
private:
    WhiteboardDocument* m_document;
    std::vector<ShortcutItem> m_shortcuts;
    std::string m_search_query;
    float m_scroll_offset;
    bool m_is_visible;
    
    void build_shortcuts_list();
    void draw_background(NVGcontext* ctx);
    void draw_panel(NVGcontext* ctx);
    void draw_header(NVGcontext* ctx, float& y);
    void draw_search_box(NVGcontext* ctx, float& y);
    void draw_shortcuts(NVGcontext* ctx, float& y);
    void draw_category(NVGcontext* ctx, const std::string& category, float& y);
    void draw_shortcut_item(NVGcontext* ctx, const ShortcutItem& item, float& y);
    void draw_close_button(NVGcontext* ctx);
    
    std::vector<ShortcutItem> filter_shortcuts(const std::string& query);
};

} // namespace whiteboard
