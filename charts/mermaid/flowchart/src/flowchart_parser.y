%name FlowchartParser
%token_prefix FC_
%extra_argument { FlowchartParserContext *ctx }

%token NEWLINE GRAPH FLOWCHART SUBGRAPH END DIR DIRECTION ARROW AMP CLASSDEF COMMA STYLE CLICK LINKSTYLE PUNCT
       DOUBLE_CIRCLE_START DOUBLE_CIRCLE_END CPS CPE STADIUM_START STADIUM_END
       SUBROUTINE_START SUBROUTINE_END HEXAGON_START HEXAGON_END
       RHOMBUS_START RHOMBUS_END ELLIPSE_START ELLIPSE_END
       CYLINDER_START CYLINDER_END TRAP_START INV_TRAP_START TRAP_END ODD_START
       SQS SQE PS PE PIPE SEMI COLON DASH HTML IDENTIFIER STRING CLASS EOF.

%token_type { char* }
%token_destructor { free($$); }
%type node { FlowchartNode* }
%type label_text { char* }
%type node_label { char* }
%type node_group { FlowchartNodeRef* }
%type edge_chain { FlowchartNodeRef* }
%type class_ids { FlowchartNodeRef* }
%type style_spec { FlowchartStyle* }
%type style_specs { FlowchartStyle* }
%destructor label_text { free($$); }
%destructor node_label { free($$); }
%destructor class_ids { free_node_refs($$); }
%destructor node_group { free_node_refs($$); }
%destructor edge_chain { free_node_refs($$); }
%destructor style_spec { free_styles($$); }
%destructor style_specs { free_styles($$); }

