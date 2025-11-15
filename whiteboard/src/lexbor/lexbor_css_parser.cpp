#include <whiteboard/lexbor/lexbor_css_parser.h>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace whiteboard {
namespace lexbor {

// ============================================================================
// LexborCSSParser Implementation
// ============================================================================

bool LexborCSSParser::parse(const std::string& css) {
    clear();
    
    try {
        parser_ = std::make_unique<CSSParser>();
        lxb_css_stylesheet_t* sheet = parser_->parse(css);
        stylesheet_ = std::make_unique<CSSStyleSheet>(sheet);
        return true;
    } catch (const std::exception& e) {
        errors_.push_back(std::string("CSS parsing error: ") + e.what());
        return false;
    }
}

void LexborCSSParser::clear() {
    parser_.reset();
    stylesheet_.reset();
    errors_.clear();
}

// ============================================================================
// LexborSelectorMatcher Implementation
// ============================================================================

bool LexborSelectorMatcher::matches(
    const std::string& selector,
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states) const {
    
    // Check if selector contains combinators
    if (selector.find('>') != std::string::npos ||
        selector.find('+') != std::string::npos ||
        selector.find('~') != std::string::npos ||
        (selector.find(' ') != std::string::npos && selector.find(' ') != selector.find_last_not_of(" \t\n\r") + 1)) {
        // Complex selector with combinators
        // For now, just match the last component (rightmost selector)
        // Full combinator support requires shape hierarchy
        auto complex = parse_complex_selector(selector);
        if (!complex.components.empty()) {
            return matches_simple_selector(complex.components.back(), shape_id, shape_type, classes, attributes, pseudo_states);
        }
        return false;
    }
    
    // Simple selector - parse and match
    auto sel = parse_selector(selector);
    return matches_simple_selector(sel, shape_id, shape_type, classes, attributes, pseudo_states);
}

int LexborSelectorMatcher::calculate_specificity(const std::string& selector) const {
    auto sel = parse_selector(selector);
    
    int specificity = 0;
    
    // ID selector: 100
    if (!sel.id.empty()) {
        specificity += 100;
    }
    
    // Class selectors: 10 each
    specificity += static_cast<int>(sel.classes.size()) * 10;
    
    // Attribute selectors: 10 each
    specificity += static_cast<int>(sel.attributes.size()) * 10;
    
    // Pseudo-classes: 10 each
    specificity += static_cast<int>(sel.pseudo_classes.size()) * 10;
    
    // Type selector: 1
    if (!sel.type.empty() && sel.type != "*") {
        specificity += 1;
    }
    
    return specificity;
}

LexborSelectorMatcher::SelectorComponents 
LexborSelectorMatcher::parse_selector(const std::string& selector) const {
    SelectorComponents result;
    
    std::string trimmed = selector;
    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    size_t i = 0;
    while (i < trimmed.size()) {
        char c = trimmed[i];
        
        if (c == '#') {
            // ID selector
            i++;
            size_t start = i;
            while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_' || trimmed[i] == '-')) {
                i++;
            }
            result.id = trimmed.substr(start, i - start);
        }
        else if (c == '.') {
            // Class selector
            i++;
            size_t start = i;
            while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_' || trimmed[i] == '-')) {
                i++;
            }
            result.classes.push_back(trimmed.substr(start, i - start));
        }
        else if (c == '[') {
            // Attribute selector
            i++;
            size_t start = i;
            while (i < trimmed.size() && trimmed[i] != ']') {
                i++;
            }
            std::string attr_expr = trimmed.substr(start, i - start);
            
            // Trim whitespace from attribute expression
            attr_expr.erase(0, attr_expr.find_first_not_of(" \t"));
            attr_expr.erase(attr_expr.find_last_not_of(" \t") + 1);
            
            // Parse attribute expression with operators
            // Supported: [attr], [attr=value], [attr^=value], [attr$=value], [attr*=value], [attr~=value], [attr|=value]
            std::string attr_name;
            std::string attr_operator = "="; // Default to exact match
            std::string attr_value;
            
            // Check for operators
            size_t op_pos = std::string::npos;
            if ((op_pos = attr_expr.find("^=")) != std::string::npos) {
                attr_operator = "^=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find("$=")) != std::string::npos) {
                attr_operator = "$=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find("*=")) != std::string::npos) {
                attr_operator = "*=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find("~=")) != std::string::npos) {
                attr_operator = "~=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find("|=")) != std::string::npos) {
                attr_operator = "|=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 2);
            } else if ((op_pos = attr_expr.find('=')) != std::string::npos) {
                attr_operator = "=";
                attr_name = attr_expr.substr(0, op_pos);
                attr_value = attr_expr.substr(op_pos + 1);
            } else {
                // Just [attr] - attribute exists
                attr_name = attr_expr;
                attr_operator = "exists";
                attr_value = "";
            }
            
            // Trim attribute name and value
            attr_name.erase(0, attr_name.find_first_not_of(" \t"));
            attr_name.erase(attr_name.find_last_not_of(" \t") + 1);
            attr_value.erase(0, attr_value.find_first_not_of(" \t"));
            attr_value.erase(attr_value.find_last_not_of(" \t") + 1);
            
            // Remove quotes from value
            if (!attr_value.empty() && (attr_value.front() == '"' || attr_value.front() == '\'')) {
                attr_value = attr_value.substr(1, attr_value.size() - 2);
            }
            
            // Store with operator prefix
            result.attributes[attr_operator + ":" + attr_name] = attr_value;
            
            if (i < trimmed.size()) i++; // Skip ]
        }
        else if (c == ':') {
            // Pseudo-class
            i++;
            size_t start = i;
            while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_' || trimmed[i] == '-')) {
                i++;
            }
            result.pseudo_classes.push_back(trimmed.substr(start, i - start));
        }
        else if (std::isalpha(c) || c == '*') {
            // Type selector
            size_t start = i;
            while (i < trimmed.size() && (std::isalnum(trimmed[i]) || trimmed[i] == '_' || trimmed[i] == '-' || trimmed[i] == '*')) {
                i++;
            }
            result.type = trimmed.substr(start, i - start);
        }
        else {
            i++;
        }
    }
    
    return result;
}

