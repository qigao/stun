#pragma once

#include <functional>
#include <map>
#include <string>
#include <chrono>
#include <pugixml.hpp>
#include <fmtlog.h>
#include <flexui/widget.h>
#include <flexui/button.h>
#include <flexui/label.h>
#include <flexui/textbox.h>
#include <flexui/checkbox.h>
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
#include "xml_utils.h"

namespace flexui {

class Screen;

using WidgetFactory = std::function<Widget*(NVGCSSRenderer*, const std::string&, pugi::xml_node&, Screen*)>;

// Enum mappings
static const std::map<std::string, AlertType> alert_type_map = {
    {"info", AlertType::Info},
    {"success", AlertType::Success},
    {"warning", AlertType::Warning},
    {"error", AlertType::Error}
};

static const std::map<std::string, ToastType> toast_type_map = {
    {"info", ToastType::Info},
    {"success", ToastType::Success},
    {"warning", ToastType::Warning},
    {"error", ToastType::Error}
};

// Helper to get current year/month
inline std::pair<int, int> get_current_year_month() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = {};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    return {tm.tm_year + 1900, tm.tm_mon + 1};
}

// Widget factory registry
static const std::map<std::string, WidgetFactory> widget_factories = {
    {"div", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Widget(r, id, "div");
    }},
    {"container", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Widget(r, id, "div");
    }},
    {"button", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Button(r, id, n.text().as_string());
    }},
    {"label", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Label(r, id, n.text().as_string());
    }},
    {"input", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new TextBox(r, id, n.attribute("placeholder").as_string());
    }},
    {"textbox", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new TextBox(r, id, n.attribute("placeholder").as_string());
    }},
    {"checkbox", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Checkbox(r, id, n.attribute("checked").as_bool(false));
    }},
    {"progressbar", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new ProgressBar(r, id, n.attribute("value").as_float(0.0f));
    }},
    {"slider", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Slider(r, id, 
            n.attribute("value").as_float(0.5f),
            n.attribute("min").as_float(0.0f),
            n.attribute("max").as_float(1.0f));
    }},
    {"radio", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* radio = new RadioButton(r, id,
            n.attribute("name").as_string(),
            n.attribute("value").as_string(),
            n.attribute("checked").as_bool(false));
        radio->setScreen(s);
        s->registerRadioButton(radio);
        return radio;
    }},
    {"hr", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Divider(r, id, n.attribute("vertical").as_bool(false));
    }},
    {"divider", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Divider(r, id, n.attribute("vertical").as_bool(false));
    }},
    {"img", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        if (!n.attribute("src")) {
            logw("ImageView '{}' missing required 'src' attribute", id);
            return nullptr;
        }
        return new ImageView(r, id, n.attribute("src").as_string());
    }},
    {"image", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        if (!n.attribute("src")) {
            logw("ImageView '{}' missing required 'src' attribute", id);
            return nullptr;
        }
        return new ImageView(r, id, n.attribute("src").as_string());
    }},
    {"tabbar", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto tabs = xml_utils::parse_comma_separated(n.attribute("tabs").as_string());
        return new TabBar(r, id, tabs);
    }},
    {"select", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto items = xml_utils::parse_comma_separated(n.attribute("items").as_string());
        return new Dropdown(r, id, items);
    }},
    {"dropdown", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto items = xml_utils::parse_comma_separated(n.attribute("items").as_string());
        return new Dropdown(r, id, items);
    }},
    {"switch", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Switch(r, id, n.attribute("on").as_bool(false));
    }},
    {"toggle", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Switch(r, id, n.attribute("on").as_bool(false));
    }},
    {"rating", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Rating(r, id, n.attribute("value").as_int(0));
    }},
    {"searchbox", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new SearchBox(r, id, n.attribute("placeholder").as_string());
    }},
    {"search", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new SearchBox(r, id, n.attribute("placeholder").as_string());
    }},
    {"spinner", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Spinner(r, id);
    }},
    {"alert", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto type = xml_utils::parse_enum(n.attribute("type").as_string("info"), alert_type_map, AlertType::Info);
        return new Alert(r, id, n.text().as_string(), type);
    }},
    {"badge", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Badge(r, id, n.text().as_string());
    }},
    {"card", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Card(r, id);
    }},
    {"avatar", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Avatar(r, id, n.text().as_string());
    }},
    {"chip", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Chip(r, id, n.text().as_string());
    }},
    {"toast", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto type = xml_utils::parse_enum(n.attribute("type").as_string("info"), toast_type_map, ToastType::Info);
        return new Toast(r, id, n.text().as_string(), type);
    }},
    {"breadcrumb", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto items = xml_utils::parse_comma_separated(n.attribute("items").as_string());
        return new Breadcrumb(r, id, items);
    }},
    {"pagination", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Pagination(r, id, 
            n.attribute("total").as_int(1),
            n.attribute("current").as_int(1));
    }},
    {"menu", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Menu(r, id, std::vector<MenuItem>());
    }},
    {"modal", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Modal(r, id, 
            n.attribute("title").as_string(),
            n.text().as_string());
    }},
    {"table", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Table(r, id, std::vector<std::string>(), std::vector<float>());
    }},
    {"iconbutton", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        if (!n.attribute("icon")) {
            logw("IconButton '{}' missing required 'icon' attribute", id);
            return nullptr;
        }
        return new IconButton(r, id, n.attribute("icon").as_string());
    }},
    {"calendar", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto [year, month] = get_current_year_month();
        return new Calendar(r, id,
            n.attribute("year").as_int(year),
            n.attribute("month").as_int(month));
    }},
    {"colorpicker", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new ColorPicker(r, id);
    }},
    {"snackbar", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Snackbar(r, id, n.text().as_string());
    }},
    {"tablist", [](NVGCSSRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto tabs = xml_utils::parse_comma_separated(n.attribute("tabs").as_string());
        return new TabList(r, id, tabs);
    }}
};

} // namespace flexui
