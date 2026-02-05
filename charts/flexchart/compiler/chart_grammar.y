/*
 * Chart DSL Parser - Lemon Grammar
 */

%include {
#include "flexchart/chart_ast.h"
#include "chart_token.h"
#include "chart_context.h"
#include <memory>
#include <vector>
#include <string>

using namespace flex::chart;
}

%name ChartParse
%token_prefix TOK_
%token_type { std::string* }
%default_type { std::string* }
%extra_argument { flex::chart::ParseContext* ctx }

// Declare tokens that are used in lexer but not yet in grammar rules
%token DOT.
%token QUESTION.

%start_symbol program

program ::= statement_list.

// Recursive left-associative list for statements
statement_list ::= statement_list statement.
statement_list ::= .

// Top-level statements
statement ::= data_block.
statement ::= signal_block.
statement ::= scale_block.
statement ::= axis_block.
statement ::= legend_block.
statement ::= projection_block.
statement ::= mark_block.
statement ::= composition_block.
statement ::= encoding_block.
statement ::= generic_stmt. 

// Unified Identifier Key map
// All keywords that can be used as property keys must be listed here.
key(A) ::= IDENTIFIER(I). { A = I; }
key(A) ::= TITLE(I).      { A = I; }
key(A) ::= WIDTH(I).      { A = I; }
key(A) ::= HEIGHT(I).     { A = I; }
key(A) ::= MARGIN(I).     { A = I; }
key(A) ::= THEME(I).      { A = I; }
key(A) ::= DATA(I).       { A = I; }
key(A) ::= SOURCE(I).     { A = I; }
key(A) ::= FORMAT(I).     { A = I; }
key(A) ::= VALUES(I).     { A = I; }
key(A) ::= TRANSFORM(I).  { A = I; }
key(A) ::= TYPE(I).       { A = I; }
key(A) ::= BAR(I).        { A = I; }
key(A) ::= LINE(I).       { A = I; }
key(A) ::= ARC(I).        { A = I; }
key(A) ::= AREA(I).       { A = I; }
key(A) ::= POINT(I).      { A = I; }
key(A) ::= RECT(I).       { A = I; }
key(A) ::= RULE(I).       { A = I; }
key(A) ::= TICK(I).       { A = I; }
key(A) ::= TEXT(I).       { A = I; }
key(A) ::= TRAIL(I).      { A = I; }
key(A) ::= BOXPLOT(I).    { A = I; }
key(A) ::= ERRORBAR(I).   { A = I; }
key(A) ::= ERRORBAND(I).  { A = I; }
key(A) ::= GEOSHAPE(I).   { A = I; }
key(A) ::= IMAGE(I).      { A = I; }
key(A) ::= ENTER(I).      { A = I; }
key(A) ::= UPDATE(I).     { A = I; }
key(A) ::= HOVER(I).      { A = I; }
key(A) ::= EXIT(I).       { A = I; }
key(A) ::= ENCODING(I).   { A = I; }
key(A) ::= ON(I).         { A = I; }
key(A) ::= VALUE(I).      { A = I; }
// key(A) ::= SPEC(I).       { A = I; }
// key(A) ::= RESOLVE(I).    { A = I; }
key(A) ::= SIGNAL(I).     { A = I; }
key(A) ::= SCALE(I).      { A = I; }
key(A) ::= AXIS(I).       { A = I; }
key(A) ::= LEGEND(I).     { A = I; }
key(A) ::= PROJECTION(I). { A = I; }
key(A) ::= PIE(I).        { A = I; }
key(A) ::= RADAR(I).      { A = I; }

// Generic Statement (Root Properties)
generic_stmt ::= key(K) COLON value(V). {
    std::string k = *K;
    ctx->ensure_chart();
    if (k == "title") ctx->current_chart->title = *V;
    else if (k == "theme") ctx->current_chart->theme = *V;
    else if (k == "width") ctx->current_chart->width = *V;
    else if (k == "height") ctx->current_chart->height = *V;
    else if (k == "margin") ctx->current_chart->margin = *V;
}

