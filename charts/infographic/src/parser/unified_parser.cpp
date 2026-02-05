#include <parser/unified_parser.h>
#include <regex>
#include <sstream>
#include <cctype>
#include <iostream>

namespace flex::modules::infographic {

ParseResult UnifiedParser::parse(const std::string& text) {
    try {
        auto tokens = tokenize(text);
        Parser parser(tokens);
        return parser.parse();
    } catch (const std::exception& e) {
        ParseResult result;
        result.success = false;
        result.error = e.what();
        return result;
    }
}

std::vector<UnifiedParser::Token> UnifiedParser::tokenize(const std::string& text) {
    std::vector<Token> tokens;
    
    std::istringstream stream(text);
    std::string line_text;
    int line_number = 1;
    
    while (std::getline(stream, line_text)) {
        // 计算行的缩进级别
        int indent_level = 0;
        size_t first_non_space = 0;
        
        for (size_t i = 0; i < line_text.length(); i++) {
            if (line_text[i] == ' ') {
                indent_level++;
            } else if (line_text[i] == '\t') {
                indent_level += 4;
            } else {
                first_non_space = i;
                break;
            }
        }
        
        // 转换为缩进级别（每2个空格为1级）
        indent_level = indent_level / 2;
        
        // 如果是空行，跳过
        if (first_non_space >= line_text.length()) {
            tokens.push_back({Token::NEWLINE, "\n", line_number, 1, 0});
            line_number++;
            continue;
        }
        
        // 处理这一行的内容
        size_t i = first_non_space;
        int column = static_cast<int>(first_non_space) + 1;
        
        while (i < line_text.length()) {
            char c = line_text[i];
            
            // 跳过空白字符
            if (c == ' ' || c == '\t' || c == '\r') {
                column++;
                i++;
                continue;
            }
            
            // 字符串（带引号）
            if (c == '"' || c == '\'') {
                char quote = c;
                std::string value;
                i++; column++;
                while (i < line_text.length() && line_text[i] != quote) {
                    if (line_text[i] == '\\' && i + 1 < line_text.length()) {
                        i++; column++;
                        value += line_text[i];
                    } else {
                        value += line_text[i];
                    }
                    i++; column++;
                }
                if (i < line_text.length()) { i++; column++; } // 跳过结束引号
                tokens.push_back({Token::STRING, value, line_number, column, indent_level});
                continue;
            }
            
            // 数组项标记
            if (c == '-' && (i + 1 >= line_text.length() || line_text[i + 1] == ' ')) {
                tokens.push_back({Token::DASH, "-", line_number, column, indent_level});
                i++; column++;
                continue;
            }
            
            // 标识符、关键字、数字
            if (std::isalpha(c) || c == '_' || std::isdigit(c)) {
                std::string value;
                int start_column = column;
                
                // 收集字符
                while (i < line_text.length() && (std::isalnum(line_text[i]) || line_text[i] == '_' || line_text[i] == '-' || line_text[i] == '.')) {
                    value += line_text[i];
                    i++; column++;
                }
                
                // 识别 token 类型
                Token::Type type = Token::IDENTIFIER;
                if (value == "infographic") type = Token::INFOGRAPHIC;
                else if (value == "data") type = Token::DATA;
                else if (value == "theme") type = Token::THEME;
                else if (value == "items") type = Token::IDENTIFIER; // items 是普通标识符
                else if (value == "title") type = Token::IDENTIFIER;
                else if (value == "desc") type = Token::IDENTIFIER;
                else if (value == "label") type = Token::IDENTIFIER;
                else if (value == "true" || value == "false") type = Token::BOOLEAN;
                else if (std::regex_match(value, std::regex(R"(\d+(\.\d+)?)"))) type = Token::NUMBER;
                else if (value.find('-') != std::string::npos && value != "true" && value != "false") {
                    // 可能是模板名称
                    type = Token::TEMPLATE_NAME;
                }
                
                tokens.push_back({type, value, line_number, start_column, indent_level});
                continue;
            }
            
            // 未知字符，作为符号处理
            if (c == '+' || c == '%' || c == '$' || c == '@' || c == '#' || c == '&' || 
                c == '*' || c == '!' || c == '?' || c == '=' || c == '<' || c == '>' ||
                c == '/' || c == '\\' || c == '|' || c == ':' || c == ';' || c == ',' ||
                c == '.' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}') {
                tokens.push_back({Token::SYMBOL, std::string(1, c), line_number, column, indent_level});
                i++; column++;
                continue;
            }
            
            // 其他未知字符，跳过
            i++; column++;
        }
        
        // 行结束，添加换行token
        tokens.push_back({Token::NEWLINE, "\n", line_number, column, 0});
        line_number++;
    }
    