%include {
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "flowchart/flowchart_ast.h"

static void push_subgraph(FlowchartParserContext *ctx, FlowchartSubGraph* sg) {
    FlowchartSubGraphStack* s = (FlowchartSubGraphStack*)malloc(sizeof(FlowchartSubGraphStack));
    if (!s) {
        ctx->error_count++;
        return;
    }
    s->sg = sg;
    s->next = (FlowchartSubGraphStack*)ctx->active_subgraphs;
    ctx->active_subgraphs = s;
}

static FlowchartSubGraph* pop_subgraph(FlowchartParserContext *ctx) {
    FlowchartSubGraphStack* s = (FlowchartSubGraphStack*)ctx->active_subgraphs;
    if (!s) return NULL;
    FlowchartSubGraph* sg = s->sg;
    ctx->active_subgraphs = s->next;
    free(s);
    return sg;
}

static FlowchartSubGraph* current_subgraph(FlowchartParserContext *ctx) {
    FlowchartSubGraphStack* s = (FlowchartSubGraphStack*)ctx->active_subgraphs;
    return s ? s->sg : NULL;
}

static int has_node_ref(const FlowchartSubGraph* sg, const char* id) {
    if (!sg || !id) return 0;
    for (const FlowchartNodeRef* ref = sg->node_refs; ref; ref = ref->next)
        if (ref->id && strcmp(ref->id, id) == 0) return 1;
    return 0;
}

static int node_registered_in_another_subgraph(
    const FlowchartSubGraph* siblings,
    const FlowchartSubGraph* active,
    const char* id) {
    for (const FlowchartSubGraph* sg = siblings; sg; sg = sg->next) {
        if (sg != active && has_node_ref(sg, id))
            return 1;
        if (node_registered_in_another_subgraph(sg->children, active, id))
            return 1;
    }
    return 0;
}

static void add_node_ref(FlowchartParserContext* ctx, FlowchartSubGraph* sg, const char* id) {
    /* Mermaid's serialized subgraph node list reflects node references and
       edge endpoints, including repeated references. Preserve each occurrence. */
    FlowchartNodeRef* ref = (FlowchartNodeRef*)calloc(1, sizeof(FlowchartNodeRef));
    if (!ref) {
        ctx->error_count++;
        return;
    }
    ref->id = strdup(id);
    if (!ref->id) {
        free(ref);
        ctx->error_count++;
        return;
    }
    ref->next = sg->node_refs;
    sg->node_refs = ref;
}

static FlowchartNode* find_or_create_node(FlowchartParserContext *ctx, const char* id, const char* label) {
    if (!id) return NULL;
    FlowchartNode* curr = ctx->diagram->nodes;
    while (curr) {
        if (strcmp(curr->id, id) == 0) {
            /* Existing edge endpoints are recorded by add_edge(), once per
               explicit edge. Re-entering an existing node in another edge
               must not create a second implicit node declaration. */
            if (label && (!curr->label || strcmp(curr->label, id) == 0)) {
                char* replacement = strdup(label);
                if (!replacement) {
                    ctx->error_count++;
                    return NULL;
                }
                free(curr->label);
                curr->label = replacement;
            }
            return curr;
        }
        curr = curr->next;
    }
    FlowchartNode* n = (FlowchartNode*)calloc(1, sizeof(FlowchartNode));
    if (!n) {
        ctx->error_count++;
        return NULL;
    }
    n->id = strdup(id);
    n->label = label ? strdup(label) : strdup(id);
    if (!n->id || !n->label) {
        free(n->id);
        free(n->label);
        free(n);
        ctx->error_count++;
        return NULL;
    }
    n->shape = FC_SHAPE_RECT;
    n->next = ctx->diagram->nodes;
    ctx->diagram->nodes = n;
    
    FlowchartSubGraph* sg = current_subgraph(ctx);
    if (sg) add_node_ref(ctx, sg, id);

    return n;
}

static FlowchartSubGraph* find_parent_of_nested_subgraph(
    FlowchartSubGraph* siblings, const char* child_id) {
    for (FlowchartSubGraph* parent = siblings; parent; parent = parent->next) {
        for (FlowchartSubGraph* child = parent->children; child; child = child->next) {
            if (child->id && strcmp(child->id, child_id) == 0)
                return parent;
        }
        FlowchartSubGraph* ancestor = find_parent_of_nested_subgraph(parent->children, child_id);
        if (ancestor) return ancestor;
    }
    return NULL;
}

static void add_edge(FlowchartParserContext *ctx, const char* from, const char* to, const char* arrow, const char* label) {
    if (!from || !to) return;
    /* Nodes are recorded at declaration/reference time and edge endpoints are
       serialized as additional subgraph member references by Mermaid. */
    FlowchartSubGraph* active = current_subgraph(ctx);
    if (active) {
        add_node_ref(ctx, active, from);
        add_node_ref(ctx, active, to);
    } else {
        /* An edge between two sibling subgraphs contributes both endpoint
           references to their common owner even if the edge is declared after
           that owner's END. Mermaid keeps these as additional parent members.
           Prepend in target/source order to preserve the source-facing order
           in the head-inserted reference chain. */
        FlowchartSubGraph* owner_from =
            find_parent_of_nested_subgraph(ctx->diagram->subgraphs, from);
        FlowchartSubGraph* owner_to =
            find_parent_of_nested_subgraph(ctx->diagram->subgraphs, to);
        if (owner_from && owner_from == owner_to) {
            add_node_ref(ctx, owner_from, to);
            add_node_ref(ctx, owner_from, from);
        }
    }
    FlowchartEdge* e = (FlowchartEdge*)calloc(1, sizeof(FlowchartEdge));
    if (!e) {
        ctx->error_count++;
        return;
    }
    e->from = strdup(from);
    e->to = strdup(to);
    e->label = label ? strdup(label) : NULL;
    e->arrow_type = arrow ? strdup(arrow) : strdup("-->");
    if (!e->from || !e->to || !e->arrow_type || (label && !e->label)) {
        free(e->from);
        free(e->to);
        free(e->label);
        free(e->arrow_type);
        free(e);
        ctx->error_count++;
        return;
    }
    e->next = ctx->diagram->edges;
    ctx->diagram->edges = e;
}


static FlowchartNodeRef* make_node_ref(FlowchartParserContext *ctx, const char* id) {
    FlowchartNodeRef* ref = (FlowchartNodeRef*)calloc(1, sizeof(FlowchartNodeRef));
    if (!ref) {
        ctx->error_count++;
        return NULL;
    }
    ref->id = strdup(id);
    if (!ref->id) {
        free(ref);
        ctx->error_count++;
        return NULL;
    }
    return ref;
}

static FlowchartNodeRef* append_node_ref(FlowchartParserContext *ctx,
                                         FlowchartNodeRef* refs,
                                         const char* id) {
    FlowchartNodeRef* added = make_node_ref(ctx, id);
    if (!added) return refs;
    if (!refs) return added;
    FlowchartNodeRef* tail = refs;
    while (tail->next) tail = tail->next;
    tail->next = added;
    return refs;
}

static void add_edges_between(FlowchartParserContext *ctx,
                              const FlowchartNodeRef* sources,
                              const FlowchartNodeRef* targets,
                              const char* arrow,
                              const char* label) {
    for (const FlowchartNodeRef* source = sources; source; source = source->next)
        for (const FlowchartNodeRef* target = targets; target; target = target->next)
            add_edge(ctx, source->id, target->id, arrow, label);
}

static void free_node_refs(FlowchartNodeRef* refs) {
    while (refs) {
        FlowchartNodeRef* next = refs->next;
        free(refs->id);
        free(refs);
        refs = next;
    }
}

static void free_styles(FlowchartStyle* styles) {
    while (styles) {
        FlowchartStyle* next = styles->next;
        free(styles->key);
        free(styles->value);
        free(styles);
        styles = next;
    }
}

static FlowchartStyle* clone_style_list(FlowchartParserContext* ctx,
                                        const FlowchartStyle* source) {
    FlowchartStyle* head = NULL;
    FlowchartStyle** tail = &head;
    for (const FlowchartStyle* item = source; item; item = item->next) {
        FlowchartStyle* copied = (FlowchartStyle*)calloc(1, sizeof(FlowchartStyle));
        if (!copied) {
            ctx->error_count++;
            free_styles(head);
            return NULL;
        }
        copied->key = item->key ? strdup(item->key) : NULL;
        copied->value = item->value ? strdup(item->value) : NULL;
        if (!copied->key || !copied->value) {
            ctx->error_count++;
            free(copied->key);
            free(copied->value);
            free(copied);
            free_styles(head);
            return NULL;
        }
        *tail = copied;
        tail = &copied->next;
    }
    return head;
}

static char* copy_trimmed(const char* start, size_t length) {
    while (length && (*start == ' ' || *start == '\t' || *start == '\r')) {
        ++start;
        --length;
    }
    while (length && (start[length - 1] == ' ' || start[length - 1] == '\t' ||
                      start[length - 1] == '\r'))
        --length;
    char* result = (char*)malloc(length + 1);
    if (!result) return NULL;
    memcpy(result, start, length);
    result[length] = '\0';
    return result;
}

static FlowchartStyle* parse_style(FlowchartParserContext* ctx, const char* raw) {
    const char* colon = raw ? strchr(raw, ':') : NULL;
    if (!colon) {
        ctx->error_count++;
        return NULL;
    }
    char* key = copy_trimmed(raw, (size_t)(colon - raw));
    char* value = copy_trimmed(colon + 1, strlen(colon + 1));
    if (!key || !value || !key[0]) {
        free(key);
        free(value);
        ctx->error_count++;
        return NULL;
    }
    FlowchartStyle* style = (FlowchartStyle*)calloc(1, sizeof(FlowchartStyle));
    if (!style) {
        free(key);
        free(value);
        ctx->error_count++;
        return NULL;
    }
    style->key = key;
    style->value = value;
    return style;
}

static FlowchartStyle* append_styles(FlowchartStyle* first, FlowchartStyle* rest) {
    if (!first) return rest;
    FlowchartStyle* tail = first;
    while (tail->next) tail = tail->next;
    tail->next = rest;
    return first;
}

static void add_class_assignment(FlowchartParserContext* ctx,
                                 const char* node_id, const char* class_name) {
    if (!node_id || !class_name) return;
    for (FlowchartClass* existing = ctx->diagram->classes; existing; existing = existing->next) {
        if (strcmp(existing->node_id, node_id) == 0 &&
            strcmp(existing->class_name, class_name) == 0)
            return;
    }
    FlowchartClass* assignment = (FlowchartClass*)calloc(1, sizeof(FlowchartClass));
    if (!assignment) {
        ctx->error_count++;
        return;
    }
    assignment->node_id = strdup(node_id);
    assignment->class_name = strdup(class_name);
    if (!assignment->node_id || !assignment->class_name) {
        free(assignment->node_id);
        free(assignment->class_name);
        free(assignment);
        ctx->error_count++;
        return;
    }
    assignment->next = ctx->diagram->classes;
    ctx->diagram->classes = assignment;
}



static void add_link_style(FlowchartParserContext* ctx, char* index,
                           FlowchartStyle* styles) {
    if (!index || !styles) {
        free(index);
        free_styles(styles);
        ctx->error_count++;
        return;
    }
    FlowchartLinkStyle* item = (FlowchartLinkStyle*)calloc(1, sizeof(FlowchartLinkStyle));
    if (!item) {
        free(index);
        free_styles(styles);
        ctx->error_count++;
        return;
    }
    item->index = index;
    item->styles = styles;
    item->next = ctx->diagram->link_styles;
    ctx->diagram->link_styles = item;
}

static void add_click(FlowchartParserContext* ctx, const char* node_id,
                      const char* target, int is_href) {
    if (!node_id || !target) {
        ctx->error_count++;
        return;
    }
    FlowchartClick* click = (FlowchartClick*)calloc(1, sizeof(FlowchartClick));
    if (!click) {
        ctx->error_count++;
        return;
    }
    click->node_id = strdup(node_id);
    click->target = strdup(target);
    click->is_href = is_href;
    if (!click->node_id || !click->target) {
        free(click->node_id);
        free(click->target);
        free(click);
        ctx->error_count++;
        return;
    }
    click->next = ctx->diagram->clicks;
    ctx->diagram->clicks = click;
}

static void add_class_definition(FlowchartParserContext* ctx, char* id,
                                 FlowchartStyle* styles) {
    if (!id) {
        free_styles(styles);
        ctx->error_count++;
        return;
    }
    for (FlowchartClassDef* existing = ctx->diagram->class_defs;
         existing; existing = existing->next) {
        if (strcmp(existing->id, id) == 0) {
            free(existing->id);
            free_styles(existing->styles);
            existing->id = id;
            existing->styles = styles;
            return;
        }
    }
    FlowchartClassDef* definition = (FlowchartClassDef*)calloc(1, sizeof(FlowchartClassDef));
    if (!definition) {
        free(id);
        free_styles(styles);
        ctx->error_count++;
        return;
    }
    definition->id = id;
    definition->styles = styles;
    definition->next = ctx->diagram->class_defs;
    ctx->diagram->class_defs = definition;
}

static char* concat(FlowchartParserContext* ctx, char* a, char* b, const char* sep) {
    if (!a && !b) return NULL;
    if (!a) return b;
    if (!b) return a;
    size_t len = strlen(a) + strlen(b) + strlen(sep) + 1;
    char* r = (char*)malloc(len);
    if (!r) {
        free(a); free(b);
        ctx->error_count++;
        return NULL;
    }
    sprintf(r, "%s%s%s", a, sep, b);
    free(a); free(b);
    return r;
}

static void start_subgraph(FlowchartParserContext* ctx,
                           const char* id, const char* label) {
    FlowchartSubGraph* sg = (FlowchartSubGraph*)calloc(1, sizeof(FlowchartSubGraph));
    if (!sg) {
        ctx->error_count++;
        return;
    }
    sg->id = strdup(id);
    sg->label = strdup(label);
    if (!sg->id || !sg->label) {
        free(sg->id);
        free(sg->label);
        free(sg);
        ctx->error_count++;
        return;
    }

    FlowchartSubGraph* parent = current_subgraph(ctx);
    if (parent) {
        /* Nested subgraphs are members of their parent, in source order. */
        add_node_ref(ctx, parent, id);
        sg->next = parent->children;
        parent->children = sg;
    } else {
        sg->next = ctx->diagram->subgraphs;
        ctx->diagram->subgraphs = sg;
    }
    push_subgraph(ctx, sg);
}
}