LexborSelectorMatcher::ComplexSelector 
LexborSelectorMatcher::parse_complex_selector(const std::string& selector) const {
    ComplexSelector result;
    
    std::string trimmed = selector;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    std::string current_selector;
    size_t i = 0;
    
    while (i < trimmed.size()) {
        char c = trimmed[i];
        
        // Check for combinators
        if (c == '>' || c == '+' || c == '~') {
            // Parse the selector before the combinator
            if (!current_selector.empty()) {
                result.components.push_back(parse_selector(current_selector));
                current_selector.clear();
            }
            result.combinators.push_back(c);
            i++;
            // Skip whitespace after combinator
            while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t')) {
                i++;
            }
        }
        else if (c == ' ' || c == '\t') {
            // Descendant combinator (space)
            // Skip multiple spaces
            while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t')) {
                i++;
            }
            // Only add descendant combinator if there's more content
            if (i < trimmed.size() && !current_selector.empty()) {
                result.components.push_back(parse_selector(current_selector));
                current_selector.clear();
                result.combinators.push_back(' ');
            }
        }
        else {
            current_selector += c;
            i++;
        }
    }
    
    // Add the last selector
    if (!current_selector.empty()) {
        result.components.push_back(parse_selector(current_selector));
    }
    
    return result;
}