    tokens.push_back({Token::END_OF_FILE, "", line_number, 1, 0});
    return tokens;
}

// Parser 实现
UnifiedParser::Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

ParseResult UnifiedParser::Parser::parse() {
    ParseResult result;
    
    try {
        if (!parse_infographic()) {
            result.success = false;
            result.error = "Failed to parse infographic";
            if (current_ < tokens_.size()) {
                result.error_line = current_token().line;
                result.error_column = current_token().column;
            }
            return result;
        }
        
        result.infographic = std::move(infographic_);
        result.success = true;
        return result;
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
        if (current_ < tokens_.size()) {
            result.error_line = current_token().line;
            result.error_column = current_token().column;
        }
        return result;
    }
}

bool UnifiedParser::Parser::parse_infographic() {
    // 跳过前导换行
    while (match(Token::NEWLINE)) {
        advance();
    }
    
    if (!parse_header()) return false;
    
    // 解析主体块
    while (!at_end()) {
        if (match(Token::NEWLINE)) {
            advance();
            continue;
        }
        
        if (match(Token::DATA)) {
            if (!parse_data_block()) return false;
        } else if (match(Token::THEME)) {
            if (!parse_theme_block()) return false;
        } else {
            advance(); // 跳过未知 token
        }
    }
    
    // 验证解析结果
    if (!infographic_->validate()) {
        error(infographic_->get_validation_error());
        return false;
    }
    
    return true;
}

bool UnifiedParser::Parser::parse_header() {
    if (!consume(Token::INFOGRAPHIC)) {
        error("Expected 'infographic' keyword");
        return false;
    }
    
    if (!match(Token::TEMPLATE_NAME) && !match(Token::IDENTIFIER)) {
        error("Expected template name");
        return false;
    }
    
    std::string template_name = current_token().value;
    TemplateType type = identify_template_type(template_name);
    
    infographic_ = create_infographic(type);
    advance();
    
    return true;
}

bool UnifiedParser::Parser::parse_data_block() {
    if (!consume(Token::DATA)) return false;
    
    // 跳过换行
    while (match(Token::NEWLINE)) {
        advance();
    }
    
    // 解析数据字段
    while (!at_end() && !match(Token::THEME) && !match(Token::END_OF_FILE)) {
        if (match(Token::NEWLINE)) {
            advance();
            continue;
        }
        
        // 检查缩进级别 - 数据字段应该在级别 1
        if (current_token().indent_level == 1) {
            if (!parse_data_field()) return false;
        } else if (current_token().indent_level == 0) {
            // 回到顶级，结束数据块
            break;
        } else {
            advance(); // 跳过其他级别的 token
        }
    }
    
    return true;
}

bool UnifiedParser::Parser::parse_data_field() {
    if (!match(Token::IDENTIFIER)) return false;
    
    std::string field_name = current_token().value;
    advance();
    
    if (field_name == "title") {
        std::string title = parse_string_value();
        infographic_->set_title(title);
    } else if (field_name == "desc") {
        std::string desc = parse_string_value();
        infographic_->set_desc(desc);
    } else if (field_name == "items") {
        if (!parse_items_array()) return false;
    } else {
        // 其他字段作为属性存储
        std::string value = parse_string_value();
        infographic_->set_prop(field_name, value);
    }
    
    return true;
}

bool UnifiedParser::Parser::parse_items_array() {
    // 跳过换行
    while (match(Token::NEWLINE)) {
        advance();
    }
    
    // 解析数组项
    while (!at_end() && current_token().indent_level >= 2) {
        if (match(Token::NEWLINE)) {
            advance();
            continue;
        }
        
        if (match(Token::DASH) && current_token().indent_level == 2) {
            if (!parse_item()) {
                return false;
            }
        } else if (current_token().indent_level < 2) {
            break;
        } else {
            advance();
        }
    }
    
    return true;
}

