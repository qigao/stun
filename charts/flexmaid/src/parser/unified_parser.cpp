#include <flex/modules/flexmaid/parser/unified_parser.h>
#include <regex>
#include <sstream>
#include <cctype>
#include <iostream>
namespace flex::modules::flexmaid {

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
    
    int line = 1;
    int column = 1;
    
    for (size_t i = 0; i < text.length(); ) {
        char c = text[i];
        
        // 跳过空白字符（除了换行）
        if (c == ' ' || c == '\t' || c == '\r') {
            if (c == '\t') column += 4;
            else column++;
            i++;
            continue;
        }
        
        // 换行
        if (c == '\n') {
            tokens.push_back({Token::NEWLINE, "\n", line, column});
            line++;
            column = 1;
            i++;
            continue;
        }
        
        // 注释
        if (c == '%' && i + 1 < text.length() && text[i + 1] == '%') {
            // 跳过到行尾
            while (i < text.length() && text[i] != '\n') {
                i++;
                column++;
            }
            continue;
        }
        
        // 字符串
        if (c == '"') {
            std::string value;
            i++; column++;
            while (i < text.length() && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < text.length()) {
                    i++; column++;
                    value += text[i];
                } else {
                    value += text[i];
                }
                i++; column++;
            }
            if (i < text.length()) { i++; column++; } // 跳过结束引号
            tokens.push_back({Token::STRING, value, line, column});
            continue;
        }
        
        // 统一箭头识别逻辑
        if (c == '-' || c == '=' || c == '.') {
            std::string arrow(1, c);
            i++; column++;
            
            // 收集所有可能的箭头字符，直到遇到非箭头字符
            while (i < text.length()) {
                char next = text[i];
                if (next == '-' || next == '=' || next == '.' || 
                    next == '>' || next == 'o' || next == 'x') {
                    arrow += next;
                    i++; column++;
                } else {
                    break;
                }
            }
            
            tokens.push_back({Token::ARROW, arrow, line, column});
            continue;
        }
        
        // 特殊节点标识符处理（如状态图的 [*]）
        if (c == '[' && i + 2 < text.length() && text[i + 1] == '*' && text[i + 2] == ']') {
            tokens.push_back({Token::IDENTIFIER, "[*]", line, column});
            i += 3; column += 3;
            continue;
        }
        
        // 形状开始符号
        if (c == '[' || c == '(' || c == '{') {
            std::string shape(1, c);
            i++; column++;
            
            // 检查多字符形状
            if (i < text.length()) {
                char next = text[i];
                if ((c == '[' && (next == '[' || next == '(' || next == '/' || next == '\\')) ||
                    (c == '(' && (next == '(' || next == '['))) {
                    shape += next;
                    i++; column++;
                    // 检查三字符形状 (((
                    if (c == '(' && next == '(' && i < text.length() && text[i] == '(') {
                        shape += text[i];
                        i++; column++;
                    }
                }
            }
            
            tokens.push_back({Token::SHAPE_OPEN, shape, line, column});
            continue;
        }
        
        // 形状结束符号
        if (c == ']' || c == ')' || c == '}') {
            std::string shape(1, c);
            i++; column++;
            
            // 检查多字符形状
            if (i < text.length()) {
                char next = text[i];
                if ((c == ']' && (next == ']' || next == ')' || next == '/' || next == '\\')) ||
                    (c == ')' && (next == ')' || next == ']'))) {
                    shape += next;
                    i++; column++;
                    // 检查三字符形状 )))
                    if (c == ')' && next == ')' && i < text.length() && text[i] == ')') {
                        shape += text[i];
                        i++; column++;
                    }
                }
            }
            
            tokens.push_back({Token::SHAPE_CLOSE, shape, line, column});
            continue;
        }
        
        // 冒号和分号
        if (c == ':') {
            tokens.push_back({Token::COLON, ":", line, column});
            i++; column++;
            continue;
        }
        
        if (c == ';') {
            tokens.push_back({Token::SEMICOLON, ";", line, column});
            i++; column++;
            continue;
        }
        
        // 管道符号
        if (c == '|') {
            tokens.push_back({Token::PIPE, "|", line, column});
            i++; column++;
            continue;
        }
        