bool LexborSelectorMatcher::matches_simple_selector(
    const SelectorComponents& sel,
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states) const {
    
    // Check ID
    if (!sel.id.empty() && sel.id != shape_id) {
        return false;
    }
    
    // Check type
    if (!sel.type.empty() && sel.type != "*" && sel.type != shape_type) {
        return false;
    }
    
    // Check classes
    for (const auto& cls : sel.classes) {
        if (std::find(classes.begin(), classes.end(), cls) == classes.end()) {
            return false;
        }
    }
    
    // Check attributes
    for (const auto& [attr_key, attr_value] : sel.attributes) {
        // Parse operator and attribute name from key (format: "operator:attr_name")
        size_t colon_pos = attr_key.find(':');
        if (colon_pos == std::string::npos) {
            // Old format without operator prefix - treat as exact match
            auto it = attributes.find(attr_key);
            if (it == attributes.end() || it->second != attr_value) {
                return false;
            }
            continue;
        }
        
        std::string op = attr_key.substr(0, colon_pos);
        std::string attr_name = attr_key.substr(colon_pos + 1);
        
        auto it = attributes.find(attr_name);
        
        if (op == "exists") {
            // [attr] - just check if attribute exists
            if (it == attributes.end()) {
                return false;
            }
        } else if (op == "=") {
            // [attr=value] - exact match
            if (it == attributes.end() || it->second != attr_value) {
                return false;
            }
        } else if (op == "^=") {
            // [attr^=value] - starts with
            if (it == attributes.end() || it->second.find(attr_value) != 0) {
                return false;
            }
        } else if (op == "$=") {
            // [attr$=value] - ends with
            if (it == attributes.end() || 
                it->second.size() < attr_value.size() ||
                it->second.substr(it->second.size() - attr_value.size()) != attr_value) {
                return false;
            }
        } else if (op == "*=") {
            // [attr*=value] - contains
            if (it == attributes.end() || it->second.find(attr_value) == std::string::npos) {
                return false;
            }
        } else if (op == "~=") {
            // [attr~=value] - word match (space-separated)
            if (it == attributes.end()) {
                return false;
            }
            // Check if attr_value appears as a complete word in the attribute value
            // Words are separated by spaces
            const std::string& value_str = it->second;
            bool found = false;
            
            // Check if it's the only word
            if (value_str == attr_value) {
                found = true;
            } else {
                // Check if it's at the start followed by space
                if (value_str.find(attr_value + " ") == 0) {
                    found = true;
                }
                // Check if it's at the end preceded by space
                else if (value_str.size() >= attr_value.size() + 1 &&
                         value_str.substr(value_str.size() - attr_value.size() - 1) == " " + attr_value) {
                    found = true;
                }
                // Check if it's in the middle surrounded by spaces
                else if (value_str.find(" " + attr_value + " ") != std::string::npos) {
                    found = true;
                }
            }
            
            if (!found) {
                return false;
            }
        } else if (op == "|=") {
            // [attr|=value] - prefix match (value or value-)
            if (it == attributes.end()) {
                return false;
            }
            if (it->second != attr_value && 
                it->second.find(attr_value + "-") != 0) {
                return false;
            }
        }
    }
    
    // Check pseudo-classes
    for (const auto& pseudo : sel.pseudo_classes) {
        if (pseudo_states.find(pseudo) == pseudo_states.end()) {
            return false;
        }
    }
    
    return true;
}

bool LexborSelectorMatcher::matches_with_hierarchy(
    const std::string& selector,
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states,
    const std::string& parent_shape_id,
    std::function<const void*(const std::string&)> get_shape_func) const {
    
    // Parse complex selector
    auto complex = parse_complex_selector(selector);
    
    // If no combinators, just match simple selector
    if (complex.combinators.empty()) {
        if (!complex.components.empty()) {
            return matches_simple_selector(complex.components[0], shape_id, shape_type, classes, attributes, pseudo_states);
        }
        return false;
    }
    
    // TODO: Implement full combinator matching with shape hierarchy
    // For now, just match the rightmost selector (the target element)
    if (!complex.components.empty()) {
        return matches_simple_selector(complex.components.back(), shape_id, shape_type, classes, attributes, pseudo_states);
    }
    
    return false;
}

