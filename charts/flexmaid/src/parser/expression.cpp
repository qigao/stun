#include <flexmaid/parser/expression.h>
#include <algorithm>
#include <string>
#include <vector>
#include <iostream>
namespace flex::modules::flexmaid {

// ParseContext 实现
const Token& ParseContext::current() const {
    static Token eof{Token::END_OF_FILE, "", 0, 0};
    return pos < tokens->size() ? (*tokens)[pos] : eof;
}

const Token& ParseContext::peek(size_t offset) const {
    static Token eof{Token::END_OF_FILE, "", 0, 0};
    return (pos + offset) < tokens->size() ? (*tokens)[pos + offset] : eof;
}

bool ParseContext::match(Token::Type type) const { return current().type == type; }
bool ParseContext::match_value(const std::string& val) const { return current().value == val; }
void ParseContext::advance() { if (pos < tokens->size()) pos++; }
bool ParseContext::at_end() const { return pos >= tokens->size() || current().type == Token::END_OF_FILE; }
void ParseContext::skip_newlines() { while (match(Token::NEWLINE)) advance(); }

// NodeExpr
bool NodeExpr::interpret(ParseContext& ctx) {
    if (!ctx.match(Token::IDENTIFIER)) return false;
    
    std::string id = ctx.current().value;
    size_t saved = ctx.pos;
    ctx.advance();
    
    // 检查是否有形状定义 [label] 或 (label) 等
    std::string label = id;
    NodeShape shape = NodeShape::Rectangle;
    
    if (ctx.match(Token::SHAPE_OPEN)) {
        std::string open = ctx.current().value;
        ctx.advance();
        
        label.clear();
        while (!ctx.at_end() && !ctx.match(Token::SHAPE_CLOSE)) {
            if (!label.empty()) label += " ";
            label += ctx.current().value;
            ctx.advance();
        }
        
        if (ctx.match(Token::SHAPE_CLOSE)) {
            std::string close = ctx.current().value;
            ctx.advance();
            
            // 识别形状
            if (open == "[" && close == "]") shape = NodeShape::Rectangle;
            else if (open == "(" && close == ")") shape = NodeShape::RoundRect;
            else if (open == "{" && close == "}") shape = NodeShape::Diamond;
            else if (open == "[[" && close == "]]") shape = NodeShape::Subroutine;
            else if (open == "((" && close == "))") shape = NodeShape::Circle;
            else if (open == "([" && close == "])") shape = NodeShape::Stadium;
            else if (open == "[(" && close == ")]") shape = NodeShape::Cylinder;
        }
    }
    
    // 如果后面紧跟箭头，这不是纯节点定义，回退
    if (ctx.match(Token::ARROW)) {
        ctx.pos = saved;
        return false;
    }
    
    ctx.diagram->ensure_node(id, label.empty() ? id : label, shape);
    
    if (!ctx.subgraph_stack.empty()) {
        auto& nodes = ctx.diagram->subgraphs[ctx.subgraph_stack.back()].node_ids;
        if (std::find(nodes.begin(), nodes.end(), id) == nodes.end())
            nodes.push_back(id);
    }
    
    return true;
}

// EdgeExpr
bool EdgeExpr::interpret(ParseContext& ctx) {
    if (!ctx.match(Token::IDENTIFIER)) return false;
    
    std::string from = ctx.current().value;
    size_t saved = ctx.pos;
    ctx.advance();
    
    // 跳过可能的端口定义 :port
    if (ctx.match(Token::COLON)) {
        size_t p = ctx.pos;
        ctx.advance();
        if (ctx.match(Token::IDENTIFIER)) ctx.advance();
        else ctx.pos = p;
    }
    
    std::string from_multiplicity;
    // 跳过可能的形状定义或基数 "1"
    while (ctx.match(Token::SHAPE_OPEN) || ctx.match(Token::STRING)) {
        if (ctx.match(Token::SHAPE_OPEN)) {
            while (!ctx.at_end() && !ctx.match(Token::SHAPE_CLOSE)) ctx.advance();
            if (ctx.match(Token::SHAPE_CLOSE)) ctx.advance();
        } else {
            from_multiplicity = ctx.current().value;
            ctx.advance(); // Skip multiplicity string
        }
    }
    
    if (!ctx.match(Token::ARROW)) {
        ctx.pos = saved;
        return false;
    }
    
    ctx.diagram->ensure_node(from);
    if (!ctx.subgraph_stack.empty()) {
        auto& nodes = ctx.diagram->subgraphs[ctx.subgraph_stack.back()].node_ids;
        if (std::find(nodes.begin(), nodes.end(), from) == nodes.end())
            nodes.push_back(from);
    }
    
    std::vector<std::string> chain = {from};
    
    while (ctx.match(Token::ARROW)) {
        std::string arrow = ctx.current().value;
        ctx.advance();
        
        // 边标签 |label| 或基数 "many"
        std::string edge_label;
        std::string to_multiplicity;
        
        while (ctx.match(Token::PIPE) || ctx.match(Token::STRING) || ctx.match(Token::COLON)) {
            if (ctx.match(Token::PIPE)) {
                ctx.advance();
                while (!ctx.at_end() && !ctx.match(Token::PIPE)) {
                    if (!edge_label.empty()) edge_label += " ";
                    edge_label += ctx.current().value;
                    ctx.advance();
                }
                if (ctx.match(Token::PIPE)) ctx.advance();
            } else if (ctx.match(Token::STRING)) {
                to_multiplicity = ctx.current().value;
                ctx.advance();
            } else if (ctx.match(Token::COLON)) {
                // Handle :text label at the end of edge
                size_t p = ctx.pos;
                ctx.advance();
                // Check if it's a port identifier:port
                if (ctx.match(Token::IDENTIFIER) && !ctx.peek(1).type == Token::IDENTIFIER && !ctx.peek(1).type == Token::STRING) {
                     // likely a label, but wait, if there's no arrow after it might be a label.
                     // simplified: just treat as label if it's the last thing on the line
                }
                ctx.pos = p; // fallback for now
                ctx.advance();
                while (!ctx.at_end() && !ctx.match(Token::NEWLINE) && !ctx.match(Token::SEMICOLON)) {
                    if (!edge_label.empty()) edge_label += " ";
                    edge_label += ctx.current().value;
                    ctx.advance();
                }
                break; // Colon label is usually terminal
            }
        }
        
        if (!ctx.match(Token::IDENTIFIER)) break;
        
        std::string to = ctx.current().value;
        ctx.advance();
        
        // 目标节点形状
        std::string to_label = to;
        NodeShape to_shape = NodeShape::Rectangle;
        if (ctx.match(Token::SHAPE_OPEN)) {
            std::string open = ctx.current().value;
            ctx.advance();
            to_label.clear();
            while (!ctx.at_end() && !ctx.match(Token::SHAPE_CLOSE)) {
                if (!to_label.empty()) to_label += " ";
                to_label += ctx.current().value;
                ctx.advance();
            }
            if (ctx.match(Token::SHAPE_CLOSE)) {
                std::string close = ctx.current().value;
                ctx.advance();
                if (open == "[" && close == "]") to_shape = NodeShape::Rectangle;
                else if (open == "(" && close == ")") to_shape = NodeShape::RoundRect;
                else if (open == "{" && close == "}") to_shape = NodeShape::Diamond;
            }
        }
        
        ctx.diagram->ensure_node(to, to_label.empty() ? to : to_label, to_shape);
        if (!ctx.subgraph_stack.empty()) {
            auto& nodes = ctx.diagram->subgraphs[ctx.subgraph_stack.back()].node_ids;
            if (std::find(nodes.begin(), nodes.end(), to) == nodes.end())
                nodes.push_back(to);
        }

        // Handle : label at the end (common in Class diagrams)
        // DEBUG: Check token state
        // std::cout << "DEBUG: After node " << to << ", current token: " << ctx.current().value << " type: " << (int)ctx.current().type << std::endl;
        
        if (ctx.match(Token::COLON)) {
            ctx.advance(); // Skip :
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE) && !ctx.match(Token::SEMICOLON)) {
                 // Stop if we see an arrow to theoretically support chaining, though rare with colon labels
                if (ctx.match(Token::ARROW)) break;

                if (!edge_label.empty()) edge_label += " ";
                edge_label += ctx.current().value;
                ctx.advance();
            }
        }
        
