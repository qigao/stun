#pragma once

#include <flex/modules/flexmaid/ir/unified_diagram.h>
#include <memory>
#include <string>
#include <optional>

namespace flex::modules::flexmaid {

// 解析结果
struct ParseResult {
    std::unique_ptr<UnifiedDiagram> diagram;
    bool success = false;
    std::optional<std::string> error;
    int error_line = 0;
    int error_column = 0;
    
    // 便利方法
    bool has_error() const { return !success; }
    std::string get_error() const { return error.value_or("Unknown error"); }
};

// 统一解析器 - 替代复杂的 Lemon 语法文件
class UnifiedParser {
public:
    // 解析 Mermaid 文本
    ParseResult parse(const std::string& text);
    
private:
    // 词法分析
    struct Token {
        enum Type {
            DIAGRAM_TYPE,    // flowchart, classDiagram, etc.
            DIRECTION,       // TD, LR, etc.
            IDENTIFIER,      // node IDs
            STRING,          // quoted strings
            ARROW,           // -->, ---, etc.
            SHAPE_OPEN,      // [, (, {, etc.
            SHAPE_CLOSE,     // ], ), }, etc.
            COLON,           // :
            SEMICOLON,       // ;
            NEWLINE,         // \n
            PIPE,            // |
            SUBGRAPH,        // subgraph
            END,             // end
            STYLING,         // classDef, class, style, etc.
            DIRECTION_KEYWORD, // direction
            // Pie
            PIE, TITLE,
            // GitGraph
            GITGRAPH, COMMIT, BRANCH, CHECKOUT, MERGE,
            END_OF_FILE
        };
        
        Type type;
        std::string value;
        int line = 1;
        int column = 1;
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
        std::unique_ptr<UnifiedDiagram> diagram_;
        std::vector<size_t> subgraph_stack_; // 追踪当前所在的子图层级

        
        // 解析方法
        bool parse_diagram();
        bool parse_header();
        bool parse_statements();
        bool parse_statement();
        bool parse_node();
        bool parse_edge();
        bool parse_subgraph(); // 新增子图解析
        bool parse_styling();  // 新增样式解析
        bool parse_pie_statement(); // 新增饼图解析
        bool parse_gitgraph_statement(); // 新增 GitGraph 解析
        bool parse_property();
        
        // 辅助方法
        const Token& current_token() const;
        const Token& peek_token(size_t offset = 1) const;
        bool match(Token::Type type);
        bool consume(Token::Type type);
        void advance();
        bool at_end() const;
        
        // 错误处理
        void error(const std::string& message);
        
        // 类型识别
        DiagramType identify_diagram_type(const std::string& type_str);
        NodeShape identify_node_shape(const std::string& open, const std::string& close);
        EdgeStyle identify_edge_style(const std::string& arrow);
        EdgeDecoration identify_edge_decoration(const std::string& arrow);
    };
};

} // namespace flex::modules::flexmaid