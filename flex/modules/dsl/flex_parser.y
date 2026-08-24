/*
 * Flex DSL Parser - Lemon Grammar
 * Generates LALR parser for .flex files
 *
 * Uses pointer-based semantic values to avoid C union limitations with C++ types
 */

%include {
#include "flex/dsl/detail/flex_parse_context.h"
#include "flex/dsl/flex_token.h"

using namespace flex::parser;
}

// Parser name - controls generated function names (Parse, ParseAlloc, ParseFree)
%name Parse

// Use int as token type (token ID only), actual Token stored in lexer
%token_prefix TOK_
%token_type { int }
%default_type { int }

// Extra argument - parse context
%extra_argument { ParseContext* ctx }

// Keep nesting bounded while allowing substantially deeper documents than
// Lemon's default stack. Overflow is reported instead of silently ignored.
%stack_size 256

// Error handling
%syntax_error {
    ctx->error_message = "Syntax error";
    ctx->error_line = 0;
    ctx->error_column = 0;
}

%parse_failure {
    if (ctx->error_message.empty()) {
        ctx->error_message = "Parser failed";
    }
}

%stack_overflow {
    ctx->error_message = "Parser nesting limit exceeded";
}

// Start symbol
%start_symbol program

// ============================================================================
// Program Structure
// ============================================================================

program ::= top_level_list.

top_level_list ::= top_level_list top_level_block.
top_level_list ::= .

top_level_block ::= import_stmt.
top_level_block ::= const_decl.
top_level_block ::= var_decl.
top_level_block ::= scene_block.
top_level_block ::= ui_block.
top_level_block ::= component_block.
top_level_block ::= anim_block.
top_level_block ::= machine_block.
top_level_block ::= data_block.
top_level_block ::= assets_block.

// ============================================================================
// Import Statement - import "path/to/file.flex"
// ============================================================================

import_stmt ::= IMPORT STRING.

// ============================================================================
// Const/Var Declarations - const name = expr, var name = expr
// ============================================================================

const_decl ::= CONST IDENTIFIER ASSIGN value_expr.
var_decl ::= VAR IDENTIFIER ASSIGN value_expr.

// ============================================================================
// Scene Block
// ============================================================================

scene_block ::= scene_start scene_body RBRACE.

scene_start ::= SCENE IDENTIFIER LBRACE.
scene_start ::= ARTBOARD IDENTIFIER LBRACE.

scene_body ::= scene_body scene_item.
scene_body ::= .

scene_item ::= scene_property.
scene_item ::= node_def.
scene_item ::= repeat_block.
scene_item ::= for_block.

scene_property ::= prop_key COLON value_expr opt_comma.

// ============================================================================
// UI Document Block
// ============================================================================

ui_block ::= UI IDENTIFIER LBRACE ui_body RBRACE.

ui_body ::= ui_body ui_item.
ui_body ::= .

ui_item ::= node_def.
ui_item ::= repeat_block.
ui_item ::= for_block.

// ============================================================================
// Node Definition
// ============================================================================

node_def ::= node_start node_body RBRACE.

node_start ::= NODE_TYPE IDENTIFIER LBRACE.
node_start ::= NODE_TYPE IDENTIFIER AT IDENTIFIER LBRACE.
node_start ::= IDENTIFIER IDENTIFIER LBRACE.
node_start ::= IDENTIFIER IDENTIFIER AT IDENTIFIER LBRACE.

node_body ::= node_body node_item.
node_body ::= .

node_item ::= node_property.
node_item ::= node_def.
node_item ::= pseudo_class_block.
node_item ::= repeat_block.
node_item ::= for_block.

node_property ::= prop_key COLON value_expr opt_comma.
node_property ::= DATA COLON value_expr opt_comma.

prop_key ::= IDENTIFIER.
prop_key ::= NODE_TYPE.
prop_key ::= IDENTIFIER DOT IDENTIFIER.
prop_key ::= IDENTIFIER DOT NODE_TYPE.

// ============================================================================
// Value Expressions
// ============================================================================

%left PLUS MINUS.
%left STAR SLASH.

value_expr ::= term.
value_expr ::= value_expr STAR value_expr.
value_expr ::= value_expr SLASH value_expr.
value_expr ::= value_expr PLUS value_expr.
value_expr ::= value_expr MINUS value_expr.
value_expr ::= LPAREN value_expr RPAREN.

term ::= NUMBER.
term ::= STRING.
term ::= COLOR.
term ::= BOOL.
term ::= IDENTIFIER.
term ::= DOLLAR IDENTIFIER.
term ::= HASH IDENTIFIER.
term ::= AT IDENTIFIER.
term ::= IDENTIFIER DOT IDENTIFIER.
term ::= IDENTIFIER LPAREN function_args RPAREN.
term ::= BINDING.

function_args ::= value_expr.
function_args ::= function_args COMMA value_expr.

opt_comma ::= COMMA.
opt_comma ::= .

// ============================================================================
// Pseudo-class Block
// ============================================================================

pseudo_class_block ::= pseudo_start pseudo_props RBRACE.

pseudo_start ::= IDENTIFIER LBRACE.

pseudo_props ::= pseudo_props pseudo_prop.
pseudo_props ::= .