%syntax_error {
    // fprintf(stderr, "Syntax error!\n");
    ctx->error_count++;
}

start ::= flowchart_type direction_opt statements EOF.

flowchart_type ::= FLOWCHART.
flowchart_type ::= GRAPH.

direction_opt ::= .
direction_opt ::= DIR(A). {
    ctx->diagram->direction = strdup(A);
    if (!ctx->diagram->direction) ctx->error_count++;
    free(A);
}

statements ::= statements statement.
statements ::= .

statement ::= node_definition.
statement ::= edge_statement.
statement ::= subgraph_statement.
statement ::= class_statement.
statement ::= classdef_statement.
statement ::= click_statement.
statement ::= linkstyle_statement.
/* Preserve local direction on its subgraph without mutating diagram direction. */
statement ::= DIRECTION DIR(A). {
    FlowchartSubGraph* sg = current_subgraph(ctx);
    if (sg) {
        char* direction = strdup(A);
        if (!direction) ctx->error_count++;
        else { free(sg->direction); sg->direction = direction; }
    }
    free(A);
}
statement ::= NEWLINE.
statement ::= SEMI.

class_statement ::= CLASS class_ids(A) label_text(B) SEMI. {
    for (FlowchartNodeRef* id = A; id; id = id->next)
        add_class_assignment(ctx, id->id, B);
    free(B);
    free_node_refs(A);
}
class_statement ::= CLASS class_ids(A) label_text(B) NEWLINE. {
    for (FlowchartNodeRef* id = A; id; id = id->next)
        add_class_assignment(ctx, id->id, B);
    free(B);
    free_node_refs(A);
}
class_ids(R) ::= label_text(A). {
    R = (FlowchartNodeRef*)calloc(1, sizeof(FlowchartNodeRef));
    if (!R) { free(A); ctx->error_count++; }
    else R->id = A;
}
class_ids(R) ::= label_text(A) COMMA class_ids(B). {
    R = (FlowchartNodeRef*)calloc(1, sizeof(FlowchartNodeRef));
    if (!R) { free(A); free_node_refs(B); ctx->error_count++; }
    else { R->id = A; R->next = B; }
}
classdef_statement ::= CLASSDEF label_text(A) style_specs(B) SEMI. {
    add_class_definition(ctx, A, B);
}
classdef_statement ::= CLASSDEF label_text(A) style_specs(B) NEWLINE. {
    add_class_definition(ctx, A, B);
}
style_specs(R) ::= style_spec(A). { R = A; }
style_specs(R) ::= style_specs(A) COMMA style_spec(B). {
    R = append_styles(A, B);
}
style_spec(R) ::= STYLE(A). {
    R = parse_style(ctx, A);
    free(A);
}