// Data Block
data_block ::= data_head LBRACE data_body RBRACE. {
    ctx->current_data = nullptr;
}

data_head ::= DATA IDENTIFIER(ID). {
    ctx->current_data = std::make_shared<AstData>();
    ctx->current_data->name = *ID;
    if (ctx->current_chart) ctx->current_chart->datasets.push_back(ctx->current_data);
    else ctx->program->global_datasets.push_back(ctx->current_data);
}
data_head ::= DATA. {
    ctx->current_data = std::make_shared<AstData>();
    ctx->ensure_chart();
    ctx->current_data->name = "inline_" + std::to_string(ctx->current_chart->datasets.size());
    ctx->current_chart->datasets.push_back(ctx->current_data);
}

data_body ::= data_body data_item.
data_body ::= .

// Data Item: Unified entry to avoid conflict with specific keywords
data_item ::= key(K) COLON value(V). {
    if (ctx->current_data) {
        if (*K == "source") ctx->current_data->source = *V;
        else if (*K == "format") ctx->current_data->format = *V;
    }
}
data_item ::= key COLON LBRACKET transform_list RBRACKET. {
    // Handling "values: [...]" or "transform: [...]"
}

// Comma-separated simple values (for "values: [...]")
inline_values ::= inline_values COMMA value(V). { if (ctx->current_data) ctx->current_data->inline_values.push_back(ctx->to_value(V)); }
inline_values ::= value(V). { if (ctx->current_data) ctx->current_data->inline_values.push_back(ctx->to_value(V)); }

// List can be either inline values OR objects (transforms)
transform_list ::= inline_values. 
transform_list ::= real_transform_list. 

real_transform_list ::= real_transform_list COMMA transform_item.
real_transform_list ::= transform_item.

// Transform item is an object: { "key": "value", ... }
transform_item ::= LBRACE. {
    ctx->current_transform = std::make_shared<AstTransform>();
    ctx->current_transform->type = "object";
    if (ctx->current_data) ctx->current_data->transforms.push_back(ctx->current_transform);
}
transform_item ::= transform_item body_item_list RBRACE. { ctx->current_transform = nullptr; }

// Value definitions
value(A) ::= NUMBER(V).     { A = V; }
value(A) ::= STRING(V).     { A = V; }
value(A) ::= BOOL(V).       { A = V; }
value(A) ::= COLOR(V).      { A = V; }
value(A) ::= PERCENTAGE(V). { A = V; }
value(A) ::= key(V).        { A = V; }
value(A) ::= LPAREN value(V) RPAREN. { A = V; }

// Mark Block
mark_block ::= mark_head LBRACE mark_body RBRACE. {
    ctx->current_mark = nullptr;
}

// mark type identifiers
mark_head ::= mark_type(MT) IDENTIFIER(ID). {
    ctx->current_mark = std::make_shared<AstMark>();
    ctx->current_mark->type = *MT;
    ctx->current_mark->name = *ID;
    ctx->ensure_chart();
    ctx->current_chart->marks.push_back(ctx->current_mark);
}
mark_head ::= mark_type(MT). {
    ctx->current_mark = std::make_shared<AstMark>();
    ctx->current_mark->type = *MT;
    ctx->ensure_chart();
    ctx->current_chart->marks.push_back(ctx->current_mark);
}

mark_type(A) ::= BAR(M). { A = M; }
mark_type(A) ::= LINE(M). { A = M; }
mark_type(A) ::= ARC(M). { A = M; }
mark_type(A) ::= AREA(M). { A = M; }
mark_type(A) ::= POINT(M). { A = M; }
mark_type(A) ::= RECT(M). { A = M; }
mark_type(A) ::= RULE(M). { A = M; }
mark_type(A) ::= TICK(M). { A = M; }
mark_type(A) ::= TEXT(M). { A = M; }
mark_type(A) ::= TRAIL(M). { A = M; }
mark_type(A) ::= BOXPLOT(M). { A = M; }
mark_type(A) ::= ERRORBAR(M). { A = M; }
mark_type(A) ::= ERRORBAND(M). { A = M; }
mark_type(A) ::= GEOSHAPE(M). { A = M; }
mark_type(A) ::= IMAGE(M). { A = M; }
mark_type(A) ::= PIE(M). { A = M; }
mark_type(A) ::= RADAR(M). { A = M; }