pseudo_prop ::= prop_key COLON value_expr opt_comma.
pseudo_prop ::= DATA COLON value_expr opt_comma.

// ============================================================================
// Repeat Block - repeat N { ... }
// ============================================================================

repeat_block ::= REPEAT NUMBER LBRACE repeat_body RBRACE.

repeat_body ::= repeat_body node_def.
repeat_body ::= .

// ============================================================================
// Component Definition
// ============================================================================

component_block ::= COMPONENT IDENTIFIER LBRACE component_body RBRACE.

component_body ::= component_body component_item.
component_body ::= .

component_item ::= node_property.
component_item ::= node_def.

// ============================================================================
// Animation Block
// ============================================================================

anim_block ::= anim_start anim_body RBRACE.

anim_start ::= ANIM STRING LBRACE.

anim_body ::= anim_body anim_item.
anim_body ::= .

anim_item ::= anim_property.
anim_item ::= track_block.
anim_item ::= trigger_stmt.

anim_property ::= IDENTIFIER COLON value_expr opt_comma.
trigger_stmt ::= TRIGGER NUMBER ARROW STRING.

// ============================================================================
// Track Block
// ============================================================================

track_block ::= track_start track_body RBRACE.

track_start ::= TRACK STRING LBRACE.
track_start ::= TRACK HASH IDENTIFIER SLASH IDENTIFIER LBRACE.

track_body ::= track_body track_item.
track_body ::= .

track_item ::= KEYFRAME NUMBER ARROW value_expr.
track_item ::= IDENTIFIER COLON value_expr opt_comma.

// ============================================================================
// State Machine Block
// ============================================================================

machine_block ::= machine_start machine_body RBRACE.

machine_start ::= MACHINE IDENTIFIER LBRACE.

machine_body ::= machine_body layer_block.
machine_body ::= .

// ============================================================================
// Layer Block
// ============================================================================

layer_block ::= layer_start layer_body RBRACE.

layer_start ::= LAYER IDENTIFIER LBRACE.

layer_body ::= layer_body layer_item.
layer_body ::= .

layer_item ::= state_block.
layer_item ::= transition_stmt.

// ============================================================================
// State Block
// ============================================================================

state_block ::= state_start state_body RBRACE.

state_start ::= STATE IDENTIFIER LBRACE.

state_body ::= state_body state_prop.
state_body ::= .

state_prop ::= IDENTIFIER COLON value_expr opt_comma.
state_prop ::= SET HASH IDENTIFIER DOT IDENTIFIER COLON value_expr.
state_prop ::= PLAY STRING WITH LBRACE anim_params RBRACE.
state_prop ::= PLAY STRING.
state_prop ::= PLAY COLON value_expr opt_comma.

anim_params ::= anim_params anim_param.
anim_params ::= .

anim_param ::= IDENTIFIER COLON value_expr opt_comma.

// ============================================================================
// Transition Statement
// ============================================================================

transition_stmt ::= TRANSITION IDENTIFIER ARROW IDENTIFIER transition_condition.

transition_condition ::= WHEN IDENTIFIER GT NUMBER.
transition_condition ::= WHEN IDENTIFIER LT NUMBER.
transition_condition ::= WHEN IDENTIFIER EQ NUMBER.
transition_condition ::= WHEN IDENTIFIER NEQ NUMBER.
transition_condition ::= WHEN BINDING.
transition_condition ::= .

// ============================================================================
// Data Block - data name { item: { ... }, ... }
// ============================================================================

data_block ::= DATA IDENTIFIER LBRACE data_items RBRACE.

data_items ::= data_items data_item.
data_items ::= .

data_item ::= IDENTIFIER COLON LBRACE data_props RBRACE.

data_props ::= data_props data_prop.
data_props ::= .

data_prop ::= IDENTIFIER COLON value_expr opt_comma.
data_prop ::= DATA COLON value_expr opt_comma.

// ============================================================================
// For Loop Block - for item in data { ... }
// ============================================================================

for_block ::= FOR IDENTIFIER IN IDENTIFIER LBRACE for_body RBRACE.

for_body ::= for_body node_def.
for_body ::= .

// ============================================================================
// Assets Block - assets { audio click: "sounds/click.wav", ... }
// ============================================================================

assets_block ::= ASSETS LBRACE asset_items RBRACE.

asset_items ::= asset_items asset_item.
asset_items ::= .

// Simple asset: audio click: "sounds/click.wav"
asset_item ::= AUDIO IDENTIFIER COLON STRING.
asset_item ::= NODE_TYPE IDENTIFIER COLON STRING.
asset_item ::= FONT IDENTIFIER COLON STRING.

// Asset with options block: audio bgm: "bgm.mp3" { loop: true, volume: 0.5 }
asset_item ::= AUDIO IDENTIFIER COLON STRING LBRACE asset_opts RBRACE.
asset_item ::= NODE_TYPE IDENTIFIER COLON STRING LBRACE asset_opts RBRACE.
asset_item ::= FONT IDENTIFIER COLON STRING LBRACE asset_opts RBRACE.

asset_opts ::= asset_opts asset_opt.
asset_opts ::= .

asset_opt ::= IDENTIFIER COLON value_expr opt_comma.
