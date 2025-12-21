#include <tvgbox2/style_engine.h>
#include <lexbor/css/css.h>
#include <lexbor/css/stylesheet.h>
#include <lexbor/css/rule.h>
#include <lexbor/css/selectors/selectors.h>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <cmath>
#include <cstdio>

namespace tvgbox2 {

class LexborCSSParser {
public:
    LexborCSSParser() {
        parser_ = lxb_css_parser_create();
        if (!parser_) throw std::runtime_error("Failed to create lexbor");
        if (lxb_css_parser_init(parser_, nullptr) != LXB_STATUS_OK) {
            lxb_css_parser_destroy(parser_, true);
            throw std::runtime_error("Failed to init lexbor");
        }
    }
    ~LexborCSSParser() { if (parser_) lxb_css_parser_destroy(parser_, true); }
    LexborCSSParser(const LexborCSSParser&) = delete;
    LexborCSSParser& operator=(const LexborCSSParser&) = delete;
    lxb_css_stylesheet_t* parse(const char* css, size_t len) {
        auto* sheet = lxb_css_stylesheet_parse(parser_, reinterpret_cast<const lxb_char_t*>(css), len);
        if (!sheet) throw std::runtime_error("Failed to parse CSS");
        return sheet;
    }
private:
    lxb_css_parser_t* parser_ = nullptr;
};

struct CSSRule {
    std::string selector;
    std::map<std::string, std::string> properties;
    uint32_t specificity;
};

struct ParsedSelector {
    std::string type, id, pseudo_class;
    std::vector<std::string> classes;
};

static ParsedSelector parse_selector_simple(const std::string& selector) {
    ParsedSelector result;
    std::string s = selector;
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start != std::string::npos) s = s.substr(start, end - start + 1);
    size_t i = 0;
    while (i < s.size()) {
        char c = s[i];
        if (c == '#') {
            i++;
            size_t st = i;
            while (i < s.size() && (std::isalnum(s[i]) || s[i] == '_' || s[i] == '-')) i++;
            result.id = s.substr(st, i - st);
        } else if (c == '.') {
            i++;
            size_t st = i;
            while (i < s.size() && (std::isalnum(s[i]) || s[i] == '_' || s[i] == '-')) i++;
            result.classes.push_back(s.substr(st, i - st));
        } else if (c == ':') {
            i++;
            size_t st = i;
            while (i < s.size() && (std::isalnum(s[i]) || s[i] == '_' || s[i] == '-')) i++;
            result.pseudo_class = s.substr(st, i - st);
        } else if (std::isalpha(c) || c == '*') {
            size_t st = i;
            while (i < s.size() && (std::isalnum(s[i]) || s[i] == '_' || s[i] == '-')) i++;
            result.type = s.substr(st, i - st);
        } else {
            i++;
        }
    }
    return result;
}

static bool matches_selector(const std::string& selector, const Element* elem) {
    auto sel = parse_selector_simple(selector);
    if (!sel.id.empty() && sel.id != elem->id) return false;
    if (!sel.type.empty() && sel.type != "*" && sel.type != elem->tag) return false;
    for (const auto& cls : sel.classes) {
        if (std::find(elem->classes.begin(), elem->classes.end(), cls) == elem->classes.end())
            return false;
    }
    if (!sel.pseudo_class.empty() && elem->pseudo_states.find(sel.pseudo_class) == elem->pseudo_states.end())
        return false;
    return true;
}

static uint32_t calculate_specificity(const std::string& selector) {
    auto sel = parse_selector_simple(selector);
    uint32_t spec = 0;
    if (!sel.id.empty()) spec += 100;
    spec += static_cast<uint32_t>(sel.classes.size()) * 10;
    if (!sel.pseudo_class.empty()) spec += 10;
    if (!sel.type.empty() && sel.type != "*") spec += 1;
    return spec;
}

class StyleEngine::Impl {
public:
    void parse_css(const std::string& css) {
        rules_.clear();
        LexborCSSParser parser;
        auto* sheet = parser.parse(css.c_str(), css.size());
        extract_rules(sheet);
        lxb_css_stylesheet_destroy(sheet, true);
    }