        // 标识符和关键字
        if (std::isalpha(c) || c == '_') {
            std::string identifier;
            while (i < text.length() && (std::isalnum(text[i]) || text[i] == '_' || text[i] == '-')) {
                identifier += text[i];
                i++; column++;
            }
            
            // 检查是否是图表类型
            static const std::vector<std::string> diagram_types = {
                "flowchart", "graph", "sequenceDiagram", "classDiagram", 
                "stateDiagram", "stateDiagram-v2", "erDiagram", "pie",
                "gantt", "timeline", "journey", "mindmap", "gitGraph",
                "xychart", "xychart-beta", "requirementDiagram", 
                "sankey", "sankey-beta"
            };
            
            bool is_diagram_type = std::find(diagram_types.begin(), diagram_types.end(), identifier) != diagram_types.end();
            
            // 检查是否是方向
            static const std::vector<std::string> directions = {"TD", "TB", "LR", "RL", "BT"};
            bool is_direction = std::find(directions.begin(), directions.end(), identifier) != directions.end();
            
            Token::Type type = Token::IDENTIFIER;
            if (is_diagram_type) type = Token::DIAGRAM_TYPE;
            else if (is_direction) type = Token::DIRECTION;
            else if (identifier == "subgraph") type = Token::SUBGRAPH;
            else if (identifier == "end") type = Token::END;
            else if (identifier == "classDef" || identifier == "class" || identifier == "style") type = Token::STYLING;
            else if (identifier == "direction") type = Token::DIRECTION_KEYWORD;
            else if (identifier == "pie") type = Token::PIE;
            else if (identifier == "title") type = Token::TITLE;
            else if (identifier == "gitGraph") type = Token::GITGRAPH;
            else if (identifier == "commit") type = Token::COMMIT;
            else if (identifier == "branch") type = Token::BRANCH;
            else if (identifier == "checkout") type = Token::CHECKOUT;
            else if (identifier == "merge") type = Token::MERGE;
            
            tokens.push_back({type, identifier, line, column});
            continue;
        }
        
        // 数字
        if (std::isdigit(c)) {
            std::string number;
            while (i < text.length() && (std::isdigit(text[i]) || text[i] == '.')) {
                number += text[i];
                i++; column++;
            }
            tokens.push_back({Token::IDENTIFIER, number, line, column}); // 数字作为标识符处理
            continue;
        }
        
        // 未知字符，跳过
        i++; column++;
    }
    
    tokens.push_back({Token::END_OF_FILE, "", line, column});
    return tokens;
}

// Parser 实现
UnifiedParser::Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

