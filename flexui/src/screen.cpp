#include <flexui/screen.h>
#include <flexui/widget.h>
#include <flexui/textbox.h>
#include <flexui/radiobutton.h>
#include <flexui/scrollview.h>
#include <flexui/dropdown.h>
#include <flexui/jsengine.h>
#include <flexui/font_manager.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

#include <cssbox_internal.h>
#include <cssbox_svg_xml.h>
#include <fmtlog.h>
#include <pugixml.hpp>
#include "widget_factory.h"
#include "xml_utils.h"
#include "spatial_index.h"
#include <fstream>
#include <sstream>
namespace flexui {

// Static GLFW callback functions that forward to Screen instance
static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* screen = static_cast<Screen*>(glfwGetWindowUserPointer(window));
    if (screen) screen->handleCursorPos(xpos, ypos);
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* screen = static_cast<Screen*>(glfwGetWindowUserPointer(window));
    if (screen) screen->handleMouseButton(button, action, mods);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* screen = static_cast<Screen*>(glfwGetWindowUserPointer(window));
    if (screen) screen->handleKey(key, scancode, action, mods);
}

static void charCallback(GLFWwindow* window, unsigned int codepoint) {
    auto* screen = static_cast<Screen*>(glfwGetWindowUserPointer(window));
    if (screen) screen->handleChar(codepoint);
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* screen = static_cast<Screen*>(glfwGetWindowUserPointer(window));
    if (screen) screen->handleScroll(xoffset, yoffset);
}

static void windowRefreshCallback(GLFWwindow* window) {
    auto* screen = static_cast<Screen*>(glfwGetWindowUserPointer(window));
    if (screen) screen->handleWindowRefresh();
}