bool UnifiedParser::Parser::parse_item() {
    if (!consume(Token::DASH)) return false;
    
    auto item = DataItem::create("");
    
    // 检查 dash 后面是否有同行的字段（如 "- label Item 1"）
    if (match(Token::IDENTIFIER)) {
        std::string field_name = current_token().value;
        advance();
        
        if (field_name == "label") {
            item->label = parse_string_value();
        } else if (field_name == "desc") {
            item->desc = parse_string_value();
        } else if (field_name == "value") {
            item->value = parse_number_value();
        } else {
            // 其他字段作为属性
            std::string value = parse_string_value();
            item->set_prop(field_name, value);
        }
    }
    
    // 解析后续行的字段（缩进级别 >= 3）
    while (!at_end()) {
        if (match(Token::NEWLINE)) {
            advance();
            continue;
        }
        
        // 停止条件：缩进级别 < 3 或遇到新的 DASH token（新 item）
        if (current_token().indent_level < 3) {
            break;
        }
        
        if (match(Token::DASH) && current_token().indent_level == 2) {
            // 遇到新的 item，停止解析当前 item
            break;
        }
        
        if (current_token().indent_level == 3 && match(Token::IDENTIFIER)) {
            std::string field_name = current_token().value;
            advance();
            
            if (field_name == "label") {
                item->label = parse_string_value();
            } else if (field_name == "desc") {
                item->desc = parse_string_value();
            } else if (field_name == "value") {
                item->value = parse_number_value();
            } else if (field_name == "icon") {
                item->icon = parse_string_value();
            } else if (field_name == "illus") {
                item->illus = parse_string_value();
            } else if (field_name == "time") {
                item->time = parse_string_value();
            } else if (field_name == "done") {
                item->done = parse_boolean_value();
            } else if (field_name == "children") {
                if (!parse_children_array()) return false;
            } else {
                // 其他字段作为属性
                std::string value = parse_string_value();
                item->set_prop(field_name, value);
            }
        } else {
            // 跳过不匹配的 token
            advance();
        }
    }
    
    infographic_->add_item(std::move(item));
    return true;
}

bool UnifiedParser::Parser::parse_children_array() {
    // 类似 parse_items_array，但用于子项
    while (match(Token::NEWLINE)) {
        advance();
    }
    
    // 这里需要更复杂的逻辑来处理嵌套的子项
    // 为了简化，暂时跳过子项解析
    // TODO: 实现完整的嵌套解析
    
    return true;
}

bool UnifiedParser::Parser::parse_theme_block() {
    if (!consume(Token::THEME)) return false;
    
    // 检查是否是预设主题
    if (match(Token::IDENTIFIER)) {
        std::string preset = current_token().value;
        if (preset == "dark") {
            infographic_->set_theme(Theme::dark());
        } else if (preset == "hand-drawn") {
            infographic_->set_theme(Theme::hand_drawn());
        }
        advance();
        return true;
    }
    
    // 解析自定义主题字段
    while (match(Token::NEWLINE)) {
        advance();
    }
    
    while (!at_end() && current_token().indent_level >= 1) {
        if (match(Token::NEWLINE)) {
            advance();
            continue;
        }
        
        if (current_token().indent_level == 1 && match(Token::IDENTIFIER)) {
            if (!parse_theme_field()) return false;
        } else if (current_token().indent_level < 1) {
            break;
        } else {
            advance();
        }
    }
    
    return true;
}

bool UnifiedParser::Parser::parse_theme_field() {
    std::string field_name = current_token().value;
    advance();
    
    if (field_name == "palette") {
        if (!parse_palette()) return false;
    } else if (field_name == "stylize") {
        std::string stylize = parse_string_value();
        infographic_->theme.stylize = stylize;
    } else {
        // 其他主题属性
        std::string value = parse_string_value();
        infographic_->theme.custom_properties[field_name] = value;
    }
    
    return true;
}

