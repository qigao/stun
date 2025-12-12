#pragma once

struct GLFWwindow;
#include <nanovg.h>
#include <cssbox.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

// Forward declaration for pugixml (avoids header dependency)
namespace pugi {
class xml_node;
}

namespace flexui {

class Widget;
class TextBox;
class RadioButton;
class JSEngine;
class SpatialIndex;
class FontManager;

class Screen {
public:
    using CustomDrawCallback = std::function<void(NVGcontext*)>;
    using EventHandler = std::function<bool(Widget*)>;

    Screen(int width, int height, const std::string& title);
    ~Screen();

    void setFocusedTextBox(TextBox* textbox) { focused_textbox_ = textbox; }
    void markSpatialIndexDirty() { spatial_index_dirty_ = true; }
    void markDirty() { needs_redraw_ = true; }  // Mark screen for redraw

    bool loadCSS(const std::string& css);
    bool loadCSSFile(const std::string& path);  // Load CSS from file
    bool loadXML(const std::string& xml);  // Load UI from XML
    bool loadXMLFile(const std::string& path);  // Load XML from file
    bool loadJS(const std::string& code);  // Load JavaScript code
    bool loadJSFile(const std::string& path);  // Load JavaScript from file
    bool loadJSModule(const std::string& path);  // Load ES6 module from file
    Widget* addWidget(const std::string& id, const std::string& tag);

    // CSS Variables
    void setCSSVariable(const std::string& name, const std::string& value);
    std::string getCSSVariable(const std::string& name);

    // SVG element creation
    Widget* createLine(const std::string& id, float x1, float y1, float x2, float y2);
    Widget* createCircle(const std::string& id, float cx, float cy, float r);
    Widget* createEllipse(const std::string& id, float cx, float cy, float rx, float ry);
    Widget* createRect(const std::string& id, float x, float y, float w, float h);
    Widget* createPath(const std::string& id);
    Widget* createPolygon(const std::string& id, const std::vector<std::pair<float, float>>& points);
    Widget* createPolyline(const std::string& id, const std::vector<std::pair<float, float>>& points);

    // JavaScript engine access
    JSEngine* jsEngine() { return js_engine_.get(); }

    // Event handler registration
    void registerHandler(const std::string& name, EventHandler handler);
    Widget* findWidget(const std::string& id);  // Find widget by ID

    // Radio button group management
    void registerRadioButton(RadioButton* radio);
    void setRadioGroupValue(const std::string& group, const std::string& value);
    std::string getRadioGroupValue(const std::string& group) const;

    template<typename T, typename... Args>
    T* createWidget(Args&&... args) {
        auto widget = std::make_unique<T>(renderer_, std::forward<Args>(args)...);
        T* ptr = widget.get();
        widgets_.push_back(std::move(widget));
        return ptr;
    }

    bool pollEvents();
    void draw();

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    
    void setCustomDrawCallback(CustomDrawCallback cb) { custom_draw_callback_ = cb; }
    
    NVGcontext* vg() { return vg_; }
    cssboxRenderer* renderer() { return renderer_; }
    GLFWwindow* window() { return window_; }
    FontManager* fontManager() { return font_manager_.get(); }

    // GLFW callback handlers (called from static callbacks)
    void handleCursorPos(double xpos, double ypos);
    void handleMouseButton(int button, int action, int mods);
    void handleKey(int key, int scancode, int action, int mods);
    void handleChar(unsigned int codepoint);
    void handleScroll(double xoffset, double yoffset);
    void handleWindowRefresh();

private:
    GLFWwindow* window_ = nullptr;
    NVGcontext* vg_ = nullptr;
    cssboxRenderer* renderer_ = nullptr;

    std::vector<std::unique_ptr<Widget>> widgets_;
    std::unordered_map<std::string, Widget*> widget_map_;  // ID -> Widget lookup
    std::unordered_map<std::string, EventHandler> event_handlers_;  // Handler name -> function
    std::unordered_map<std::string, std::vector<RadioButton*>> radio_groups_;  // Group name -> RadioButtons
    std::unique_ptr<JSEngine> js_engine_;  // JavaScript engine
    std::unique_ptr<SpatialIndex> spatial_index_;  // Spatial index for fast widget lookup
    std::unique_ptr<FontManager> font_manager_;  // Font manager
    CustomDrawCallback custom_draw_callback_;
    TextBox* focused_textbox_ = nullptr;
    int width_, height_;
    bool spatial_index_dirty_ = true;  // Rebuild spatial index when true
    bool needs_redraw_ = true;  // Redraw screen when true (Retained Mode)
    int widget_counter_ = 0;
    double last_mouse_x_ = 0, last_mouse_y_ = 0;  // Track mouse position for scroll events

    // XML parsing helpers (pugi::xml_node forward declared above)
    Widget* parseXMLNode(pugi::xml_node& node, Widget* parent);
    void applyXMLAttributes(Widget* widget, pugi::xml_node& node);
};

} // namespace flexui