Screen::Screen(int width, int height, const std::string& title)
    : width_(width), height_(height),
      spatial_index_(std::make_unique<SpatialIndex>((float)width, (float)height)) {
    
    if (!glfwInit()) {
        loge("Failed to initialize GLFW");
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) {
        loge("Failed to create GLFW window");
        glfwTerminate();
        return;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    // Set up callbacks
    glfwSetWindowUserPointer(window_, this);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetCharCallback(window_, charCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetWindowRefreshCallback(window_, windowRefreshCallback);

    gladLoadGL();
    vg_ = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    
    // Initialize font manager and load default fonts
    font_manager_ = std::make_unique<FontManager>(vg_);
    font_manager_->loadDefaultFonts();
    
    renderer_ = cssboxCreateRenderer(vg_);
    cssboxSetViewport(renderer_, (float)width, (float)height);

    // Initialize JavaScript engine
    js_engine_ = std::make_unique<JSEngine>();
    js_engine_->init(this);
}

Screen::~Screen() {
    widgets_.clear();
    if (renderer_) cssboxDeleteRenderer(renderer_);
    if (vg_) nvgDeleteGL3(vg_);
    if (window_) glfwDestroyWindow(window_);
    glfwTerminate();
}

bool Screen::loadCSS(const std::string& css) {
    bool result = cssboxParseCSS(renderer_, css.c_str()) != 0;
    if (result) {
        needs_redraw_ = true;  // CSS changes require redraw
    }
    return result;
}

void Screen::setCSSVariable(const std::string& name, const std::string& value) {
    cssboxSetVariable(renderer_, name.c_str(), value.c_str());
    needs_redraw_ = true;  // CSS variable change requires redraw
}

std::string Screen::getCSSVariable(const std::string& name) {
    char buffer[256];
    if (cssboxGetVariable(renderer_, name.c_str(), buffer, sizeof(buffer))) {
        return std::string(buffer);
    }
    return "";
}

Widget* Screen::createLine(const std::string& id, float x1, float y1, float x2, float y2) {
    auto widget = std::make_unique<Widget>(renderer_, id, "line");
    widget->setInlineStyle("x1", std::to_string((int)x1) + "px");
    widget->setInlineStyle("y1", std::to_string((int)y1) + "px");
    widget->setInlineStyle("x2", std::to_string((int)x2) + "px");
    widget->setInlineStyle("y2", std::to_string((int)y2) + "px");
    Widget* ptr = widget.get();
    widgets_.push_back(std::move(widget));
    if (!id.empty()) widget_map_[id] = ptr;
    needs_redraw_ = true;  // New SVG element requires redraw
    return ptr;
}

Widget* Screen::createCircle(const std::string& id, float cx, float cy, float r) {
    auto widget = std::make_unique<Widget>(renderer_, id, "circle");
    widget->setInlineStyle("cx", std::to_string((int)cx) + "px");
    widget->setInlineStyle("cy", std::to_string((int)cy) + "px");
    widget->setInlineStyle("r", std::to_string((int)r) + "px");
    Widget* ptr = widget.get();
    widgets_.push_back(std::move(widget));
    if (!id.empty()) widget_map_[id] = ptr;
    needs_redraw_ = true;  // New SVG element requires redraw
    return ptr;
}

Widget* Screen::createEllipse(const std::string& id, float cx, float cy, float rx, float ry) {
    auto widget = std::make_unique<Widget>(renderer_, id, "ellipse");
    widget->setInlineStyle("cx", std::to_string((int)cx) + "px");
    widget->setInlineStyle("cy", std::to_string((int)cy) + "px");
    widget->setInlineStyle("rx", std::to_string((int)rx) + "px");
    widget->setInlineStyle("ry", std::to_string((int)ry) + "px");
    Widget* ptr = widget.get();
    widgets_.push_back(std::move(widget));
    if (!id.empty()) widget_map_[id] = ptr;
    needs_redraw_ = true;  // New SVG element requires redraw
    return ptr;
}

Widget* Screen::createRect(const std::string& id, float x, float y, float w, float h) {
    auto widget = std::make_unique<Widget>(renderer_, id, "rect");
    widget->setInlineStyle("x", std::to_string((int)x) + "px");
    widget->setInlineStyle("y", std::to_string((int)y) + "px");
    widget->setInlineStyle("width", std::to_string((int)w) + "px");
    widget->setInlineStyle("height", std::to_string((int)h) + "px");
    Widget* ptr = widget.get();
    widgets_.push_back(std::move(widget));
    if (!id.empty()) widget_map_[id] = ptr;
    needs_redraw_ = true;  // New SVG element requires redraw
    return ptr;
}

Widget* Screen::createPath(const std::string& id) {
    auto widget = std::make_unique<Widget>(renderer_, id, "path");
    Widget* ptr = widget.get();
    widgets_.push_back(std::move(widget));
    if (!id.empty()) widget_map_[id] = ptr;
    needs_redraw_ = true;  // New SVG element requires redraw
    return ptr;
}

Widget* Screen::createPolygon(const std::string& id, const std::vector<std::pair<float, float>>& points) {
    auto widget = std::make_unique<Widget>(renderer_, id, "path");
    for (const auto& point : points) {
        widget->addPathPoint(point.first, point.second);
    }
    // Close the path by adding first point again
    if (!points.empty()) {
        widget->addPathPoint(points[0].first, points[0].second);
    }
    Widget* ptr = widget.get();
    widgets_.push_back(std::move(widget));
    if (!id.empty()) widget_map_[id] = ptr;
    needs_redraw_ = true;  // New SVG element requires redraw
    return ptr;
}

Widget* Screen::createPolyline(const std::string& id, const std::vector<std::pair<float, float>>& points) {
    auto widget = std::make_unique<Widget>(renderer_, id, "path");
    for (const auto& point : points) {
        widget->addPathPoint(point.first, point.second);
    }
    Widget* ptr = widget.get();
    widgets_.push_back(std::move(widget));
    if (!id.empty()) widget_map_[id] = ptr;
    needs_redraw_ = true;  // New SVG element requires redraw
    return ptr;
}

bool Screen::loadJS(const std::string& code) {
    return js_engine_ ? js_engine_->eval(code) : false;
}

bool Screen::loadJSFile(const std::string& path) {
    return js_engine_ ? js_engine_->loadFile(path) : false;
}

bool Screen::loadJSModule(const std::string& path) {
    return js_engine_ ? js_engine_->loadModule(path) : false;
}

bool Screen::loadCSSFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        loge("Failed to open CSS file: {}", path);
        return false;
    }
    
    std::string css((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    return loadCSS(css);
}

bool Screen::loadXMLFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        loge("Failed to open XML file: {}", path);
        return false;
    }
    
    std::string xml((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    return loadXML(xml);
}

Widget* Screen::addWidget(const std::string& id, const std::string& tag) {
    auto widget = std::make_unique<Widget>(renderer_, id, tag);
    Widget* ptr = widget.get();
    widgets_.push_back(std::move(widget));

    // Add to widget map if ID is provided
    if (!id.empty()) {
        widget_map_[id] = ptr;
    }

    spatial_index_dirty_ = true;  // New widget added
    needs_redraw_ = true;  // New widget requires redraw
    return ptr;
}

// Helper function to calculate accumulated scroll offset from all ancestor containers
static void getAccumulatedScrollOffset(cssboxRenderer* renderer, cssboxElement* element,
                                        float& scroll_x, float& scroll_y) {
    scroll_x = 0.0f;
    scroll_y = 0.0f;

    // Walk up the parent chain and accumulate scroll offsets
    cssboxElement* parent = cssboxGetParent(renderer, element);
    while (parent) {
        scroll_x += parent->scroll_x;
        scroll_y += parent->scroll_y;
        parent = cssboxGetParent(renderer, parent);
    }
}

// GLFW callback handler implementations
void Screen::handleCursorPos(double xpos, double ypos) {
    float mx = (float)xpos;
    float my = (float)ypos;
    last_mouse_x_ = xpos;
    last_mouse_y_ = ypos;

    for (auto& widget : widgets_) {
        if (auto* dropdown = dynamic_cast<Dropdown*>(widget.get())) {
            if (dropdown->isOpen()) {
                dropdown->handleMouseMove(mx, my);
            }
        }
    }

    auto candidates = spatial_index_->query(mx, my);
    for (auto* widget : candidates) {
        widget->handleHover(mx, my);
        if (widget->handleMouseMove(mx, my)) {
            spatial_index_dirty_ = true;
            needs_redraw_ = true;
        }
    }
}

void Screen::handleMouseButton(int button, int action, int mods) {
    float mx = (float)last_mouse_x_;
    float my = (float)last_mouse_y_;

    if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
        if (focused_textbox_) {
            auto* el = focused_textbox_->element();
            float scroll_x, scroll_y;
            getAccumulatedScrollOffset(renderer_, el, scroll_x, scroll_y);
            float visual_x = el->layout.x - scroll_x;
            float visual_y = el->layout.y - scroll_y;

            bool inside = mx >= visual_x &&
                          mx <= visual_x + el->layout.width &&
                          my >= visual_y &&
                          my <= visual_y + el->layout.height;
            if (!inside) {
                focused_textbox_->blur();
            }
        }

        bool handled_by_dropdown = false;
        for (auto& widget : widgets_) {
            if (auto* dropdown = dynamic_cast<Dropdown*>(widget.get())) {
                if (dropdown->isOpen() && dropdown->handleMouseDown(mx, my)) {
                    handled_by_dropdown = true;
                    break;
                }
            }
        }

        if (!handled_by_dropdown) {
            auto candidates = spatial_index_->query(mx, my);
            // Reverse order: children (more specific) should be processed before parents
            std::reverse(candidates.begin(), candidates.end());
            for (auto* widget : candidates) {
                auto* el = widget->element();
                // Verify click is actually within widget bounds
                float wx = el->layout.x;
                float wy = el->layout.y;
                float ww = el->layout.width;
                float wh = el->layout.height;
                if (mx < wx || mx > wx + ww || my < wy || my > wy + wh) {
                    continue;
                }
                if (widget->handleClick(mx, my)) {
                    // Click was handled, stop propagation
                    widget->handleMouseDown(mx, my);
                    break;
                }
                widget->handleMouseDown(mx, my);
            }
        }
        needs_redraw_ = true;
    }

    if (action == GLFW_RELEASE && button == GLFW_MOUSE_BUTTON_LEFT) {
        auto candidates = spatial_index_->query(mx, my);
        for (auto* widget : candidates) {
            cssboxSetPseudoStateEx(renderer_, widget->element(), "active", 0);
            widget->handleMouseUp(mx, my);
        }
        needs_redraw_ = true;
    }
}

void Screen::handleKey(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (focused_textbox_) {
            bool shift = (mods & GLFW_MOD_SHIFT) != 0;
            bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
            focused_textbox_->handleKeyPress(key, shift, ctrl);
            needs_redraw_ = true;
        }
    }
}

