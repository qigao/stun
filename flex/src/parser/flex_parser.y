/*
 * Flex DSL Parser - Lemon Grammar
 * Generates LALR parser for .flex files
 *
 * Uses pointer-based semantic values to avoid C union limitations with C++ types
 */

%include {
#include "parser/flex_ast.h"
#include "parser/flex_token.h"
#include <memory>
#include <vector>
#include <stack>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace flex {
namespace parser {

// Parse context - manages AST construction state
struct ParseContext {
    AstProgram* program;

    // Node construction stack
    std::stack<std::shared_ptr<AstNode>> node_stack;

    // Current scene being built
    std::shared_ptr<AstScene> current_scene;

    // Current animation being built
    std::shared_ptr<AstAnim> current_anim;
    AstTrack current_track;

    // Current machine being built
    std::shared_ptr<AstMachine> current_machine;
    AstLayer current_layer;
    AstState current_state;
    AstTransition current_transition;

    // Value stack for passing semantic values
    std::stack<AstValue> value_stack;

    // Error state
    std::string error_message;
    int error_line = 0;
    int error_column = 0;

    ParseContext(AstProgram* p) : program(p), current_track("") {}
};

} // namespace parser
} // namespace flex

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

scene_block ::= scene_start scene_body RBRACE. {
    if (ctx->current_scene) {
        ctx->program->scene = ctx->current_scene;
        ctx->current_scene = nullptr;
    }
}

scene_start ::= SCENE IDENTIFIER LBRACE.
scene_start ::= ARTBOARD IDENTIFIER LBRACE.

scene_body ::= scene_body scene_item.
scene_body ::= .

scene_item ::= scene_property.
scene_item ::= node_def.
scene_item ::= repeat_block.
scene_item ::= for_block.

scene_property ::= IDENTIFIER COLON value_expr opt_comma.

// ============================================================================
// Node Definition
// ============================================================================

node_def ::= node_start node_body RBRACE. {
    if (!ctx->node_stack.empty()) {
        auto node = ctx->node_stack.top();
        ctx->node_stack.pop();

        if (!ctx->node_stack.empty()) {
            ctx->node_stack.top()->children.push_back(node);
        } else if (ctx->current_scene) {
            ctx->current_scene->children.push_back(node);
        }
    }
}

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

node_property ::= IDENTIFIER COLON value_expr opt_comma.

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
term ::= BINDING.

opt_comma ::= COMMA.
opt_comma ::= .

// ============================================================================
// Pseudo-class Block
// ============================================================================

pseudo_class_block ::= pseudo_start pseudo_props RBRACE.

pseudo_start ::= IDENTIFIER LBRACE.

pseudo_props ::= pseudo_props pseudo_prop.
pseudo_props ::= .

pseudo_prop ::= IDENTIFIER COLON value_expr opt_comma.

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

anim_block ::= anim_start anim_body RBRACE. {
    if (ctx->current_anim) {
        ctx->program->animations.push_back(ctx->current_anim);
        ctx->current_anim = nullptr;
    }
}

anim_start ::= ANIM STRING LBRACE.

anim_body ::= anim_body anim_item.
anim_body ::= .

anim_item ::= anim_property.
anim_item ::= track_block.

anim_property ::= IDENTIFIER COLON value_expr opt_comma.

// ============================================================================
// Track Block
// ============================================================================

track_block ::= track_start track_body RBRACE. {
    if (ctx->current_anim) {
        ctx->current_anim->tracks.push_back(ctx->current_track);
    }
    ctx->current_track = AstTrack("");
}

track_start ::= TRACK STRING LBRACE.
track_start ::= TRACK HASH IDENTIFIER SLASH IDENTIFIER LBRACE.

track_body ::= track_body track_item.
track_body ::= .

track_item ::= KEYFRAME NUMBER ARROW value_expr.

// ============================================================================
// State Machine Block
// ============================================================================

machine_block ::= machine_start machine_body RBRACE. {
    if (ctx->current_machine) {
        ctx->program->machines.push_back(ctx->current_machine);
        ctx->current_machine = nullptr;
    }
}

machine_start ::= MACHINE IDENTIFIER LBRACE.

machine_body ::= machine_body layer_block.
machine_body ::= .

// ============================================================================
// Layer Block
// ============================================================================

layer_block ::= layer_start layer_body RBRACE. {
    if (ctx->current_machine) {
        ctx->current_machine->layers.push_back(ctx->current_layer);
    }
    ctx->current_layer = AstLayer("");
}

layer_start ::= LAYER IDENTIFIER LBRACE.

layer_body ::= layer_body layer_item.
layer_body ::= .

layer_item ::= state_block.
layer_item ::= transition_stmt.

// ============================================================================
// State Block
// ============================================================================

state_block ::= state_start state_body RBRACE. {
    ctx->current_layer.states.push_back(ctx->current_state);
    ctx->current_state = AstState("");
}

state_start ::= STATE IDENTIFIER LBRACE.

state_body ::= state_body state_prop.
state_body ::= .

state_prop ::= IDENTIFIER COLON value_expr opt_comma.

// ============================================================================
// Transition Statement
// ============================================================================

transition_stmt ::= TRANSITION IDENTIFIER ARROW IDENTIFIER transition_condition. {
    ctx->current_layer.transitions.push_back(ctx->current_transition);
    ctx->current_transition = AstTransition();
}

transition_condition ::= WHEN IDENTIFIER GT NUMBER.
transition_condition ::= WHEN IDENTIFIER LT NUMBER.
transition_condition ::= WHEN IDENTIFIER EQ NUMBER.
transition_condition ::= WHEN IDENTIFIER NEQ NUMBER.
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
