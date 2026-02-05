/*
 * Infographic DSL Parser - Lemon Grammar
 * 
 * Syntax:
 *   infographic <template-name>
 *   data { title: "..." items: [...] }
 *   theme { palette: #xxx #yyy }
 */

%include {
#include <ir/unified_infographic.h>
#include "infographic_token.h"
#include "infographic_context.h"
#include <memory>
#include <string>

using namespace flex::modules::infographic;
}

%name InfographicParse
%token_prefix TOK_
%token_type { std::string* }
%default_type { std::string* }
%extra_argument { flex::modules::infographic::ParseContext* ctx }

%start_symbol program

// ============================================================================
// Program Structure
// ============================================================================

program ::= header block_list.

header ::= INFOGRAPHIC TEMPLATE_NAME(T). {
    ctx->set_template(*T);
}
header ::= INFOGRAPHIC IDENTIFIER(T). {
    ctx->set_template(*T);
}

block_list ::= block_list block.
block_list ::= .

block ::= data_block.
block ::= theme_block.

// ============================================================================
// Data Block
// ============================================================================

data_block ::= DATA LBRACE data_body RBRACE.

data_body ::= data_body data_field.
data_body ::= .

data_field ::= TITLE COLON string_value(V). { ctx->set_title(V); }
data_field ::= DESC COLON string_value(V). { ctx->set_desc(V); }
data_field ::= ITEMS COLON LBRACKET items_list RBRACKET.
data_field ::= IDENTIFIER COLON value. {
    // Generic property - ignored for now
}

// ============================================================================
// Items Array
// ============================================================================

items_list ::= items_list item.
items_list ::= items_list COMMA item.
items_list ::= .

item ::= item_start item_body RBRACE. { ctx->end_item(); }

item_start ::= LBRACE. { ctx->begin_item(); }

item_body ::= item_body item_field.
item_body ::= item_body COMMA item_field.
item_body ::= .

item_field ::= LABEL COLON string_value(V). { ctx->set_item_label(V); }
item_field ::= DESC COLON string_value(V). { ctx->set_item_desc(V); }
item_field ::= VALUE COLON number_value(V). { ctx->set_item_value(V); }
item_field ::= ICON COLON string_value(V). { ctx->set_item_icon(V); }
item_field ::= ILLUS COLON string_value(V). { ctx->set_item_illus(V); }
item_field ::= DONE COLON bool_value(V). { ctx->set_item_done(V); }
item_field ::= CHILDREN COLON children_start items_list RBRACKET. { ctx->end_children(); }

children_start ::= LBRACKET. { ctx->begin_children(); }

// ============================================================================
// Theme Block
// ============================================================================

theme_block ::= THEME LBRACE theme_body RBRACE.
theme_block ::= THEME IDENTIFIER(V). { ctx->set_preset(V); }

theme_body ::= theme_body theme_field.
theme_body ::= .

theme_field ::= PALETTE COLON color_list.
theme_field ::= PRESET COLON IDENTIFIER(V). { ctx->set_preset(V); }
theme_field ::= STYLIZE COLON IDENTIFIER(V). { ctx->set_stylize(V); }

color_list ::= color_list COLOR(C). { ctx->add_palette_color(C); }
color_list ::= COLOR(C). { ctx->add_palette_color(C); }

// ============================================================================
// Value Types
// ============================================================================

string_value(A) ::= STRING(V). { A = V; }
string_value(A) ::= IDENTIFIER(V). { A = V; }
string_value(A) ::= TEMPLATE_NAME(V). { A = V; }

number_value(A) ::= NUMBER(V). { A = V; }

bool_value(A) ::= BOOL(V). { A = V; }

value(A) ::= STRING(V). { A = V; }
value(A) ::= NUMBER(V). { A = V; }
value(A) ::= BOOL(V). { A = V; }
value(A) ::= COLOR(V). { A = V; }
value(A) ::= IDENTIFIER(V). { A = V; }
