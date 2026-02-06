#include <parser/diagram_interpreter.h>
#include <parser/unified_parser.h>
#include <sstream>

namespace flex::modules::flexmaid {

// ============================================================================
// ER Diagram Expressions
// ============================================================================

class EREdgeExpr : public EdgeExpr {
protected:
    EdgeDecoration identify_decoration(const std::string& arrow) override {
        if (arrow.find("o{") != std::string::npos || arrow.find("}o") != std::string::npos)
            return EdgeDecoration::Circle;
        if (arrow.find("|{") != std::string::npos || arrow.find("}|") != std::string::npos)
            return EdgeDecoration::Arrow;
        if (arrow.find("o|") != std::string::npos || arrow.find("|o") != std::string::npos)
            return EdgeDecoration::Diamond;
        return EdgeDecoration::None;
    }
};

class EREntityExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string id = ctx.current().value;
        size_t saved = ctx.pos;
        ctx.advance();
        ctx.skip_newlines();
        
        if (!ctx.match_value("{")) { ctx.pos = saved; return false; }
        ctx.advance();
        ctx.skip_newlines();
        
        Node& node = ctx.diagram->ensure_node(id, id, NodeShape::Entity);
        int attr_idx = 0;
        
        while (!ctx.at_end() && !ctx.match_value("}")) {
            std::string type, name;
            if (ctx.match(Token::IDENTIFIER)) { type = ctx.current().value; ctx.advance(); }
            if (ctx.match(Token::IDENTIFIER)) { name = ctx.current().value; ctx.advance(); }
            if (!type.empty() || !name.empty())
                node.set_prop("attr_" + std::to_string(attr_idx++), type + (name.empty() ? "" : " " + name));
            ctx.skip_newlines();
        }
        if (ctx.match_value("}")) ctx.advance();
        return true;
    }
};

// ============================================================================
// Class Diagram Expressions
// ============================================================================

class ClassEdgeExpr : public EdgeExpr {
protected:
    EdgeDecoration identify_decoration(const std::string& arrow) override {
        if (arrow.find("<|") != std::string::npos || arrow.find("|>") != std::string::npos)
            return EdgeDecoration::Triangle;
        if (arrow.find("*") != std::string::npos)
            return EdgeDecoration::DiamondFilled;
        if (arrow.find("o") != std::string::npos && arrow.find("o{") == std::string::npos)
            return EdgeDecoration::Diamond;
        if (arrow.find(">") != std::string::npos || arrow.find("<") != std::string::npos)
            return EdgeDecoration::Arrow;
        return EdgeDecoration::None;
    }
};

class ClassDefExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match_value("class")) return false;
        ctx.advance();
        if (!ctx.match(Token::IDENTIFIER)) return false;
        
        std::string name = ctx.current().value;
        ctx.advance();
        ctx.skip_newlines();
        
        Node& node = ctx.diagram->ensure_node(name, name, NodeShape::Class);
        if (!ctx.match_value("{")) return true;
        ctx.advance();
        ctx.skip_newlines();
        
        int attr_idx = 0, method_idx = 0;
        while (!ctx.at_end() && !ctx.match_value("}")) {
            std::string member;
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE) && !ctx.match_value("}")) {
                const std::string& val = ctx.current().value;
                bool no_space = member.empty() || val == "(" || val == ")" ||
                    (!member.empty() && (member.back() == '+' || member.back() == '-' ||
                     member.back() == '#' || member.back() == '~' || member.back() == '('));
                if (!no_space) member += " ";
                member += val;
                ctx.advance();
            }
            if (!member.empty()) {
                if (member.find("(") != std::string::npos)
                    node.set_prop("method_" + std::to_string(method_idx++), member);
                else
                    node.set_prop("attr_" + std::to_string(attr_idx++), member);
            }
            ctx.skip_newlines();
        }
        if (ctx.match_value("}")) ctx.advance();
        return true;
    }
};

// ============================================================================
// Sequence Diagram Expressions
// ============================================================================

class ParticipantExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::PARTICIPANT)) return false;
        ctx.advance();
        if (!ctx.match(Token::IDENTIFIER)) return false;
        
        std::string id = ctx.current().value;
        ctx.advance();
        
        std::string label = id;
        if (ctx.match(Token::IDENTIFIER) && ctx.current().value == "as") {
            ctx.advance();
            if (ctx.match(Token::IDENTIFIER) || ctx.match(Token::STRING)) {
                label = ctx.current().value;
                ctx.advance();
            }
        }
        ctx.diagram->ensure_node(id, label, NodeShape::Rectangle);
        return true;
    }
};

class SequenceEdgeExpr : public EdgeExpr {
protected:
    EdgeStyle identify_style(const std::string& arrow) override {
        if (arrow.find("--") != std::string::npos) return EdgeStyle::Dashed;
        return EdgeStyle::Solid;
    }
};

// ============================================================================
// Pie Chart Expressions
// ============================================================================

class PieDataExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (ctx.match(Token::TITLE)) {
            ctx.advance();
            std::string title;
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                if (!title.empty()) title += " ";
                title += ctx.current().value;
                ctx.advance();
            }
            ctx.diagram->set_prop("title", title);
            return true;
        }
        
        std::string label;
        if (ctx.match(Token::STRING)) {
            label = ctx.current().value;
            ctx.advance();
        } else if (ctx.match(Token::IDENTIFIER)) {
            label = ctx.current().value;
            ctx.advance();
        } else return false;
        
        if (!ctx.match(Token::COLON)) return false;
        ctx.advance();
        
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string value = ctx.current().value;
        ctx.advance();
        
        ctx.diagram->set_prop("data_" + label, value);
        ctx.diagram->ensure_node(label, label + ": " + value, NodeShape::Rectangle);
        return true;
    }
};

// ============================================================================
// GitGraph Expressions
// ============================================================================

class GitGraphExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (ctx.match(Token::COMMIT)) {
            ctx.advance();
            std::string id = "commit_" + std::to_string(ctx.diagram->nodes.size());
            if (ctx.match(Token::IDENTIFIER) && ctx.current().value == "id") {
                ctx.advance();
                if (ctx.match(Token::COLON)) ctx.advance();
                if (ctx.match(Token::STRING) || ctx.match(Token::IDENTIFIER)) {
                    id = ctx.current().value;
                    ctx.advance();
                }
            }
            ctx.diagram->ensure_node(id, id, NodeShape::Circle);
            return true;
        }
        if (ctx.match(Token::BRANCH)) {
            ctx.advance();
            if (ctx.match(Token::IDENTIFIER)) {
                ctx.diagram->set_prop("branch_" + ctx.current().value, "created");
                ctx.advance();
            }
            return true;
        }
        if (ctx.match(Token::CHECKOUT)) {
            ctx.advance();
            if (ctx.match(Token::IDENTIFIER)) {
                ctx.diagram->set_prop("current_branch", ctx.current().value);
                ctx.advance();
            }
            return true;
        }
        if (ctx.match(Token::MERGE)) {
            ctx.advance();
            if (ctx.match(Token::IDENTIFIER)) {
                std::string branch = ctx.current().value;
                ctx.advance();
                ctx.diagram->set_prop("merge_" + branch, "merged");
            }
            return true;
        }
        return false;
    }
};

// ============================================================================
// Gantt Chart Expressions
// ============================================================================

class GanttExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (ctx.match(Token::TITLE)) {
            ctx.advance();
            std::string title;
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                if (!title.empty()) title += " ";
                title += ctx.current().value;
                ctx.advance();
            }
            ctx.diagram->set_prop("title", title);
            return true;
        }
        if (ctx.match(Token::IDENTIFIER)) {
            std::string key = ctx.current().value;
            if (key == "dateFormat" || key == "section") {
                ctx.advance();
                std::string value;
                while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                    if (!value.empty()) value += " ";
                    value += ctx.current().value;
                    ctx.advance();
                }
                if (key == "section") {
                    Subgraph sg;
                    sg.id = "section_" + std::to_string(ctx.diagram->subgraphs.size());
                    sg.label = value;
                    ctx.diagram->subgraphs.push_back(std::move(sg));
                } else {
                    ctx.diagram->set_prop(key, value);
                }
                return true;
            }
            // Task line: Task Name : status, id, date, duration
            std::string task_name = key;
            ctx.advance();
            if (ctx.match(Token::COLON)) {
                ctx.advance();
                std::string task_def;
                while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                    if (!task_def.empty()) task_def += " ";
                    task_def += ctx.current().value;
                    ctx.advance();
                }
                Node& n = ctx.diagram->ensure_node(task_name, task_name, NodeShape::Rectangle);
                n.set_prop("definition", task_def);
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// Journey Expressions
// ============================================================================

class JourneyExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (ctx.match(Token::TITLE)) {
            ctx.advance();
            std::string title;
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                if (!title.empty()) title += " ";
                title += ctx.current().value;
                ctx.advance();
            }
            ctx.diagram->set_prop("title", title);
            return true;
        }
        if (ctx.match(Token::IDENTIFIER) && ctx.current().value == "section") {
            ctx.advance();
            std::string section;
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                if (!section.empty()) section += " ";
                section += ctx.current().value;
                ctx.advance();
            }
            Subgraph sg;
            sg.id = "section_" + std::to_string(ctx.diagram->subgraphs.size());
            sg.label = section;
            ctx.diagram->subgraphs.push_back(std::move(sg));
            return true;
        }
        if (ctx.match(Token::IDENTIFIER)) {
            std::string step = ctx.current().value;
            ctx.advance();
            while (!ctx.at_end() && !ctx.match(Token::COLON) && !ctx.match(Token::NEWLINE)) {
                step += " " + ctx.current().value;
                ctx.advance();
            }
            if (ctx.match(Token::COLON)) {
                ctx.advance();
                std::string score, actors;
                if (ctx.match(Token::IDENTIFIER)) { score = ctx.current().value; ctx.advance(); }
                if (ctx.match(Token::COLON)) {
                    ctx.advance();
                    while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                        if (!actors.empty()) actors += ", ";
                        actors += ctx.current().value;
                        ctx.advance();
                        if (ctx.match(Token::COMMA)) ctx.advance();
                    }
                }
                Node& n = ctx.diagram->ensure_node(step, step, NodeShape::Rectangle);
                n.set_prop("score", score);
                n.set_prop("actors", actors);
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// Mindmap / Treemap Expressions (indentation-based)
// ============================================================================

class MindmapExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER) && !ctx.match(Token::SHAPE_OPEN)) return false;
        
        std::string label;
        NodeShape shape = NodeShape::Rectangle;
        
        if (ctx.match(Token::IDENTIFIER) && ctx.current().value == "root") {
            ctx.advance();
            if (ctx.match(Token::SHAPE_OPEN)) {
                std::string open = ctx.current().value;
                ctx.advance();
                while (!ctx.at_end() && !ctx.match(Token::SHAPE_CLOSE)) {
                    label += ctx.current().value;
                    ctx.advance();
                }
                if (ctx.match(Token::SHAPE_CLOSE)) ctx.advance();
                if (open == "((") shape = NodeShape::Circle;
            }
        } else {
            label = ctx.current().value;
            ctx.advance();
        }
        
        if (!label.empty()) {
            ctx.diagram->ensure_node(label, label, shape);
        }
        return true;
    }
};

class TreemapExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string label = ctx.current().value;
        ctx.advance();
        
        if (ctx.match(Token::COLON)) {
            ctx.advance();
            std::string value;
            if (ctx.match(Token::IDENTIFIER)) {
                value = ctx.current().value;
                ctx.advance();
            }
            Node& n = ctx.diagram->ensure_node(label, label, NodeShape::Rectangle);
            n.set_prop("value", value);
            return true;
        }
        return false;
    }
};

// ============================================================================
// Timeline Expressions
// ============================================================================

class TimelineExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (ctx.match(Token::TITLE)) {
            ctx.advance();
            std::string title;
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                if (!title.empty()) title += " ";
                title += ctx.current().value;
                ctx.advance();
            }
            ctx.diagram->set_prop("title", title);
            return true;
        }
        if (ctx.match(Token::IDENTIFIER)) {
            std::string year = ctx.current().value;
            ctx.advance();
            if (ctx.match(Token::COLON)) {
                ctx.advance();
                std::string event;
                while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                    if (!event.empty()) event += " ";
                    event += ctx.current().value;
                    ctx.advance();
                }
                Node& n = ctx.diagram->ensure_node(year, year, NodeShape::Rectangle);
                n.set_prop("event", event);
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// Quadrant / XYChart / Radar Expressions
// ============================================================================

class QuadrantExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (ctx.match(Token::TITLE)) {
            ctx.advance();
            std::string title;
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                if (!title.empty()) title += " ";
                title += ctx.current().value;
                ctx.advance();
            }
            ctx.diagram->set_prop("title", title);
            return true;
        }
        if (ctx.match(Token::IDENTIFIER)) {
            std::string key = ctx.current().value;
            ctx.advance();
            
            // x-axis, y-axis, quadrant-N
            if (key.find("axis") != std::string::npos || key.find("quadrant") != std::string::npos) {
                std::string value;
                while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                    if (!value.empty()) value += " ";
                    value += ctx.current().value;
                    ctx.advance();
                }
                ctx.diagram->set_prop(key, value);
                return true;
            }
            
            // Data point: Name : [x, y]
            if (ctx.match(Token::COLON)) {
                ctx.advance();
                std::string coords;
                while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                    coords += ctx.current().value;
                    ctx.advance();
                }
                Node& n = ctx.diagram->ensure_node(key, key, NodeShape::Circle);
                n.set_prop("coords", coords);
                return true;
            }
        }
        return false;
    }
};

class XYChartExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string key = ctx.current().value;
        ctx.advance();
        
        std::string value;
        while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
            if (!value.empty()) value += " ";
            value += ctx.current().value;
            ctx.advance();
        }
        ctx.diagram->set_prop(key, value);
        return true;
    }
};

class RadarExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string key = ctx.current().value;
        ctx.advance();
        
        std::string value;
        while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
            if (!value.empty()) value += " ";
            value += ctx.current().value;
            ctx.advance();
        }
        ctx.diagram->set_prop(key, value);
        return true;
    }
};

// ============================================================================
// Sankey Expressions
// ============================================================================

class SankeyExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string from = ctx.current().value;
        ctx.advance();
        
        if (!ctx.match(Token::COMMA)) return false;
        ctx.advance();
        
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string to = ctx.current().value;
        ctx.advance();
        
        if (!ctx.match(Token::COMMA)) return false;
        ctx.advance();
        
        std::string value;
        if (ctx.match(Token::IDENTIFIER)) {
            value = ctx.current().value;
            ctx.advance();
        }
        
        ctx.diagram->ensure_node(from, from, NodeShape::Rectangle);
        ctx.diagram->ensure_node(to, to, NodeShape::Rectangle);
        
        Edge e;
        e.from = from;
        e.to = to;
        e.label = value;
        ctx.diagram->edges.push_back(std::move(e));
        return true;
    }
};

// ============================================================================
// C4 / Architecture Expressions
// ============================================================================

class C4Expr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string keyword = ctx.current().value;
        ctx.advance();
        
        if (!ctx.match(Token::SHAPE_OPEN) || ctx.current().value != "(") return false;
        ctx.advance();
        
        std::vector<std::string> args;
        while (!ctx.at_end() && !ctx.match(Token::SHAPE_CLOSE)) {
            if (ctx.match(Token::IDENTIFIER) || ctx.match(Token::STRING)) {
                args.push_back(ctx.current().value);
            }
            ctx.advance();
        }
        if (ctx.match(Token::SHAPE_CLOSE)) ctx.advance();
        
        if (keyword.find("Rel") != std::string::npos && args.size() >= 2) {
            Edge e;
            e.from = args[0];
            e.to = args[1];
            if (args.size() >= 3) e.label = args[2];
            e.end_decoration = EdgeDecoration::Arrow;
            ctx.diagram->edges.push_back(std::move(e));
            return true;
        }
        
        if (keyword.find("Boundary") != std::string::npos && !args.empty()) {
            Subgraph sg;
            sg.id = args[0];
            sg.label = args.size() > 1 ? args[1] : args[0];
            ctx.diagram->subgraphs.push_back(std::move(sg));
            ctx.subgraph_stack.push_back(ctx.diagram->subgraphs.size() - 1);
            
            if (ctx.match(Token::SHAPE_OPEN) && ctx.current().value == "{") {
                ctx.advance();
            }
            return true;
        }
        
        if (!args.empty()) {
            std::string id = args[0];
            std::string label = args.size() > 1 ? args[1] : id;
            NodeShape shape = NodeShape::Rectangle;
            if (keyword == "Person" || keyword == "Person_Ext") shape = NodeShape::Actor;
            if (keyword.find("Db") != std::string::npos) shape = NodeShape::Cylinder;
            
            ctx.diagram->ensure_node(id, label, shape);
            ctx.diagram->nodes[id].set_prop("c4_type", keyword);
            if (!ctx.subgraph_stack.empty()) {
                ctx.diagram->subgraphs[ctx.subgraph_stack.back()].node_ids.push_back(id);
            }
            return true;
        }
        return false;
    }
};

class ArchitectureExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string keyword = ctx.current().value;
        ctx.advance();
        
        if (keyword == "group" || keyword == "service") {
            if (!ctx.match(Token::IDENTIFIER)) return false;
            std::string id = ctx.current().value;
            ctx.advance();
            
            // Parse (icon_name)
            std::string icon;
            if (ctx.match(Token::SHAPE_OPEN) && ctx.current().value == "(") {
                ctx.advance();
                while (!ctx.at_end() && !ctx.match(Token::SHAPE_CLOSE)) {
                    icon += ctx.current().value;
                    ctx.advance();
                }
                if (ctx.match(Token::SHAPE_CLOSE)) ctx.advance();
            }
            
            // [Label]
            std::string label = id;
            if (ctx.match(Token::SHAPE_OPEN) && ctx.current().value == "[") {
                ctx.advance();
                label.clear();
                while (!ctx.at_end() && !ctx.match(Token::SHAPE_CLOSE)) {
                    if (!label.empty()) label += " ";
                    label += ctx.current().value;
                    ctx.advance();
                }
                if (ctx.match(Token::SHAPE_CLOSE)) ctx.advance();
            }
            
            // Parse "in group_id"
            std::string parent_group;
            if (ctx.match(Token::IDENTIFIER) && ctx.current().value == "in") {
                ctx.advance();
                if (ctx.match(Token::IDENTIFIER)) {
                    parent_group = ctx.current().value;
                    ctx.advance();
                }
            }
            
            if (keyword == "group") {
                Subgraph sg;
                sg.id = id;
                sg.label = label;
                if (!icon.empty()) sg.set_prop("icon", icon);
                ctx.diagram->subgraphs.push_back(std::move(sg));
                ctx.subgraph_stack.push_back(ctx.diagram->subgraphs.size() - 1);
            } else {
                auto& node = ctx.diagram->ensure_node(id, label, NodeShape::Rectangle);
                if (!icon.empty()) node.set_prop("icon", icon);
                
                // Find parent group and add node to it
                if (!parent_group.empty()) {
                    for (auto& sg : ctx.diagram->subgraphs) {
                        if (sg.id == parent_group) {
                            sg.node_ids.push_back(id);
                            break;
                        }
                    }
                } else if (!ctx.subgraph_stack.empty()) {
                    ctx.diagram->subgraphs[ctx.subgraph_stack.back()].node_ids.push_back(id);
                }
            }
            return true;
        }
        return false;
    }
};

// Architecture edge: web:R --> L:db
class ArchitectureEdgeExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        
        std::string from = ctx.current().value;
        size_t saved = ctx.pos;
        ctx.advance();
        
        // Parse optional :port
        std::string from_port;
        if (ctx.match(Token::COLON)) {
            ctx.advance();
            if (ctx.match(Token::IDENTIFIER)) {
                from_port = ctx.current().value;
                ctx.advance();
            }
        }
        
        if (!ctx.match(Token::ARROW)) {
            ctx.pos = saved;
            return false;
        }
        ctx.advance(); // skip arrow
        
        // Parse optional port: before target node
        std::string to_port;
        if (ctx.match(Token::IDENTIFIER)) {
            std::string maybe_port = ctx.current().value;
            size_t p = ctx.pos;
            ctx.advance();
            
            if (ctx.match(Token::COLON)) {
                // It's port:node format
                to_port = maybe_port;
                ctx.advance();
                if (!ctx.match(Token::IDENTIFIER)) {
                    ctx.pos = saved;
                    return false;
                }
            } else {
                // No colon, so maybe_port is actually the target node
                ctx.pos = p;
            }
        }
        
        if (!ctx.match(Token::IDENTIFIER)) {
            ctx.pos = saved;
            return false;
        }
        
        std::string to = ctx.current().value;
        ctx.advance();
        
        Edge edge;
        edge.from = from;
        edge.to = to;
        edge.end_decoration = EdgeDecoration::Arrow;
        if (!from_port.empty()) edge.set_prop("from_port", from_port);
        if (!to_port.empty()) edge.set_prop("to_port", to_port);
        ctx.diagram->edges.push_back(std::move(edge));
        
        return true;
    }
};

