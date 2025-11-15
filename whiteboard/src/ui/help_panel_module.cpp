#include "whiteboard/ui/help_panel_module.h"
#include "whiteboard/model/whiteboard_document.h"
#include <nanogui/opengl.h>
#include <nanogui/theme.h>
#include <nanogui/keys.h>
#include <nanovg.h>
#include <algorithm>

namespace whiteboard {

HelpPanelModule::HelpPanelModule(nanogui::Widget* parent, WhiteboardDocument* document)
    : Widget(parent), m_document(document), m_scroll_offset(0.0f), m_is_visible(false) {
    set_visible(false);
    build_shortcuts_list();
}

void HelpPanelModule::show() {
    m_is_visible = true;
    set_visible(true);
}

void HelpPanelModule::hide() {
    m_is_visible = false;
    set_visible(false);
}

void HelpPanelModule::build_shortcuts_list() {
    m_shortcuts.clear();
    
    // File operations
    m_shortcuts.push_back({"File", "New", "Ctrl+N"});
    m_shortcuts.push_back({"File", "Open", "Ctrl+O"});
    m_shortcuts.push_back({"File", "Save", "Ctrl+S"});
    m_shortcuts.push_back({"File", "Save As", "Ctrl+Shift+S"});
    
    // Edit operations
    m_shortcuts.push_back({"Edit", "Undo", "Ctrl+Z"});
    m_shortcuts.push_back({"Edit", "Redo", "Ctrl+Y"});
    m_shortcuts.push_back({"Edit", "Copy", "Ctrl+C"});
    m_shortcuts.push_back({"Edit", "Cut", "Ctrl+X"});
    m_shortcuts.push_back({"Edit", "Paste", "Ctrl+V"});
    m_shortcuts.push_back({"Edit", "Duplicate", "Ctrl+D"});
    m_shortcuts.push_back({"Edit", "Delete", "Del / Backspace"});
    
    // Selection
    m_shortcuts.push_back({"Selection", "Select All", "Ctrl+A"});
    m_shortcuts.push_back({"Selection", "Clear Selection", "Esc"});
    m_shortcuts.push_back({"Selection", "Multi-select", "Shift+Click"});
    m_shortcuts.push_back({"Selection", "Quick Select (any tool)", "Ctrl+Click"});
    
    // Grouping
    m_shortcuts.push_back({"Grouping", "Group", "Ctrl+G"});
    m_shortcuts.push_back({"Grouping", "Ungroup", "Ctrl+Shift+G"});
    
    // Layers
    m_shortcuts.push_back({"Layers", "Bring Forward", "Ctrl+]"});
    m_shortcuts.push_back({"Layers", "Send Backward", "Ctrl+["});
    m_shortcuts.push_back({"Layers", "Bring to Front", "Ctrl+Shift+]"});
    m_shortcuts.push_back({"Layers", "Send to Back", "Ctrl+Shift+["});
    
    // Navigation
    m_shortcuts.push_back({"Navigation", "Pan", "Space+Drag / Middle Mouse"});
    m_shortcuts.push_back({"Navigation", "Zoom In", "Mouse Wheel Up"});
    m_shortcuts.push_back({"Navigation", "Zoom Out", "Mouse Wheel Down"});
    
    // Tools
    m_shortcuts.push_back({"Tools", "Select Tool", "V"});
    m_shortcuts.push_back({"Tools", "Pan Tool", "H"});
    m_shortcuts.push_back({"Tools", "Pen Tool", "P"});
    m_shortcuts.push_back({"Tools", "Rectangle Tool", "R"});
    m_shortcuts.push_back({"Tools", "Circle Tool", "C"});
    m_shortcuts.push_back({"Tools", "Text Tool", "T"});
    
    // View
    m_shortcuts.push_back({"View", "Toggle Grid", "Ctrl+'"});
    m_shortcuts.push_back({"View", "Toggle Guides", "Ctrl+;"});
    m_shortcuts.push_back({"View", "Search", "Ctrl+F"});
    m_shortcuts.push_back({"View", "Help", "F1 / Ctrl+/"});
}

std::vector<HelpPanelModule::ShortcutItem> HelpPanelModule::filter_shortcuts(const std::string& query) {
    if (query.empty()) {
        return m_shortcuts;
    }
    
    std::vector<ShortcutItem> filtered;
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    
    for (const auto& item : m_shortcuts) {
        std::string lower_desc = item.description;
        std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);
        
        std::string lower_keys = item.keys;
        std::transform(lower_keys.begin(), lower_keys.end(), lower_keys.begin(), ::tolower);
        
        if (lower_desc.find(lower_query) != std::string::npos ||
            lower_keys.find(lower_query) != std::string::npos) {
            filtered.push_back(item);
        }
    }
    
