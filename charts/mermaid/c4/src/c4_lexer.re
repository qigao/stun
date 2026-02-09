#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "c4/c4_ast.h"
#include "c4_parser_gen.h"

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

static char* copy_token(const char* start, const char* end) {
    size_t len = end - start;
    char* res = (char*)malloc(len + 1);
    memcpy(res, start, len);
    res[len] = '\0';
    return res;
}

static char* copy_quoted(const char* start, const char* end) {
    if (end - start < 2) return strdup("");
    size_t len = (end - start) - 2;
    char* res = (char*)malloc(len + 1);
    memcpy(res, start + 1, len);
    res[len] = '\0';
    return res;
}

void c4_scan(Scanner *s, void *parser, C4ParserContext *ctx) {
    const char *token;

    loop:
    if (s->cursor >= s->limit || *s->cursor == '\0') {
        C4Parser(parser, C4_EOF, NULL, ctx);
        return;
    }
    
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        newline = [\n];
        ws = [ \t]+;
        comment = ("%%"| "#") [^\n]* newline;

        newline { s->line++; C4Parser(parser, C4_NL, NULL, ctx); goto loop; }
        comment { s->line++; C4Parser(parser, C4_NL, NULL, ctx); goto loop; }
        ws { goto loop; }

        "C4Context"    { C4Parser(parser, C4_C4_CONTEXT, NULL, ctx); goto loop; }
        "C4Container"  { C4Parser(parser, C4_C4_CONTAINER, NULL, ctx); goto loop; }
        "C4Component"  { C4Parser(parser, C4_C4_COMPONENT, NULL, ctx); goto loop; }
        
        "Person_Ext"      { C4Parser(parser, C4_PERSON_EXT, strdup("Person_Ext"), ctx); goto loop; }
        "Person"          { C4Parser(parser, C4_PERSON, strdup("Person"), ctx); goto loop; }
        "System_Ext_Db"   { C4Parser(parser, C4_SYSTEM_EXT_DB, strdup("System_Ext_Db"), ctx); goto loop; }
        "System_Ext_Queue" { C4Parser(parser, C4_SYSTEM_EXT_QUEUE, strdup("System_Ext_Queue"), ctx); goto loop; }
        "System_Ext"      { C4Parser(parser, C4_SYSTEM_EXT, strdup("System_Ext"), ctx); goto loop; }
        "SystemDb"        { C4Parser(parser, C4_SYSTEM_DB, strdup("SystemDb"), ctx); goto loop; }
        "SystemQueue"     { C4Parser(parser, C4_SYSTEM_QUEUE, strdup("SystemQueue"), ctx); goto loop; }
        "System"          { C4Parser(parser, C4_SYSTEM, strdup("System"), ctx); goto loop; }

        "Enterprise_Boundary" { C4Parser(parser, C4_ENTERPRISE_BOUNDARY, strdup("Enterprise_Boundary"), ctx); goto loop; }
        "System_Boundary"     { C4Parser(parser, C4_SYSTEM_BOUNDARY, strdup("System_Boundary"), ctx); goto loop; }
        "Container_Boundary"  { C4Parser(parser, C4_CONTAINER_BOUNDARY, strdup("Container"), ctx); goto loop; }
        "Boundary"            { C4Parser(parser, C4_BOUNDARY, strdup("Boundary"), ctx); goto loop; }

        "Container_Ext_Db"    { C4Parser(parser, C4_CONTAINER_EXT_DB, strdup("Container_Ext_Db"), ctx); goto loop; }
        "Container_Ext_Queue" { C4Parser(parser, C4_CONTAINER_EXT_QUEUE, strdup("Container_Ext_Queue"), ctx); goto loop; }
        "Container_Ext"       { C4Parser(parser, C4_CONTAINER_EXT, strdup("Container_Ext"), ctx); goto loop; }
        "ContainerDb"         { C4Parser(parser, C4_CONTAINER_DB, strdup("ContainerDb"), ctx); goto loop; }
        "ContainerQueue"      { C4Parser(parser, C4_CONTAINER_QUEUE, strdup("ContainerQueue"), ctx); goto loop; }
        "Container"           { C4Parser(parser, C4_CONTAINER, strdup("Container"), ctx); goto loop; }

        "Component_Ext_Db"    { C4Parser(parser, C4_COMPONENT_EXT_DB, strdup("Component_Ext_Db"), ctx); goto loop; }
        "Component_Ext_Queue" { C4Parser(parser, C4_COMPONENT_EXT_QUEUE, strdup("Component_Ext_Queue"), ctx); goto loop; }
        "Component_Ext"       { C4Parser(parser, C4_COMPONENT_EXT, strdup("Component_Ext"), ctx); goto loop; }
        "ComponentDb"         { C4Parser(parser, C4_COMPONENT_DB, strdup("ComponentDb"), ctx); goto loop; }
        "ComponentQueue"      { C4Parser(parser, C4_COMPONENT_QUEUE, strdup("ComponentQueue"), ctx); goto loop; }
        "Component"           { C4Parser(parser, C4_COMPONENT, strdup("Component"), ctx); goto loop; }

        "Rel_U" | "Rel_Up"    { C4Parser(parser, C4_REL_U, NULL, ctx); goto loop; }
        "Rel_D" | "Rel_Down"  { C4Parser(parser, C4_REL_D, NULL, ctx); goto loop; }
        "Rel_L" | "Rel_Left"  { C4Parser(parser, C4_REL_L, NULL, ctx); goto loop; }
        "Rel_R" | "Rel_Right" { C4Parser(parser, C4_REL_R, NULL, ctx); goto loop; }
        "Rel_Back"            { C4Parser(parser, C4_REL_B, NULL, ctx); goto loop; }
        "BiRel"               { C4Parser(parser, C4_BIREL, NULL, ctx); goto loop; }
        "Rel"                 { C4Parser(parser, C4_REL, NULL, ctx); goto loop; }

        "UpdateElementStyle"  { C4Parser(parser, C4_UPDATE_EL_STYLE, NULL, ctx); goto loop; }
        "UpdateRelStyle"      { C4Parser(parser, C4_UPDATE_REL_STYLE, NULL, ctx); goto loop; }
        "UpdateLayoutConfig"  { C4Parser(parser, C4_UPDATE_LAYOUT_CONFIG, NULL, ctx); goto loop; }

        "title" [ \t]+        { goto title_state; }
        "accTitle" [ \t]* ":" [ \t]* { C4Parser(parser, C4_ACC_TITLE, NULL, ctx); goto acc_value_state; }
        "accDescr" [ \t]* ":" [ \t]* { C4Parser(parser, C4_ACC_DESCR, NULL, ctx); goto acc_value_state; }

        "{"                   { C4Parser(parser, C4_LBRACE, NULL, ctx); goto loop; }
        "}"                   { C4Parser(parser, C4_RBRACE, NULL, ctx); goto loop; }
        "("                   { C4Parser(parser, C4_LPAREN, NULL, ctx); goto attr_state; }

        "\000" { C4Parser(parser, C4_EOF, NULL, ctx); return; }
        * { s->cursor++; goto loop; }
    */

    attr_state:
    if (s->cursor >= s->limit || *s->cursor == '\0') {
        C4Parser(parser, C4_EOF, NULL, ctx);
        return;
    }
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;

        ")" { C4Parser(parser, C4_RPAREN, NULL, ctx); goto loop; }
        "," { C4Parser(parser, C4_COMMA, NULL, ctx); goto attr_state; }
        
        "\"" [^\"]* "\"" {
            C4Parser(parser, C4_STRING, copy_quoted(token, s->cursor), ctx);
            goto attr_state;
        }

        [a-zA-Z0-9_\-\.]+ {
             C4Parser(parser, C4_STR_UNQUOTED, copy_token(token, s->cursor), ctx);
             goto attr_state;
        }
        
        "=" { C4Parser(parser, C4_EQUALS, NULL, ctx); goto attr_state; }
        
        [ \t\n]+ { 
            if(*token == '\n') s->line++;
            goto attr_state; 
        }

        * { s->cursor++; goto attr_state; }
    */

    title_state:
    if (s->cursor >= s->limit || *s->cursor == '\0') {
        C4Parser(parser, C4_EOF, NULL, ctx);
        return;
    }
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;
        
        [^\n\000]+ {
            C4Parser(parser, C4_TITLE_VALUE, copy_token(token, s->cursor), ctx);
            goto loop;
        }
        * { goto loop; }
    */

    acc_value_state:
    if (s->cursor >= s->limit || *s->cursor == '\0') {
        C4Parser(parser, C4_EOF, NULL, ctx);
        return;
    }
    token = s->cursor;
    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = s->cursor;
        re2c:define:YYLIMIT = s->limit;
        re2c:define:YYMARKER = s->marker;
        re2c:yyfill:enable = 0;
        
        [^\n\000]+ {
            C4Parser(parser, C4_ACC_VALUE, copy_token(token, s->cursor), ctx);
            goto loop;
        }
        * { goto loop; }
    */
}