// ============================================================================
// Kanban Expressions
// ============================================================================

class KanbanExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string id = ctx.current().value;
        ctx.advance();
        
        if (ctx.match(Token::SHAPE_OPEN) && ctx.current().value == "[") {
            ctx.advance();
            std::string label;
            while (!ctx.at_end() && !ctx.match(Token::SHAPE_CLOSE)) {
                if (!label.empty()) label += " ";
                label += ctx.current().value;
                ctx.advance();
            }
            if (ctx.match(Token::SHAPE_CLOSE)) ctx.advance();
            
            // Check if this is a column (followed by newline + indented items) or a card
            // For simplicity, treat first-level as columns, nested as cards
            if (ctx.subgraph_stack.empty()) {
                Subgraph sg;
                sg.id = id;
                sg.label = label;
                ctx.diagram->subgraphs.push_back(std::move(sg));
                ctx.subgraph_stack.push_back(ctx.diagram->subgraphs.size() - 1);
            } else {
                ctx.diagram->ensure_node(id, label, NodeShape::Rectangle);
                ctx.diagram->subgraphs[ctx.subgraph_stack.back()].node_ids.push_back(id);
            }
            return true;
        }
        return false;
    }
};

// ============================================================================
// Packet / Requirement Expressions
// ============================================================================

class PacketExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string range = ctx.current().value;
        ctx.advance();
        
        // Handle "0-7" format
        if (ctx.match(Token::ARROW) && ctx.current().value.find("-") == 0) {
            range += ctx.current().value;
            ctx.advance();
        }
        
        if (!ctx.match(Token::COLON)) return false;
        ctx.advance();
        
        std::string label;
        if (ctx.match(Token::STRING)) {
            label = ctx.current().value;
            ctx.advance();
        } else {
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                if (!label.empty()) label += " ";
                label += ctx.current().value;
                ctx.advance();
            }
        }
        
        Node& n = ctx.diagram->ensure_node(range, label, NodeShape::Rectangle);
        n.set_prop("range", range);
        return true;
    }
};

class RequirementExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string keyword = ctx.current().value;
        
        if (keyword == "requirement" || keyword == "functionalRequirement" ||
            keyword == "performanceRequirement" || keyword == "interfaceRequirement" ||
            keyword == "physicalRequirement" || keyword == "designConstraint" ||
            keyword == "element") {
            ctx.advance();
            if (!ctx.match(Token::IDENTIFIER)) return false;
            std::string id = ctx.current().value;
            ctx.advance();
            
            Node& n = ctx.diagram->ensure_node(id, id, NodeShape::Rectangle);
            n.set_prop("type", keyword);
            
            if (ctx.match(Token::SHAPE_OPEN) && ctx.current().value == "{") {
                ctx.advance();
                ctx.skip_newlines();
                
                while (!ctx.at_end() && !ctx.match_value("}")) {
                    if (ctx.match(Token::IDENTIFIER)) {
                        std::string prop = ctx.current().value;
                        ctx.advance();
                        if (ctx.match(Token::COLON)) {
                            ctx.advance();
                            std::string val;
                            while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                                if (!val.empty()) val += " ";
                                val += ctx.current().value;
                                ctx.advance();
                            }
                            n.set_prop(prop, val);
                        }
                    }
                    ctx.skip_newlines();
                }
                if (ctx.match_value("}")) ctx.advance();
            }
            return true;
        }
        
        // Relationship: req1 - satisfies -> req2
        std::string from = keyword;
        ctx.advance();
        
        if (ctx.match(Token::ARROW)) {
            std::string arrow = ctx.current().value;
            ctx.advance();
            
            std::string rel_type;
            if (ctx.match(Token::IDENTIFIER)) {
                rel_type = ctx.current().value;
                ctx.advance();
            }
            
            if (ctx.match(Token::ARROW)) ctx.advance();
            
            if (ctx.match(Token::IDENTIFIER)) {
                std::string to = ctx.current().value;
                ctx.advance();
                
                Edge e;
                e.from = from;
                e.to = to;
                e.label = rel_type;
                ctx.diagram->edges.push_back(std::move(e));
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// State Diagram Expressions
// ============================================================================

class StateExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (ctx.match(Token::IDENTIFIER) && ctx.current().value == "state") {
            ctx.advance();
            
            std::string label;
            if (ctx.match(Token::STRING)) {
                label = ctx.current().value;
                ctx.advance();
            }
            
            if (ctx.match(Token::IDENTIFIER) && ctx.current().value == "as") {
                ctx.advance();
                if (ctx.match(Token::IDENTIFIER)) {
                    std::string id = ctx.current().value;
                    ctx.advance();
                    ctx.diagram->ensure_node(id, label.empty() ? id : label, NodeShape::State);
                    return true;
                }
            }
            return true;
        }
        return false;
    }
};