// ============================================================================
// LexborStyleComputer Implementation
// ============================================================================

std::map<std::string, std::string> LexborStyleComputer::compute_style(
    lxb_css_stylesheet_t* stylesheet,
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states,
    const std::map<std::string, std::string>& inline_style,
    const std::map<std::string, std::string>& parent_style) const {
    
    // 1. Start with default styles
    std::map<std::string, std::string> result = get_default_styles(shape_type);
    
    // 2. Apply inheritance
    apply_inheritance(result, parent_style);
    
    // 3. Apply stylesheet rules (would need Lexbor API to iterate rules)
    // For now, this is a placeholder - full implementation would iterate
    // through stylesheet rules and apply matching ones
    
    // 4. Apply inline styles (highest priority)
    for (const auto& [key, value] : inline_style) {
        result[key] = value;
    }
    
    return result;
}

bool LexborStyleComputer::is_inheritable(const std::string& property) const {
    static const std::set<std::string> inheritable = {
        "color",
        "font-family",
        "font-size",
        "font-weight",
        "font-style",
        "text-align",
        "line-height",
        "opacity"
    };
    
    return inheritable.count(property) > 0;
}

std::map<std::string, std::string> 
LexborStyleComputer::get_default_styles(const std::string& shape_type) const {
    std::map<std::string, std::string> defaults;
    
    // Common defaults
    defaults["fill"] = "none";
    defaults["stroke"] = "black";
    defaults["stroke-width"] = "1";
    defaults["opacity"] = "1";
    
    // Type-specific defaults
    if (shape_type == "text") {
        defaults["font-family"] = "sans-serif";
        defaults["font-size"] = "14";
        defaults["font-weight"] = "normal";
        defaults["text-align"] = "left";
        defaults["fill"] = "black";
    } else if (shape_type == "rect" || shape_type == "circle" || 
               shape_type == "ellipse" || shape_type == "path") {
        defaults["fill"] = "#cccccc";
    }
    
    return defaults;
}

void LexborStyleComputer::apply_inheritance(
    std::map<std::string, std::string>& result,
    const std::map<std::string, std::string>& parent_style) const {
    
    for (const auto& [key, value] : parent_style) {
        if (is_inheritable(key)) {
            result[key] = value;
        }
    }
}

// ============================================================================
// EnhancedStyleSheet Implementation
// ============================================================================

bool EnhancedStyleSheet::parse_css(const std::string& css) {
    clear_cache();
    return parser_.parse(css);
}

void EnhancedStyleSheet::add_rule(
    const std::string& selector,
    const std::map<std::string, std::string>& properties) {
    
    // Build CSS string from rule
    std::ostringstream css;
    css << selector << " { ";
    for (const auto& [key, value] : properties) {
        css << key << ": " << value << "; ";
    }
    css << "}";
    
    // Parse and add to stylesheet
    parse_css(css.str());
}

std::map<std::string, std::string> EnhancedStyleSheet::compute_style(
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::map<std::string, std::string>& attributes,
    const std::set<std::string>& pseudo_states,
    const std::map<std::string, std::string>& inline_style,
    const std::map<std::string, std::string>& parent_style) {
    
    // Generate cache key
    std::string cache_key = generate_cache_key(shape_id, shape_type, classes, pseudo_states);
    
    // Check cache
    auto it = cache_.find(cache_key);
    if (it != cache_.end()) {
        cache_stats_.hits++;
        
        // Merge with inline styles (not cached)
        auto result = it->second;
        for (const auto& [key, value] : inline_style) {
            result[key] = value;
        }
        return result;
    }
    
    // Cache miss - compute style
    cache_stats_.misses++;
    
    auto result = computer_.compute_style(
        parser_.get_stylesheet(),
        shape_id, shape_type, classes, attributes, pseudo_states,
        inline_style, parent_style
    );
    
    // Store in cache (without inline styles)
    if (cache_.size() >= MAX_CACHE_SIZE) {
        evict_lru();
    }
    
    auto cached_result = result;
    for (const auto& [key, value] : inline_style) {
        cached_result.erase(key);
    }
    cache_[cache_key] = cached_result;
    cache_stats_.size = cache_.size();
    
    return result;
}