        // std::cout << "DEBUG: Final edge_label: '" << edge_label << "'" << std::endl;

        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\n\r"));
            s.erase(s.find_last_not_of(" \t\n\r") + 1);
        };
        trim(edge_label);

        Edge edge;
        edge.from = chain.back();
        edge.to = to;
        edge.label = edge_label;
        edge.style = identify_style(arrow);
        
        // Determine decoration placement based on arrow direction
        // Left side markers: <|, <, o--, *-- etc. go to start_decoration
        // Right side markers: |>, >, --o, --* etc. go to end_decoration
        EdgeDecoration dec = identify_decoration(arrow);
        bool has_left_marker = (arrow.find("<") != std::string::npos) || 
                               (arrow.length() > 0 && (arrow[0] == 'o' || arrow[0] == '*'));
        bool has_right_marker = (arrow.find(">") != std::string::npos) ||
                                (arrow.length() > 0 && (arrow.back() == 'o' || arrow.back() == '*'));
        
        if (has_left_marker && !has_right_marker) {
            edge.start_decoration = dec;
        } else if (has_right_marker && !has_left_marker) {
            edge.end_decoration = dec;
        } else if (has_left_marker && has_right_marker) {
            // Bidirectional: both ends get decoration
            edge.start_decoration = dec;
            edge.end_decoration = dec;
        } else {
            // Default: end decoration (for plain arrows like --)
            edge.end_decoration = dec;
        }
        
        if (!from_multiplicity.empty()) edge.set_prop("from_multiplicity", from_multiplicity);
        if (!to_multiplicity.empty()) edge.set_prop("to_multiplicity", to_multiplicity);
        ctx.diagram->edges.push_back(std::move(edge));
        
        chain.push_back(to);
        from_multiplicity.clear(); // Clear for next in chain if any
    }
    
    return chain.size() > 1;
}