linkstyle_statement ::= LINKSTYLE class_ids(A) style_specs(B) SEMI. {
    for (const FlowchartNodeRef* index = A; index; index = index->next)
        add_link_style(ctx, strdup(index->id), clone_style_list(ctx, B));
    free_node_refs(A);
    free_styles(B);
}
linkstyle_statement ::= LINKSTYLE class_ids(A) style_specs(B) NEWLINE. {
    for (const FlowchartNodeRef* index = A; index; index = index->next)
        add_link_style(ctx, strdup(index->id), clone_style_list(ctx, B));
    free_node_refs(A);
    free_styles(B);
}
click_statement ::= CLICK label_text(A) STRING(B). {
    add_click(ctx, A, B, 1);
    free(A); free(B);
}
click_statement ::= CLICK label_text(A) IDENTIFIER(B). {
    add_click(ctx, A, B, 0);
    free(A); free(B);
}

node_definition ::= node_group(A). {
    /* Explicit declarations of an existing node inside a subgraph still add
       membership. A newly created node has already been registered by
       find_or_create_node(), so do not double-count it. */
    FlowchartSubGraph* active = current_subgraph(ctx);
    if (active) {
        for (FlowchartNodeRef* ref = A; ref; ref = ref->next) {
            if (!has_node_ref(active, ref->id) &&
                !node_registered_in_another_subgraph(
                    ctx->diagram->subgraphs, active, ref->id))
                add_node_ref(ctx, active, ref->id);
        }
    }
    free_node_refs(A);
}