mark_body ::= mark_body mark_item.
mark_body ::= .

// Unified Mark Item: Covers properties, encoding shorthand, and sub-blocks (states/encodings)
mark_item ::= block_header body_item_list RBRACE. {
    ctx->current_state = nullptr;
}

block_header ::= key(K) LBRACE. {
    std::string k = *K;
    if (k == "encoding") {
        // Handle explicit encoding block similarly to others or just pass properties
        // For simplicity, we treat properties inside as generic body items which are routed to encoding config
    } else if (k == "enter" || k == "update" || k == "hover" || k == "exit") {
        ctx->current_state = std::make_shared<AstState>();
        ctx->current_state->name = k;
        if (ctx->current_mark) ctx->current_mark->states.push_back(ctx->current_state);
    }
}

mark_item ::= key(K) COLON value(V). {
    std::string k = *K;
    if (k == "data") {
        if (ctx->current_mark) ctx->current_mark->data_ref = *V;
    } else {
        auto enc = std::make_shared<AstEncoding>();
        enc->channel = k;
        enc->field = *V;
        if (ctx->current_mark) ctx->current_mark->encodings.push_back(enc);
        if (ctx->current_state) ctx->current_state->encodings.push_back(enc);
    }
}

mark_item ::= mark_encoding_start body_item_list RBRACE. {
    ctx->current_encoding = nullptr;
}

mark_encoding_start ::= key(K) COLON value(V) LBRACE. {
    auto enc = std::make_shared<AstEncoding>();
    enc->channel = *K;
    enc->field = *V;
    if (ctx->current_mark) ctx->current_mark->encodings.push_back(enc);
    if (ctx->current_state) ctx->current_state->encodings.push_back(enc);
    ctx->current_encoding = enc;
}

mark_item ::= key(K1) DOT key(K2) COLON value(V). {
    if (ctx->current_mark && *K1 == "mark") {
        ctx->current_mark->styles[*K2] = ctx->to_value(V);
    }
}

body_item_list ::= body_item_list body_item.
body_item_list ::= .

body_item ::= key(K) COLON value(V). {
    std::string k = *K;
    if (ctx->current_transform) {
        if (k == "type") ctx->current_transform->type = *V;
        else if (ctx->current_transform->type == "object") ctx->current_transform->type = k;
        ctx->current_transform->properties[k] = ctx->to_value(V);
    }
    else if (ctx->current_scale) ctx->current_scale->properties[k] = ctx->to_value(V);
    else if (ctx->current_axis) ctx->current_axis->properties[k] = ctx->to_value(V);
    else if (ctx->current_encoding) ctx->current_encoding->config[k] = ctx->to_value(V);
    else if (ctx->current_state) {
        auto enc = std::make_shared<AstEncoding>();
        enc->channel = k;
        enc->field = *V; 
        ctx->current_state->encodings.push_back(enc);
    }
}

body_item ::= STRING(K) COLON value(V). {
    std::string k = *K;
    if (ctx->current_transform) {
        if (k == "type") ctx->current_transform->type = *V;
        else if (ctx->current_transform->type == "object") ctx->current_transform->type = k;
        ctx->current_transform->properties[k] = ctx->to_value(V);
    }
    else if (ctx->current_scale) ctx->current_scale->properties[k] = ctx->to_value(V);
    else if (ctx->current_axis) ctx->current_axis->properties[k] = ctx->to_value(V);
    else if (ctx->current_encoding) ctx->current_encoding->config[k] = ctx->to_value(V);
}


