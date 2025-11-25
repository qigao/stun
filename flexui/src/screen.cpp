#include <flexui/screen.h>
#include <flexui/widget.h>
#include <flexui/textbox.h>
#include <flexui/button.h>
#include <flexui/checkbox.h>
#include <flexui/label.h>
#include <flexui/progressbar.h>
#include <flexui/slider.h>
#include <flexui/radiobutton.h>
#include <flexui/divider.h>
#include <flexui/imageview.h>
#include <flexui/tabbar.h>
#include <flexui/dropdown.h>
#include <flexui/switch.h>
#include <flexui/rating.h>
#include <flexui/searchbox.h>
#include <flexui/spinner.h>
#include <flexui/alert.h>
#include <flexui/badge.h>
#include <flexui/card.h>
#include <flexui/avatar.h>
#include <flexui/chip.h>
#include <flexui/toast.h>
#include <flexui/breadcrumb.h>
#include <flexui/pagination.h>
#include <flexui/menu.h>
#include <flexui/modal.h>
#include <flexui/table.h>
#include <flexui/iconbutton.h>
#include <flexui/calendar.h>
#include <flexui/colorpicker.h>
#include <flexui/snackbar.h>
#include <flexui/tablist.h>
#include <flexui/jsengine.h>
#include <glad/glad.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

#include <nanovg_css_internal.h>
#include <fmtlog.h>
#include <pugixml.hpp>

