#pragma once

#include <SDL3/SDL.h>
#include <nanovg.h>
#include <nanovg_css.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace flexui {

class Widget;
class TextBox;
class RadioButton;
class JSEngine;

class Screen {
public:
    using CustomDrawCallback = std::function<void(NVGcontext*)>;
    using EventHandler = std::function<bool(Widget*)>;

    Screen(int width, int height, const std::string& title);
    ~Screen();

    void setFocusedTextBox(TextBox* textbox) { focused_textbox_ = textbox; }

    bool loadCSS(const std::string& css);
    bool loadXML(const std::string& xml);  // Load UI from XML
    bool loadJS(const std::string& code);  // Load JavaScript code
    bool loadJSFile(const std::string& path);  // Load JavaScript from file
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
    NVGCSSRenderer* renderer() { return renderer_; }
    SDL_Window* window() { return window_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    NVGcontext* vg_ = nullptr;
    NVGCSSRenderer* renderer_ = nullptr;

    std::vector<std::unique_ptr<Widget>> widgets_;
    std::unordered_map<std::string, Widget*> widget_map_;  // ID -> Widget lookup
    std::unordered_map<std::string, EventHandler> event_handlers_;  // Handler name -> function
    std::unordered_map<std::string, std::vector<RadioButton*>> radio_groups_;  // Group name -> RadioButtons
    std::unique_ptr<JSEngine> js_engine_;  // JavaScript engine
    CustomDrawCallback custom_draw_callback_;
    TextBox* focused_textbox_ = nullptr;
    int width_, height_;
    int widget_counter_ = 0;

    // XML parsing helpers
    Widget* parseXMLNode(void* node, Widget* parent);  // void* to avoid pugixml dependency in header
    void applyXMLAttributes(Widget* widget, void* node);
};

} // namespace flexui