bool UnifiedParser::Parser::parse_palette() {
    std::vector<std::string> colors;
    
    // 检查是否是内联格式：# + 颜色值
    if (match(Token::SYMBOL) && current_token().value == "#") {
        // 内联格式：palette #color1 #color2 #color3
        while (match(Token::SYMBOL) && current_token().value == "#") {
            advance(); // 跳过 #
            if (match(Token::IDENTIFIER)) {
                std::string color = "#" + current_token().value;
                colors.push_back(color);
                advance();
            }
        }
    } else {
        // 数组格式
        while (match(Token::NEWLINE)) {
            advance();
        }
        
        while (current_token().indent_level >= 2 && match(Token::DASH)) {
            advance();
            if (match(Token::SYMBOL) && current_token().value == "#") {
                advance(); // 跳过 #
                if (match(Token::IDENTIFIER)) {
                    std::string color = "#" + current_token().value;
                    colors.push_back(color);
                    advance();
                }
            }
        }
    }
    
    infographic_->theme.palette = colors;
    return true;
}

// 辅助方法实现
const UnifiedParser::Token& UnifiedParser::Parser::current_token() const {
    if (current_ >= tokens_.size()) {
        static Token eof_token = {Token::END_OF_FILE, "", 0, 0, 0};
        return eof_token;
    }
    return tokens_[current_];
}

const UnifiedParser::Token& UnifiedParser::Parser::peek_token(size_t offset) const {
    size_t pos = current_ + offset;
    if (pos >= tokens_.size()) {
        static Token eof_token = {Token::END_OF_FILE, "", 0, 0, 0};
        return eof_token;
    }
    return tokens_[pos];
}

bool UnifiedParser::Parser::match(Token::Type type) const {
    return current_token().type == type;
}

bool UnifiedParser::Parser::match_indent(int expected_level) const {
    return current_token().indent_level == expected_level;
}

bool UnifiedParser::Parser::consume(Token::Type type) {
    if (match(type)) {
        advance();
        return true;
    }
    return false;
}

void UnifiedParser::Parser::advance() {
    if (current_ < tokens_.size()) {
        current_++;
    }
}

bool UnifiedParser::Parser::at_end() const {
    return current_ >= tokens_.size() || match(Token::END_OF_FILE);
}

void UnifiedParser::Parser::error(const std::string& message) {
    throw std::runtime_error(message);
}

std::string UnifiedParser::Parser::parse_string_value() {
    std::string result;
    
    // 读取当前行剩余的所有token作为值
    while (!at_end() && !match(Token::NEWLINE) && !match(Token::END_OF_FILE)) {
        if (match(Token::STRING)) {
            if (!result.empty()) result += " ";
            result += current_token().value;
            advance();
        } else if (match(Token::IDENTIFIER) || match(Token::TEMPLATE_NAME) || 
                   match(Token::NUMBER) || match(Token::BOOLEAN)) {
            if (!result.empty()) result += " ";
            result += current_token().value;
            advance();
        } else if (match(Token::SYMBOL)) {
            // 符号字符：前缀符号加空格，后缀符号不加空格
            std::string symbol = current_token().value;
            if ((symbol == "+" || symbol == "-") && !result.empty()) {
                result += " ";
            }
            result += symbol;
            advance();
        } else {
            // 遇到其他类型的token，停止解析
            break;
        }
    }
    
    return result;
}

double UnifiedParser::Parser::parse_number_value() {
    if (match(Token::NUMBER)) {
        double value = std::stod(current_token().value);
        advance();
        return value;
    }
    return 0.0;
}

bool UnifiedParser::Parser::parse_boolean_value() {
    if (match(Token::BOOLEAN)) {
        bool value = current_token().value == "true";
        advance();
        return value;
    }
    return false;
}

std::vector<std::string> UnifiedParser::Parser::parse_string_array() {
    std::vector<std::string> result;
    
    while (match(Token::DASH)) {
        advance();
        if (match(Token::STRING) || match(Token::IDENTIFIER)) {
            result.push_back(current_token().value);
            advance();
        }
    }
    
    return result;
}

TemplateType UnifiedParser::Parser::identify_template_type(const std::string& template_name) {
    return string_to_template_type(template_name);
}

} // namespace flex::modules::infographic