namespace flexui {

Screen::Screen(int width, int height, const std::string& title)
    : width_(width), height_(height) {
    
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

    return ptr;
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
            for (auto& widget : widgets_) {
                widget->handleHover(mx, my);
                widget->handleMouseMove(mx, my);
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            float mx = (float)event.button.x;
            float my = (float)event.button.y;
            for (auto& widget : widgets_) {
                widget->handleClick(mx, my);
                widget->handleMouseDown(mx, my);
            }
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            float mx = (float)event.button.x;
            float my = (float)event.button.y;
            for (auto& widget : widgets_) {
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
    }
    return true;
}

void Screen::draw() {
    int win_w, win_h, fb_w, fb_h;
    SDL_GetWindowSize(window_, &win_w, &win_h);
    SDL_GetWindowSizeInPixels(window_, &fb_w, &fb_h);
    float pixel_ratio = (float)fb_w / (float)win_w;

    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.98f, 0.98f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(vg_, win_w, win_h, pixel_ratio);
    nvgcssSetViewport(renderer_, (float)win_w, (float)win_h);
    nvgcssComputeLayout(renderer_);
    nvgcssRender(renderer_);

    // Draw all widgets (for custom rendering)
    for (auto& widget : widgets_) {
        // Skip invisible widgets
        if (!widget->isVisible()) {
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

    return true;
}

Widget* Screen::parseXMLNode(void* node_ptr, Widget* parent) {
    pugi::xml_node& node = *static_cast<pugi::xml_node*>(node_ptr);

    std::string tag = node.name();
    std::string id = node.attribute("id").as_string();

    // Generate ID if not provided
    if (id.empty()) {
        id = tag + "_" + std::to_string(widget_counter_++);
    }

    Widget* widget = nullptr;

    // Create widget based on tag name
    if (tag == "div" || tag == "container") {
        widget = addWidget(id, "div");
    } else if (tag == "button") {
        std::string text = node.text().as_string();
        widget = createWidget<Button>(id, text);
        widget_map_[id] = widget;
    } else if (tag == "label") {
        std::string text = node.text().as_string();
        widget = createWidget<Label>(id, text);
        widget_map_[id] = widget;
    } else if (tag == "input" || tag == "textbox") {
        std::string placeholder = node.attribute("placeholder").as_string();
        widget = createWidget<TextBox>(id, placeholder);
        widget_map_[id] = widget;
    } else if (tag == "checkbox") {
        bool checked = node.attribute("checked").as_bool(false);
        widget = createWidget<Checkbox>(id, checked);
        widget_map_[id] = widget;
    } else if (tag == "progressbar") {
        float progress = node.attribute("value").as_float(0.0f);
        widget = createWidget<ProgressBar>(id, progress);
        widget_map_[id] = widget;
    } else if (tag == "slider") {
        float value = node.attribute("value").as_float(0.5f);
        float min = node.attribute("min").as_float(0.0f);
        float max = node.attribute("max").as_float(1.0f);
        widget = createWidget<Slider>(id, value, min, max);
        widget_map_[id] = widget;
    } else if (tag == "radio") {
        std::string group = node.attribute("name").as_string();  // HTML uses "name" for grouping
        std::string value = node.attribute("value").as_string();
        bool checked = node.attribute("checked").as_bool(false);
        widget = createWidget<RadioButton>(id, group, value, checked);
        widget_map_[id] = widget;

        // Set Screen pointer and register
        auto* radio = static_cast<RadioButton*>(widget);
        radio->setScreen(this);
        registerRadioButton(radio);
    } else if (tag == "hr" || tag == "divider") {
        bool vertical = node.attribute("vertical").as_bool(false);
        widget = createWidget<Divider>(id, vertical);
        widget_map_[id] = widget;
    } else if (tag == "img" || tag == "image") {
        std::string src = node.attribute("src").as_string();
        widget = createWidget<ImageView>(id, src);
        widget_map_[id] = widget;
    } else if (tag == "tabbar") {
        std::vector<std::string> tabs;
        std::string tabsAttr = node.attribute("tabs").as_string();
        // Parse comma-separated tab names
        if (!tabsAttr.empty()) {
            size_t pos = 0;
            while ((pos = tabsAttr.find(',')) != std::string::npos) {
                std::string tab = tabsAttr.substr(0, pos);
                // Trim whitespace
                tab.erase(0, tab.find_first_not_of(" \t"));
                tab.erase(tab.find_last_not_of(" \t") + 1);
                tabs.push_back(tab);
                tabsAttr.erase(0, pos + 1);
            }
            tabsAttr.erase(0, tabsAttr.find_first_not_of(" \t"));
            tabsAttr.erase(tabsAttr.find_last_not_of(" \t") + 1);
            if (!tabsAttr.empty()) tabs.push_back(tabsAttr);
        }
        widget = createWidget<TabBar>(id, tabs);
        widget_map_[id] = widget;
    } else if (tag == "select" || tag == "dropdown") {
        std::vector<std::string> items;
        std::string itemsAttr = node.attribute("items").as_string();
        // Parse comma-separated items
        if (!itemsAttr.empty()) {
            size_t pos = 0;
            while ((pos = itemsAttr.find(',')) != std::string::npos) {
                std::string item = itemsAttr.substr(0, pos);
                item.erase(0, item.find_first_not_of(" \t"));
                item.erase(item.find_last_not_of(" \t") + 1);
                items.push_back(item);
                itemsAttr.erase(0, pos + 1);
            }
            itemsAttr.erase(0, itemsAttr.find_first_not_of(" \t"));
            itemsAttr.erase(itemsAttr.find_last_not_of(" \t") + 1);
            if (!itemsAttr.empty()) items.push_back(itemsAttr);
        }
        widget = createWidget<Dropdown>(id, items);
        widget_map_[id] = widget;
    } else if (tag == "switch" || tag == "toggle") {
        bool on = node.attribute("on").as_bool(false);
        widget = createWidget<Switch>(id, on);
        widget_map_[id] = widget;
    } else if (tag == "rating") {
        int value = node.attribute("value").as_int(0);
        widget = createWidget<Rating>(id, value);
        widget_map_[id] = widget;
    } else if (tag == "searchbox" || tag == "search") {
        std::string placeholder = node.attribute("placeholder").as_string("Search...");
        widget = createWidget<SearchBox>(id, placeholder);
        widget_map_[id] = widget;
    } else if (tag == "spinner") {
        widget = createWidget<Spinner>(id);
        widget_map_[id] = widget;
    } else if (tag == "alert") {
        std::string message = node.text().as_string();
        std::string typeStr = node.attribute("type").as_string("info");
        AlertType type = AlertType::Info;
        if (typeStr == "success") type = AlertType::Success;
        else if (typeStr == "warning") type = AlertType::Warning;
        else if (typeStr == "error") type = AlertType::Error;
        widget = createWidget<Alert>(id, message, type);
        widget_map_[id] = widget;
    } else if (tag == "badge") {
        std::string text = node.text().as_string();
        widget = createWidget<Badge>(id, text);
        widget_map_[id] = widget;
    } else if (tag == "card") {
        widget = createWidget<Card>(id);
        widget_map_[id] = widget;
    } else if (tag == "avatar") {
        std::string initials = node.text().as_string();
        widget = createWidget<Avatar>(id, initials);
        widget_map_[id] = widget;
    } else if (tag == "chip") {
        std::string text = node.text().as_string();
        widget = createWidget<Chip>(id, text);
        widget_map_[id] = widget;
    } else if (tag == "toast") {
        std::string message = node.text().as_string();
        std::string typeStr = node.attribute("type").as_string("info");
        ToastType type = ToastType::Info;
        if (typeStr == "success") type = ToastType::Success;
        else if (typeStr == "warning") type = ToastType::Warning;
        else if (typeStr == "error") type = ToastType::Error;
        widget = createWidget<Toast>(id, message, type);
        widget_map_[id] = widget;
    } else if (tag == "breadcrumb") {
        // Parse items from comma-separated attribute or child elements
        std::vector<std::string> items;
        std::string itemsAttr = node.attribute("items").as_string();
        if (!itemsAttr.empty()) {
            size_t pos = 0;
            while ((pos = itemsAttr.find(',')) != std::string::npos) {
                std::string item = itemsAttr.substr(0, pos);
                item.erase(0, item.find_first_not_of(" \t"));
                item.erase(item.find_last_not_of(" \t") + 1);
                items.push_back(item);
                itemsAttr.erase(0, pos + 1);
            }
            itemsAttr.erase(0, itemsAttr.find_first_not_of(" \t"));
            itemsAttr.erase(itemsAttr.find_last_not_of(" \t") + 1);
            if (!itemsAttr.empty()) items.push_back(itemsAttr);
        }
        widget = createWidget<Breadcrumb>(id, items);
        widget_map_[id] = widget;
    } else if (tag == "pagination") {
        int totalPages = node.attribute("total").as_int(1);
        int currentPage = node.attribute("current").as_int(1);
        widget = createWidget<Pagination>(id, totalPages, currentPage);
        widget_map_[id] = widget;
    } else if (tag == "menu") {
        // Menu items will be added via child nodes or API calls
        std::vector<MenuItem> items;
        widget = createWidget<Menu>(id, items);
        widget_map_[id] = widget;
    } else if (tag == "modal") {
        std::string title = node.attribute("title").as_string("Modal");
        std::string content = node.text().as_string();
        widget = createWidget<Modal>(id, title, content);
        widget_map_[id] = widget;
    } else if (tag == "table") {
        // Table headers and column widths will be set via API
        std::vector<std::string> headers;
        std::vector<float> columnWidths;
        widget = createWidget<Table>(id, headers, columnWidths);
        widget_map_[id] = widget;
    } else if (tag == "iconbutton") {
        std::string icon = node.attribute("icon").as_string("☰");
        widget = createWidget<IconButton>(id, icon);
        widget_map_[id] = widget;
    } else if (tag == "calendar") {
        int year = node.attribute("year").as_int(2025);
        int month = node.attribute("month").as_int(1);
        widget = createWidget<Calendar>(id, year, month);
        widget_map_[id] = widget;
    } else if (tag == "colorpicker") {
        widget = createWidget<ColorPicker>(id);
        widget_map_[id] = widget;
    } else if (tag == "snackbar") {
        std::string message = node.text().as_string("Notification");
        widget = createWidget<Snackbar>(id, message);
        widget_map_[id] = widget;
    } else if (tag == "tablist") {
        std::vector<std::string> tabs;
        std::string tabsAttr = node.attribute("tabs").as_string();
        if (!tabsAttr.empty()) {
            size_t pos = 0;
            while ((pos = tabsAttr.find(',')) != std::string::npos) {
                std::string tab = tabsAttr.substr(0, pos);
                tab.erase(0, tab.find_first_not_of(" \t"));
                tab.erase(tab.find_last_not_of(" \t") + 1);
                tabs.push_back(tab);
                tabsAttr.erase(0, pos + 1);
            }
            tabsAttr.erase(0, tabsAttr.find_first_not_of(" \t"));
            tabsAttr.erase(tabsAttr.find_last_not_of(" \t") + 1);
            if (!tabsAttr.empty()) tabs.push_back(tabsAttr);
        }
        widget = createWidget<TabList>(id, tabs);
        widget_map_[id] = widget;
    } else {
        logw("Unknown tag: {}", tag);
        return nullptr;
    }

    if (!widget) return nullptr;

    // Apply XML attributes
    applyXMLAttributes(widget, &node);

    // Add to parent
    if (parent) {
        parent->addChild(widget);
    }

    // Parse children recursively
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
        std::string style_str = style_attr.as_string();
        // Parse style string (e.g., "display:none; color:red")
        size_t pos = 0;
        while (pos < style_str.length()) {
            size_t colon_pos = style_str.find(':', pos);
            if (colon_pos == std::string::npos) break;

            size_t semicolon_pos = style_str.find(';', colon_pos);
            if (semicolon_pos == std::string::npos) semicolon_pos = style_str.length();

            std::string property = style_str.substr(pos, colon_pos - pos);
            std::string value = style_str.substr(colon_pos + 1, semicolon_pos - colon_pos - 1);

            // Trim whitespace
            property.erase(0, property.find_first_not_of(" \t"));
            property.erase(property.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            widget->setInlineStyle(property, value);

            pos = semicolon_pos + 1;
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