    return filtered;
}

nanogui::Vector2i HelpPanelModule::preferred_size_impl(NVGcontext*) const {
    return nanogui::Vector2i(600, 700);
}

bool HelpPanelModule::mouse_button_event(const nanogui::Vector2i& p, int button, bool down, int modifiers) {
    if (!m_is_visible)
        return false;
    
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    // Check if clicking outside the panel
    int panel_width = 600;
    int panel_height = 700;
    int panel_x = (root->width() - panel_width) / 2;
    int panel_y = (root->height() - panel_height) / 2;
    
    if (p.x() < panel_x || p.x() > panel_x + panel_width ||
        p.y() < panel_y || p.y() > panel_y + panel_height) {
        if (down) {
            hide();
        }
        return true;
    }
    
    return Widget::mouse_button_event(p, button, down, modifiers);
}

bool HelpPanelModule::keyboard_event(int key, int scancode, int action, int modifiers) {
    if (!m_is_visible)
        return false;
    
    if (action == NANOGUI_KEY_PRESS && key == NANOGUI_KEY_ESCAPE) {
        hide();
        return true;
    }
    
    return Widget::keyboard_event(key, scancode, action, modifiers);
}

void HelpPanelModule::draw(NVGcontext* ctx) {
    if (!m_is_visible)
        return;
    
    draw_background(ctx);
    draw_panel(ctx);
    
    Widget::draw(ctx);
}

void HelpPanelModule::draw_background(NVGcontext* ctx) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    // Draw dimmed overlay
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, (float)root->width(), (float)root->height());
    nvgFillColor(ctx, nvgRGBA(0, 0, 0, 128));
    nvgFill(ctx);
}

void HelpPanelModule::draw_panel(NVGcontext* ctx) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_width = 600;
    int panel_height = 700;
    int panel_x = (root->width() - panel_width) / 2;
    int panel_y = (root->height() - panel_height) / 2;
    
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    nvgSave(ctx);
    
    // Draw shadow
    NVGpaint shadow_paint = nvgBoxGradient(ctx, panel_x, panel_y + 4, panel_width, panel_height,
                                          12, 20, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, panel_x - 10, panel_y - 10, panel_width + 20, panel_height + 20, 14);
    nvgFillPaint(ctx, shadow_paint);
    nvgFill(ctx);
    
    // Draw panel background (dark or light)
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, panel_x, panel_y, panel_width, panel_height, 12);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(43, 43, 43, 255));  // Dark background
    } else {
        nvgFillColor(ctx, nvgRGBA(250, 250, 250, 255));  // Light background
    }
    nvgFill(ctx);
    
    // Draw border
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, panel_x, panel_y, panel_width, panel_height, 12);
    if (dark_mode) {
        nvgStrokeColor(ctx, nvgRGBA(80, 80, 80, 255));  // Light border for dark mode
    } else {
        nvgStrokeColor(ctx, nvgRGBA(200, 200, 200, 255));  // Dark border for light mode
    }
    nvgStrokeWidth(ctx, 1.0f);
    nvgStroke(ctx);
    
    // Set up scissor for content
    nvgScissor(ctx, panel_x + 20, panel_y + 20, panel_width - 40, panel_height - 40);
    
    float y = panel_y + 30;
    draw_header(ctx, y);
    draw_shortcuts(ctx, y);
    
    nvgResetScissor(ctx);
    
    draw_close_button(ctx);
    
    nvgRestore(ctx);
}

