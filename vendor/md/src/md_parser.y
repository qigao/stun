%include {
#include "md_re2c.h"
#include <iostream>
}

%name MDParse
%token_prefix TOK_
%token_type { std::string* }
%extra_argument { md_re2c::ParseContext* ctx }

%left NEWLINE.
%left LIST_MARKER OL_MARKER PIPE.
%right STAR DOUBLE_STAR UNDERSCORE DOUBLE_UNDERSCORE TILDE_TILDE.
%right LBRACKET RBRACKET LPAREN RPAREN BANG.
%right U_OPEN U_CLOSE.
%nonassoc TEXT NBSP BR.

%type doc { md_re2c::Node* }
%type blocks { md_re2c::Node* }
%type block { md_re2c::Node* }
%type header_block { md_re2c::Node* }
%type paragraph_block { md_re2c::Node* }
%type inline { md_re2c::Node* }
%type inlines { md_re2c::Node* }
%type inline_or_pipe { md_re2c::Node* }
%type unordered_list { md_re2c::Node* }
%type ordered_list { md_re2c::Node* }
%type unordered_item { md_re2c::Node* }
%type ordered_item { md_re2c::Node* }

%start_symbol doc

doc ::= blocks(B). { ctx->root->children = std::move(B->children); }

blocks(A) ::= blocks(L) block(R). { 
    if (R) L->children.push_back(R);
    A = L;
}
blocks(A) ::= . { A = ctx->create_node(md_re2c::NodeType::Document); }

block(A) ::= header_block(B). { A = B; }
block(A) ::= paragraph_block(B). { A = B; }
block(A) ::= unordered_list(B). [NEWLINE] { A = B; }
block(A) ::= ordered_list(B). [NEWLINE] { A = B; }
block(A) ::= HR NEWLINE. { A = ctx->create_node(md_re2c::NodeType::HorizontalRule); }
block(A) ::= HR_TAG NEWLINE. { A = ctx->create_node(md_re2c::NodeType::HorizontalRule); }
block(A) ::= NEWLINE. { A = nullptr; }

header_block(A) ::= H1 inlines(B) NEWLINE. { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 1; for(auto c : B->children) A->children.push_back(c); }
header_block(A) ::= H2 inlines(B) NEWLINE. { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 2; for(auto c : B->children) A->children.push_back(c); }
header_block(A) ::= H3 inlines(B) NEWLINE. { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 3; for(auto c : B->children) A->children.push_back(c); }
header_block(A) ::= H4 inlines(B) NEWLINE. { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 4; for(auto c : B->children) A->children.push_back(c); }
header_block(A) ::= H5 inlines(B) NEWLINE. { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 5; for(auto c : B->children) A->children.push_back(c); }
header_block(A) ::= H6 inlines(B) NEWLINE. { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 6; for(auto c : B->children) A->children.push_back(c); }

paragraph_block(A) ::= inlines(B) NEWLINE. {
    A = ctx->create_node(md_re2c::NodeType::Paragraph);
    for(auto c : B->children) A->children.push_back(c);
}

unordered_list(A) ::= unordered_list(L) unordered_item(R). { L->children.push_back(R); A = L; }
unordered_list(A) ::= unordered_item(I). { A = ctx->create_node(md_re2c::NodeType::List); A->children.push_back(I); }

unordered_item(A) ::= LIST_MARKER inlines(B) NEWLINE. { 
    A = ctx->create_node(md_re2c::NodeType::ListItem); 
    for(auto c : B->children) A->children.push_back(c);
}

ordered_list(A) ::= ordered_list(L) ordered_item(R). { L->children.push_back(R); A = L; }
ordered_list(A) ::= ordered_item(I). { A = ctx->create_node(md_re2c::NodeType::OrderedList); A->children.push_back(I); }

ordered_item(A) ::= OL_MARKER inlines(B) NEWLINE. { 
    A = ctx->create_node(md_re2c::NodeType::ListItem); 
    for(auto c : B->children) A->children.push_back(c);
}

inlines(A) ::= inlines(L) inline_or_pipe(R). {
    L->children.push_back(R);
    A = L;
}
inlines(A) ::= inline_or_pipe(I). {
    A = ctx->create_node(md_re2c::NodeType::Text);
    A->children.push_back(I);
}

inline_or_pipe(A) ::= inline(I). { A = I; }
inline_or_pipe(A) ::= PIPE. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "|"; }

inline(A) ::= TEXT(B). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = *B; delete B; }
inline(A) ::= NBSP. { A = ctx->create_node(md_re2c::NodeType::HtmlEntity); A->text = "&nbsp;"; }
inline(A) ::= BR. { A = ctx->create_node(md_re2c::NodeType::LineBreak); }

inline(A) ::= STAR. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "*"; }
inline(A) ::= DOUBLE_STAR. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "**"; }
inline(A) ::= UNDERSCORE. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "_"; }
inline(A) ::= DOUBLE_UNDERSCORE. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "__"; }
inline(A) ::= TILDE_TILDE. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "~~"; }

inline(A) ::= LBRACKET. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "["; }
inline(A) ::= RBRACKET. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "]"; }
inline(A) ::= LPAREN. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "("; }
inline(A) ::= RPAREN. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = ")"; }
inline(A) ::= BANG. { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "!"; }

inline(A) ::= U_OPEN inlines(B) U_CLOSE. { A = ctx->create_node(md_re2c::NodeType::Underline); for(auto c : B->children) A->children.push_back(c); }

%syntax_error {
    std::cerr << "Syntax error in Markdown" << std::endl;
}

// Add destructor to clean up tokens not consumed by actions
%token_destructor { delete $$; }
