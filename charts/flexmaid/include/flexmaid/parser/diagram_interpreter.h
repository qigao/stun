#pragma once

#include <parser/expression.h>
#include <unordered_map>
#include <functional>

namespace flex::modules::flexmaid {

struct ParseResult;

class DiagramInterpreter {
public:
    DiagramInterpreter();
    
    ParseResult interpret(const std::vector<Token>& tokens);
    
    using ExprBuilder = std::function<std::unique_ptr<StatementsExpr>()>;
    void register_grammar(DiagramType type, ExprBuilder builder);
    
    static DiagramType identify_type(const std::string& type_str);
    
private:
    std::unordered_map<DiagramType, ExprBuilder> grammars_;
    
    static std::unique_ptr<StatementsExpr> build_flowchart_grammar();
    static std::unique_ptr<StatementsExpr> build_class_grammar();
    static std::unique_ptr<StatementsExpr> build_er_grammar();
    static std::unique_ptr<StatementsExpr> build_sequence_grammar();
    static std::unique_ptr<StatementsExpr> build_state_grammar();
    static std::unique_ptr<StatementsExpr> build_pie_grammar();
    static std::unique_ptr<StatementsExpr> build_gitgraph_grammar();
    static std::unique_ptr<StatementsExpr> build_gantt_grammar();
    static std::unique_ptr<StatementsExpr> build_journey_grammar();
    static std::unique_ptr<StatementsExpr> build_mindmap_grammar();
    static std::unique_ptr<StatementsExpr> build_timeline_grammar();
    static std::unique_ptr<StatementsExpr> build_quadrant_grammar();
    static std::unique_ptr<StatementsExpr> build_xychart_grammar();
    static std::unique_ptr<StatementsExpr> build_sankey_grammar();
    static std::unique_ptr<StatementsExpr> build_c4_grammar();
    static std::unique_ptr<StatementsExpr> build_architecture_grammar();
    static std::unique_ptr<StatementsExpr> build_kanban_grammar();
    static std::unique_ptr<StatementsExpr> build_packet_grammar();
    static std::unique_ptr<StatementsExpr> build_requirement_grammar();
    static std::unique_ptr<StatementsExpr> build_treemap_grammar();
    static std::unique_ptr<StatementsExpr> build_radar_grammar();
    static std::unique_ptr<StatementsExpr> build_zenuml_grammar();
};

} // namespace flex::modules::flexmaid
