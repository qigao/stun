#pragma once

#include <ir/unified_infographic.h>
#include <memory>
#include <string>
#include <optional>

namespace flex::modules::infographic {

// 解析结果
struct ParseResult {
    std::unique_ptr<UnifiedInfographic> infographic;
    bool success = false;
    std::optional<std::string> error;
    int error_line = 0;
    int error_column = 0;
    
    // 便利方法
    bool has_error() const { return !success; }
    std::string get_error() const { return error.value_or("Unknown error"); }
};

// 统一解析器 - 基于 FlexMaid 的"好品味"设计
class UnifiedParser {
public:
    // 解析 infographic 文本
    ParseResult parse(const std::string& text);
    
private:
    // 词法分析
    struct Token {
        enum Type {
            INFOGRAPHIC,     // infographic keyword
            TEMPLATE_NAME,   // template name
            DATA,            // data keyword
            THEME,           // theme keyword
            IDENTIFIER,      // field names, values
            STRING,          // quoted strings
            NUMBER,          // numeric values
            BOOLEAN,         // true/false
            SYMBOL,          // special characters like +, %, $, @, etc.
            DASH,            // - for array items
            NEWLINE,         // \n
            END_OF_FILE
        };
        
        Type type;
        std::string value;
        int line = 1;
        int column = 1;
        int indent_level = 0;  // Track indentation for YAML-like parsing
    };
    
    std::vector<Token> tokenize(const std::string& text);
    
    // 语法分析
    class Parser {
    public:
        Parser(const std::vector<Token>& tokens);
        ParseResult parse();
        
    private:
        const std::vector<Token>& tokens_;
        size_t current_ = 0;
        std::unique_ptr<UnifiedInfographic> infographic_;
        
        // 解析方法
        bool parse_infographic();
        bool parse_header();
        bool parse_data_block();
        bool parse_theme_block();
        bool parse_data_field();
        bool parse_items_array();
        bool parse_item();
        std::unique_ptr<DataItem> parse_item_at_indent(int item_indent);
        bool parse_item_field(DataItem& item, int item_indent);
        bool parse_children_array(DataItem& parent, int child_indent);
        bool parse_theme_field();
        bool parse_palette();
        
        // 辅助方法
        const Token& current_token() const;
        const Token& peek_token(size_t offset = 1) const;
        bool match(Token::Type type) const;
        bool match_indent(int expected_level) const;
        bool consume(Token::Type type);
        void advance();
        bool at_end() const;
        
        // 错误处理
        void error(const std::string& message);
        
        // 值解析
        std::string parse_string_value();
        double parse_number_value();
        bool parse_boolean_value();
        std::vector<std::string> parse_string_array();
        
        // 模板识别
        TemplateType identify_template_type(const std::string& template_name);
    };
};

} // namespace flex::modules::infographic