// Signal Block
signal_block ::= SIGNAL IDENTIFIER(ID) ASSIGN value(V). {
    auto signal = std::make_shared<AstSignal>();
    signal->name = *ID;
    signal->initial_value = ctx->to_value(V);
    ctx->ensure_chart();
    ctx->current_chart->signals.push_back(signal);
}
signal_block ::= signal_head LBRACE signal_body RBRACE. {
    ctx->current_signal = nullptr;
}

signal_head ::= SIGNAL IDENTIFIER(ID). {
    ctx->current_signal = std::make_shared<AstSignal>();
    ctx->current_signal->name = *ID;
    ctx->ensure_chart();
    ctx->current_chart->signals.push_back(ctx->current_signal);
}

signal_body ::= signal_body signal_item.
signal_body ::= .

signal_item ::= VALUE COLON value(V). { if (ctx->current_signal) ctx->current_signal->initial_value = ctx->to_value(V); }
signal_item ::= ON COLON LBRACKET signal_handlers RBRACKET.

signal_handlers ::= signal_handlers COMMA signal_handler.
signal_handlers ::= signal_handler.

signal_handler ::= handler_head signal_handler_body RBRACE. { ctx->current_signal_event = nullptr; }

handler_head ::= LBRACE. {
    ctx->current_signal_event = std::make_shared<AstSignalEvent>();
    if (ctx->current_signal) ctx->current_signal->handlers.push_back(ctx->current_signal_event);
}

// Signal handlers use generic key-value, no nesting issues expected
signal_handler_body ::= signal_handler_body signal_handler_item.
signal_handler_body ::= .

signal_handler_item ::= key(K) COLON value(V). {
    if (ctx->current_signal_event) {
        if (*K == "events") ctx->current_signal_event->event = *V;
        else if (*K == "update") ctx->current_signal_event->update_expr = *V;
    }
}

// Scale, Axis, Legend, Projection Blocks using unified `body_item_list`
scale_block ::= SCALE IDENTIFIER(ID) LBRACE body_item_list RBRACE. {
    ctx->current_scale = std::make_shared<AstScale>();
    ctx->current_scale->name = *ID;
    ctx->ensure_chart();
    ctx->current_chart->scales.push_back(ctx->current_scale);
    ctx->current_scale = nullptr;
}

axis_block ::= AXIS IDENTIFIER(ID) LBRACE body_item_list RBRACE. {
    ctx->current_axis = std::make_shared<AstAxis>();
    ctx->current_axis->name = *ID;
    ctx->ensure_chart();
    ctx->current_chart->axes.push_back(ctx->current_axis);
    ctx->current_axis = nullptr;
}

legend_block ::= LEGEND IDENTIFIER LBRACE body_item_list RBRACE. 

projection_block ::= PROJECTION IDENTIFIER LBRACE body_item_list RBRACE. 

encoding_block ::= ENCODING LBRACE body_item_list RBRACE. 

// Composition Block
composition_block ::= comp_head LBRACE comp_body RBRACE. {
    ctx->current_composition = nullptr;
}

comp_head ::= comp_type(CT) IDENTIFIER(ID). {
    ctx->current_composition = std::make_shared<AstComposition>();
    ctx->current_composition->type = *CT;
    ctx->current_composition->name = *ID;
    ctx->program->views.push_back(ctx->current_composition);
}
comp_head ::= comp_type(CT). {
    ctx->current_composition = std::make_shared<AstComposition>();
    ctx->current_composition->type = *CT;
    ctx->program->views.push_back(ctx->current_composition);
}

comp_type(A) ::= VCONCAT(T). { A = T; }
comp_type(A) ::= HCONCAT(T). { A = T; }
comp_type(A) ::= FACET(T). { A = T; }
comp_type(A) ::= REPEAT(T). { A = T; }

comp_body ::= comp_body comp_item.
comp_body ::= .

comp_item ::= generic_stmt. 
comp_item ::= SPEC COLON LBRACE statement_list RBRACE.
comp_item ::= RESOLVE COLON LBRACE body_item_list RBRACE.