void EnhancedStyleSheet::clear_cache() {
    cache_.clear();
    cache_stats_ = CacheStats();
}

std::string EnhancedStyleSheet::generate_cache_key(
    const std::string& shape_id,
    const std::string& shape_type,
    const std::vector<std::string>& classes,
    const std::set<std::string>& pseudo_states) const {
    
    std::ostringstream key;
    key << shape_id << "|" << shape_type << "|";
    
    for (const auto& cls : classes) {
        key << cls << ",";
    }
    key << "|";
    
    for (const auto& state : pseudo_states) {
        key << state << ",";
    }
    
    return key.str();
}

void EnhancedStyleSheet::evict_lru() {
    // Simple eviction: remove first entry
    // TODO: Implement proper LRU with access tracking
    if (!cache_.empty()) {
        cache_.erase(cache_.begin());
    }
}

// ============================================================================
// Calc() Expression Evaluation
// ============================================================================

std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::evaluate_calc(const std::string& expr, float context_value) const {
    // Remove "calc(" prefix and ")" suffix
    std::string trimmed = expr;
    
    // Trim whitespace
    size_t start = trimmed.find_first_not_of(" \t\n\r");
    size_t end = trimmed.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return std::nullopt;
    }
    trimmed = trimmed.substr(start, end - start + 1);
    
    // Check for calc() wrapper
    if (trimmed.substr(0, 5) == "calc(" && trimmed.back() == ')') {
        trimmed = trimmed.substr(5, trimmed.size() - 6);
    }
    
    // Tokenize the expression
    auto tokens = tokenize_calc(trimmed);
    if (tokens.empty()) {
        return std::nullopt;
    }
    
    // Evaluate the tokens
    return evaluate_tokens(tokens, context_value);
}

std::vector<LexborCSSParser::Token> 
LexborCSSParser::tokenize_calc(const std::string& expr) const {
    std::vector<Token> tokens;
    size_t i = 0;
    
    while (i < expr.size()) {
        char c = expr[i];
        
        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }
        
        // Operators
        if (c == '+' || c == '-' || c == '*' || c == '/') {
            Token token;
            token.type = Token::OPERATOR;
            token.op = c;
            tokens.push_back(token);
            i++;
            continue;
        }
        
        // Parentheses
        if (c == '(') {
            Token token;
            token.type = Token::LPAREN;
            tokens.push_back(token);
            i++;
            continue;
        }
        
        if (c == ')') {
            Token token;
            token.type = Token::RPAREN;
            tokens.push_back(token);
            i++;
            continue;
        }
        
        // Numbers
        if (std::isdigit(c) || c == '.') {
            size_t start = i;
            while (i < expr.size() && (std::isdigit(expr[i]) || expr[i] == '.')) {
                i++;
            }
            
            // Parse unit
            std::string unit;
            while (i < expr.size() && std::isalpha(expr[i])) {
                unit += expr[i];
                i++;
            }
            
            // Handle percentage
            if (i < expr.size() && expr[i] == '%') {
                unit = "%";
                i++;
            }
            
            Token token;
            token.type = Token::NUMBER;
            token.value = std::stof(expr.substr(start, i - start));
            token.unit = unit;
            tokens.push_back(token);
            continue;
        }
        
        // Unknown character
        return {};
    }
    
    // Add END token
    Token end_token;
    end_token.type = Token::END;
    tokens.push_back(end_token);
    
    return tokens;
}

