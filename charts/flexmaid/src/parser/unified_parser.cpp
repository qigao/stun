#include <parser/unified_parser.h>
#include <parser/expression.h>
#include <parser/diagram_interpreter.h>
#include <algorithm>
#include <cctype>

namespace flex::modules::flexmaid {

UnifiedParser::UnifiedParser() : interpreter_(std::make_unique<DiagramInterpreter>()) {}
UnifiedParser::~UnifiedParser() = default;

ParseResult UnifiedParser::parse(const std::string& text) {
    try {
        auto tokens = tokenize(text);
        return interpreter_->interpret(tokens);
    } catch (const std::exception& e) {
        ParseResult result;
        result.success = false;
        result.error = e.what();
        return result;
    }
}

std::vector<Token> UnifiedParser::tokenize(const std::string& text) {
    std::vector<Token> tokens;
    int line = 1, column = 1;
    
    for (size_t i = 0; i < text.length(); ) {
        char c = text[i];
        
        if (c == ' ' || c == '\t' || c == '\r') {
            column += (c == '\t') ? 4 : 1;
            i++;
            continue;
        }
        
        if (c == '\n') {
            tokens.push_back({Token::NEWLINE, "\n", line, column});
            line++; column = 1; i++;
            continue;
        }
        
        if (c == '%' && i + 1 < text.length() && text[i + 1] == '%') {
            while (i < text.length() && text[i] != '\n') { i++; column++; }
            continue;
        }
        
        if (c == '"') {
            std::string value;
            i++; column++;
            while (i < text.length() && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < text.length()) { i++; column++; }
                value += text[i]; i++; column++;
            }
            if (i < text.length()) { i++; column++; }
            tokens.push_back({Token::STRING, value, line, column});
            continue;
        }
        
        // Arrow characters
        if (c == '-' || c == '=' || c == '.' || c == '~' || c == '<' || c == 'x' || c == 'o' || c == '+' || c == '*') {
            std::string arrow(1, c);
            int start_col = column;
            i++; column++;
            
            while (i < text.length()) {
                char next = text[i];
                if (next == '-' || next == '=' || next == '.' || next == '~' ||
                    next == '>' || next == '<' || next == 'o' || next == 'x' || next == '+' || next == '*' || next == '|') {
                    if (next == '|') {
                        if (i + 1 < text.length()) {
                            char after = text[i+1];
                            if (after == '-' || after == '.' || after == '=') {
                                arrow += next; i++; column++;
                                continue;
                            }
                        }
                        break;
                    }
                    arrow += next; i++; column++;
                } else break;
            }
            tokens.push_back({Token::ARROW, arrow, line, start_col});
            continue;
        }
        
        // Pipe |
        if (c == '|') {
            if (i + 1 < text.length() && (text[i+1] == '|' || text[i+1] == '-')) {
                std::string arrow(1, c);
                i++; column++;
                while (i < text.length()) {
                    char next = text[i];
                    if (next == '-' || next == '=' || next == '.' || next == 'o' || next == '{' || next == '}' || next == '|') {
                        arrow += next; i++; column++;
                    } else break;
                }
                tokens.push_back({Token::ARROW, arrow, line, column});
                continue;
            }
            tokens.push_back({Token::PIPE, "|", line, column});
            i++; column++;
            continue;
        }
        
        // Special [*] for state diagrams
        if (c == '[' && i + 2 < text.length() && text[i + 1] == '*' && text[i + 2] == ']') {
            tokens.push_back({Token::IDENTIFIER, "[*]", line, column});
            i += 3; column += 3;
            continue;
        }
        
        // Shape open
        if (c == '[' || c == '(' || c == '{') {
            std::string shape(1, c);
            i++; column++;
            if (i < text.length()) {
                char next = text[i];
                if ((c == '[' && (next == '[' || next == '(' || next == '/' || next == '\\')) ||
                    (c == '(' && (next == '(' || next == '['))) {
                    shape += next; i++; column++;
                    if (c == '(' && next == '(' && i < text.length() && text[i] == '(') {
                        shape += text[i]; i++; column++;
                    }
                }
            }
            tokens.push_back({Token::SHAPE_OPEN, shape, line, column});
            continue;
        }
        
        // Shape close
        if (c == ']' || c == ')' || c == '}') {
            std::string shape(1, c);
            i++; column++;
            if (i < text.length()) {
                char next = text[i];
                if ((c == ']' && (next == ']' || next == ')' || next == '/' || next == '\\')) ||
                    (c == ')' && (next == ')' || next == ']'))) {
                    shape += next; i++; column++;
                    if (c == ')' && next == ')' && i < text.length() && text[i] == ')') {
                        shape += text[i]; i++; column++;
                    }
                }
            }
            tokens.push_back({Token::SHAPE_CLOSE, shape, line, column});
            continue;
        }

        if (c == '/' && i + 1 < text.length() && text[i+1] == ']') {
            tokens.push_back({Token::SHAPE_CLOSE, "/]", line, column});
            i += 2; column += 2;
            continue;
        }
        
        if (c == ':') { tokens.push_back({Token::COLON, ":", line, column}); i++; column++; continue; }
        if (c == ';') { tokens.push_back({Token::SEMICOLON, ";", line, column}); i++; column++; continue; }
        if (c == ',') { tokens.push_back({Token::COMMA, ",", line, column}); i++; column++; continue; }
        if (c == '#') { tokens.push_back({Token::HASH, "#", line, column}); i++; column++; continue; }
        if (c == '&') { tokens.push_back({Token::AND, "&", line, column}); i++; column++; continue; }
        
        // Identifiers and keywords
        if (std::isalpha(c) || c == '_' || c == '$') {
            std::string id;
            while (i < text.length()) {
                char ch = text[i];
                if (std::isalnum(ch) || ch == '_') {
                    id += ch; i++; column++;
                } else if (ch == '-' || ch == '.') {
                    // Look ahead: if next char is arrow-like, stop here
                    if (i + 1 < text.length()) {
                        char next = text[i + 1];
                        if (next == '-' || next == '>' || next == '<' || next == '.' || next == '=') {
                            break; // Don't consume the dash
                        }
                    }
                    id += ch; i++; column++;
                } else {
                    break;
                }
            }
            
            static const std::vector<std::string> diagram_types = {
                "flowchart", "graph", "sequenceDiagram", "classDiagram", 
                "stateDiagram", "stateDiagram-v2", "erDiagram", "pie",
                "gantt", "timeline", "journey", "mindmap", "gitGraph",
                "xychart", "xychart-beta", "requirementDiagram", 
                "sankey", "sankey-beta", "architecture", "architecture-beta",
                "block", "block-beta",
                "C4", "C4Context", "C4Container", "C4Component", "C4Dynamic", "C4Deployment",
                "kanban", "packet", "packet-beta", "radar", "radar-beta",
                "treemap", "treemap-beta", "zenuml", "quadrant", "quadrantChart"
            };
            static const std::vector<std::string> directions = {"TD", "TB", "LR", "RL", "BT"};
            
            Token::Type type = Token::IDENTIFIER;
            if (std::find(diagram_types.begin(), diagram_types.end(), id) != diagram_types.end())
                type = Token::DIAGRAM_TYPE;
            else if (std::find(directions.begin(), directions.end(), id) != directions.end())
                type = Token::DIRECTION;
            else if (id == "subgraph") type = Token::SUBGRAPH;
            else if (id == "end") type = Token::END;
            else if (id == "classDef" || id == "class" || id == "style" || id == "linkStyle")
                type = Token::STYLING;
            else if (id == "direction") type = Token::DIRECTION_KEYWORD;
            else if (id == "pie") type = Token::PIE;
            else if (id == "title") type = Token::TITLE;
            else if (id == "accTitle") type = Token::ACC_TITLE;
            else if (id == "accDescr") type = Token::ACC_DESCR;
            else if (id == "participant" || id == "actor") type = Token::PARTICIPANT;
            else if (id == "note") type = Token::NOTE;
            else if (id == "gitGraph") type = Token::GITGRAPH;
            else if (id == "commit") type = Token::COMMIT;
            else if (id == "branch") type = Token::BRANCH;
            else if (id == "checkout") type = Token::CHECKOUT;
            else if (id == "merge") type = Token::MERGE;
            
            tokens.push_back({type, id, line, column});
            continue;
        }
        
        // Numbers
        if (std::isdigit(c)) {
            std::string num;
            while (i < text.length() && (std::isdigit(text[i]) || text[i] == '.')) {
                num += text[i]; i++; column++;
            }
            tokens.push_back({Token::IDENTIFIER, num, line, column});
            continue;
        }
        
        i++; column++;
    }
    
    tokens.push_back({Token::END_OF_FILE, "", line, column});
    return tokens;
}

} // namespace flex::modules::flexmaid
