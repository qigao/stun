%include {
#include "md_re2c.h"
#include <iostream>
}

%name MDParse
%token_prefix TOK_
%token_type { std::string* }
%extra_argument { md_re2c::ParseContext* ctx }

// Resolve shift/reduce conflicts using precedence hierarchy
%left LOW_PRIO.
%left NEWLINE.
%left TEXT NBSP BR.
%left PIPE.
%left STAR DOUBLE_STAR UNDERSCORE DOUBLE_UNDERSCORE TILDE_TILDE U_OPEN U_CLOSE.
%left LBRACKET RBRACKET LPAREN RPAREN BANG.
%left BACKTICK.
%left H1 H2 H3 H4 H5 H6.
%left HR HR_TAG.
%left CODE_FENCE.
%left LIST_MARKER OL_MARKER.
%left HIGH_PRIO.

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
%type unordered_items { md_re2c::Node* }
%type ordered_items { md_re2c::Node* }
%type unordered_item { md_re2c::Node* }
%type ordered_item { md_re2c::Node* }
%type code_block { md_re2c::Node* }
%type code_lines { md_re2c::Node* }
%type code_line { md_re2c::Node* }
%type code_lines_opt { md_re2c::Node* }

%start_symbol doc

doc ::= blocks(B). { ctx->root->children = std::move(B->children); }
doc ::= . { }

blocks(A) ::= blocks(L) block(R). { 
    if (R) L->children.push_back(R);
    A = L;
}
blocks(A) ::= block(B). [LOW_PRIO] { 
    A = ctx->create_node(md_re2c::NodeType::Document);
    if (B) A->children.push_back(B);
}

block(A) ::= header_block(B). { A = B; }
block(A) ::= unordered_list(B). { A = B; }
block(A) ::= ordered_list(B). { A = B; }
block(A) ::= code_block(B). { A = B; }
block(A) ::= HR(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::HorizontalRule); delete X; delete Y; }
block(A) ::= HR_TAG(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::HorizontalRule); delete X; delete Y; }
block(A) ::= paragraph_block(B). { A = B; }
block(A) ::= NEWLINE(X). { A = nullptr; delete X; }

header_block(A) ::= H1(X) inlines(B) NEWLINE(Y). { 
    A = ctx->create_node(md_re2c::NodeType::Header); 
    A->level = 1; 
    for(auto c : B->children) A->children.push_back(c); 
    delete X; delete Y;
}
header_block(A) ::= H1(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 1; delete X; delete Y; }
header_block(A) ::= H2(X) inlines(B) NEWLINE(Y). { 
    A = ctx->create_node(md_re2c::NodeType::Header); 
    A->level = 2; 
    for(auto c : B->children) A->children.push_back(c); 
    delete X; delete Y;
}
header_block(A) ::= H2(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 2; delete X; delete Y; }
header_block(A) ::= H3(X) inlines(B) NEWLINE(Y). { 
    A = ctx->create_node(md_re2c::NodeType::Header); 
    A->level = 3; 
    for(auto c : B->children) A->children.push_back(c); 
    delete X; delete Y;
}
header_block(A) ::= H3(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 3; delete X; delete Y; }
header_block(A) ::= H4(X) inlines(B) NEWLINE(Y). { 
    A = ctx->create_node(md_re2c::NodeType::Header); 
    A->level = 4; 
    for(auto c : B->children) A->children.push_back(c); 
    delete X; delete Y;
}
header_block(A) ::= H4(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 4; delete X; delete Y; }
header_block(A) ::= H5(X) inlines(B) NEWLINE(Y). { 
    A = ctx->create_node(md_re2c::NodeType::Header); 
    A->level = 5; 
    for(auto c : B->children) A->children.push_back(c); 
    delete X; delete Y;
}
header_block(A) ::= H5(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 5; delete X; delete Y; }
header_block(A) ::= H6(X) inlines(B) NEWLINE(Y). { 
    A = ctx->create_node(md_re2c::NodeType::Header); 
    A->level = 6; 
    for(auto c : B->children) A->children.push_back(c); 
    delete X; delete Y;
}
header_block(A) ::= H6(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::Header); A->level = 6; delete X; delete Y; }

paragraph_block(A) ::= inlines(B) NEWLINE(X). {
    A = ctx->create_node(md_re2c::NodeType::Paragraph);
    for(auto c : B->children) A->children.push_back(c);
    delete X;
}

code_block(A) ::= CODE_FENCE(F) code_lines_opt(B) CODE_FENCE(E). {
    A = ctx->create_node(md_re2c::NodeType::CodeBlock);
    if (F && F->size() > 3) {
        std::string lang = F->substr(3);
        size_t last = lang.find_last_not_of(" \t\n\r");
        if (last != std::string::npos) lang = lang.substr(0, last + 1);
        A->text = lang;
    }
    if (B) for(auto c : B->children) A->children.push_back(c);
    delete F; delete E;
}

code_lines_opt(A) ::= code_lines(B). { A = B; }
code_lines_opt(A) ::= . [LOW_PRIO] { A = nullptr; }

code_lines(A) ::= code_lines(L) code_line(R). {
    L->children.push_back(R);
    A = L;
}
code_lines(A) ::= code_line(I). [LOW_PRIO] {
    A = ctx->create_node(md_re2c::NodeType::Document);
    A->children.push_back(I);
}

