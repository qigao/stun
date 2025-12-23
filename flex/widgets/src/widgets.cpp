/*
 * Flex Widgets - Implementation
 * All standard widgets for Fluent Design UI
 */

#include "widgets/widgets.h"
#include "flex/group.h"
#include "flex/shape.h"
#include "flex/text.h"

namespace flex {
namespace widgets {

// ============================================================================
// Button
// ============================================================================
// Props: text, width, height, variant (accent/secondary/outline), enabled

void register_button(const WidgetTheme& theme) {
    auto button = Component::create("Button");

    button->add_prop("text", std::string("Button"));
    button->add_prop("width", 120.0f);
    button->add_prop("height", theme.control_height);
    button->add_prop("variant", std::string("accent"));  // accent, secondary, outline
    button->add_prop("enabled", true);

    button->set_builder([theme](const Props& props) -> Node::Ptr {
        auto text_str = get_string(props, "text", "Button");
        auto width = get_float(props, "width", 120.0f);
        auto height = get_float(props, "height", theme.control_height);
        auto variant = get_string(props, "variant", "accent");
        auto enabled = get_bool(props, "enabled", true);

        auto group = Group::create();
        group->set_id("button_root");

        // Determine colors based on variant
        ThemeColor bg_color = theme.accent;
        ThemeColor text_color = theme.text_on_accent;
        ThemeColor border_color = theme.accent;

        if (variant == "secondary") {
            bg_color = theme.surface;
            text_color = theme.text_primary;
            border_color = theme.border;
        } else if (variant == "outline") {
            bg_color = ThemeColor(0x00FFFFFF);  // Transparent
            text_color = theme.accent;
            border_color = theme.accent;
        }

        if (!enabled) {
            bg_color = theme.disabled_bg;
            text_color = theme.text_disabled;
            border_color = theme.border;
        }

        // Background
        auto bg = create_rect_bg(width, height, bg_color, theme.corner_radius, &border_color, theme.border_width);
        bg->set_id("bg");
        group->add_child(bg);

        // Label (centered)
        auto label = create_text(text_str, theme.font_size, text_color, theme.font_family);
        label->set_id("label");
        // Center the text - approximate centering
        label->set_position(width / 2 - text_str.length() * theme.font_size * 0.3f, height / 2 - theme.font_size / 2);
        group->add_child(label);

        return group;
    });

    ComponentRegistry::instance().register_component(button);
}

// ============================================================================
// Label
// ============================================================================
// Props: text, fontSize, color, align

void register_label(const WidgetTheme& theme) {
    auto label = Component::create("Label");

    label->add_prop("text", std::string("Label"));
    label->add_prop("fontSize", theme.font_size);
    label->add_prop("color", theme.text_primary.value);
    label->add_prop("align", std::string("left"));  // left, center, right

    label->set_builder([theme](const Props& props) -> Node::Ptr {
        auto text_str = get_string(props, "text", "Label");
        auto font_size = get_float(props, "fontSize", theme.font_size);
        auto color_val = get_color(props, "color", theme.text_primary.value);
        // auto align = get_string(props, "align", "left");

        ThemeColor color(color_val);
        auto text = create_text(text_str, font_size, color, theme.font_family);
        text->set_id("text");

        return text;
    });

    ComponentRegistry::instance().register_component(label);
}

// ============================================================================
// Checkbox
// ============================================================================
// Props: checked, text, enabled

void register_checkbox(const WidgetTheme& theme) {
    auto checkbox = Component::create("Checkbox");

    checkbox->add_prop("checked", false);
    checkbox->add_prop("text", std::string(""));
    checkbox->add_prop("enabled", true);

    checkbox->set_builder([theme](const Props& props) -> Node::Ptr {
        auto checked = get_bool(props, "checked", false);
        auto text_str = get_string(props, "text", "");
        auto enabled = get_bool(props, "enabled", true);

        auto group = Group::create();
        group->set_id("checkbox_root");

        float box_size = 18.0f;

        // Checkbox box
        ThemeColor bg_color = checked ? theme.accent : theme.surface;
        ThemeColor border_color = checked ? theme.accent : theme.border;

        if (!enabled) {
            bg_color = theme.disabled_bg;
            border_color = theme.border;
        }

        auto box = create_rect_bg(box_size, box_size, bg_color, 3.0f, &border_color, theme.border_width);
        box->set_id("box");
        group->add_child(box);

        // Checkmark (visible when checked)
        if (checked) {
            auto checkmark = Shape::create();
            checkmark->set_id("checkmark");
            // Simple checkmark using path
            checkmark->set_path("M 4 9 L 7 12 L 14 5");
            checkmark->set_stroke(Color(1, 1, 1, 1), 2.0f);
            checkmark->set_fill(Color(0, 0, 0, 0));
            group->add_child(checkmark);
        }

        // Label text
        if (!text_str.empty()) {
            ThemeColor text_color = enabled ? theme.text_primary : theme.text_disabled;
            auto label = create_text(text_str, theme.font_size, text_color, theme.font_family);
            label->set_id("label");
            label->set_position(box_size + theme.spacing, (box_size - theme.font_size) / 2);
            group->add_child(label);
        }

        return group;
    });

    ComponentRegistry::instance().register_component(checkbox);
}

// ============================================================================
// Toggle
// ============================================================================
// Props: on, width, enabled

void register_toggle(const WidgetTheme& theme) {
    auto toggle = Component::create("Toggle");

    toggle->add_prop("on", false);
    toggle->add_prop("width", 44.0f);
    toggle->add_prop("enabled", true);

    toggle->set_builder([theme](const Props& props) -> Node::Ptr {
        auto on = get_bool(props, "on", false);
        auto width = get_float(props, "width", 44.0f);
        auto enabled = get_bool(props, "enabled", true);

        auto group = Group::create();
        group->set_id("toggle_root");

        float height = 22.0f;
        float thumb_radius = 8.0f;
        float thumb_margin = 3.0f;

        // Track
        ThemeColor track_color = on ? theme.accent : theme.surface;
        ThemeColor border_color = on ? theme.accent : theme.border;

        if (!enabled) {
            track_color = theme.disabled_bg;
            border_color = theme.border;
        }

        auto track = create_rect_bg(width, height, track_color, height / 2, &border_color, theme.border_width);
        track->set_id("track");
        group->add_child(track);

        // Thumb
        ThemeColor thumb_color = on ? theme.text_on_accent : theme.text_secondary;
        if (!enabled) {
            thumb_color = theme.text_disabled;
        }

        auto thumb = create_circle(thumb_radius, thumb_color);
        thumb->set_id("thumb");
        float thumb_x = on ? (width - thumb_radius - thumb_margin) : (thumb_radius + thumb_margin);
        thumb->set_position(thumb_x, height / 2);
        group->add_child(thumb);

        return group;
    });

    ComponentRegistry::instance().register_component(toggle);
}

// ============================================================================
// Slider
// ============================================================================
// Props: value, min, max, width, enabled

void register_slider(const WidgetTheme& theme) {
    auto slider = Component::create("Slider");

    slider->add_prop("value", 0.5f);
    slider->add_prop("min", 0.0f);
    slider->add_prop("max", 1.0f);
    slider->add_prop("width", 200.0f);
    slider->add_prop("enabled", true);

    slider->set_builder([theme](const Props& props) -> Node::Ptr {
        auto value = get_float(props, "value", 0.5f);
        auto min_val = get_float(props, "min", 0.0f);
        auto max_val = get_float(props, "max", 1.0f);
        auto width = get_float(props, "width", 200.0f);
        auto enabled = get_bool(props, "enabled", true);

        auto group = Group::create();
        group->set_id("slider_root");

        float height = 4.0f;
        float thumb_radius = 8.0f;

        // Normalize value
        float norm = (value - min_val) / (max_val - min_val);
        if (norm < 0) norm = 0;
        if (norm > 1) norm = 1;

        float fill_width = width * norm;

        // Track background
        ThemeColor track_bg = theme.border;
        if (!enabled) track_bg = theme.disabled_bg;

        auto track = create_rect_bg(width, height, track_bg, height / 2);
        track->set_id("track");
        track->set_position(0, thumb_radius - height / 2);
        group->add_child(track);

        // Fill
        ThemeColor fill_color = enabled ? theme.accent : theme.text_disabled;

        if (fill_width > 0) {
            auto fill = create_rect_bg(fill_width, height, fill_color, height / 2);
            fill->set_id("fill");
            fill->set_position(0, thumb_radius - height / 2);
            group->add_child(fill);
        }

        // Thumb
        ThemeColor thumb_color = enabled ? theme.accent : theme.text_disabled;
        ThemeColor thumb_border = theme.surface;

        auto thumb = create_circle(thumb_radius, thumb_color, &thumb_border, 2.0f);
        thumb->set_id("thumb");
        thumb->set_position(fill_width, thumb_radius);
        group->add_child(thumb);

        return group;
    });

    ComponentRegistry::instance().register_component(slider);
}

// ============================================================================
// ProgressBar
// ============================================================================
// Props: value, width, height, variant (default/striped)

void register_progressbar(const WidgetTheme& theme) {
    auto progress = Component::create("ProgressBar");

    progress->add_prop("value", 0.5f);
    progress->add_prop("width", 200.0f);
    progress->add_prop("height", 4.0f);

    progress->set_builder([theme](const Props& props) -> Node::Ptr {
        auto value = get_float(props, "value", 0.5f);
        auto width = get_float(props, "width", 200.0f);
        auto height = get_float(props, "height", 4.0f);

        if (value < 0) value = 0;
        if (value > 1) value = 1;

        auto group = Group::create();
        group->set_id("progress_root");

        // Track
        auto track = create_rect_bg(width, height, theme.border, height / 2);
        track->set_id("track");
        group->add_child(track);

        // Fill
        float fill_width = width * value;
        if (fill_width > 0) {
            auto fill = create_rect_bg(fill_width, height, theme.accent, height / 2);
            fill->set_id("fill");
            group->add_child(fill);
        }

        return group;
    });

    ComponentRegistry::instance().register_component(progress);
}

// ============================================================================
// TextBox
// ============================================================================
// Props: text, placeholder, width, enabled, readonly

void register_textbox(const WidgetTheme& theme) {
    auto textbox = Component::create("TextBox");

    textbox->add_prop("text", std::string(""));
    textbox->add_prop("placeholder", std::string(""));
    textbox->add_prop("width", 200.0f);
    textbox->add_prop("enabled", true);
    textbox->add_prop("readonly", false);

    textbox->set_builder([theme](const Props& props) -> Node::Ptr {
        auto text_str = get_string(props, "text", "");
        auto placeholder = get_string(props, "placeholder", "");
        auto width = get_float(props, "width", 200.0f);
        auto enabled = get_bool(props, "enabled", true);
        // auto readonly = get_bool(props, "readonly", false);

        auto group = Group::create();
        group->set_id("textbox_root");

        float height = theme.control_height;

        // Background
        ThemeColor bg_color = enabled ? theme.surface : theme.disabled_bg;
        ThemeColor border_color = theme.border;

        auto bg = create_rect_bg(width, height, bg_color, theme.corner_radius, &border_color, theme.border_width);
        bg->set_id("bg");
        group->add_child(bg);

        // Text content or placeholder
        std::string display_text = text_str.empty() ? placeholder : text_str;
        ThemeColor text_color = text_str.empty() ? theme.text_secondary : theme.text_primary;
        if (!enabled) text_color = theme.text_disabled;

        auto text = create_text(display_text, theme.font_size, text_color, theme.font_family);
        text->set_id("content");
        text->set_position(theme.padding_h, (height - theme.font_size) / 2);
        group->add_child(text);

        return group;
    });

    ComponentRegistry::instance().register_component(textbox);
}

// ============================================================================
// RadioButton
// ============================================================================
// Props: selected, text, groupName, enabled

void register_radiobutton(const WidgetTheme& theme) {
    auto radio = Component::create("RadioButton");

    radio->add_prop("selected", false);
    radio->add_prop("text", std::string(""));
    radio->add_prop("groupName", std::string("default"));
    radio->add_prop("enabled", true);

    radio->set_builder([theme](const Props& props) -> Node::Ptr {
        auto selected = get_bool(props, "selected", false);
        auto text_str = get_string(props, "text", "");
        // auto group_name = get_string(props, "groupName", "default");
        auto enabled = get_bool(props, "enabled", true);

        auto group = Group::create();
        group->set_id("radio_root");

        float outer_radius = 9.0f;
        float inner_radius = 4.0f;

        // Outer circle
        ThemeColor outer_color = theme.surface;
        ThemeColor border_color = selected ? theme.accent : theme.border;
        if (!enabled) {
            outer_color = theme.disabled_bg;
            border_color = theme.border;
        }

        auto outer = create_circle(outer_radius, outer_color, &border_color, theme.border_width);
        outer->set_id("outer");
        outer->set_position(outer_radius, outer_radius);
        group->add_child(outer);

        // Inner dot (visible when selected)
        if (selected) {
            ThemeColor inner_color = enabled ? theme.accent : theme.text_disabled;
            auto inner = create_circle(inner_radius, inner_color);
            inner->set_id("inner");
            inner->set_position(outer_radius, outer_radius);
            group->add_child(inner);
        }

        // Label text
        if (!text_str.empty()) {
            ThemeColor text_color = enabled ? theme.text_primary : theme.text_disabled;
            auto label = create_text(text_str, theme.font_size, text_color, theme.font_family);
            label->set_id("label");
            label->set_position(outer_radius * 2 + theme.spacing, outer_radius - theme.font_size / 2);
            group->add_child(label);
        }

        return group;
    });

    ComponentRegistry::instance().register_component(radio);
}

// ============================================================================
// ComboBox
// ============================================================================
// Props: selectedIndex, items (comma-separated), width, enabled

void register_combobox(const WidgetTheme& theme) {
    auto combo = Component::create("ComboBox");

    combo->add_prop("selectedIndex", 0.0f);
    combo->add_prop("items", std::string("Item 1,Item 2,Item 3"));
    combo->add_prop("width", 200.0f);
    combo->add_prop("enabled", true);

    combo->set_builder([theme](const Props& props) -> Node::Ptr {
        auto selected_idx = static_cast<int>(get_float(props, "selectedIndex", 0.0f));
        auto items_str = get_string(props, "items", "Item 1,Item 2,Item 3");
        auto width = get_float(props, "width", 200.0f);
        auto enabled = get_bool(props, "enabled", true);

        // Parse items
        std::vector<std::string> items;
        size_t pos = 0;
        while ((pos = items_str.find(',')) != std::string::npos) {
            items.push_back(items_str.substr(0, pos));
            items_str.erase(0, pos + 1);
        }
        if (!items_str.empty()) items.push_back(items_str);

        auto group = Group::create();
        group->set_id("combo_root");

        float height = theme.control_height;

        // Background
        ThemeColor bg_color = enabled ? theme.surface : theme.disabled_bg;
        ThemeColor border_color = theme.border;

        auto bg = create_rect_bg(width, height, bg_color, theme.corner_radius, &border_color, theme.border_width);
        bg->set_id("bg");
        group->add_child(bg);

        // Selected text
        std::string selected_text = (selected_idx >= 0 && selected_idx < (int)items.size())
            ? items[selected_idx] : "";
        ThemeColor text_color = enabled ? theme.text_primary : theme.text_disabled;

        auto text = create_text(selected_text, theme.font_size, text_color, theme.font_family);
        text->set_id("selected_text");
        text->set_position(theme.padding_h, (height - theme.font_size) / 2);
        group->add_child(text);

        // Dropdown arrow
        auto arrow = Shape::create();
        arrow->set_id("arrow");
        arrow->set_path("M 0 0 L 6 6 L 12 0");
        arrow->set_stroke(Color(
            text_color.r() / 255.0f,
            text_color.g() / 255.0f,
            text_color.b() / 255.0f,
            text_color.a() / 255.0f
        ), 1.5f);
        arrow->set_fill(Color(0, 0, 0, 0));
        arrow->set_position(width - theme.padding_h - 12, (height - 6) / 2);
        group->add_child(arrow);

        return group;
    });

    ComponentRegistry::instance().register_component(combo);
}

// ============================================================================
// ListBox
// ============================================================================
// Props: selectedIndex, items (comma-separated), width, height

void register_listbox(const WidgetTheme& theme) {
    auto listbox = Component::create("ListBox");

    listbox->add_prop("selectedIndex", 0.0f);
    listbox->add_prop("items", std::string("Item 1,Item 2,Item 3"));
    listbox->add_prop("width", 200.0f);
    listbox->add_prop("height", 150.0f);

    listbox->set_builder([theme](const Props& props) -> Node::Ptr {
        auto selected_idx = static_cast<int>(get_float(props, "selectedIndex", 0.0f));
        auto items_str = get_string(props, "items", "Item 1,Item 2,Item 3");
        auto width = get_float(props, "width", 200.0f);
        auto height = get_float(props, "height", 150.0f);

        // Parse items
        std::vector<std::string> items;
        size_t pos = 0;
        std::string temp = items_str;
        while ((pos = temp.find(',')) != std::string::npos) {
            items.push_back(temp.substr(0, pos));
            temp.erase(0, pos + 1);
        }
        if (!temp.empty()) items.push_back(temp);

        auto group = Group::create();
        group->set_id("listbox_root");

        // Background
        auto bg = create_rect_bg(width, height, theme.surface, theme.corner_radius, &theme.border, theme.border_width);
        bg->set_id("bg");
        group->add_child(bg);

        // Items container
        auto items_group = Group::create();
        items_group->set_id("items");
        items_group->set_position(0, 0);

        float item_height = theme.control_height;
        float y = 0;

        for (size_t i = 0; i < items.size(); ++i) {
            auto item_group = Group::create();
            item_group->set_id("item" + std::to_string(i));
            item_group->set_position(0, y);

            // Selection highlight
            if (static_cast<int>(i) == selected_idx) {
                auto highlight = create_rect_bg(width - 2, item_height, theme.accent.with_alpha(30), 2.0f);
                highlight->set_id("highlight");
                highlight->set_position(1, 0);
                item_group->add_child(highlight);
            }

            // Item text
            ThemeColor text_color = (static_cast<int>(i) == selected_idx)
                ? theme.accent : theme.text_primary;

            auto text = create_text(items[i], theme.font_size, text_color, theme.font_family);
            text->set_id("text");
            text->set_position(theme.padding_h, (item_height - theme.font_size) / 2);
            item_group->add_child(text);

            items_group->add_child(item_group);
            y += item_height;
        }

        group->add_child(items_group);

        return group;
    });

    ComponentRegistry::instance().register_component(listbox);
}

} // namespace widgets
} // namespace flex
