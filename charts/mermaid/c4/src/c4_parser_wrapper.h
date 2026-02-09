#ifndef C4_PARSER_WRAPPER_H
#define C4_PARSER_WRAPPER_H

#include "c4/c4_ast.h"

#ifdef __cplusplus
extern "C" {
#endif

// Function prototypes used in parser
C4Element* c4_create_element(C4ElementType type, C4Attribute* attrs);
void c4_add_element(C4ParserContext* ctx, C4Element* el);
C4Rel* c4_create_rel(C4RelType type, C4Attribute* attrs);
void c4_add_rel(C4ParserContext* ctx, C4Rel* rel);

// Main parse function
C4Diagram* c4_parse(const char* input);
void c4_free_diagram(C4Diagram* diagram);

// JSON serialization
char* c4_to_json(C4Diagram* diagram);

#ifdef __cplusplus
}
#endif

#endif // C4_PARSER_WRAPPER_H
