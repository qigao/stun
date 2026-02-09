#ifndef SANKEY_AST_H
#define SANKEY_AST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SankeyLink {
    char* source;
    char* target;
    double value;
    struct SankeyLink* next;
} SankeyLink;

typedef struct SankeyDiagram {
    SankeyLink* links;
} SankeyDiagram;

typedef struct {
    SankeyDiagram* diagram;
    int error_count;
    char* error_message;
} SankeyParserContext;

SankeyDiagram* sankey_create_diagram();
void sankey_free_diagram(SankeyDiagram* d);

void sankey_add_link(SankeyParserContext* ctx, const char* source, const char* target, double value);

char* sankey_to_json(SankeyDiagram* d);

#ifdef __cplusplus
}
#endif

#endif // SANKEY_AST_H
