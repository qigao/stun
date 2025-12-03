#include <flexui/screen.h>
#include <flexui/widget.h>
#include <flexui/textbox.h>
#include <flexui/radiobutton.h>
#include <flexui/scrollview.h>
#include <flexui/jsengine.h>
#include <glad/glad.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

#include <nanovg_css_internal.h>
#include <nanovg_css_svg_xml.h>
#include <fmtlog.h>
#include <pugixml.hpp>
#include "widget_factory.h"
#include "xml_utils.h"
#include "spatial_index.h"

namespace flexui {

Screen::Screen(int width, int height, const std::string& title)
    : width_(width), height_(height),
      spatial_index_(std::make_unique<SpatialIndex>((float)width, (float)height)) {
    
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    window_ = SDL_CreateWindow(title.c_str(), width, height,
                               SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    gl_context_ = SDL_GL_CreateContext(window_);
    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1);

    gladLoadGL();
    vg_ = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    
    if (nvgCreateFont(vg_, "sans-serif", "resources/Roboto-Regular.ttf") == -1) {
        loge("Failed to load font");
    }
    
    renderer_ = nvgcssCreateRenderer(vg_);
    nvgcssSetViewport(renderer_, (float)width, (float)height);

    // Initialize JavaScript engine
    js_engine_ = std::make_unique<JSEngine>();
    js_engine_->init(this);
}

Screen::~Screen() {
    widgets_.clear();
    if (renderer_) nvgcssDeleteRenderer(renderer_);
    if (vg_) nvgDeleteGL3(vg_);
    if (gl_context_) SDL_GL_DestroyContext(gl_context_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool Screen::loadCSS(const std::string& css) {
    return nvgcssParseCSS(renderer_, css.c_str()) != 0;
}

void Screen::setCSSVariable(const std::string& name, const std::string& value) {
    nvgcssSetVariable(renderer_, name.c_str(), value.c_str());
}

std::string Screen::getCSSVariable(const std::string& name) {
    char buffer[256];
    if (nvgcssGetVariable(renderer_, name.c_str(), buffer, sizeof(buffer))) {
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
    return ptr;
}

Widget* Screen::createPath(const std::string& id) {
    auto widget = std::make_unique<Widget>(renderer_, id, "path");
    Widget* ptr = widget.get();
    widgets_.push_back(std::move(widget));
    if (!id.empty()) widget_map_[id] = ptr;
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
    return ptr;
}

bool Screen::loadJS(const std::string& code) {
    return js_engine_ ? js_engine_->eval(code) : false;
}

bool Screen::loadJSFile(const std::string& path) {
    return js_engine_ ? js_engine_->loadFile(path) : false;
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
    return ptr;
}

// Helper function to calculate accumulated scroll offset from all ancestor containers
static void getAccumulatedScrollOffset(NVGCSSRenderer* renderer, NVGCSSElement* element,
                                        float& scroll_x, float& scroll_y) {
    scroll_x = 0.0f;
    scroll_y = 0.0f;

    // Walk up the parent chain and accumulate scroll offsets
    NVGCSSElement* parent = nvgcssGetParent(renderer, element);
    while (parent) {
        scroll_x += parent->scroll_x;
        scroll_y += parent->scroll_y;
        parent = nvgcssGetParent(renderer, parent);
    }
}

bool Screen::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }

        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            float mx = (float)event.motion.x;
            float my = (float)event.motion.y;
            auto candidates = spatial_index_->query(mx, my);
            for (auto* widget : candidates) {
                widget->handleHover(mx, my);
                widget->handleMouseMove(mx, my);
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            float mx = (float)event.button.x;
            float my = (float)event.button.y;

            // Blur focused textbox if clicking outside of it
            if (focused_textbox_) {
                auto* el = focused_textbox_->element();
                // Calculate visual position with scroll offset
                float scroll_x, scroll_y;
                getAccumulatedScrollOffset(renderer_, el, scroll_x, scroll_y);
                float visual_x = el->computed.x - scroll_x;
                float visual_y = el->computed.y - scroll_y;

                bool inside = mx >= visual_x &&
                              mx <= visual_x + el->computed.width &&
                              my >= visual_y &&
                              my <= visual_y + el->computed.height;
                if (!inside) {
                    focused_textbox_->blur();
                }
            }

            auto candidates = spatial_index_->query(mx, my);
            for (auto* widget : candidates) {
                widget->handleClick(mx, my);
                widget->handleMouseDown(mx, my);
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            float mx = (float)event.button.x;
            float my = (float)event.button.y;
            auto candidates = spatial_index_->query(mx, my);
            for (auto* widget : candidates) {
                nvgcssSetPseudoState(widget->element(), "active", 0);
                widget->handleMouseUp(mx, my);
            }
        }

        // Handle text input for focused textbox
        if (event.type == SDL_EVENT_TEXT_INPUT && focused_textbox_) {
            focused_textbox_->handleTextInput(event.text.text);
        }

        // Handle key presses for focused textbox
        if (event.type == SDL_EVENT_KEY_DOWN && focused_textbox_) {
            focused_textbox_->handleKeyPress(event.key.key);
        }

        // Handle mouse wheel scrolling
        if (event.type == SDL_EVENT_MOUSE_WHEEL) {
            float mx, my;
            SDL_GetMouseState(&mx, &my);
            float deltaX = event.wheel.x * 30.0f;  // Scale for smoother scrolling
            float deltaY = event.wheel.y * 30.0f;
            auto candidates = spatial_index_->query(mx, my);
            for (auto* widget : candidates) {
                if (widget->handleScroll(mx, my, deltaX, deltaY)) {
                    spatial_index_dirty_ = true;  // Scroll changed, rebuild index
                    break;  // Stop if a widget consumed the scroll
                }
            }
        }
    }
    return true;
}

void Screen::draw() {
    int win_w, win_h, fb_w, fb_h;
    SDL_GetWindowSize(window_, &win_w, &win_h);
    SDL_GetWindowSizeInPixels(window_, &fb_w, &fb_h);
    float pixel_ratio = (float)fb_w / (float)win_w;

    // Check if window size changed
    if (win_w != width_ || win_h != height_) {
        width_ = win_w;
        height_ = win_h;
        spatial_index_dirty_ = true;
    }

    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.98f, 0.98f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(vg_, win_w, win_h, pixel_ratio);

    // Check if layout will be recomputed
    if (renderer_->layout_dirty || renderer_->style_dirty) {
        spatial_index_dirty_ = true;
    }

    nvgcssSetViewport(renderer_, (float)win_w, (float)win_h);
    nvgcssComputeLayout(renderer_);
    nvgcssRender(renderer_);

    // Rebuild spatial index only when dirty
    if (spatial_index_dirty_) {
        spatial_index_->clear();
        for (auto& widget : widgets_) {
            auto* elem = widget->element();
            // Skip widgets with zero dimensions
            if (elem->computed.width > 0 && elem->computed.height > 0) {
                // Calculate visual position by subtracting accumulated scroll offset
                float scroll_x, scroll_y;
                getAccumulatedScrollOffset(renderer_, elem, scroll_x, scroll_y);

                float visual_x = elem->computed.x - scroll_x;
                float visual_y = elem->computed.y - scroll_y;

                spatial_index_->insert(widget.get(),
                    visual_x, visual_y,
                    elem->computed.width, elem->computed.height);
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
        NVGCSSElement* elem = widget->element();
        while (elem) {
            if (elem->style.display == nvgcss::Display::NONE) {
                is_hidden = true;
                break;
            }
            elem = nvgcssGetParent(renderer_, elem);
        }
        if (is_hidden) {
            continue;
        }
        widget->draw(vg_);
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

    SDL_GL_SwapWindow(window_);
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
        parseXMLNode(&node, nullptr);
    }

    // Resolve gradient references after all elements are created
    // This caches gradient pointers in elements for O(1) lookup during rendering
    for (auto* root : renderer_->root_elements) {
        nvgcss::SVGXMLParser::resolve_gradient_references(renderer_, root);
    }

    spatial_index_dirty_ = true;  // Widgets added via XML
    return true;
}

Widget* Screen::parseXMLNode(void* node_ptr, Widget* parent) {
    pugi::xml_node& node = *static_cast<pugi::xml_node*>(node_ptr);

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

    applyXMLAttributes(widget, &node);

    if (parent) {
        parent->addChild(widget);
    }

    for (pugi::xml_node child : node.children()) {
        if (child.type() == pugi::node_element) {
            parseXMLNode(&child, widget);
        }
    }

    return widget;
}

void Screen::applyXMLAttributes(Widget* widget, void* node_ptr) {
    pugi::xml_node& node = *static_cast<pugi::xml_node*>(node_ptr);

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
