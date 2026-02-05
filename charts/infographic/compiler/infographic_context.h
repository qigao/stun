#pragma once

#include <ir/unified_infographic.h>
#include "infographic_token.h"
#include <memory>
#include <string>
#include <vector>

namespace flex::modules::infographic {

struct ParseContext {
    UnifiedInfographic* infographic;
    std::unique_ptr<DataItem> current_item;
    std::vector<std::unique_ptr<DataItem>> item_stack;
    std::string error_message;
    std::vector<std::unique_ptr<std::string>> string_pool;

    explicit ParseContext(UnifiedInfographic* info) : infographic(info) {}

    std::string* add_string(const std::string& s) {
        string_pool.push_back(std::make_unique<std::string>(s));
        return string_pool.back().get();
    }

    double to_number(std::string* s) {
        if (!s) return 0.0;
        try { return std::stod(*s); } catch (...) { return 0.0; }
    }

    bool to_bool(std::string* s) { return s && *s == "true"; }

    void set_template(const std::string& name) {
        infographic->template_type = string_to_template_type(name);
        infographic->category = get_template_category(infographic->template_type);
    }

    void set_title(std::string* s) { if (s) infographic->set_title(*s); }
    void set_desc(std::string* s) { if (s) infographic->set_desc(*s); }
    void add_palette_color(std::string* s) { if (s) infographic->theme.palette.push_back(*s); }
    void set_preset(std::string* s) { if (s) infographic->theme.preset = *s; }
    void set_stylize(std::string* s) { if (s) infographic->theme.stylize = *s; }

    void begin_item() { current_item = DataItem::create(""); }

    void end_item() {
        if (!current_item) return;
        if (!item_stack.empty()) {
            item_stack.back()->children.push_back(std::move(current_item));
        } else {
            infographic->add_item(std::move(current_item));
        }
        current_item = nullptr;
    }

    void set_item_label(std::string* s) { if (current_item && s) current_item->label = *s; }
    void set_item_desc(std::string* s) { if (current_item && s) current_item->desc = *s; }
    void set_item_value(std::string* s) { if (current_item && s) current_item->value = to_number(s); }
    void set_item_icon(std::string* s) { if (current_item && s) current_item->icon = *s; }
    void set_item_illus(std::string* s) { if (current_item && s) current_item->illus = *s; }
    void set_item_done(std::string* s) { if (current_item && s) current_item->done = to_bool(s); }

    void begin_children() {
        if (current_item) {
            item_stack.push_back(std::move(current_item));
            current_item = nullptr;
        }
    }

    void end_children() {
        if (!item_stack.empty()) {
            current_item = std::move(item_stack.back());
            item_stack.pop_back();
        }
    }
};

} // namespace flex::modules::infographic