class ZenUMLExpr : public IExpression {
public:
    bool interpret(ParseContext& ctx) override {
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string from = ctx.current().value;
        ctx.advance();
        
        if (!ctx.match(Token::ARROW)) return false;
        std::string arrow = ctx.current().value;
        ctx.advance();
        
        if (!ctx.match(Token::IDENTIFIER)) return false;
        std::string to = ctx.current().value;
        ctx.advance();
        
        std::string label;
        if (ctx.match(Token::COLON)) {
            ctx.advance();
            while (!ctx.at_end() && !ctx.match(Token::NEWLINE)) {
                if (!label.empty()) label += " ";
                label += ctx.current().value;
                ctx.advance();
            }
        }
        
        ctx.diagram->ensure_node(from, from, NodeShape::Rectangle);
        ctx.diagram->ensure_node(to, to, NodeShape::Rectangle);
        
        Edge e;
        e.from = from;
        e.to = to;
        e.label = label;
        e.style = (arrow.find("--") != std::string::npos) ? EdgeStyle::Dashed : EdgeStyle::Solid;
        ctx.diagram->edges.push_back(std::move(e));
        return true;
    }
};

// ============================================================================
// DiagramInterpreter Implementation
// ============================================================================

DiagramInterpreter::DiagramInterpreter() {
    grammars_[DiagramType::Flowchart] = build_flowchart_grammar;
    grammars_[DiagramType::Class] = build_class_grammar;
    grammars_[DiagramType::ER] = build_er_grammar;
    grammars_[DiagramType::Sequence] = build_sequence_grammar;
    grammars_[DiagramType::State] = build_state_grammar;
    grammars_[DiagramType::Block] = build_flowchart_grammar;
    grammars_[DiagramType::Pie] = build_pie_grammar;
    grammars_[DiagramType::GitGraph] = build_gitgraph_grammar;
    grammars_[DiagramType::Gantt] = build_gantt_grammar;
    grammars_[DiagramType::Journey] = build_journey_grammar;
    grammars_[DiagramType::Mindmap] = build_mindmap_grammar;
    grammars_[DiagramType::Timeline] = build_timeline_grammar;
    grammars_[DiagramType::Quadrant] = build_quadrant_grammar;
    grammars_[DiagramType::XYChart] = build_xychart_grammar;
    grammars_[DiagramType::Sankey] = build_sankey_grammar;
    grammars_[DiagramType::C4] = build_c4_grammar;
    grammars_[DiagramType::Architecture] = build_architecture_grammar;
    grammars_[DiagramType::Kanban] = build_kanban_grammar;
    grammars_[DiagramType::Packet] = build_packet_grammar;
    grammars_[DiagramType::Requirement] = build_requirement_grammar;
    grammars_[DiagramType::Treemap] = build_treemap_grammar;
    grammars_[DiagramType::Radar] = build_radar_grammar;
    grammars_[DiagramType::ZenUML] = build_zenuml_grammar;
}

void DiagramInterpreter::register_grammar(DiagramType type, ExprBuilder builder) {
    grammars_[type] = std::move(builder);
}