ParseResult UnifiedParser::Parser::parse() {
    ParseResult result;
    
    try {
        if (!parse_diagram()) {
            result.success = false;
            result.error = "Failed to parse diagram";
            if (current_ < tokens_.size()) {
                result.error_line = current_token().line;
                result.error_column = current_token().column;
            }
            return result;
        }
        
        result.diagram = std::move(diagram_);
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

bool UnifiedParser::Parser::parse_diagram() {
    if (!parse_header()) return false;
    if (!parse_statements()) return false;
    return true;
}

bool UnifiedParser::Parser::parse_header() {
    // 跳过前导空白和换行
    while (match(Token::NEWLINE)) {
        advance();
    }
    
    if (!match(Token::DIAGRAM_TYPE)) return false;
    
    std::string type_str = current_token().value;
    DiagramType type = identify_diagram_type(type_str);
    
    diagram_ = create_diagram(type);
    advance();
    
    // 可选的方向
    if (match(Token::DIRECTION)) {
        diagram_->set_direction(current_token().value);
        advance();
    }
    
    // 跳过换行
    while (match(Token::NEWLINE)) {
        advance();
    }
    
    return true;
}

bool UnifiedParser::Parser::parse_statements() {
    while (!at_end() && !match(Token::END_OF_FILE)) {
        if (match(Token::NEWLINE) || match(Token::SEMICOLON)) {
            advance();
            continue;
        }
        
        if (!parse_statement()) {
            return false;
        }
    }
    return true;
}

bool UnifiedParser::Parser::parse_statement() {
    // 跳过空白
    while (match(Token::NEWLINE)) {
        advance();
    }
    
    if (at_end()) return true;
    
    // 尝试解析特定关键字
    if (match(Token::DIRECTION_KEYWORD)) {
        advance();
        if (match(Token::DIRECTION)) {
            diagram_->set_direction(current_token().value);
            advance();
        }
        return true;
    }
    
    // 兼容旧的直接写方向 (如 flowchart LR)
    if (match(Token::DIRECTION)) {
        diagram_->set_direction(current_token().value);
        advance();
        return true;
    }
    
    // 尝试解析样式
    if (parse_styling()) return true;
    
    // 尝试解析子图
    if (parse_subgraph()) return true;
    
    // 尝试解析饼图
    if (parse_pie_statement()) return true;
    
    // 尝试解析 GitGraph
    if (parse_gitgraph_statement()) return true;
    
    // 先尝试解析边（因为边包含节点信息）
    if (parse_edge()) return true;
    
    // 如果不是边，尝试解析单独的节点
    if (parse_node()) return true;
    
    // 尝试解析属性
    if (parse_property()) return true;
    
    // 如果都不匹配，报错而不是静默跳过
    error("Unexpected token: " + current_token().value);
    return false;
}

bool UnifiedParser::Parser::parse_node() {
    if (!match(Token::IDENTIFIER) && !match(Token::STRING)) return false;
    
    std::string node_id = current_token().value;
    advance();
    
    // 检查是否有形状定义
    if (match(Token::SHAPE_OPEN)) {
        std::string open = current_token().value;
        advance();
        
        std::string label;
        while (!at_end() && !match(Token::SHAPE_CLOSE)) {
            if (!label.empty()) label += " ";
            label += current_token().value;
            advance();
        }
        
        if (!match(Token::SHAPE_CLOSE)) {
            error("Expected closing shape bracket");
        }
        
        std::string close = current_token().value;
        advance();
        
        NodeShape shape = identify_node_shape(open, close);
        diagram_->ensure_node(node_id, label.empty() ? node_id : label, shape);
    } else {
        diagram_->ensure_node(node_id);
    }

    // 如果在子图中，添加节点ID（去重）
    if (!subgraph_stack_.empty()) {
        auto& sub_nodes = diagram_->subgraphs[subgraph_stack_.back()].node_ids;
        if (std::find(sub_nodes.begin(), sub_nodes.end(), node_id) == sub_nodes.end()) {
            sub_nodes.push_back(node_id);
        }
    }
    
    return true;
}

bool UnifiedParser::Parser::parse_edge() {
    // 保存当前位置
    size_t saved_pos = current_;
    
    // 解析第一个节点
    if (!match(Token::IDENTIFIER) && !match(Token::STRING)) {
        current_ = saved_pos;
        return false;
    }
    
    std::string from = current_token().value;
    advance();
    
    // 检查是否有形状定义
    if (match(Token::SHAPE_OPEN)) {
        std::string open = current_token().value;
        advance();
        
        std::string label;
        while (!at_end() && !match(Token::SHAPE_CLOSE)) {
            if (!label.empty()) label += " ";
            label += current_token().value;
            advance();
        }
        
        if (!match(Token::SHAPE_CLOSE)) {
            current_ = saved_pos;
            return false;
        }
        
        std::string close = current_token().value;
        advance();
        
        NodeShape shape = identify_node_shape(open, close);
        diagram_->ensure_node(from, label.empty() ? from : label, shape);
    } else {
        diagram_->ensure_node(from);
    }
    
    if (!subgraph_stack_.empty()) {
        auto& sub_nodes = diagram_->subgraphs[subgraph_stack_.back()].node_ids;
        if (std::find(sub_nodes.begin(), sub_nodes.end(), from) == sub_nodes.end()) {
            sub_nodes.push_back(from);
        }
    }
    
    // 解析箭头
    if (!match(Token::ARROW)) {
        current_ = saved_pos;
        return false;
    }
    
    std::string arrow = current_token().value;
    advance();
    
    // 检查是否有中间标签 (如 -->|Label| B)
    std::string edge_label;
    if (match(Token::PIPE)) {
        advance();
        while (!at_end() && !match(Token::PIPE)) {
            if (!edge_label.empty()) edge_label += " ";
            edge_label += current_token().value;
            advance();
        }
        consume(Token::PIPE);
    }
    
    // 解析第二个节点
    if (!match(Token::IDENTIFIER) && !match(Token::STRING)) {
        current_ = saved_pos;
        return false;
    }
    
    std::string to = current_token().value;
    advance();
    
    // 检查第二个节点是否有形状定义
    if (match(Token::SHAPE_OPEN)) {
        std::string open = current_token().value;
        advance();
        
        std::string label;
        while (!at_end() && !match(Token::SHAPE_CLOSE)) {
            if (!label.empty()) label += " ";
            label += current_token().value;
            advance();
        }
        
        if (!match(Token::SHAPE_CLOSE)) {
            current_ = saved_pos;
            return false;
        }
        
        std::string close = current_token().value;
        advance();
        
        NodeShape shape = identify_node_shape(open, close);
        diagram_->ensure_node(to, label.empty() ? to : label, shape);
    } else {
        diagram_->ensure_node(to);
    }
    
    if (!subgraph_stack_.empty()) {
        auto& sub_nodes = diagram_->subgraphs[subgraph_stack_.back()].node_ids;
        if (std::find(sub_nodes.begin(), sub_nodes.end(), to) == sub_nodes.end()) {
            sub_nodes.push_back(to);
        }
    }
    
    // 创建边
    Edge edge;
    edge.from = from;
    edge.to = to;
    edge.style = identify_edge_style(arrow);
    edge.end_decoration = identify_edge_decoration(arrow);
    edge.label = edge_label;
    
    // 检查是否有消息标签（序列图语法：Alice --> Bob: Hello）
    if (match(Token::COLON)) {
        advance();
        std::string colon_label;
        while (!at_end() && !match(Token::NEWLINE) && !match(Token::SEMICOLON)) {
            if (!colon_label.empty()) colon_label += " ";
            colon_label += current_token().value;
            advance();
        }
        if (edge.label.empty()) {
            edge.label = colon_label;
        } else {
            edge.label += " " + colon_label;
        }
    }
    
    diagram_->edges.push_back(std::move(edge));
    return true;
}

bool UnifiedParser::Parser::parse_subgraph() {
    if (!match(Token::SUBGRAPH)) return false;
    advance();
    
    std::string sub_id;
    std::string sub_label;
    
    // subgraph id [label] or just subgraph label
    if (match(Token::IDENTIFIER) || match(Token::STRING)) {
        sub_id = current_token().value;
        sub_label = sub_id;
        advance();
        
        if (match(Token::SHAPE_OPEN) && current_token().value == "[") {
            advance();
            sub_label = "";
            while (!at_end() && !match(Token::SHAPE_CLOSE)) {
                if (!sub_label.empty()) sub_label += " ";
                sub_label += current_token().value;
                advance();
            }
            if (!consume(Token::SHAPE_CLOSE)) {
                error("Expected ']' after subgraph label");
            }
        }
    }
    
    // 创建子图
    Subgraph sub;
    sub.id = sub_id;
    sub.label = sub_label;
    diagram_->subgraphs.push_back(std::move(sub));
    
    // 进入子图
    subgraph_stack_.push_back(diagram_->subgraphs.size() - 1);
    
    // 解析子图内部语句
    while (!at_end() && !match(Token::END)) {
        if (match(Token::NEWLINE) || match(Token::SEMICOLON)) {
            advance();
            continue;
        }
        
        // 特殊处理子图内的方向
        if (match(Token::DIRECTION_KEYWORD)) {
            advance();
            if (match(Token::DIRECTION)) {
                diagram_->subgraphs[subgraph_stack_.back()].props["direction"] = current_token().value;
                advance();
            }
            continue;
        }
        
        if (match(Token::DIRECTION)) {
            diagram_->subgraphs[subgraph_stack_.back()].props["direction"] = current_token().value;
            advance();
            continue;
        }
        
        if (!parse_statement()) break;
    }
    
    if (!match(Token::END)) {
        error("Expected 'end' at the end of subgraph");
    }
    advance();
    
    // 退出子图
    subgraph_stack_.pop_back();
    
    return true;
}

bool UnifiedParser::Parser::parse_styling() {
    if (!match(Token::STYLING)) return false;
    
    std::string type = current_token().value;
    advance();
    
    // 简单的样式收集：将所有内容直到换行收集为属性或特殊指令
    // 在真实实现中，这里应该解析具体的 CSS 属性
    std::string styling_content;
    while (!at_end() && !match(Token::NEWLINE) && !match(Token::SEMICOLON)) {
        styling_content += current_token().value + " ";
        advance();
    }
    
    // 存储到图表属性中，供渲染器使用
    // 使用特殊的前缀以区分普通属性
    static int style_count = 0;
    diagram_->set_prop("style_" + std::to_string(style_count++), type + " " + styling_content);
    
    return true;
}

bool UnifiedParser::Parser::parse_pie_statement() {
    if (diagram_->type != DiagramType::Pie) return false;
    
    // title "..."
    if (match(Token::TITLE)) {
        advance();
        if (match(Token::STRING) || match(Token::IDENTIFIER)) {
            diagram_->set_title(current_token().value);
            advance();
        }
        return true;
    }
    
    // "Label" : Value
    if (match(Token::STRING) || match(Token::IDENTIFIER)) {
        std::string label = current_token().value;
        advance();
        
        if (match(Token::COLON)) {
            advance();
            if (match(Token::IDENTIFIER)) { // 数字也被标记为标识符
                diagram_->set_prop("data_" + label, current_token().value);
                advance();
                return true;
            }
        }
    }
    
    return false;
}

bool UnifiedParser::Parser::parse_gitgraph_statement() {
    if (diagram_->type != DiagramType::GitGraph) return false;
    
    if (match(Token::COMMIT)) {
        advance();
        // 简单处理：记录一个 commit 节点
        static int commit_count = 0;
        std::string commit_id = "commit_" + std::to_string(commit_count++);
        diagram_->add_node(commit_id, "Commit", NodeShape::Circle);
        
        // 如果有前一个 commit，连线
        if (commit_count > 1) {
            diagram_->add_edge("commit_" + std::to_string(commit_count - 2), commit_id);
        }
        return true;
    }
    
    if (match(Token::BRANCH) || match(Token::CHECKOUT) || match(Token::MERGE)) {
        std::string op = current_token().value;
        advance();
        if (match(Token::IDENTIFIER)) {
            // 解析操作
            advance();
        }
        return true;
    }
    
    return false;
}

bool UnifiedParser::Parser::parse_property() {
    if (!match(Token::IDENTIFIER)) return false;
    
    std::string key = current_token().value;
    advance();
    
    if (!match(Token::COLON)) return false;
    advance();
    
    if (!match(Token::STRING) && !match(Token::IDENTIFIER)) return false;
    
    std::string value = current_token().value;
    advance();
    
    diagram_->set_prop(key, value);
    return true;
}

// 辅助方法实现
const UnifiedParser::Token& UnifiedParser::Parser::current_token() const {
    if (current_ >= tokens_.size()) {
        static Token eof_token = {Token::END_OF_FILE, "", 0, 0};
        return eof_token;
    }
    return tokens_[current_];
}

const UnifiedParser::Token& UnifiedParser::Parser::peek_token(size_t offset) const {
    size_t pos = current_ + offset;
    if (pos >= tokens_.size()) {
        static Token eof_token = {Token::END_OF_FILE, "", 0, 0};
        return eof_token;
    }
    return tokens_[pos];
}

bool UnifiedParser::Parser::match(Token::Type type) {
    return current_token().type == type;
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
    return current_ >= tokens_.size() || 
           (current_ < tokens_.size() && tokens_[current_].type == Token::END_OF_FILE);
}

void UnifiedParser::Parser::error(const std::string& message) {
    throw std::runtime_error(message);
}

DiagramType UnifiedParser::Parser::identify_diagram_type(const std::string& type_str) {
    return string_to_diagram_type(type_str);
}

NodeShape UnifiedParser::Parser::identify_node_shape(const std::string& open, const std::string& close) {
    if (open == "[" && close == "]") return NodeShape::Rectangle;
    if (open == "(" && close == ")") return NodeShape::RoundRect;
    if (open == "((" && close == "))") return NodeShape::Circle;
    if (open == "(((" && close == ")))") return NodeShape::DoubleCircle;
    if (open == "{" && close == "}") return NodeShape::Diamond;
    if (open == "([" && close == "])") return NodeShape::Stadium;
    if (open == "[[" && close == "]]") return NodeShape::Subroutine;
    if (open == "[(" && close == ")]") return NodeShape::Cylinder;
    if (open == "[/" && (close == "/]" || close == "/")) return NodeShape::Parallelogram;
    if (open == "[\\" && (close == "\\]" || close == "\\")) return NodeShape::ParallelogramAlt;
    if (open == "[/" && (close == "\\]" || close == "\\")) return NodeShape::Trapezoid;
    if (open == "[\\" && (close == "/]" || close == "/")) return NodeShape::TrapezoidAlt;
    
    return NodeShape::Rectangle; // 默认
}

EdgeStyle UnifiedParser::Parser::identify_edge_style(const std::string& arrow) {
    if (arrow.find("-.") != std::string::npos || arrow.find("..") != std::string::npos) {
        return EdgeStyle::Dotted;
    }
    if (arrow.find("==") != std::string::npos) {
        return EdgeStyle::Thick;
    }
    return EdgeStyle::Solid;
}

EdgeDecoration UnifiedParser::Parser::identify_edge_decoration(const std::string& arrow) {
    if (arrow.find(">") != std::string::npos) return EdgeDecoration::Arrow;
    if (arrow.find("o") != std::string::npos) return EdgeDecoration::Circle;
    if (arrow.find("x") != std::string::npos) return EdgeDecoration::Cross;
    return EdgeDecoration::None;
}

} // namespace flex::modules::flexmaid