void HelpPanelModule::draw_header(NVGcontext* ctx, float& y) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_width = 600;
    int panel_x = (root->width() - panel_width) / 2;
    
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    // Draw title
    nvgFontSize(ctx, 28.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(224, 224, 224, 255));  // Light text for dark mode
    } else {
        nvgFillColor(ctx, nvgRGBA(50, 50, 50, 255));  // Dark text for light mode
    }
    nvgText(ctx, panel_x + 30, y, "Keyboard Shortcuts", nullptr);
    
    y += 50;
}

void HelpPanelModule::draw_shortcuts(NVGcontext* ctx, float& y) {
    auto filtered = filter_shortcuts(m_search_query);
    
    std::string current_category;
    for (const auto& item : filtered) {
        if (item.category != current_category) {
            current_category = item.category;
            draw_category(ctx, current_category, y);
        }
        draw_shortcut_item(ctx, item, y);
    }
}

void HelpPanelModule::draw_category(NVGcontext* ctx, const std::string& category, float& y) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_width = 600;
    int panel_x = (root->width() - panel_width) / 2;
    
    y += 10;
    
    // Draw category header (blue in both modes for consistency)
    nvgFontSize(ctx, 18.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(ctx, nvgRGBA(33, 150, 243, 255));  // Blue category headers
    nvgText(ctx, panel_x + 30, y, category.c_str(), nullptr);
    
    y += 30;
}

void HelpPanelModule::draw_shortcut_item(NVGcontext* ctx, const ShortcutItem& item, float& y) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_width = 600;
    int panel_x = (root->width() - panel_width) / 2;
    
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    // Draw description
    nvgFontSize(ctx, 14.0f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(200, 200, 200, 255));  // Light gray for dark mode
    } else {
        nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));  // Dark gray for light mode
    }
    nvgText(ctx, panel_x + 50, y, item.description.c_str(), nullptr);
    
    // Draw keys (right-aligned)
    nvgFontSize(ctx, 13.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(160, 160, 160, 255));  // Medium gray for dark mode
    } else {
        nvgFillColor(ctx, nvgRGBA(120, 120, 120, 255));  // Dark gray for light mode
    }
    nvgText(ctx, panel_x + panel_width - 50, y, item.keys.c_str(), nullptr);
    
    y += 28;
}

void HelpPanelModule::draw_close_button(NVGcontext* ctx) {
    nanogui::Widget* root = this;
    while (root->parent()) root = root->parent();
    
    int panel_width = 600;
    int panel_x = (root->width() - panel_width) / 2;
    int panel_y = (root->height() - 700) / 2;
    
    bool dark_mode = m_document && m_document->get_dark_mode();
    
    float btn_x = panel_x + panel_width - 40;
    float btn_y = panel_y + 20;
    float btn_size = 24;
    
    // Draw close button background
    nvgBeginPath(ctx);
    nvgCircle(ctx, btn_x, btn_y, btn_size / 2);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(80, 80, 80, 255));  // Dark gray for dark mode
    } else {
        nvgFillColor(ctx, nvgRGBA(220, 220, 220, 255));  // Light gray for light mode
    }
    nvgFill(ctx);
    
    // Draw X
    nvgFontSize(ctx, 18.0f);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    if (dark_mode) {
        nvgFillColor(ctx, nvgRGBA(200, 200, 200, 255));  // Light text for dark mode
    } else {
        nvgFillColor(ctx, nvgRGBA(100, 100, 100, 255));  // Dark text for light mode
    }
    nvgText(ctx, btn_x, btn_y, "×", nullptr);
}

} // namespace whiteboard