// 基础值可以是标识符或字符串
label_text(R) ::= IDENTIFIER(A). { R = A; }
label_text(R) ::= STRING(A).     { R = A; }

// 形状和边标签允许由多个未加引号的单词组成；lexer 会丢弃词间空白。
node_label(R) ::= label_text(A). { R = A; }
node_label(R) ::= node_label(A) label_text(B). {
    const size_t len = A ? strlen(A) : 0;
    /* The lexer discards ordinary interword whitespace. Retain explicit
       punctuation adjacency (comma or HTML tag) rather than adding a space. */
    const char* separator =
        (len && (A[len - 1] == '>' || A[len - 1] == ',' ||
                 A[len - 1] == ' ' || A[len - 1] == '\t')) ? "" : " ";
    R = concat(ctx, A, B, separator);
}
node_label(R) ::= node_label(A) COLON label_text(B). { R = concat(ctx, A, B, ":"); }
node_label(R) ::= node_label(A) DASH label_text(B). { R = concat(ctx, A, B, "-"); }
// Commas inside unquoted node labels are content, not discarded separators.
node_label(R) ::= node_label(A) COMMA. { R = concat(ctx, A, strdup(","), ""); }
node_label(R) ::= node_label(A) HTML(B). { R = concat(ctx, A, B, ""); }
// HTML-only labels such as B[<img>] must be allowed to start a node label.
node_label(R) ::= HTML(A). { R = A; }
node_label(R) ::= PUNCT(A). { R = A; }
node_label(R) ::= node_label(A) PUNCT(B). { R = concat(ctx, A, B, ""); }