    void apply_styles(Element* elem) {
        if (!elem || !elem->computed_style) return;

        // Step 1: Inherit CSS variables from parent
        if (elem->parent && elem->parent->computed_style) {
            for (const auto& [name, value] : elem->parent->computed_style->variables) {
                // Only inherit if not already defined on this element
                // Inherit all parent variables
                elem->computed_style->variables[name] = value;
            }
        }

        // Step 2: Collect and sort matching rules by specificity
        std::vector<std::pair<uint32_t, const CSSRule*>> matched_rules;
        for (const auto& rule : rules_) {
            if (matches_selector(rule.selector, elem))
                matched_rules.push_back({rule.specificity, &rule});
        }
        std::sort(matched_rules.begin(), matched_rules.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        // Step 3: Pass 1 - Apply CSS Variables (--*)
        for (const auto& [spec, rule] : matched_rules) {
            for (const auto& [prop, value] : rule->properties) {
                if (prop.size() > 2 && prop[0] == '-' && prop[1] == '-') {
                    apply_property(prop, value, elem->computed_style);
                }
            }
        }

        // Pass 2: Apply other properties
        for (const auto& [spec, rule] : matched_rules) {
            for (const auto& [prop, value] : rule->properties) {
                if (!(prop.size() > 2 && prop[0] == '-' && prop[1] == '-')) {
                    apply_property(prop, value, elem->computed_style);
                }
            }
        }
    }

    void clear() { rules_.clear(); }

private:
    std::vector<CSSRule> rules_;



    void extract_rules(lxb_css_stylesheet_t* sheet) {
        if (!sheet || !sheet->root) return;
        lxb_css_rule_t* rule = nullptr;
        if (sheet->root->type == LXB_CSS_RULE_LIST) {
            rule = lxb_css_rule_list(sheet->root)->first;
        } else {
            rule = sheet->root;
        }
        while (rule) {
            if (rule->type == LXB_CSS_RULE_STYLE)
                extract_style_rule(lxb_css_rule_style(rule));
            rule = rule->next;
        }
    }

    void extract_style_rule(lxb_css_rule_style_t* style_rule) {
        if (!style_rule) return;
        std::string selector_text = get_selector_text(style_rule);
        if (selector_text.empty()) return;
        std::map<std::string, std::string> props;
        extract_declarations(style_rule, props);
        if (!props.empty()) {
            CSSRule rule{selector_text, props, calculate_specificity(selector_text)};
            rules_.push_back(rule);
        }
    }

    std::string get_selector_text(lxb_css_rule_style_t* style_rule) {
        if (!style_rule || !style_rule->selector) return "";
        lexbor_str_t str = {0};
        lxb_css_selector_serialize_list(style_rule->selector,
            [](const lxb_char_t* data, size_t len, void* ctx) -> lxb_status_t {
                auto* s = static_cast<lexbor_str_t*>(ctx);
                size_t new_len = (s->length ? s->length : 0) + len;
                auto* new_data = static_cast<lxb_char_t*>(realloc(s->data, new_len + 1));
                if (!new_data) return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
                memcpy(new_data + (s->length ? s->length : 0), data, len);
                new_data[new_len] = 0;
                s->data = new_data;
                s->length = new_len;
                return LXB_STATUS_OK;
            }, &str);
        std::string result;
        if (str.data) {
            result = std::string(reinterpret_cast<char*>(str.data), str.length);
            free(str.data);
        }
        return result;
    }

    void extract_declarations(lxb_css_rule_style_t* style_rule, std::map<std::string, std::string>& props) {
        if (!style_rule || !style_rule->declarations) return;
        lxb_css_rule_t* rule = style_rule->declarations->first;
        while (rule) {
            if (rule->type == LXB_CSS_RULE_DECLARATION) {
                auto* decl = lxb_css_rule_declaration(rule);
                std::string serialized = serialize_declaration(decl);
                size_t colon = serialized.find(':');
                if (colon != std::string::npos) {
                    std::string name = serialized.substr(0, colon);
                    std::string value = serialized.substr(colon + 1);
                    auto trim = [](std::string& s) {
                        size_t start = s.find_first_not_of(" \t\n\r");
                        size_t end = s.find_last_not_of(" \t\n\r");
                        if (start != std::string::npos) s = s.substr(start, end - start + 1);
                    };
                    trim(name);
                    trim(value);
                    if (!name.empty() && !value.empty()) props[name] = value;
                }
            }
            rule = rule->next;
        }
    }

    std::string serialize_declaration(lxb_css_rule_declaration_t* decl) {
        if (!decl) return "";
        lexbor_str_t str = {0};
        lxb_css_rule_declaration_serialize(decl,
            [](const lxb_char_t* data, size_t len, void* ctx) -> lxb_status_t {
                auto* s = static_cast<lexbor_str_t*>(ctx);
                size_t new_len = (s->length ? s->length : 0) + len;
                auto* new_data = static_cast<lxb_char_t*>(realloc(s->data, new_len + 1));
                if (!new_data) return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
                memcpy(new_data + (s->length ? s->length : 0), data, len);
                new_data[new_len] = 0;
                s->data = new_data;
                s->length = new_len;
                return LXB_STATUS_OK;
            }, &str);
        std::string result;
        if (str.data) {
            result = std::string(reinterpret_cast<char*>(str.data), str.length);
            free(str.data);
        }
        return result;
    }

    void apply_property(const std::string& prop, const std::string& raw_value, ComputedStyle* style) {
        // CSS Variables (--xxx)
        if (prop.size() > 2 && prop[0] == '-' && prop[1] == '-') {
            style->variables[prop] = raw_value;
            return;
        }

        // Resolve var() references
        std::string value = style->resolve_variable_value(raw_value);

        // Box Model
        if (prop == "width") {
            style->width = parse_length(value);
            style->width_is_percent = (value.find('%') != std::string::npos);
        }
        else if (prop == "height") {
            style->height = parse_length(value);
            style->height_is_percent = (value.find('%') != std::string::npos);
        }
        else if (prop == "min-width") style->variables["min-width"] = value;
        else if (prop == "max-width") style->variables["max-width"] = value;
        else if (prop == "min-height") style->variables["min-height"] = value;
        else if (prop == "max-height") style->variables["max-height"] = value;
        else if (prop == "padding") parse_box_values(value, style->padding);
        else if (prop == "padding-top") style->padding[0] = parse_length(value);
        else if (prop == "padding-right") style->padding[1] = parse_length(value);
        else if (prop == "padding-bottom") style->padding[2] = parse_length(value);
        else if (prop == "padding-left") style->padding[3] = parse_length(value);
        else if (prop == "margin") parse_box_values(value, style->margin);
        else if (prop == "margin-top") style->margin[0] = parse_length(value);
        else if (prop == "margin-right") style->margin[1] = parse_length(value);
        else if (prop == "margin-bottom") style->margin[2] = parse_length(value);
        else if (prop == "margin-left") style->margin[3] = parse_length(value);
        else if (prop == "border-width") parse_box_values(value, style->border_width);
        else if (prop == "border-radius") parse_box_values(value, style->border_radius);
        else if (prop == "box-sizing") {
            if (value == "border-box") style->box_sizing = BoxSizing::BorderBox;
            else style->box_sizing = BoxSizing::ContentBox;
        }

        // Display & Position
        else if (prop == "display") {
            if (value == "none") style->display = Display::None;
            else if (value == "block") style->display = Display::Block;
            else if (value == "inline") style->display = Display::Inline;
            else if (value == "flex") style->display = Display::Flex;
            else if (value == "grid") style->display = Display::Grid;
        }
        else if (prop == "position") {
            if (value == "static") style->position = Position::Static;
            else if (value == "relative") style->position = Position::Relative;
            else if (value == "absolute") style->position = Position::Absolute;
            else if (value == "fixed") style->position = Position::Fixed;
        }
        else if (prop == "top") style->top = parse_position_offset(value);
        else if (prop == "right") style->right = parse_position_offset(value);
        else if (prop == "bottom") style->bottom = parse_position_offset(value);
        else if (prop == "left") style->left = parse_position_offset(value);
        else if (prop == "z-index") style->z_index = std::stoi(value);

        // Flexbox
        else if (prop == "flex-direction") {
            if (value == "row") style->flex_direction = FlexDirection::Row;
            else if (value == "row-reverse") style->flex_direction = FlexDirection::RowReverse;
            else if (value == "column") style->flex_direction = FlexDirection::Column;
            else if (value == "column-reverse") style->flex_direction = FlexDirection::ColumnReverse;
        }
        else if (prop == "justify-content") {
            if (value == "flex-start") style->justify_content = JustifyContent::FlexStart;
            else if (value == "flex-end") style->justify_content = JustifyContent::FlexEnd;
            else if (value == "center") style->justify_content = JustifyContent::Center;
            else if (value == "space-between") style->justify_content = JustifyContent::SpaceBetween;
            else if (value == "space-around") style->justify_content = JustifyContent::SpaceAround;
            else if (value == "space-evenly") style->justify_content = JustifyContent::SpaceEvenly;
        }
        else if (prop == "align-items") {
            if (value == "flex-start") style->align_items = AlignItems::FlexStart;
            else if (value == "flex-end") style->align_items = AlignItems::FlexEnd;
            else if (value == "center") style->align_items = AlignItems::Center;
            else if (value == "stretch") style->align_items = AlignItems::Stretch;
            else if (value == "baseline") style->align_items = AlignItems::Baseline;
        }
        else if (prop == "gap") style->gap = parse_length(value);
        else if (prop == "flex-grow") style->variables["flex-grow"] = value;
        else if (prop == "flex-shrink") style->variables["flex-shrink"] = value;
        else if (prop == "flex-basis") style->variables["flex-basis"] = value;
        else if (prop == "flex-wrap") style->variables["flex-wrap"] = value;
        else if (prop == "flex") {
            // Shorthand: flex: <grow> [<shrink>] [<basis>]
            // Common cases: flex: 1, flex: 1 1 0, flex: none, flex: auto
            if (value == "none") {
                style->variables["flex-grow"] = "0";
                style->variables["flex-shrink"] = "0";
            } else if (value == "auto") {
                style->variables["flex-grow"] = "1";
                style->variables["flex-shrink"] = "1";
            } else {
                std::istringstream ss(value);
                std::string grow, shrink, basis;
                ss >> grow;
                if (ss >> shrink) {
                    ss >> basis;
                }
                style->variables["flex-grow"] = grow;
                if (!shrink.empty()) style->variables["flex-shrink"] = shrink;
                if (!basis.empty()) style->variables["flex-basis"] = basis;
            }
        }

        // Typography
        else if (prop == "font-size") style->font_size = parse_length(value);
        else if (prop == "font-family") style->font_family = value;
        else if (prop == "font-weight") {
            if (value == "normal") style->font_weight = FontWeight::Normal;
            else if (value == "bold") style->font_weight = FontWeight::Bold;
            else if (value == "light" || value == "300") style->font_weight = FontWeight::Light;
            else if (value == "medium" || value == "500") style->font_weight = FontWeight::Medium;
            else if (value == "600") style->font_weight = FontWeight::SemiBold;
            else if (value == "800") style->font_weight = FontWeight::ExtraBold;
            else if (value == "900") style->font_weight = FontWeight::Black;
            else style->font_weight = static_cast<FontWeight>(std::stoi(value));
        }
        else if (prop == "font-style") {
            if (value == "normal") style->font_style = FontStyle::Normal;
            else if (value == "italic") style->font_style = FontStyle::Italic;
            else if (value == "oblique") style->font_style = FontStyle::Oblique;
        }
        else if (prop == "text-align") {
            if (value == "left") style->text_align = TextAlign::Left;
            else if (value == "center") style->text_align = TextAlign::Center;
            else if (value == "right") style->text_align = TextAlign::Right;
            else if (value == "justify") style->text_align = TextAlign::Justify;
        }

        // Colors
        else if (prop == "color") style->text_color = parse_color(value);
        else if (prop == "background-color" || prop == "background") {
            // Skip gradients for now
            if (value.find("gradient") == std::string::npos) {
                style->background_color = parse_color(value);
            }
        }
        else if (prop == "border-color") style->border_color = parse_color(value);

        // Visibility & Overflow
        else if (prop == "opacity") style->opacity = std::stof(value);
        else if (prop == "visibility") {
            if (value == "visible") style->visibility = Visibility::Visible;
            else if (value == "hidden") style->visibility = Visibility::Hidden;
            else if (value == "collapse") style->visibility = Visibility::Collapse;
        }
        else if (prop == "overflow") {
            Overflow ov = Overflow::Visible;
            if (value == "hidden") ov = Overflow::Hidden;
            else if (value == "scroll") ov = Overflow::Scroll;
            else if (value == "auto") ov = Overflow::Auto;
            style->overflow_x = style->overflow_y = ov;
        }
        else if (prop == "overflow-x") {
            if (value == "hidden") style->overflow_x = Overflow::Hidden;
            else if (value == "scroll") style->overflow_x = Overflow::Scroll;
            else if (value == "auto") style->overflow_x = Overflow::Auto;
            else style->overflow_x = Overflow::Visible;
        }
        else if (prop == "overflow-y") {
            if (value == "hidden") style->overflow_y = Overflow::Hidden;
            else if (value == "scroll") style->overflow_y = Overflow::Scroll;
            else if (value == "auto") style->overflow_y = Overflow::Auto;
            else style->overflow_y = Overflow::Visible;
        }

        // Transform
        else if (prop == "transform") {
            // Parse transform: translate(x, y) scale(s) rotate(deg)
            if (value.find("translate") != std::string::npos) {
                size_t start = value.find("translate(") + 10;
                size_t end = value.find(")", start);
                std::string inner = value.substr(start, end - start);
                std::istringstream ss(inner);
                float x = 0, y = 0;
                char comma;
                ss >> x >> comma >> y;
                style->transform_x = x;
                style->transform_y = y;
            }
            if (value.find("scale") != std::string::npos) {
                size_t start = value.find("scale(") + 6;
                size_t end = value.find(")", start);
                style->transform_scale = std::stof(value.substr(start, end - start));
            }
            if (value.find("rotate") != std::string::npos) {
                size_t start = value.find("rotate(") + 7;
                size_t end = value.find(")", start);
                std::string deg = value.substr(start, end - start);
                if (deg.find("deg") != std::string::npos) {
                    deg = deg.substr(0, deg.find("deg"));
                }
                style->transform_rotate = std::stof(deg);
            }
        }

        // Effects
        else if (prop == "box-shadow") parse_box_shadow(value, style);
        else if (prop == "transition") style->transition = value;
    }

    float parse_length(const std::string& value) {
        if (value.empty() || value == "auto") return 0;
        std::string num = value;
        // Strip unit suffix
        if (num.size() >= 2 && num.substr(num.size() - 2) == "px") {
            num = num.substr(0, num.size() - 2);
        } else if (num.size() >= 2 && num.substr(num.size() - 2) == "em") {
            num = num.substr(0, num.size() - 2);
            return std::stof(num) * 16.0f;  // Simplified: 1em = 16px
        } else if (num.size() >= 1 && num.back() == '%') {
            // Percentage - store raw value, will be resolved during layout
            num = num.substr(0, num.size() - 1);
            return std::stof(num);  // TODO: proper percentage handling
        }
        return std::stof(num);
    }

    float parse_position_offset(const std::string& value) {
        if (value.empty() || value == "auto") return NAN;
        return parse_length(value);
    }

    Color parse_color(const std::string& value) {
        if (value.empty()) return Color(0, 0, 0, 0);

        // Hex color
        if (value[0] == '#') {
            std::string hex = value.substr(1);
            if (hex.size() == 3) {
                // #RGB -> #RRGGBB
                hex = std::string(2, hex[0]) + std::string(2, hex[1]) + std::string(2, hex[2]);
            }
            if (hex.size() == 6) {
                unsigned int r, g, b;
                sscanf(hex.c_str(), "%2x%2x%2x", &r, &g, &b);
                return Color(r, g, b, 255);
            }
            if (hex.size() == 8) {
                unsigned int r, g, b, a;
                sscanf(hex.c_str(), "%2x%2x%2x%2x", &r, &g, &b, &a);
                return Color(r, g, b, a);
            }
        }

        // rgba(r, g, b, a)
        if (value.find("rgba") == 0) {
            size_t start = value.find('(') + 1;
            size_t end = value.find(')');
            std::string inner = value.substr(start, end - start);
            // Replace commas with spaces for easier parsing
            for (char& c : inner) if (c == ',') c = ' ';
            std::istringstream ss(inner);
            int r, g, b;
            float a;
            ss >> r >> g >> b >> a;
            return Color(r, g, b, static_cast<uint8_t>(a * 255));
        }

        // rgb(r, g, b)
        if (value.find("rgb") == 0) {
            size_t start = value.find('(') + 1;
            size_t end = value.find(')');
            std::string inner = value.substr(start, end - start);
            for (char& c : inner) if (c == ',') c = ' ';
            std::istringstream ss(inner);
            int r, g, b;
            ss >> r >> g >> b;
            return Color(r, g, b, 255);
        }

        // Named colors (common ones)
        if (value == "transparent") return Color(0, 0, 0, 0);
        if (value == "black") return Color(0, 0, 0, 255);
        if (value == "white") return Color(255, 255, 255, 255);
        if (value == "red") return Color(255, 0, 0, 255);
        if (value == "green") return Color(0, 128, 0, 255);
        if (value == "blue") return Color(0, 0, 255, 255);
        if (value == "yellow") return Color(255, 255, 0, 255);
        if (value == "gray" || value == "grey") return Color(128, 128, 128, 255);

        return Color(0, 0, 0, 255);  // Default: black
    }

    void parse_box_shadow(const std::string& value, ComputedStyle* style) {
        // Parse: "offset-x offset-y blur spread color" or "none"
        if (value == "none" || value.empty()) {
            style->has_shadow = false;
            return;
        }

        style->has_shadow = true;
        BoxShadow& shadow = style->shadow;

        // Simple parse: space-separated values
        std::vector<std::string> parts;
        std::string current;
        int paren_depth = 0;

        for (char c : value) {
            if (c == '(') paren_depth++;
            if (c == ')') paren_depth--;
            if (c == ' ' && paren_depth == 0) {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) parts.push_back(current);

        // Parse numeric values and color
        std::vector<float> nums;
        for (const auto& part : parts) {
            if (part.empty()) continue;

            // Check for inset
            if (part == "inset") {
                shadow.inset = true;
                continue;
            }

            // Check for color (rgba format)
            if (part.find("rgba") == 0 || part.find("rgb") == 0 || part[0] == '#') {
                // Parse color - simplified for now
                if (part.find("rgba") == 0) {
                    size_t start = part.find('(') + 1;
                    size_t end = part.find(')');
                    std::string inner = part.substr(start, end - start);
                    std::istringstream ss(inner);
                    int r, g, b;
                    float a;
                    char comma;
                    ss >> r >> comma >> g >> comma >> b >> comma >> a;
                    shadow.color = Color(r, g, b, static_cast<uint8_t>(a * 255));
                }
                continue;
            }

            // Must be a number (possibly with px suffix)
            std::string num = part;
            if (num.size() >= 2 && num.substr(num.size() - 2) == "px") {
                num = num.substr(0, num.size() - 2);
            }
            nums.push_back(std::stof(num));
        }

        // Assign values: offset-x, offset-y, blur, spread
        if (nums.size() >= 1) shadow.offset_x = nums[0];
        if (nums.size() >= 2) shadow.offset_y = nums[1];
        if (nums.size() >= 3) shadow.blur_radius = nums[2];
        if (nums.size() >= 4) shadow.spread_radius = nums[3];
    }

    void parse_box_values(const std::string& value, float out[4]) {
        std::istringstream ss(value);
        std::vector<float> values;
        float v;
        while (ss >> v) values.push_back(v);
        if (values.empty()) return;
        if (values.size() == 1) {
            out[0] = out[1] = out[2] = out[3] = values[0];
        } else if (values.size() == 2) {
            out[0] = out[2] = values[0];
            out[1] = out[3] = values[1];
        } else if (values.size() == 3) {
            out[0] = values[0];
            out[1] = out[3] = values[1];
            out[2] = values[2];
        } else {
            out[0] = values[0];
            out[1] = values[1];
            out[2] = values[2];
            out[3] = values[3];
        }
    }
};

StyleEngine::StyleEngine() : impl_(new Impl()) {}
StyleEngine::~StyleEngine() = default;
void StyleEngine::parse_css(const std::string& css) { impl_->parse_css(css); }
void StyleEngine::apply_styles(Element* elem) { impl_->apply_styles(elem); }
void StyleEngine::clear() { impl_->clear(); }


} // namespace tvgbox2