std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::evaluate_tokens(const std::vector<Token>& tokens, float context_value) const {
    size_t pos = 0;
    return parse_expression(tokens, pos, context_value);
}

// Parse expression: term (('+' | '-') term)*
std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::parse_expression(const std::vector<Token>& tokens, size_t& pos, float context_value) const {
    auto left = parse_term(tokens, pos, context_value);
    if (!left) {
        return std::nullopt;
    }
    
    while (pos < tokens.size() && tokens[pos].type == Token::OPERATOR &&
           (tokens[pos].op == '+' || tokens[pos].op == '-')) {
        char op = tokens[pos].op;
        pos++;
        
        auto right = parse_term(tokens, pos, context_value);
        if (!right) {
            return std::nullopt;
        }
        
        // Check unit compatibility
        if (!left->unit.empty() && !right->unit.empty() && left->unit != right->unit) {
            return std::nullopt;  // Can't add/subtract different units
        }
        
        // Perform operation
        if (op == '+') {
            left->value += right->value;
        } else {
            left->value -= right->value;
        }
        
        // Keep the unit from the left operand
        if (left->unit.empty() && !right->unit.empty()) {
            left->unit = right->unit;
        }
    }
    
    return left;
}

// Parse term: factor (('*' | '/') factor)*
std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::parse_term(const std::vector<Token>& tokens, size_t& pos, float context_value) const {
    auto left = parse_factor(tokens, pos, context_value);
    if (!left) {
        return std::nullopt;
    }
    
    while (pos < tokens.size() && tokens[pos].type == Token::OPERATOR &&
           (tokens[pos].op == '*' || tokens[pos].op == '/')) {
        char op = tokens[pos].op;
        pos++;
        
        auto right = parse_factor(tokens, pos, context_value);
        if (!right) {
            return std::nullopt;
        }
        
        // Multiplication/division rules:
        // - At least one operand must be unitless
        // - Result takes unit from the other operand
        if (!left->unit.empty() && !right->unit.empty()) {
            return std::nullopt;  // Can't multiply/divide two values with units
        }
        
        if (op == '*') {
            left->value *= right->value;
            if (left->unit.empty() && !right->unit.empty()) {
                left->unit = right->unit;
            }
        } else {
            // Division by zero check
            if (right->value == 0.0f) {
                return std::nullopt;
            }
            left->value /= right->value;
            // For division, left keeps its unit
        }
    }
    
    return left;
}

// Parse factor: NUMBER | '(' expression ')'
std::optional<LexborCSSParser::CalcResult> 
LexborCSSParser::parse_factor(const std::vector<Token>& tokens, size_t& pos, float context_value) const {
    if (pos >= tokens.size()) {
        return std::nullopt;
    }
    
    const Token& token = tokens[pos];
    
    // Number
    if (token.type == Token::NUMBER) {
        pos++;
        CalcResult result;
        result.value = token.value;
        result.unit = token.unit;
        
        // Convert percentage to absolute value if context is provided
        if (result.unit == "%" && context_value != 0.0f) {
            result.value = (result.value / 100.0f) * context_value;
            result.unit = "px";  // Assume px after percentage conversion
        }
        
        return result;
    }
    
    // Parenthesized expression
    if (token.type == Token::LPAREN) {
        pos++;
        auto result = parse_expression(tokens, pos, context_value);
        if (!result) {
            return std::nullopt;
        }
        
        // Expect closing parenthesis
        if (pos >= tokens.size() || tokens[pos].type != Token::RPAREN) {
            return std::nullopt;
        }
        pos++;
        
        return result;
    }
    
    return std::nullopt;
}

// ============================================================================
// CSS Variable Resolver Implementation
// ============================================================================

void CSSVariableResolver::set_variable(const std::string& name, const std::string& value) {
    variables_[name] = value;
}