node(R) ::= label_text(A). {
    R = find_or_create_node(ctx, A, NULL);
    free(A);
}
node(R) ::= label_text(A) SQS node_label(B) SQE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_RECT;
    free(A); free(B);
}
node(R) ::= label_text(A) PS node_label(B) PE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_ROUND;
    free(A); free(B);
}
node(R) ::= label_text(A) CPS node_label(B) CPE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_CIRCLE;
    free(A); free(B);
}
// An empty/space-only circle keeps Mermaid's visible single-space label.
node(R) ::= label_text(A) CPS CPE. {
    R = find_or_create_node(ctx, A, " ");
    if (R) R->shape = FC_SHAPE_CIRCLE;
    free(A);
}
node(R) ::= label_text(A) DOUBLE_CIRCLE_START node_label(B) DOUBLE_CIRCLE_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_DOUBLECIRCLE;
    free(A); free(B);
}
node(R) ::= label_text(A) RHOMBUS_START node_label(B) RHOMBUS_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_RHOMBUS;
    free(A); free(B);
}
node(R) ::= label_text(A) STADIUM_START node_label(B) STADIUM_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_STADIUM;
    free(A); free(B);
}
node(R) ::= label_text(A) SUBROUTINE_START node_label(B) SUBROUTINE_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_SUBROUTINE;
    free(A); free(B);
}
node(R) ::= label_text(A) HEXAGON_START node_label(B) HEXAGON_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_HEXAGON;
    free(A); free(B);
}
node(R) ::= label_text(A) ELLIPSE_START node_label(B) ELLIPSE_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_ELLIPSE;
    free(A); free(B);
}
node(R) ::= label_text(A) CYLINDER_START node_label(B) CYLINDER_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_CYLINDER;
    free(A); free(B);
}
node(R) ::= label_text(A) TRAP_START node_label(B) TRAP_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_TRAPEZOID;
    free(A); free(B);
}
node(R) ::= label_text(A) INV_TRAP_START node_label(B) TRAP_END. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_INV_TRAPEZOID;
    free(A); free(B);
}
node(R) ::= label_text(A) ODD_START node_label(B) SQE. {
    R = find_or_create_node(ctx, A, B);
    if (R) R->shape = FC_SHAPE_ODD;
    free(A); free(B);
}

edge_statement ::= edge_chain(A). { free_node_refs(A); }

node_group(R) ::= node(A). {
    R = A ? make_node_ref(ctx, A->id) : NULL;
}
node_group(R) ::= node_group(A) AMP node(B). {
    R = B ? append_node_ref(ctx, A, B->id) : A;
}

edge_chain(R) ::= node_group(A) ARROW(L) node_group(B). {
    add_edges_between(ctx, A, B, L, NULL);
    free_node_refs(A);
    free(L);
    R = B;
}
edge_chain(R) ::= edge_chain(A) ARROW(L) node_group(B). {
    add_edges_between(ctx, A, B, L, NULL);
    free_node_refs(A);
    free(L);
    R = B;
}
edge_chain(R) ::= node_group(A) ARROW(L) PIPE node_label(C) PIPE node_group(B). {
    add_edges_between(ctx, A, B, L, C);
    free_node_refs(A);
    free(L); free(C);
    R = B;
}
edge_chain(R) ::= edge_chain(A) ARROW(L) PIPE node_label(C) PIPE node_group(B). {
    add_edges_between(ctx, A, B, L, C);
    free_node_refs(A);
    free(L); free(C);
    R = B;
}

subgraph_statement ::= subgraph_start statements END. {
    pop_subgraph(ctx);
}

subgraph_start ::= SUBGRAPH label_text(A). {
    start_subgraph(ctx, A, A);
    free(A);
}

subgraph_start ::= SUBGRAPH label_text(A) SQS node_label(B) SQE. {
    start_subgraph(ctx, A, B);
    free(A); free(B);
}
