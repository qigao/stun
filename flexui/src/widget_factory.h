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
#include <flexui/scrollview.h>
#include "xml_utils.h"

namespace flexui {

class Screen;

using WidgetFactory = std::function<Widget*(cssboxRenderer*, const std::string&, pugi::xml_node&, Screen*)>;

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
    {"div", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Widget(r, id, "div");
    }},
    {"container", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Widget(r, id, "div");
    }},
    {"button", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Button(r, id, n.text().as_string());
    }},
    {"label", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Label(r, id, n.text().as_string());
    }},
    {"input", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new TextBox(r, id, n.attribute("placeholder").as_string());
    }},
    {"textbox", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new TextBox(r, id, n.attribute("placeholder").as_string());
    }},
    {"checkbox", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Checkbox(r, id, n.attribute("checked").as_bool(false));
    }},
    {"progressbar", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new ProgressBar(r, id, n.attribute("value").as_float(0.0f));
    }},
    {"slider", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Slider(r, id, 
            n.attribute("value").as_float(0.5f),
            n.attribute("min").as_float(0.0f),
            n.attribute("max").as_float(1.0f));
    }},
    {"radio", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* radio = new RadioButton(r, id,
            n.attribute("name").as_string(),
            n.attribute("value").as_string(),
            n.attribute("checked").as_bool(false));
        radio->setScreen(s);
        s->registerRadioButton(radio);
        return radio;
    }},
    {"hr", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Divider(r, id, n.attribute("vertical").as_bool(false));
    }},
    {"divider", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Divider(r, id, n.attribute("vertical").as_bool(false));
    }},
    {"img", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        if (!n.attribute("src")) {
            logw("ImageView '{}' missing required 'src' attribute", id);
            return nullptr;
        }
        return new ImageView(r, id, n.attribute("src").as_string());
    }},
    {"image", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        // Check if it's an HTML-style image (src attribute) or SVG image (href attribute)
        if (auto src = n.attribute("src")) {
            // HTML-style: <image src="..."/>
            return new ImageView(r, id, src.as_string());
        } else if (n.attribute("href") || n.attribute("xlink:href")) {
            // SVG-style: <image href="..." x="..." y="..."/>
            auto* widget = new Widget(r, id, "image");
            if (auto href = n.attribute("href")) widget->setInlineStyle("href", href.as_string());
            if (auto xhref = n.attribute("xlink:href")) widget->setInlineStyle("href", xhref.as_string());
            if (auto x = n.attribute("x")) widget->setInlineStyle("x", x.as_string());
            if (auto y = n.attribute("y")) widget->setInlineStyle("y", y.as_string());
            if (auto w = n.attribute("width")) widget->setInlineStyle("width", w.as_string());
            if (auto h = n.attribute("height")) widget->setInlineStyle("height", h.as_string());
            if (auto par = n.attribute("preserveAspectRatio")) widget->setInlineStyle("preserveAspectRatio", par.as_string());
            return widget;
        } else {
            logw("Image '{}' missing required 'src' or 'href' attribute", id);
            return nullptr;
        }
    }},
    {"tabbar", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto tabs = xml_utils::parse_comma_separated(n.attribute("tabs").as_string());
        return new TabBar(r, id, tabs);
    }},
    {"select", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto items = xml_utils::parse_comma_separated(n.attribute("items").as_string());
        return new Dropdown(r, id, items);
    }},
    {"dropdown", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto items = xml_utils::parse_comma_separated(n.attribute("items").as_string());
        return new Dropdown(r, id, items);
    }},
    {"switch", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Switch(r, id, n.attribute("on").as_bool(false));
    }},
    {"toggle", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Switch(r, id, n.attribute("on").as_bool(false));
    }},
    {"rating", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Rating(r, id, n.attribute("value").as_int(0));
    }},
    {"searchbox", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new SearchBox(r, id, n.attribute("placeholder").as_string());
    }},
    {"search", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new SearchBox(r, id, n.attribute("placeholder").as_string());
    }},
    {"spinner", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Spinner(r, id);
    }},
    {"alert", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto type = xml_utils::parse_enum(n.attribute("type").as_string("info"), alert_type_map, AlertType::Info);
        return new Alert(r, id, n.text().as_string(), type);
    }},
    {"badge", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Badge(r, id, n.text().as_string());
    }},
    {"card", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Card(r, id);
    }},
    {"avatar", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Avatar(r, id, n.text().as_string());
    }},
    {"chip", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Chip(r, id, n.text().as_string());
    }},
    {"toast", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto type = xml_utils::parse_enum(n.attribute("type").as_string("info"), toast_type_map, ToastType::Info);
        return new Toast(r, id, n.text().as_string(), type);
    }},
    {"breadcrumb", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto items = xml_utils::parse_comma_separated(n.attribute("items").as_string());
        return new Breadcrumb(r, id, items);
    }},
    {"pagination", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Pagination(r, id, 
            n.attribute("total").as_int(1),
            n.attribute("current").as_int(1));
    }},
    {"menu", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Menu(r, id, std::vector<MenuItem>());
    }},
    {"modal", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Modal(r, id, 
            n.attribute("title").as_string(),
            n.text().as_string());
    }},
    {"table", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Table(r, id, std::vector<std::string>(), std::vector<float>());
    }},
    {"iconbutton", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        if (!n.attribute("icon")) {
            logw("IconButton '{}' missing required 'icon' attribute", id);
            return nullptr;
        }
        return new IconButton(r, id, n.attribute("icon").as_string());
    }},
    {"calendar", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto [year, month] = get_current_year_month();
        return new Calendar(r, id,
            n.attribute("year").as_int(year),
            n.attribute("month").as_int(month));
    }},
    {"colorpicker", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new ColorPicker(r, id);
    }},
    {"snackbar", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new Snackbar(r, id, n.text().as_string());
    }},
    {"tablist", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto tabs = xml_utils::parse_comma_separated(n.attribute("tabs").as_string());
        return new TabList(r, id, tabs);
    }},
    {"scrollview", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new ScrollView(r, id);
    }},
    {"scroll", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        return new ScrollView(r, id);
    }},

    // ========================================================================
    // SVG Elements (RFC 7991/7996 compliant)
    // ========================================================================

    {"svg", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "svg");
        widget->setInlineStyle("display", "block");  // SVG containers must be visible
        // Handle viewBox attribute
        if (auto viewBox = n.attribute("viewBox")) {
            widget->setInlineStyle("viewBox", viewBox.as_string());
        }
        // Width/height can come from attributes or CSS
        if (auto w = n.attribute("width")) {
            widget->setInlineStyle("width", w.as_string());
        }
        if (auto h = n.attribute("height")) {
            widget->setInlineStyle("height", h.as_string());
        }
        if (auto par = n.attribute("preserveAspectRatio")) {
            widget->setInlineStyle("preserveAspectRatio", par.as_string());
        }
        return widget;
    }},

    {"g", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "g");
        widget->setInlineStyle("display", "block");  // SVG elements must be visible
        widget->addClass("svg-element");  // Enable CSS targeting
        if (auto transform = n.attribute("transform")) {
            widget->setInlineStyle("transform", transform.as_string());
        }
        return widget;
    }},

    {"circle", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "circle");
        widget->setInlineStyle("display", "block");  // SVG elements must be visible
        widget->addClass("svg-element");  // Enable CSS targeting
        if (auto cx = n.attribute("cx")) widget->setInlineStyle("cx", cx.as_string());
        if (auto cy = n.attribute("cy")) widget->setInlineStyle("cy", cy.as_string());
        if (auto rad = n.attribute("r")) widget->setInlineStyle("r", rad.as_string());
        // SVG presentation attributes
        if (auto fill = n.attribute("fill")) widget->setInlineStyle("fill", fill.as_string());
        if (auto stroke = n.attribute("stroke")) widget->setInlineStyle("stroke", stroke.as_string());
        if (auto sw = n.attribute("stroke-width")) widget->setInlineStyle("stroke-width", sw.as_string());
        if (auto opacity = n.attribute("opacity")) widget->setInlineStyle("opacity", opacity.as_string());
        if (auto transform = n.attribute("transform")) widget->setInlineStyle("transform", transform.as_string());
        return widget;
    }},

    {"ellipse", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "ellipse");
        widget->setInlineStyle("display", "block");  // SVG elements must be visible
        widget->addClass("svg-element");  // Enable CSS targeting
        if (auto cx = n.attribute("cx")) widget->setInlineStyle("cx", cx.as_string());
        if (auto cy = n.attribute("cy")) widget->setInlineStyle("cy", cy.as_string());
        if (auto rx = n.attribute("rx")) widget->setInlineStyle("rx", rx.as_string());
        if (auto ry = n.attribute("ry")) widget->setInlineStyle("ry", ry.as_string());
        // SVG presentation attributes
        if (auto fill = n.attribute("fill")) widget->setInlineStyle("fill", fill.as_string());
        if (auto stroke = n.attribute("stroke")) widget->setInlineStyle("stroke", stroke.as_string());
        if (auto sw = n.attribute("stroke-width")) widget->setInlineStyle("stroke-width", sw.as_string());
        if (auto opacity = n.attribute("opacity")) widget->setInlineStyle("opacity", opacity.as_string());
        if (auto transform = n.attribute("transform")) widget->setInlineStyle("transform", transform.as_string());
        return widget;
    }},

    {"rect", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "rect");
        widget->setInlineStyle("display", "block");  // SVG elements must be visible
        widget->addClass("svg-element");  // Enable CSS targeting
        if (auto x = n.attribute("x")) widget->setInlineStyle("x", x.as_string());
        if (auto y = n.attribute("y")) widget->setInlineStyle("y", y.as_string());
        if (auto w = n.attribute("width")) widget->setInlineStyle("width", w.as_string());
        if (auto h = n.attribute("height")) widget->setInlineStyle("height", h.as_string());
        if (auto rx = n.attribute("rx")) widget->setInlineStyle("rx", rx.as_string());
        if (auto ry = n.attribute("ry")) widget->setInlineStyle("ry", ry.as_string());
        // SVG presentation attributes
        if (auto fill = n.attribute("fill")) widget->setInlineStyle("fill", fill.as_string());
        if (auto stroke = n.attribute("stroke")) widget->setInlineStyle("stroke", stroke.as_string());
        if (auto sw = n.attribute("stroke-width")) widget->setInlineStyle("stroke-width", sw.as_string());
        if (auto opacity = n.attribute("opacity")) widget->setInlineStyle("opacity", opacity.as_string());
        if (auto transform = n.attribute("transform")) widget->setInlineStyle("transform", transform.as_string());
        return widget;
    }},

    {"line", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "line");
        widget->setInlineStyle("display", "block");  // SVG elements must be visible
        widget->addClass("svg-element");  // Enable CSS targeting
        if (auto x1 = n.attribute("x1")) widget->setInlineStyle("x1", x1.as_string());
        if (auto y1 = n.attribute("y1")) widget->setInlineStyle("y1", y1.as_string());
        if (auto x2 = n.attribute("x2")) widget->setInlineStyle("x2", x2.as_string());
        if (auto y2 = n.attribute("y2")) widget->setInlineStyle("y2", y2.as_string());
        // SVG presentation attributes
        if (auto stroke = n.attribute("stroke")) widget->setInlineStyle("stroke", stroke.as_string());
        if (auto sw = n.attribute("stroke-width")) widget->setInlineStyle("stroke-width", sw.as_string());
        if (auto lc = n.attribute("stroke-linecap")) widget->setInlineStyle("stroke-linecap", lc.as_string());
        if (auto da = n.attribute("stroke-dasharray")) widget->setInlineStyle("stroke-dasharray", da.as_string());
        if (auto opacity = n.attribute("opacity")) widget->setInlineStyle("opacity", opacity.as_string());
        if (auto transform = n.attribute("transform")) widget->setInlineStyle("transform", transform.as_string());
        // Markers
        if (auto ms = n.attribute("marker-start")) widget->setInlineStyle("marker-start", ms.as_string());
        if (auto mm = n.attribute("marker-mid")) widget->setInlineStyle("marker-mid", mm.as_string());
        if (auto me = n.attribute("marker-end")) widget->setInlineStyle("marker-end", me.as_string());
        return widget;
    }},

    {"path", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "path");
        widget->setInlineStyle("display", "block");  // SVG elements must be visible
        widget->addClass("svg-element");  // Enable CSS targeting
        if (auto d = n.attribute("d")) widget->setInlineStyle("d", d.as_string());
        // SVG presentation attributes
        if (auto fill = n.attribute("fill")) widget->setInlineStyle("fill", fill.as_string());
        if (auto stroke = n.attribute("stroke")) widget->setInlineStyle("stroke", stroke.as_string());
        if (auto sw = n.attribute("stroke-width")) widget->setInlineStyle("stroke-width", sw.as_string());
        if (auto lc = n.attribute("stroke-linecap")) widget->setInlineStyle("stroke-linecap", lc.as_string());
        if (auto lj = n.attribute("stroke-linejoin")) widget->setInlineStyle("stroke-linejoin", lj.as_string());
        if (auto da = n.attribute("stroke-dasharray")) widget->setInlineStyle("stroke-dasharray", da.as_string());
        if (auto opacity = n.attribute("opacity")) widget->setInlineStyle("opacity", opacity.as_string());
        if (auto transform = n.attribute("transform")) widget->setInlineStyle("transform", transform.as_string());
        if (auto fr = n.attribute("fill-rule")) widget->setInlineStyle("fill-rule", fr.as_string());
        // Markers
        if (auto ms = n.attribute("marker-start")) widget->setInlineStyle("marker-start", ms.as_string());
        if (auto mm = n.attribute("marker-mid")) widget->setInlineStyle("marker-mid", mm.as_string());
        if (auto me = n.attribute("marker-end")) widget->setInlineStyle("marker-end", me.as_string());
        // Clip path
        if (auto cp = n.attribute("clip-path")) widget->setInlineStyle("clip-path", cp.as_string());
        return widget;
    }},

    {"polygon", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "polygon");
        widget->setInlineStyle("display", "block");  // SVG elements must be visible
        widget->addClass("svg-element");  // Enable CSS targeting
        if (auto points = n.attribute("points")) widget->setInlineStyle("points", points.as_string());
        // SVG presentation attributes
        if (auto fill = n.attribute("fill")) widget->setInlineStyle("fill", fill.as_string());
        if (auto stroke = n.attribute("stroke")) widget->setInlineStyle("stroke", stroke.as_string());
        if (auto sw = n.attribute("stroke-width")) widget->setInlineStyle("stroke-width", sw.as_string());
        if (auto lj = n.attribute("stroke-linejoin")) widget->setInlineStyle("stroke-linejoin", lj.as_string());
        if (auto opacity = n.attribute("opacity")) widget->setInlineStyle("opacity", opacity.as_string());
        if (auto transform = n.attribute("transform")) widget->setInlineStyle("transform", transform.as_string());
        // Markers
        if (auto ms = n.attribute("marker-start")) widget->setInlineStyle("marker-start", ms.as_string());
        if (auto mm = n.attribute("marker-mid")) widget->setInlineStyle("marker-mid", mm.as_string());
        if (auto me = n.attribute("marker-end")) widget->setInlineStyle("marker-end", me.as_string());
        return widget;
    }},

    {"polyline", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "polyline");
        widget->setInlineStyle("display", "block");  // SVG elements must be visible
        widget->addClass("svg-element");  // Enable CSS targeting
        if (auto points = n.attribute("points")) widget->setInlineStyle("points", points.as_string());
        // SVG presentation attributes
        if (auto fill = n.attribute("fill")) widget->setInlineStyle("fill", fill.as_string());
        if (auto stroke = n.attribute("stroke")) widget->setInlineStyle("stroke", stroke.as_string());
        if (auto sw = n.attribute("stroke-width")) widget->setInlineStyle("stroke-width", sw.as_string());
        if (auto lc = n.attribute("stroke-linecap")) widget->setInlineStyle("stroke-linecap", lc.as_string());
        if (auto lj = n.attribute("stroke-linejoin")) widget->setInlineStyle("stroke-linejoin", lj.as_string());
        if (auto opacity = n.attribute("opacity")) widget->setInlineStyle("opacity", opacity.as_string());
        if (auto transform = n.attribute("transform")) widget->setInlineStyle("transform", transform.as_string());
        // Markers
        if (auto ms = n.attribute("marker-start")) widget->setInlineStyle("marker-start", ms.as_string());
        if (auto mm = n.attribute("marker-mid")) widget->setInlineStyle("marker-mid", mm.as_string());
        if (auto me = n.attribute("marker-end")) widget->setInlineStyle("marker-end", me.as_string());
        return widget;
    }},

    {"text", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "text");
        widget->setInlineStyle("display", "block");  // SVG elements must be visible
        widget->addClass("svg-element");  // Enable CSS targeting
        widget->setText(n.text().as_string());
        if (auto x = n.attribute("x")) widget->setInlineStyle("x", x.as_string());
        if (auto y = n.attribute("y")) widget->setInlineStyle("y", y.as_string());
        if (auto dx = n.attribute("dx")) widget->setInlineStyle("dx", dx.as_string());
        if (auto dy = n.attribute("dy")) widget->setInlineStyle("dy", dy.as_string());
        if (auto ta = n.attribute("text-anchor")) widget->setInlineStyle("text-anchor", ta.as_string());
        // SVG presentation attributes
        if (auto fill = n.attribute("fill")) widget->setInlineStyle("fill", fill.as_string());
        if (auto stroke = n.attribute("stroke")) widget->setInlineStyle("stroke", stroke.as_string());
        if (auto fs = n.attribute("font-size")) widget->setInlineStyle("font-size", fs.as_string());
        if (auto ff = n.attribute("font-family")) widget->setInlineStyle("font-family", ff.as_string());
        if (auto fw = n.attribute("font-weight")) widget->setInlineStyle("font-weight", fw.as_string());
        if (auto opacity = n.attribute("opacity")) widget->setInlineStyle("opacity", opacity.as_string());
        if (auto transform = n.attribute("transform")) widget->setInlineStyle("transform", transform.as_string());
        return widget;
    }},

    {"textPath", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "textPath");
        widget->setText(n.text().as_string());
        if (auto href = n.attribute("href")) widget->setInlineStyle("href", href.as_string());
        if (auto xhref = n.attribute("xlink:href")) widget->setInlineStyle("href", xhref.as_string());
        if (auto so = n.attribute("startOffset")) widget->setInlineStyle("startOffset", so.as_string());
        if (auto ta = n.attribute("text-anchor")) widget->setInlineStyle("text-anchor", ta.as_string());
        // SVG presentation attributes
        if (auto fill = n.attribute("fill")) widget->setInlineStyle("fill", fill.as_string());
        if (auto fs = n.attribute("font-size")) widget->setInlineStyle("font-size", fs.as_string());
        if (auto ff = n.attribute("font-family")) widget->setInlineStyle("font-family", ff.as_string());
        return widget;
    }},

    {"defs", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "defs");
        widget->setInlineStyle("display", "none");  // defs content is not rendered directly
        return widget;
    }},

    {"use", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "use");
        if (auto href = n.attribute("href")) widget->setInlineStyle("href", href.as_string());
        if (auto xhref = n.attribute("xlink:href")) widget->setInlineStyle("href", xhref.as_string());
        if (auto x = n.attribute("x")) widget->setInlineStyle("x", x.as_string());
        if (auto y = n.attribute("y")) widget->setInlineStyle("y", y.as_string());
        if (auto w = n.attribute("width")) widget->setInlineStyle("width", w.as_string());
        if (auto h = n.attribute("height")) widget->setInlineStyle("height", h.as_string());
        return widget;
    }},

    {"symbol", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "symbol");
        if (auto viewBox = n.attribute("viewBox")) widget->setInlineStyle("viewBox", viewBox.as_string());
        widget->setInlineStyle("display", "none");  // symbols are not rendered directly
        return widget;
    }},

    {"marker", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "marker");
        if (auto mw = n.attribute("markerWidth")) widget->setInlineStyle("markerWidth", mw.as_string());
        if (auto mh = n.attribute("markerHeight")) widget->setInlineStyle("markerHeight", mh.as_string());
        if (auto rx = n.attribute("refX")) widget->setInlineStyle("refX", rx.as_string());
        if (auto ry = n.attribute("refY")) widget->setInlineStyle("refY", ry.as_string());
        if (auto orient = n.attribute("orient")) widget->setInlineStyle("orient", orient.as_string());
        if (auto mu = n.attribute("markerUnits")) widget->setInlineStyle("markerUnits", mu.as_string());
        widget->setInlineStyle("display", "none");  // markers are not rendered directly
        return widget;
    }},

    {"clipPath", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "clipPath");
        if (auto cu = n.attribute("clipPathUnits")) widget->setInlineStyle("clipPathUnits", cu.as_string());
        widget->setInlineStyle("display", "none");  // clipPaths are not rendered directly
        return widget;
    }},

    {"pattern", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "pattern");
        if (auto x = n.attribute("x")) widget->setInlineStyle("x", x.as_string());
        if (auto y = n.attribute("y")) widget->setInlineStyle("y", y.as_string());
        if (auto w = n.attribute("width")) widget->setInlineStyle("width", w.as_string());
        if (auto h = n.attribute("height")) widget->setInlineStyle("height", h.as_string());
        if (auto pu = n.attribute("patternUnits")) widget->setInlineStyle("patternUnits", pu.as_string());
        if (auto pcu = n.attribute("patternContentUnits")) widget->setInlineStyle("patternContentUnits", pcu.as_string());
        if (auto pt = n.attribute("patternTransform")) widget->setInlineStyle("patternTransform", pt.as_string());
        widget->setInlineStyle("display", "none");  // patterns are not rendered directly
        return widget;
    }},

    {"linearGradient", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "linearGradient");
        if (auto x1 = n.attribute("x1")) widget->setInlineStyle("x1", x1.as_string());
        if (auto y1 = n.attribute("y1")) widget->setInlineStyle("y1", y1.as_string());
        if (auto x2 = n.attribute("x2")) widget->setInlineStyle("x2", x2.as_string());
        if (auto y2 = n.attribute("y2")) widget->setInlineStyle("y2", y2.as_string());
        if (auto gu = n.attribute("gradientUnits")) widget->setInlineStyle("gradientUnits", gu.as_string());
        if (auto gt = n.attribute("gradientTransform")) widget->setInlineStyle("gradientTransform", gt.as_string());
        if (auto sm = n.attribute("spreadMethod")) widget->setInlineStyle("spreadMethod", sm.as_string());
        widget->setInlineStyle("display", "none");
        
        // Parse gradient and store in renderer registry
        GradientData gradient;
        gradient.type = GradientData::LINEAR;
        
        // Parse stops from child nodes
        for (pugi::xml_node stop_node : n.children("stop")) {
            GradientStop stop;
            
            // Parse offset
            if (auto offset_attr = stop_node.attribute("offset")) {
                std::string offset_str = offset_attr.as_string();
                if (!offset_str.empty() && offset_str.back() == '%') {
                    stop.position = std::strtof(offset_str.c_str(), nullptr) / 100.0f;
                } else {
                    stop.position = std::strtof(offset_str.c_str(), nullptr);
                }
            }
            
            // Parse stop-color
            if (auto color_attr = stop_node.attribute("stop-color")) {
                stop.color = cssbox_utils::parse_color(color_attr.as_string());
            }
            
            // Parse stop-opacity
            if (auto opacity_attr = stop_node.attribute("stop-opacity")) {
                stop.color.a = std::strtof(opacity_attr.as_string(), nullptr);
            }
            
            gradient.stops.push_back(stop);
        }
        
        // Store in renderer registry
        r->gradients_[id] = gradient;
        printf("[WIDGET_FACTORY] Stored linearGradient id='%s' stops=%zu\n", id.c_str(), gradient.stops.size());
        
        return widget;
    }},

    {"radialGradient", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "radialGradient");
        if (auto cx = n.attribute("cx")) widget->setInlineStyle("cx", cx.as_string());
        if (auto cy = n.attribute("cy")) widget->setInlineStyle("cy", cy.as_string());
        if (auto rad = n.attribute("r")) widget->setInlineStyle("r", rad.as_string());
        if (auto fx = n.attribute("fx")) widget->setInlineStyle("fx", fx.as_string());
        if (auto fy = n.attribute("fy")) widget->setInlineStyle("fy", fy.as_string());
        if (auto gu = n.attribute("gradientUnits")) widget->setInlineStyle("gradientUnits", gu.as_string());
        if (auto gt = n.attribute("gradientTransform")) widget->setInlineStyle("gradientTransform", gt.as_string());
        if (auto sm = n.attribute("spreadMethod")) widget->setInlineStyle("spreadMethod", sm.as_string());
        widget->setInlineStyle("display", "none");
        
        // Parse gradient and store in renderer registry
        GradientData gradient;
        gradient.type = GradientData::RADIAL;
        
        // Parse stops from child nodes
        for (pugi::xml_node stop_node : n.children("stop")) {
            GradientStop stop;
            
            // Parse offset
            if (auto offset_attr = stop_node.attribute("offset")) {
                std::string offset_str = offset_attr.as_string();
                if (!offset_str.empty() && offset_str.back() == '%') {
                    stop.position = std::strtof(offset_str.c_str(), nullptr) / 100.0f;
                } else {
                    stop.position = std::strtof(offset_str.c_str(), nullptr);
                }
            }
            
            // Parse stop-color
            if (auto color_attr = stop_node.attribute("stop-color")) {
                stop.color = cssbox_utils::parse_color(color_attr.as_string());
            }
            
            // Parse stop-opacity
            if (auto opacity_attr = stop_node.attribute("stop-opacity")) {
                stop.color.a = std::strtof(opacity_attr.as_string(), nullptr);
            }
            
            gradient.stops.push_back(stop);
        }
        
        // Store in renderer registry
        r->gradients_[id] = gradient;
        printf("[WIDGET_FACTORY] Stored radialGradient id='%s' stops=%zu\n", id.c_str(), gradient.stops.size());
        
        return widget;
    }},

    {"stop", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        auto* widget = new Widget(r, id, "stop");
        if (auto offset = n.attribute("offset")) widget->setInlineStyle("offset", offset.as_string());
        if (auto sc = n.attribute("stop-color")) widget->setInlineStyle("stop-color", sc.as_string());
        if (auto so = n.attribute("stop-opacity")) widget->setInlineStyle("stop-opacity", so.as_string());
        return widget;
    }},

    {"svgImage", [](cssboxRenderer* r, const std::string& id, pugi::xml_node& n, Screen* s) -> Widget* {
        // Alias for SVG image element (use <image> with href for standard SVG)
        auto* widget = new Widget(r, id, "image");
        if (auto href = n.attribute("href")) widget->setInlineStyle("href", href.as_string());
        if (auto xhref = n.attribute("xlink:href")) widget->setInlineStyle("href", xhref.as_string());
        if (auto x = n.attribute("x")) widget->setInlineStyle("x", x.as_string());
        if (auto y = n.attribute("y")) widget->setInlineStyle("y", y.as_string());
        if (auto w = n.attribute("width")) widget->setInlineStyle("width", w.as_string());
        if (auto h = n.attribute("height")) widget->setInlineStyle("height", h.as_string());
        if (auto par = n.attribute("preserveAspectRatio")) widget->setInlineStyle("preserveAspectRatio", par.as_string());
        return widget;
    }}
};

} // namespace flexui