EdgeStyle EdgeExpr::identify_style(const std::string& arrow) {
    if (arrow.find("..") != std::string::npos || arrow.find("-.") != std::string::npos)
        return EdgeStyle::Dashed;
    if (arrow.find("==") != std::string::npos)
        return EdgeStyle::Thick;
    return EdgeStyle::Solid;
}

EdgeDecoration EdgeExpr::identify_decoration(const std::string& arrow) {
    if (arrow.find(">") != std::string::npos) return EdgeDecoration::Arrow;
    if (arrow.find("x") != std::string::npos) return EdgeDecoration::Cross;
    if (arrow.find("o") != std::string::npos) return EdgeDecoration::Circle;
    return EdgeDecoration::None;
}

// SubgraphExpr
bool SubgraphExpr::interpret(ParseContext& ctx) {
    if (!ctx.match(Token::SUBGRAPH)) return false;
    ctx.advance();
    
    std::string id, label;
    if (ctx.match(Token::IDENTIFIER)) {
        id = ctx.current().value;
        ctx.advance();
    }
    
    if (ctx.match(Token::SHAPE_OPEN) && ctx.current().value == "[") {
        ctx.advance();
        while (!ctx.at_end() && !ctx.match(Token::SHAPE_CLOSE)) {
            if (!label.empty()) label += " ";
            label += ctx.current().value;
            ctx.advance();
        }
        if (ctx.match(Token::SHAPE_CLOSE)) ctx.advance();
    }
    
    if (label.empty()) label = id;
    if (id.empty()) id = "subgraph_" + std::to_string(ctx.diagram->subgraphs.size());
    
    Subgraph sg;
    sg.id = id;
    sg.label = label;
    size_t idx = ctx.diagram->subgraphs.size();
    ctx.diagram->subgraphs.push_back(std::move(sg));
    ctx.subgraph_stack.push_back(idx);
    
    return true;
}

// StatementsExpr
void StatementsExpr::add(std::unique_ptr<IExpression> expr) {
    exprs_.push_back(std::move(expr));
}

bool StatementsExpr::interpret(ParseContext& ctx) {
    while (!ctx.at_end()) {
        ctx.skip_newlines();
        if (ctx.at_end()) break;
        
        // 检查 end 关键字或 } 闭合
        if (ctx.match(Token::END) || (ctx.match(Token::SHAPE_CLOSE) && ctx.current().value == "}")) {
            ctx.advance();
            if (!ctx.subgraph_stack.empty())
                ctx.subgraph_stack.pop_back();
            continue;
        }
        
        bool matched = false;
        for (auto& expr : exprs_) {
            size_t before = ctx.pos;
            if (expr->interpret(ctx)) {
                matched = true;
                break;
            }
            ctx.pos = before;
        }
        
        if (!matched) ctx.advance();
    }
    return true;
}

} // namespace flex::modules::flexmaid