code_line(A) ::= inlines(B) NEWLINE(X). { 
    A = ctx->create_node(md_re2c::NodeType::Document); 
    for(auto c : B->children) A->children.push_back(c); 
    delete X;
}
code_line(A) ::= NEWLINE(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = ""; delete X; }

unordered_list(A) ::= unordered_items(B). [LOW_PRIO] { A = B; }
unordered_items(A) ::= unordered_items(L) unordered_item(R). { L->children.push_back(R); A = L; }
unordered_items(A) ::= unordered_item(I). [LOW_PRIO] { 
    A = ctx->create_node(md_re2c::NodeType::List); 
    A->children.push_back(I); 
}

unordered_item(A) ::= LIST_MARKER(X) inlines(B) NEWLINE(Y). { 
    A = ctx->create_node(md_re2c::NodeType::ListItem); 
    for(auto c : B->children) A->children.push_back(c);
    delete X; delete Y;
}
unordered_item(A) ::= LIST_MARKER(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::ListItem); delete X; delete Y; }

ordered_list(A) ::= ordered_items(B). [LOW_PRIO] { A = B; }
ordered_items(A) ::= ordered_items(L) ordered_item(R). { L->children.push_back(R); A = L; }
ordered_items(A) ::= ordered_item(I). [LOW_PRIO] { 
    A = ctx->create_node(md_re2c::NodeType::OrderedList); 
    A->children.push_back(I); 
}

ordered_item(A) ::= OL_MARKER(X) inlines(B) NEWLINE(Y). { 
    A = ctx->create_node(md_re2c::NodeType::ListItem); 
    for(auto c : B->children) A->children.push_back(c);
    delete X; delete Y;
}
ordered_item(A) ::= OL_MARKER(X) NEWLINE(Y). { A = ctx->create_node(md_re2c::NodeType::ListItem); delete X; delete Y; }

inlines(A) ::= inlines(L) inline_or_pipe(R). {
    L->children.push_back(R);
    A = L;
}
inlines(A) ::= inline_or_pipe(I). [LOW_PRIO] {
    A = ctx->create_node(md_re2c::NodeType::Document);
    A->children.push_back(I);
}

inline_or_pipe(A) ::= inline(I). { A = I; }
inline_or_pipe(A) ::= PIPE(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "|"; delete X; }

inline(A) ::= TEXT(B). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = *B; delete B; }
inline(A) ::= NBSP(X). { A = ctx->create_node(md_re2c::NodeType::HtmlEntity); A->text = "&nbsp;"; delete X; }
inline(A) ::= BR(X). { A = ctx->create_node(md_re2c::NodeType::LineBreak); delete X; }
inline(A) ::= STAR(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "*"; delete X; }
inline(A) ::= DOUBLE_STAR(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "**"; delete X; }
inline(A) ::= UNDERSCORE(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "_"; delete X; }
inline(A) ::= DOUBLE_UNDERSCORE(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "__"; delete X; }
inline(A) ::= TILDE_TILDE(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "~~"; delete X; }
inline(A) ::= LBRACKET(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "["; delete X; }
inline(A) ::= RBRACKET(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "]"; delete X; }
inline(A) ::= LPAREN(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "("; delete X; }
inline(A) ::= RPAREN(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = ")"; delete X; }
inline(A) ::= BANG(X). { A = ctx->create_node(md_re2c::NodeType::Text); A->text = "!"; delete X; }

inline(A) ::= U_OPEN(X) inlines(B) U_CLOSE(Y). { 
    A = ctx->create_node(md_re2c::NodeType::Underline); 
    for(auto c : B->children) A->children.push_back(c); 
    delete X; delete Y;
}
inline(A) ::= U_OPEN(X) U_CLOSE(Y). { A = ctx->create_node(md_re2c::NodeType::Underline); delete X; delete Y; }

inline(A) ::= BACKTICK(X) inlines(B) BACKTICK(Y). { 
    A = ctx->create_node(md_re2c::NodeType::CodeSpan); 
    for(auto c : B->children) A->children.push_back(c);
    delete X; delete Y;
}
inline(A) ::= BACKTICK(X) BACKTICK(Y). { A = ctx->create_node(md_re2c::NodeType::CodeSpan); delete X; delete Y; }

%syntax_error {
    // Silenced for TUI stability
}

// Destructor for tokens that own heap memory (std::string*)
%destructor NEWLINE { delete $$; }
%destructor PIPE { delete $$; }
%destructor STAR { delete $$; }
%destructor DOUBLE_STAR { delete $$; }
%destructor UNDERSCORE { delete $$; }
%destructor DOUBLE_UNDERSCORE { delete $$; }
%destructor TILDE_TILDE { delete $$; }
%destructor LBRACKET { delete $$; }
%destructor RBRACKET { delete $$; }
%destructor LPAREN { delete $$; }
%destructor RPAREN { delete $$; }
%destructor BANG { delete $$; }
%destructor BACKTICK { delete $$; }
%destructor TEXT { delete $$; }
%destructor NBSP { delete $$; }
%destructor BR { delete $$; }
%destructor CODE_FENCE { delete $$; }
%destructor LIST_MARKER { delete $$; }
%destructor OL_MARKER { delete $$; }
%destructor H1 { delete $$; }
%destructor H2 { delete $$; }
%destructor H3 { delete $$; }
%destructor H4 { delete $$; }
%destructor H5 { delete $$; }
%destructor H6 { delete $$; }
%destructor HR { delete $$; }
%destructor HR_TAG { delete $$; }
%destructor U_OPEN { delete $$; }
%destructor U_CLOSE { delete $$; }