DiagramType DiagramInterpreter::identify_type(const std::string& s) {
    if (s == "flowchart" || s == "graph") return DiagramType::Flowchart;
    if (s == "classDiagram") return DiagramType::Class;
    if (s == "erDiagram") return DiagramType::ER;
    if (s == "sequenceDiagram") return DiagramType::Sequence;
    if (s == "stateDiagram" || s == "stateDiagram-v2") return DiagramType::State;
    if (s == "pie") return DiagramType::Pie;
    if (s == "gitGraph") return DiagramType::GitGraph;
    if (s == "gantt") return DiagramType::Gantt;
    if (s == "journey") return DiagramType::Journey;
    if (s == "mindmap") return DiagramType::Mindmap;
    if (s == "timeline") return DiagramType::Timeline;
    if (s == "quadrantChart") return DiagramType::Quadrant;
    if (s.find("xychart") == 0) return DiagramType::XYChart;
    if (s.find("sankey") == 0) return DiagramType::Sankey;
    if (s.find("C4") == 0) return DiagramType::C4;
    if (s.find("block") == 0) return DiagramType::Block;
    if (s.find("architecture") == 0) return DiagramType::Architecture;
    if (s == "kanban") return DiagramType::Kanban;
    if (s.find("packet") == 0) return DiagramType::Packet;
    if (s.find("radar") == 0) return DiagramType::Radar;
    if (s == "requirementDiagram") return DiagramType::Requirement;
    if (s.find("treemap") == 0) return DiagramType::Treemap;
    if (s == "zenuml") return DiagramType::ZenUML;
    return DiagramType::Flowchart;
}

ParseResult DiagramInterpreter::interpret(const std::vector<Token>& tokens) {
    ParseResult result;
    result.diagram = std::make_unique<UnifiedDiagram>();
    
    ParseContext ctx;
    ctx.tokens = &tokens;
    ctx.diagram = result.diagram.get();
    
    ctx.skip_newlines();
    if (ctx.match(Token::DIAGRAM_TYPE)) {
        ctx.diagram->type = identify_type(ctx.current().value);
        ctx.advance();
    }
    if (ctx.match(Token::DIRECTION)) {
        ctx.diagram->set_prop("direction", ctx.current().value);
        ctx.advance();
    } else if (ctx.diagram->type == DiagramType::Architecture) {
        ctx.diagram->set_prop("direction", "LR");
    }
    // Skip optional showData for pie
    if (ctx.match(Token::IDENTIFIER) && ctx.current().value == "showData") {
        ctx.advance();
    }
    ctx.skip_newlines();
    
    auto it = grammars_.find(ctx.diagram->type);
    auto grammar = (it != grammars_.end()) ? it->second() : build_flowchart_grammar();
    
    grammar->interpret(ctx);
    result.success = true;
    return result;
}

// Grammar Builders
std::unique_ptr<StatementsExpr> DiagramInterpreter::build_flowchart_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<SubgraphExpr>());
    s->add(std::make_unique<EdgeExpr>());
    s->add(std::make_unique<NodeExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_class_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<ClassDefExpr>());
    s->add(std::make_unique<ClassEdgeExpr>());
    s->add(std::make_unique<NodeExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_er_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<EREntityExpr>());
    s->add(std::make_unique<EREdgeExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_sequence_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<ParticipantExpr>());
    s->add(std::make_unique<SequenceEdgeExpr>());
    s->add(std::make_unique<NodeExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_state_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<StateExpr>());
    s->add(std::make_unique<EdgeExpr>());
    s->add(std::make_unique<NodeExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_pie_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<PieDataExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_gitgraph_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<GitGraphExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_gantt_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<GanttExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_journey_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<JourneyExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_mindmap_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<MindmapExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_timeline_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<TimelineExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_quadrant_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<QuadrantExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_xychart_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<XYChartExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_sankey_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<SankeyExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_c4_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<C4Expr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_architecture_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<ArchitectureExpr>());
    s->add(std::make_unique<ArchitectureEdgeExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_kanban_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<KanbanExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_packet_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<PacketExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_requirement_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<RequirementExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_treemap_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<TreemapExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_radar_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<RadarExpr>());
    return s;
}

std::unique_ptr<StatementsExpr> DiagramInterpreter::build_zenuml_grammar() {
    auto s = std::make_unique<StatementsExpr>();
    s->add(std::make_unique<ZenUMLExpr>());
    return s;
}

} // namespace flex::modules::flexmaid