void CSSVariableResolver::remove_variable(const std::string& name) {
    variables_.erase(name);
}

bool CSSVariableResolver::has_variable(const std::string& name) const {
    if (variables_.count(name) > 0) {
        return true;
    }
    if (parent_) {
        return parent_->has_variable(name);
    }
    return false;
}

std::string CSSVariableResolver::get_variable(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->get_variable(name);
    }
    return "";
}

std::string CSSVariableResolver::resolve(const std::string& value) const {
    std::string result = value;
    size_t pos = 0;
    
    // Find and resolve all var() expressions
    while ((pos = result.find("var(", pos)) != std::string::npos) {
        // Find the matching closing parenthesis
        size_t start = pos;
        size_t paren_count = 1;
        size_t i = pos + 4;  // Skip "var("
        
        while (i < result.size() && paren_count > 0) {
            if (result[i] == '(') {
                paren_count++;
            } else if (result[i] == ')') {
                paren_count--;
            }
            i++;
        }
        
        if (paren_count != 0) {
            // Unmatched parentheses - skip this var()
            pos++;
            continue;
        }
        
        // Extract the var() expression
        std::string var_expr = result.substr(start, i - start);
        
        // Resolve it
        std::string resolved = resolve_var(var_expr);
        
        // Replace in result
        result.replace(start, i - start, resolved);
        
        // Continue from after the replacement
        pos = start + resolved.size();
    }
    
    return result;
}

CSSVariableResolver CSSVariableResolver::create_scope() const {
    return CSSVariableResolver(this);
}

void CSSVariableResolver::clear() {
    variables_.clear();
}

std::string CSSVariableResolver::resolve_var(const std::string& expr) const {
    std::string var_name;
    std::string fallback;
    
    if (!parse_var_expression(expr, var_name, fallback)) {
        // Invalid syntax - return as-is
        return expr;
    }
    
    // Check if variable exists
    if (has_variable(var_name)) {
        return get_variable(var_name);
    }
    
    // Variable doesn't exist - use fallback
    if (!fallback.empty()) {
        // Recursively resolve fallback (it might contain var() too)
        return resolve(fallback);
    }
    
    // No fallback - return empty or the original expression
    return expr;
}

bool CSSVariableResolver::parse_var_expression(
    const std::string& expr, 
    std::string& var_name, 
    std::string& fallback) const {
    
    // Check if it starts with "var("
    if (expr.substr(0, 4) != "var(") {
        return false;
    }
    
    // Check if it ends with ")"
    if (expr.back() != ')') {
        return false;
    }
    
    // Extract content between "var(" and ")"
    std::string content = expr.substr(4, expr.size() - 5);
    
    // Trim whitespace
    size_t start = content.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return false;  // Empty content
    }
    content = content.substr(start);
    
    // Find comma separator (if any)
    size_t comma_pos = std::string::npos;
    int paren_depth = 0;
    
    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '(') {
            paren_depth++;
        } else if (content[i] == ')') {
            paren_depth--;
        } else if (content[i] == ',' && paren_depth == 0) {
            comma_pos = i;
            break;
        }
    }
    
    if (comma_pos != std::string::npos) {
        // Has fallback
        var_name = content.substr(0, comma_pos);
        fallback = content.substr(comma_pos + 1);
        
        // Trim both
        var_name.erase(var_name.find_last_not_of(" \t\n\r") + 1);
        fallback.erase(0, fallback.find_first_not_of(" \t\n\r"));
        fallback.erase(fallback.find_last_not_of(" \t\n\r") + 1);
    } else {
        // No fallback
        var_name = content;
        var_name.erase(var_name.find_last_not_of(" \t\n\r") + 1);
        fallback = "";
    }
    
    // Validate variable name (must start with --)
    if (var_name.size() < 2 || var_name.substr(0, 2) != "--") {
        return false;
    }
    
    return true;
}

} // namespace lexbor
} // namespace whiteboard