void Screen::handleChar(unsigned int codepoint) {
    if (focused_textbox_) {
        char utf8[5] = {0};
        if (codepoint < 0x80) {
            utf8[0] = (char)codepoint;
        } else if (codepoint < 0x800) {
            utf8[0] = (char)(0xC0 | (codepoint >> 6));
            utf8[1] = (char)(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            utf8[0] = (char)(0xE0 | (codepoint >> 12));
            utf8[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            utf8[2] = (char)(0x80 | (codepoint & 0x3F));
        } else {
            utf8[0] = (char)(0xF0 | (codepoint >> 18));
            utf8[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
            utf8[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            utf8[3] = (char)(0x80 | (codepoint & 0x3F));
        }
        focused_textbox_->handleTextInput(utf8);
        needs_redraw_ = true;
    }
}

void Screen::handleScroll(double xoffset, double yoffset) {
    float mx = (float)last_mouse_x_;
    float my = (float)last_mouse_y_;
    float deltaX = (float)xoffset * 30.0f;
    float deltaY = (float)yoffset * 30.0f;

    auto candidates = spatial_index_->query(mx, my);
    for (auto* widget : candidates) {
        if (widget->handleScroll(mx, my, deltaX, deltaY)) {
            spatial_index_dirty_ = true;
            needs_redraw_ = true;
            break;
        }
    }
}

void Screen::handleWindowRefresh() {
    needs_redraw_ = true;
}

bool Screen::pollEvents() {
    if (glfwWindowShouldClose(window_)) {
        return false;
    }
    glfwPollEvents();
    return true;
}

void Screen::draw() {
    int win_w, win_h, fb_w, fb_h;
    glfwGetWindowSize(window_, &win_w, &win_h);
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    float pixel_ratio = (float)fb_w / (float)win_w;

    // Check if window size changed
    if (win_w != width_ || win_h != height_) {
        width_ = win_w;
        height_ = win_h;
        spatial_index_->resize((float)win_w, (float)win_h);  // Resize grid for new dimensions
        spatial_index_dirty_ = true;
        needs_redraw_ = true;  // Window resize requires redraw
    }

    // Update viewport (marks dirty if size changed)
    cssboxSetViewport(renderer_, (float)win_w, (float)win_h);

    // Update animations FIRST (before needs_paint check)
    // This fills animated_elements_ so cssboxNeedsPaint() can detect active animations
    static double last_time = glfwGetTime();
    double current_time = glfwGetTime();
    float delta_time = (float)(current_time - last_time);
    last_time = current_time;

    // Mark spatial index dirty if layout changed (BEFORE cssboxUpdate clears flags)
    if (renderer_->layout_dirty || renderer_->style_dirty) {
        spatial_index_dirty_ = true;
    }

    cssboxUpdate(renderer_, delta_time);

    // NOW check if paint needed (after cssboxUpdate filled animated_elements_)
    if (!cssboxNeedsPaint(renderer_)) {
        return;
    }

    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.98f, 0.98f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Pre-render patterns before main NVG frame (required for FBO-based patterns)
    cssboxPreparePatterns(renderer_);

    nvgBeginFrame(vg_, win_w, win_h, pixel_ratio);

    cssboxRender(renderer_);

    // Rebuild spatial index only when dirty
    if (spatial_index_dirty_) {
        spatial_index_->clear();
        for (auto& widget : widgets_) {
            auto* elem = widget->element();
            float w = elem->layout.width;
            float h = elem->layout.height;
            float x = elem->layout.x;
            float y = elem->layout.y;
            
            // For widgets with zero dimensions, try to use parent's dimensions
            // This handles chart widgets that fill their container
            if ((w <= 0 || h <= 0) && elem->parent_internal_id >= 0) {
                auto* parent = cssboxGetParent(renderer_, elem);
                if (parent && parent->layout.width > 0 && parent->layout.height > 0) {
                    x = parent->layout.x;
                    y = parent->layout.y;
                    w = parent->layout.width;
                    h = parent->layout.height;
                }
            }
            
            // Skip widgets with zero dimensions
            if (w > 0 && h > 0) {
                // Calculate visual position by subtracting accumulated scroll offset
                float scroll_x, scroll_y;
                getAccumulatedScrollOffset(renderer_, elem, scroll_x, scroll_y);

                float visual_x = x - scroll_x;
                float visual_y = y - scroll_y;
                
                // Skip widgets completely outside viewport
                if (visual_x + w < 0 || visual_x > width_ ||
                    visual_y + h < 0 || visual_y > height_) {
                    continue;
                }

                spatial_index_->insert(widget.get(),
                    visual_x, visual_y,
                    w, h);
            }
        }
        spatial_index_dirty_ = false;
    }

    // Draw all widgets (for custom rendering)
    for (auto& widget : widgets_) {
        // Skip invisible widgets
        if (!widget->isVisible()) {
            continue;
        }
        // Skip widgets with display: none (including those with hidden ancestors)
        bool is_hidden = false;
        cssboxElement* elem = widget->element();
        while (elem) {
            if (elem->style.display == cssbox::Display::NONE) {
                is_hidden = true;
                break;
            }
            elem = cssboxGetParent(renderer_, elem);
        }
        if (is_hidden) {
            continue;
        }

        // Track layout before draw to detect changes (e.g., charts using parent dimensions)
        elem = widget->element();
        float old_w = elem->layout.width;
        float old_h = elem->layout.height;

        // Apply scroll offset transform for widgets inside scroll containers
        float scroll_x, scroll_y;
        getAccumulatedScrollOffset(renderer_, widget->element(), scroll_x, scroll_y);

        if (scroll_x != 0.0f || scroll_y != 0.0f) {
            nvgSave(vg_);

            // First apply scissor in screen coordinates (before translate)
            cssboxElement* scroll_parent = widget->element();
            while (scroll_parent) {
                cssboxElement* parent = cssboxGetParent(renderer_, scroll_parent);
                if (parent && (parent->scroll_x != 0.0f || parent->scroll_y != 0.0f)) {
                    nvgScissor(vg_, parent->layout.x, parent->layout.y,
                              parent->layout.width, parent->layout.height);
                    break;
                }
                scroll_parent = parent;
            }

            // Then apply scroll translate
            nvgTranslate(vg_, -scroll_x, -scroll_y);

            widget->draw(vg_);
            nvgRestore(vg_);
        } else {
            widget->draw(vg_);
        }

        // If widget updated its layout during draw, mark spatial index dirty
        if (elem->layout.width != old_w || elem->layout.height != old_h) {
            spatial_index_dirty_ = true;
        }
    }

    // Call custom draw callback before ending frame
    if (custom_draw_callback_) {
        custom_draw_callback_(vg_);
    }

    nvgEndFrame(vg_);

    // Process JavaScript timers
    if (js_engine_) {
        js_engine_->processPendingTimers();
    }

    glfwSwapBuffers(window_);
    
    // Reset dirty flag after successful redraw
    needs_redraw_ = false;
}

bool Screen::loadXML(const std::string& xml) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.c_str());

    if (!result) {
        loge("Failed to parse XML: {}", result.description());
        return false;
    }

    // Parse all root nodes
    for (pugi::xml_node node : doc.children()) {
        parseXMLNode(node, nullptr);
    }

    // Resolve gradient references after all elements are created
    // This caches gradient pointers in elements for O(1) lookup during rendering
    for (auto* root : renderer_->root_elements) {
        cssbox::SVGXMLParser::resolve_gradient_references(renderer_, root);
    }

    spatial_index_dirty_ = true;  // Widgets added via XML
    needs_redraw_ = true;  // XML changes require redraw
    return true;
}

Widget* Screen::parseXMLNode(pugi::xml_node& node, Widget* parent) {
    std::string tag = node.name();
    std::string id = node.attribute("id").as_string();

    if (id.empty()) {
        id = tag + "_" + std::to_string(widget_counter_++);
    }

    auto it = widget_factories.find(tag);
    if (it == widget_factories.end()) {
        logw("Unknown tag: {}", tag);
        return nullptr;
    }

    Widget* widget = it->second(renderer_, id, node, this);
    if (!widget) return nullptr;

    widgets_.push_back(std::unique_ptr<Widget>(widget));
    if (!id.empty()) {
        widget_map_[id] = widget;
    }

    applyXMLAttributes(widget, node);

    if (parent) {
        parent->addChild(widget);
    }

    for (pugi::xml_node child : node.children()) {
        if (child.type() == pugi::node_element) {
            parseXMLNode(child, widget);
        }
    }

    return widget;
}

void Screen::applyXMLAttributes(Widget* widget, pugi::xml_node& node) {

    // Apply class attribute
    if (auto attr = node.attribute("class")) {
        widget->setClass(attr.as_string());
    }

    // Apply inline style attribute
    if (auto style_attr = node.attribute("style")) {
        auto styles = xml_utils::parse_inline_style(style_attr.as_string());
        for (const auto& [property, value] : styles) {
            widget->setInlineStyle(property, value);
        }
    }

    // Handle onclick event
    if (auto onclick = node.attribute("onclick")) {
        std::string handler_name = onclick.as_string();
        widget->setClickCallback([this, handler_name](Widget* w) {
            // First try C++ handler
            auto it = event_handlers_.find(handler_name);
            if (it != event_handlers_.end()) {
                return it->second(w);
            }
            // Then try JavaScript function
            if (js_engine_ && js_engine_->hasFunction(handler_name)) {
                return js_engine_->callFunction(handler_name);
            }
            logw("Handler not found: {}", handler_name);
            return false;
        });
    }

    // Handle TextBox-specific attributes
    if (auto* textbox = dynamic_cast<TextBox*>(widget)) {
        textbox->setScreen(this);
        if (auto password = node.attribute("password")) {
            textbox->setPasswordMode(password.as_bool());
        }
    }
}

void Screen::registerHandler(const std::string& name, EventHandler handler) {
    event_handlers_[name] = handler;
}

Widget* Screen::findWidget(const std::string& id) {
    auto it = widget_map_.find(id);
    return (it != widget_map_.end()) ? it->second : nullptr;
}

void Screen::registerRadioButton(RadioButton* radio) {
    if (!radio || radio->getGroup().empty()) return;

    const std::string& group = radio->getGroup();
    radio_groups_[group].push_back(radio);

    // If this radio is checked, uncheck others in the group
    if (radio->isChecked()) {
        setRadioGroupValue(group, radio->getValue());
    }
}

void Screen::setRadioGroupValue(const std::string& group, const std::string& value) {
    auto it = radio_groups_.find(group);
    if (it == radio_groups_.end()) return;

    // Uncheck all radios in this group, then check the one with matching value
    for (RadioButton* radio : it->second) {
        if (radio->getValue() == value) {
            radio->setChecked(true);
        } else if (radio->isChecked()) {
            radio->setChecked(false);
        }
    }
    needs_redraw_ = true;  // Radio button state change requires redraw
}

std::string Screen::getRadioGroupValue(const std::string& group) const {
    auto it = radio_groups_.find(group);
    if (it == radio_groups_.end()) return "";

    // Find the checked radio in this group
    for (RadioButton* radio : it->second) {
        if (radio->isChecked()) {
            return radio->getValue();
        }
    }

    return "";
}

} // namespace flexui
