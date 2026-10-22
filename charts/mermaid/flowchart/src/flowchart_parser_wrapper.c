#include "flowchart/flowchart_parser_wrapper.h"
#include "flowchart_parser_gen.h"
#ifndef STUN_FLOWCHART_PARSER_ONLY
#include "json_parser.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


typedef struct {
  const char *start;
  const char *cursor;
  const char *limit;
  const char *marker;
  int line;
  int in_classdef;
  int click_phase;
  int shape_label_depth;
  int after_subgraph_keyword;
  const char* subgraph_extra_gap_start;
  size_t subgraph_extra_gap_size;
} Scanner;

extern void flowchart_scan(Scanner *s, void *parser, FlowchartParserContext *ctx);

void *FlowchartParserAlloc(void *(*mallocProc)(size_t));
void FlowchartParser(void *yyp, int yymajor, void *yyminor, FlowchartParserContext *ctx);
void FlowchartParserFree(void *p, void (*freeProc)(void*));

void flowchart_diagram_free(FlowchartDiagram *diagram);
void flowchart_parsed_edge_free(FlowchartEdge *edge);

#if defined(_MSC_VER)
__declspec(thread) static char g_flowchart_last_error[128] = {0};
#else
static _Thread_local char g_flowchart_last_error[128] = {0};
#endif

const char* flowchart_get_last_error() {
  return g_flowchart_last_error;
}

static FlowchartNode *reverse_nodes(FlowchartNode *head) {
  FlowchartNode *result = NULL;
  while (head) {
    FlowchartNode *next = head->next;
    head->next = result;
    result = head;
    head = next;
  }
  return result;
}

static FlowchartEdge *reverse_edges(FlowchartEdge *head) {
  FlowchartEdge *result = NULL;
  while (head) {
    FlowchartEdge *next = head->next;
    head->next = result;
    result = head;
    head = next;
  }
  return result;
}

static FlowchartClass *reverse_classes(FlowchartClass *head) {
  FlowchartClass *result = NULL;
  while (head) {
    FlowchartClass *next = head->next;
    head->next = result;
    result = head;
    head = next;
  }
  return result;
}

static FlowchartLinkStyle *reverse_link_styles(FlowchartLinkStyle *head) {
  FlowchartLinkStyle *result = NULL;
  while (head) {
    FlowchartLinkStyle *next = head->next;
    head->next = result;
    result = head;
    head = next;
  }
  return result;
}

static FlowchartClick *reverse_clicks(FlowchartClick *head) {
  FlowchartClick *result = NULL;
  while (head) {
    FlowchartClick *next = head->next;
    head->next = result;
    result = head;
    head = next;
  }
  return result;
}

static FlowchartSubGraph *reverse_subgraphs(FlowchartSubGraph *head) {
  FlowchartSubGraph *result = NULL;
  while (head) {
    FlowchartSubGraph *next = head->next;
    /* node_refs are head-inserted; the Mermaid AST contract preserves this
       reference order rather than reversing it into source order. */
    head->children = reverse_subgraphs(head->children);
    head->next = result;
    result = head;
    head = next;
  }
  return result;
}

static void restore_source_order(FlowchartDiagram *diagram) {
  diagram->nodes = reverse_nodes(diagram->nodes);
  diagram->edges = reverse_edges(diagram->edges);
  diagram->subgraphs = reverse_subgraphs(diagram->subgraphs);
  diagram->classes = reverse_classes(diagram->classes);
  diagram->clicks = reverse_clicks(diagram->clicks);
  diagram->link_styles = reverse_link_styles(diagram->link_styles);
}

static void free_active_subgraphs(FlowchartParserContext *ctx) {
  while (ctx->active_subgraphs) {
    FlowchartSubGraphStack *entry =
        (FlowchartSubGraphStack *)ctx->active_subgraphs;
    ctx->active_subgraphs = entry->next;
    free(entry);
  }
}

FlowchartParseStatus flowchart_parse_ex(const char *input,
                                        FlowchartDiagram **out_diagram) {
  if (!out_diagram) {
    snprintf(g_flowchart_last_error, sizeof(g_flowchart_last_error),
             "Output diagram pointer is null");
    return FLOWCHART_PARSE_ERROR;
  }
  *out_diagram = NULL;
  if (!input) {
    snprintf(g_flowchart_last_error, sizeof(g_flowchart_last_error), "Input is null");
    return FLOWCHART_PARSE_ERROR;
  }
  g_flowchart_last_error[0] = '\0';

  // extern void FlowchartParserTrace(FILE *, char *);
  // FlowchartParserTrace(stdout, "Parser >> ");

  FlowchartParserContext ctx;
  memset(&ctx, 0, sizeof(FlowchartParserContext));
  ctx.diagram = (FlowchartDiagram *)malloc(sizeof(FlowchartDiagram));
  if (!ctx.diagram) {
    snprintf(g_flowchart_last_error, sizeof(g_flowchart_last_error), "Failed to allocate flowchart AST");
    return FLOWCHART_PARSE_ERROR;
  }
  memset(ctx.diagram, 0, sizeof(FlowchartDiagram));
  ctx.diagram->layout_mode = FC_LAYOUT_PROFESSIONAL;
  ctx.diagram->routing_mode = FC_ROUTE_ORTHOGONAL;
  ctx.diagram->routing_shape_buffer = -1.0;
  ctx.diagram->routing_nudging_distance = -1.0;
  ctx.diagram->routing_segment_penalty = -1.0;
  ctx.diagram->routing_angle_penalty = -1.0;
  ctx.diagram->routing_crossing_penalty = -1.0;
  ctx.diagram->routing_nudge_orthogonal_ends = -1;
  ctx.diagram->routing_nudge_shared_paths = -1;

  void *parser = FlowchartParserAlloc(malloc);
  if (!parser) {
    free(ctx.diagram);
    snprintf(g_flowchart_last_error, sizeof(g_flowchart_last_error), "Failed to allocate flowchart parser");
    return FLOWCHART_PARSE_ERROR;
  }

  Scanner s;
  s.start = input;
  s.cursor = input;
  s.limit = input + strlen(input);
  s.marker = NULL;
  s.line = 1;
  s.in_classdef = 0;
  s.click_phase = 0;
  s.shape_label_depth = 0;
  s.after_subgraph_keyword = 0;
  s.subgraph_extra_gap_start = NULL;
  s.subgraph_extra_gap_size = 0;

  flowchart_scan(&s, parser, &ctx);
  FlowchartParser(parser, 0, NULL, &ctx);
  FlowchartParserFree(parser, free);
  free_active_subgraphs(&ctx);
  restore_source_order(ctx.diagram);

  if (ctx.error_count > 0) {
    snprintf(g_flowchart_last_error, sizeof(g_flowchart_last_error),
             "Syntax error near line %d", s.line);
    if (!ctx.diagram->nodes && !ctx.diagram->edges && !ctx.diagram->subgraphs) {
      flowchart_diagram_free(ctx.diagram);
      return FLOWCHART_PARSE_ERROR;
    }
    *out_diagram = ctx.diagram;
    return FLOWCHART_PARSE_PARTIAL;
  }

  *out_diagram = ctx.diagram;
  return FLOWCHART_PARSE_COMPLETE;
}

FlowchartDiagram *flowchart_parse(const char *input) {
  FlowchartDiagram *diagram = NULL;
  (void)flowchart_parse_ex(input, &diagram);
  return diagram;
}

static void free_subgraph_node_refs(FlowchartNodeRef *nr) {
  while (nr) {
    FlowchartNodeRef *next = nr->next;
    free(nr->id);
    free(nr);
    nr = next;
  }
}

static void free_subgraph(FlowchartSubGraph *sg) {
  if (!sg)
    return;
  free(sg->id);
  free(sg->label);
  free(sg->direction);
  free_subgraph_node_refs(sg->node_refs);

  FlowchartSubGraph *child = sg->children;
  while (child) {
    FlowchartSubGraph *next = child->next;
    free_subgraph(child);
    child = next;
  }
  free(sg);
}

void flowchart_diagram_free(FlowchartDiagram *diagram) {
  if (!diagram)
    return;
  free(diagram->direction);
  FlowchartNode *n = diagram->nodes;
  while (n) {
    FlowchartNode *next = n->next;
    free(n->id);
    free(n->label);
    free(n);
    n = next;
  }
  FlowchartEdge *e = diagram->edges;
  while (e) {
    FlowchartEdge *next = e->next;
    flowchart_parsed_edge_free(e);
    e = next;
  }
  FlowchartSubGraph *sg = diagram->subgraphs;
  while (sg) {
    FlowchartSubGraph *next = sg->next;
    free_subgraph(sg);
    sg = next;
  }
  FlowchartClassDef *definition = diagram->class_defs;
  while (definition) {
    FlowchartClassDef *next = definition->next;
    FlowchartStyle *style = definition->styles;
    while (style) {
      FlowchartStyle *style_next = style->next;
      free(style->key);
      free(style->value);
      free(style);
      style = style_next;
    }
    free(definition->id);
    free(definition);
    definition = next;
  }
  FlowchartClass *assignment = diagram->classes;
  while (assignment) {
    FlowchartClass *next = assignment->next;
    free(assignment->node_id);
    free(assignment->class_name);
    free(assignment);
    assignment = next;
  }
  FlowchartClick *click = diagram->clicks;
  while (click) {
    FlowchartClick *next = click->next;
    free(click->node_id);
    free(click->target);
    free(click);
    click = next;
  }
  FlowchartLinkStyle *link_style = diagram->link_styles;
  while (link_style) {
    FlowchartLinkStyle *next = link_style->next;
    free(link_style->index);
    FlowchartStyle *style = link_style->styles;
    while (style) {
      FlowchartStyle *style_next = style->next;
      free(style->key);
      free(style->value);
      free(style);
      style = style_next;
    }
    free(link_style);
    link_style = next;
  }
  free(diagram);
}

void flowchart_set_node_size(FlowchartDiagram* diagram, const char* id, double width, double height) {
  if (!diagram || !id)
    return;
  FlowchartNode* n = diagram->nodes;
  while (n) {
    if (n->id && strcmp(n->id, id) == 0) {
      n->layout_width = width;
      n->layout_height = height;
      return;
    }
    n = n->next;
  }
}

void flowchart_set_layout_mode(FlowchartDiagram* diagram, int mode) {
  if (!diagram)
    return;
  diagram->layout_mode = (FlowchartLayoutMode)mode;
}

void flowchart_set_routing_mode(FlowchartDiagram* diagram, int mode) {
  if (!diagram)
    return;
  diagram->routing_mode = (FlowchartRoutingMode)mode;
}

/* Lightweight exact-source parser qualification omits only the JSON serializer.
   Production builds always compile the original SaltsUtils-backed serializer. */
#ifndef STUN_FLOWCHART_PARSER_ONLY
static const char *map_shape(FlowchartNodeShape shape) {
  switch (shape) {
  case FC_SHAPE_RECT:
    return "rect"; // changed from square for v2-080
  case FC_SHAPE_ROUND:
    return "round";
  case FC_SHAPE_CIRCLE:
    return "circle";
  case FC_SHAPE_RHOMBUS:
  case FC_SHAPE_DIAMOND:
    return "diamond";
  case FC_SHAPE_STADIUM:
    return "stadium";
  case FC_SHAPE_SUBROUTINE:
    return "subroutine";
  case FC_SHAPE_CYLINDER:
    return "cylinder";
  case FC_SHAPE_DOUBLECIRCLE:
    return "doublecircle";
  case FC_SHAPE_TRAPEZOID:
    return "lean_right";
  case FC_SHAPE_INV_TRAPEZOID:
    return "lean_left";
  case FC_SHAPE_TRAPEZOID_SYMMETRIC:
    return "trapezoid";
  case FC_SHAPE_INV_TRAPEZOID_SYMMETRIC:
    return "inv_trapezoid";
  case FC_SHAPE_ODD:
    return "odd";
  case FC_SHAPE_HEXAGON:
    return "hexagon";
  case FC_SHAPE_ELLIPSE:
    return "ellipse";
  default:
    return "rect";
  }
}

static const char *map_arrow(const char *arrow) {
  if (!arrow)
    return "arrow_point";
  /* Three or more '-' characters are not necessarily open edges: ---->
     remains directed, with length encoded separately. */
  if (strchr(arrow, '>'))
    return "arrow_point";
  if (strstr(arrow, "---") || strstr(arrow, "-.-"))
    return "arrow_open";
  return "arrow_point";
}

/* Consumes value on both success and failure, unlike the SDK's checked API. */
static int flowchart_json_put(json_value_t* object, const char* key, json_value_t* value) {
  if (!value) return 0;
  if (json_object_add_checked(object, key, value)) return 1;
  json_free(value);
  return 0;
}

static int flowchart_json_push(json_value_t* array, json_value_t* value) {
  if (!value) return 0;
  if (json_array_add_checked(array, value)) return 1;
  json_free(value);
  return 0;
}

/* Returned children are already owned by their parent, including on a later
 * field failure. A single ancestor cleanup releases the complete subtree. */
static json_value_t* flowchart_json_object(json_value_t* owner, const char* key) {
  json_value_t* child = json_create_object();
  return flowchart_json_put(owner, key, child) ? child : NULL;
}

static json_value_t* flowchart_json_array(json_value_t* owner, const char* key) {
  json_value_t* child = json_create_array();
  return flowchart_json_put(owner, key, child) ? child : NULL;
}

static int flowchart_json_text(json_value_t* owner, const char* key, const char* value) {
  json_value_t* text = flowchart_json_object(owner, key);
  return text && flowchart_json_put(text, "text", json_create_string(value)) &&
                 flowchart_json_put(text, "type", json_create_string("text"));
}

static int flowchart_json_styles(json_value_t* owner, const FlowchartStyle* list) {
  json_value_t* styles = flowchart_json_object(owner, "styles");
  if (!styles) return 0;
  for (const FlowchartStyle* s = list; s; s = s->next)
    if (s->key && s->value &&
        !flowchart_json_put(styles, s->key, json_create_string(s->value))) return 0;
  return 1;
}

/* The reference list is head-inserted. A group with only distinct node
 * references consists of explicit declarations; Mermaid serializes those in
 * source order. It also lists each pre-existing edge joining two members
 * after those declarations, in target/source reference order.
 *
 * Edge-containing groups already have repeated references (implicit nodes
 * plus endpoints). Preserve those as-is: their runtime AST is authoritative.
 * Only the JSON projection is affected, never parser ownership or AST order. */
static int subgraph_refs_contain(const FlowchartNodeRef* const* refs,
                                 size_t count, const char* id) {
  if (!id) return 0;
  for (size_t i = 0; i < count; ++i)
    if (refs[i]->id && strcmp(refs[i]->id, id) == 0) return 1;
  return 0;
}

static int flowchart_child_subgraph_id_unused(
    const FlowchartSubGraph* parent, const FlowchartDiagram* diagram,
    const char* id) {
  if (!id) return 0;
  for (const FlowchartSubGraph* child = parent->children; child;
       child = child->next) {
    if (child->id && strcmp(child->id, id) == 0) {
      for (const FlowchartEdge* e = diagram->edges; e; e = e->next) {
        if ((e->from && strcmp(e->from, id) == 0) ||
            (e->to && strcmp(e->to, id) == 0))
          return 0;
      }
      return 1; /* The child is not used as a graph edge endpoint. */
    }
  }
  return 0;
}

static int flowchart_in_nested_child_nodes(const FlowchartSubGraph* children,
                                           const char* id) {
  if (!id) return 0;
  for (const FlowchartSubGraph* child = children; child;
       child = child->next) {
    for (const FlowchartNodeRef* ref = child->node_refs; ref; ref = ref->next)
      if (ref->id && strcmp(ref->id, id) == 0) return 1;
    if (flowchart_in_nested_child_nodes(child->children, id)) return 1;
  }
  return 0;
}

/* For a leaf subgraph with multiple edges, implicit node references are
 * inserted before their first edge and edge endpoint pairs follow that edge.
 * Reconstruct only when every recorded member is accounted for exactly by
 * those edge uses plus at most one implicit first-use per distinct node.
 * Otherwise retain the original projection; never guess the event order. */
typedef struct {
  const char* id;
  size_t ref_count;
  size_t edge_uses;
  int seen;
} FlowchartRefProjection;

static size_t flowchart_projection_index(const FlowchartRefProjection* refs,
                                          size_t count, const char* id) {
  if (!id) return count;
  for (size_t i = 0; i < count; ++i)
    if (refs[i].id && strcmp(refs[i].id, id) == 0) return i;
  return count;
}

/* 1=emitted, 0=not uniquely reconstructible, -1=allocation failure. */
static int flowchart_emit_leaf_edge_refs(json_value_t* nodes_arr,
                                         const FlowchartDiagram* diagram,
                                         const FlowchartSubGraph* sg,
                                         const FlowchartNodeRef* const* refs,
                                         size_t count) {
  if (!count || sg->children) return 0;
  FlowchartRefProjection* groups =
      (FlowchartRefProjection*)calloc(count, sizeof(*groups));
  if (!groups) return -1;
  size_t groups_count = 0;
  for (size_t i = 0; i < count; ++i) {
    const char* id = refs[i]->id;
    size_t key = flowchart_projection_index(groups, groups_count, id);
    if (key == groups_count) {
      groups[key].id = id;
      ++groups_count;
    }
    ++groups[key].ref_count;
  }

  size_t matched_edges = 0;
  int valid = 1;
  for (const FlowchartEdge* e = diagram->edges; e; e = e->next) {
    size_t from = flowchart_projection_index(groups, groups_count, e->from);
    size_t to = flowchart_projection_index(groups, groups_count, e->to);
    if (from == groups_count || to == groups_count) continue;
    if (from == to) { valid = 0; break; }
    ++groups[from].edge_uses;
    ++groups[to].edge_uses;
    ++matched_edges;
  }
  if (matched_edges < 2) valid = 0;
  for (size_t i = 0; valid && i < groups_count; ++i) {
    if (groups[i].edge_uses == 0 ||
        groups[i].edge_uses > groups[i].ref_count ||
        groups[i].ref_count - groups[i].edge_uses > 1)
      valid = 0;
  }
  if (!valid) { free(groups); return 0; }

  /* Emit implicit first-use references before that edge's pair. The pair
     itself is target/source because the AST's references are head-inserted. */
  for (const FlowchartEdge* e = diagram->edges; e; e = e->next) {
    size_t from = flowchart_projection_index(groups, groups_count, e->from);
    size_t to = flowchart_projection_index(groups, groups_count, e->to);
    if (from == groups_count || to == groups_count) continue;
    if (!groups[to].seen && groups[to].ref_count > groups[to].edge_uses &&
        !flowchart_json_push(nodes_arr, json_create_string(e->to))) goto fail;
    if (!groups[from].seen && groups[from].ref_count > groups[from].edge_uses &&
        !flowchart_json_push(nodes_arr, json_create_string(e->from))) goto fail;
    groups[to].seen = 1;
    groups[from].seen = 1;
    if (!flowchart_json_push(nodes_arr, json_create_string(e->to)) ||
        !flowchart_json_push(nodes_arr, json_create_string(e->from))) goto fail;
  }
  free(groups);
  return 1;
fail:
  free(groups);
  return -1;
}

static int serialize_subgraph_recursive(json_value_t* parent_arr,
                                        const FlowchartDiagram* diagram,
                                        const FlowchartSubGraph* sg) {
  int count = 0;
  while (sg) {
    if (++count > 1000) return 0;
    /* Preserve the established flat children-before-parent projection. */
    if (sg->children && !serialize_subgraph_recursive(parent_arr, diagram, sg->children))
      return 0;
    json_value_t* sg_det = json_create_object();
    const FlowchartNodeRef** refs = NULL;
    if (!sg_det) return 0;
    if (!flowchart_json_put(sg_det, "id", json_create_string(sg->id ? sg->id : "")) ||
        (sg->direction && !flowchart_json_put(sg_det, "direction", json_create_string(sg->direction))))
      goto fail;
    json_value_t* nodes_arr = flowchart_json_array(sg_det, "nodes");
    if (!nodes_arr) goto fail;
    size_t refs_count = 0;
    for (const FlowchartNodeRef* ref = sg->node_refs; ref; ref = ref->next) {
      if (refs_count == SIZE_MAX / sizeof(*refs)) goto fail;
      ++refs_count;
    }
    refs = refs_count ? (const FlowchartNodeRef**)malloc(refs_count * sizeof(*refs)) : NULL;
    if (refs_count && !refs) goto fail;
    size_t i = 0;
    for (const FlowchartNodeRef* ref = sg->node_refs; ref; ref = ref->next) refs[i++] = ref;
    int declarations_only = refs_count > 0;
    for (size_t x = 0; x < refs_count && declarations_only; ++x)
      for (size_t y = 0; y < x; ++y)
        if (refs[x]->id && refs[y]->id && strcmp(refs[x]->id, refs[y]->id) == 0) {
          declarations_only = 0;
          break;
        }
    if (declarations_only) {
      for (size_t x = refs_count; x > 0; --x)
        if (refs[x - 1]->id && !flowchart_child_subgraph_id_unused(sg, diagram, refs[x - 1]->id) &&
            !flowchart_json_push(nodes_arr, json_create_string(refs[x - 1]->id))) goto fail;
      for (const FlowchartEdge* edge = diagram->edges; edge; edge = edge->next)
        if (subgraph_refs_contain(refs, refs_count, edge->from) &&
            subgraph_refs_contain(refs, refs_count, edge->to) &&
            (!flowchart_json_push(nodes_arr, json_create_string(edge->to)) ||
             !flowchart_json_push(nodes_arr, json_create_string(edge->from)))) goto fail;
    } else {
      const int rebuilt = flowchart_emit_leaf_edge_refs(nodes_arr, diagram, sg, refs, refs_count);
      if (rebuilt < 0) goto fail;
      if (!rebuilt)
        for (int borrowed = 0; borrowed <= 1; ++borrowed)
          for (size_t x = 0; x < refs_count; ++x) {
            const char* id = refs[x]->id;
            if (!id || flowchart_child_subgraph_id_unused(sg, diagram, id)) continue;
            if (flowchart_in_nested_child_nodes(sg->children, id) == borrowed &&
                !flowchart_json_push(nodes_arr, json_create_string(id))) goto fail;
          }
    }
    free(refs); refs = NULL;
    if (!flowchart_json_text(sg_det, "title", sg->label ? sg->label : (sg->id ? sg->id : "")))
      goto fail;
    if (!flowchart_json_push(parent_arr, sg_det)) return 0; // Consumed on failure too.
    sg = sg->next;
    continue;
fail:
    free(refs);
    json_free(sg_det);
    return 0;
  }
  return 1;
}

/* Keep the AST linked list in source order. JSON reflects Mermaid's scope
 * projection: nested/local edges precede links between subgraphs, followed
 * by edges not owned by any subgraph. This is stable within each category. */
static int flowchart_subgraph_has_node(const FlowchartSubGraph* sg,
                                       const char* id) {
  if (!sg || !id) return 0;
  for (const FlowchartNodeRef* ref = sg->node_refs; ref; ref = ref->next)
    if (ref->id && strcmp(ref->id, id) == 0) return 1;
  return 0;
}

static const FlowchartSubGraph* flowchart_first_edge_scope(
    const FlowchartSubGraph* siblings, const FlowchartEdge* edge) {
  for (const FlowchartSubGraph* sg = siblings; sg; sg = sg->next) {
    const FlowchartSubGraph* inner =
        flowchart_first_edge_scope(sg->children, edge);
    if (inner) return inner;
    if (flowchart_subgraph_has_node(sg, edge->from) &&
        flowchart_subgraph_has_node(sg, edge->to))
      return sg;
  }
  return NULL;
}

static int flowchart_is_subgraph_id(const FlowchartSubGraph* siblings,
                                    const char* id) {
  if (!id) return 0;
  for (const FlowchartSubGraph* sg = siblings; sg; sg = sg->next) {
    if (sg->id && strcmp(sg->id, id) == 0) return 1;
    if (flowchart_is_subgraph_id(sg->children, id)) return 1;
  }
  return 0;
}

/* Nested/local edges are emitted separately. For legacy unstyled
 * subgraph graphs, root nodes with one outgoing link are a distinct
 * projection category before root fan-out edges. This does not alter
 * the AST edge list or numbered linkStyle declaration semantics. */
static int flowchart_node_in_subgraph_tree(
    const FlowchartSubGraph* siblings, const char* id) {
  for (const FlowchartSubGraph* sg = siblings; sg; sg = sg->next) {
    if (flowchart_subgraph_has_node(sg, id) ||
        flowchart_node_in_subgraph_tree(sg->children, id))
      return 1;
  }
  return 0;
}

static int flowchart_root_single_outgoing(
    const FlowchartDiagram* diagram, const FlowchartEdge* edge) {
  if (!edge || !edge->from ||
      flowchart_node_in_subgraph_tree(diagram->subgraphs, edge->from))
    return 0;
  unsigned count = 0;
  for (const FlowchartEdge* e = diagram->edges; e; e = e->next) {
    if (e->from && strcmp(e->from, edge->from) == 0 && ++count > 1)
      return 0;
  }
  return count == 1;
}

/* Subgraph IDs used as link endpoints have their own projection phase.
 * Do not reorder ordinary root links around that explicit ordering contract. */
static int flowchart_has_subgraph_link_endpoint(const FlowchartDiagram* d) {
  for (const FlowchartEdge* e = d->edges; e; e = e->next) {
    if (flowchart_is_subgraph_id(d->subgraphs, e->from) ||
        flowchart_is_subgraph_id(d->subgraphs, e->to))
      return 1;
  }
  return 0;
}

static int flowchart_json_append_edge(json_value_t* links_arr,
                                        const FlowchartEdge* e) {
  json_value_t *e_det = json_create_object();
  if (!e_det) return 0;
  int edge_length = 1;
  if (e->arrow_type && strcmp(e->arrow_type, "---") != 0) {
    /* The inline-label scanner now preserves a leading point head. Keep
       the established legacy width of previously normalized labelled edges. */
    const char* body = e->arrow_type + (e->arrow_type[0] == '<');
    const char stroke_char = body[0];
    if (stroke_char == '-' || stroke_char == '=') {
      size_t stroke_length = 0;
      while (body[stroke_length] == stroke_char)
        ++stroke_length;
      if (stroke_length > 1)
        edge_length = (int)stroke_length - 1;
    }
  }

  const char *stroke = "normal";
  if (e->arrow_type && strstr(e->arrow_type, "=="))
    stroke = "thick";
  if (e->arrow_type &&
      (strstr(e->arrow_type, "-.-") || strstr(e->arrow_type, ".->") ||
       strstr(e->arrow_type, "-...->")))
    stroke = "dotted";
  if (!flowchart_json_put(e_det, "length", json_create_number(edge_length)) ||
      !flowchart_json_put(e_det, "source", json_create_string(e->from ? e->from : "")) ||
      !flowchart_json_put(e_det, "stroke", json_create_string(stroke)) ||
      !flowchart_json_put(e_det, "target", json_create_string(e->to ? e->to : "")) ||
      !flowchart_json_put(e_det, "type", json_create_string(map_arrow(e->arrow_type))) ||
      (e->label && !flowchart_json_text(e_det, "text", e->label))) {
    json_free(e_det);
    return 0;
  }
  return flowchart_json_push(links_arr, e_det);
}

static int flowchart_json_append_scoped_edges(json_value_t* links_arr,
                                                const FlowchartDiagram* d,
                                                const FlowchartSubGraph* sgs) {
  for (const FlowchartSubGraph* sg = sgs; sg; sg = sg->next) {
    if (!flowchart_json_append_scoped_edges(links_arr, d, sg->children)) return 0;
    /* Preserve source order when the first link of this scope has no
       label. Otherwise retain Mermaid's label-first compatibility
       projection for scopes that begin with a labelled link. */
    int label_first = 0;
    for (const FlowchartEdge* e = d->edges; e; e = e->next) {
      if (flowchart_first_edge_scope(d->subgraphs, e) == sg) {
        label_first = e->label != NULL;
        break;
      }
    }
    for (int pass = 0; pass < (label_first ? 2 : 1); ++pass) {
      for (const FlowchartEdge* e = d->edges; e; e = e->next) {
        if ((!label_first || (e->label != NULL) == (pass == 0)) &&
            flowchart_first_edge_scope(d->subgraphs, e) == sg)
          if (!flowchart_json_append_edge(links_arr, e)) return 0;
      }
    }
  }
  return 1;
}

/* Mermaid 11.12.0 destructEndLink semantics, including the length cap of 10.
 * Keep these facts separate from the intentionally sparse legacy projection. */
static int flowchart_json_v2_edge(json_value_t* links, const FlowchartEdge* e, size_t ordinal) {
  const char* op = e->arrow_type;
  const size_t n = op ? strlen(op) : 0;
  if (n < 3 || !e->from || !e->to) return 0;
  size_t dots = 0;
  int thick = 0, invisible = 0;
  for (size_t i = 0; i < n; ++i) {
    if (!strchr("-=~.<>ox", op[i])) return 0;
    dots += op[i] == '.';
    thick |= op[i] == '=';
    invisible |= op[i] == '~';
  }
  const char last = op[n - 1];
  const char* type = "arrow_open";
  const char* head = "none";
  int both = 0;
  if (last == '>') { head = "arrow_point"; both = op[0] == '<'; }
  else if (last == 'o') { head = "arrow_circle"; both = op[0] == 'o'; }
  else if (last == 'x') { head = "arrow_cross"; both = op[0] == 'x'; }
  if (strcmp(head, "none") != 0) type = head;
  if (both) {
    if (last == '>') type = "double_arrow_point";
    else if (last == 'o') type = "double_arrow_circle";
    else type = "double_arrow_cross";
  }
  size_t length = dots ? dots : n - 2 - (size_t)both;
  if (!length || (thick && dots) || (invisible && (thick || dots))) return 0;
  if (length > 10) length = 10;
  const char* id = flowchart_edge_get_id(e);
  const char* curve = flowchart_edge_get_curve(e);
  json_value_t* item = json_create_object();
  if (!item) return 0;
  if (!flowchart_json_put(item, "declarationIndex", json_create_uint64((uint64_t)ordinal)) ||
      !flowchart_json_put(item, "source", json_create_string(e->from)) ||
      !flowchart_json_put(item, "target", json_create_string(e->to)) ||
      !flowchart_json_put(item, "length", json_create_uint64((uint64_t)length)) ||
      !flowchart_json_put(item, "stroke", json_create_string(dots ? "dotted" : thick ? "thick" : invisible ? "invisible" : "normal")) ||
      !flowchart_json_put(item, "type", json_create_string(type)) ||
      !flowchart_json_put(item, "arrowTypeStart", json_create_string(both ? head : "none")) ||
      !flowchart_json_put(item, "arrowTypeEnd", json_create_string(head)) ||
      !flowchart_json_put(item, "id", id ? json_create_string(id) : json_create_null()) ||
      !flowchart_json_put(item, "curve", curve ? json_create_string(curve) : json_create_null()))
    goto fail;
  if (e->label && !flowchart_json_text(item, "text", e->label)) goto fail;
  return flowchart_json_push(links, item);
fail:
  json_free(item);
  return 0;
}

static int flowchart_json_append_legacy_projection(json_value_t* links_arr,
                                                    const FlowchartDiagram* diagram) {
  if (!flowchart_json_append_scoped_edges(links_arr, diagram, diagram->subgraphs)) return 0;
  for (int phase = 0; phase < 2; ++phase) {
    /* A root category beginning with an unlabelled statement must not
       promote later labels ahead of it. Keep the existing two-category
       projection (subgraph-ID links first, then ordinary root links). */
    int label_first = 0;
    for (const FlowchartEdge* e = diagram->edges; e; e = e->next) {
      if (flowchart_first_edge_scope(diagram->subgraphs, e))
        continue;
      const int pair_of_subgraphs =
          flowchart_is_subgraph_id(diagram->subgraphs, e->from) &&
          flowchart_is_subgraph_id(diagram->subgraphs, e->to);
      if (pair_of_subgraphs == (phase == 0)) {
        label_first = e->label != NULL;
        break;
      }
    }
    /* This precedence applies only to the unstyled ordinary-root
       category. Indexed link styles must retain their existing indices. */
    int root_has_label = 0;
    if (phase == 1 && diagram->subgraphs && !diagram->link_styles) {
      for (const FlowchartEdge* e = diagram->edges; e; e = e->next) {
        if (!flowchart_first_edge_scope(diagram->subgraphs, e) && e->label)
          root_has_label = 1;
      }
    }
    const int promote_single_outgoing =
        phase == 1 && diagram->subgraphs && !diagram->link_styles &&
        !root_has_label && !flowchart_has_subgraph_link_endpoint(diagram);
    for (int source_class = promote_single_outgoing ? 1 : 0;
         source_class >= 0; --source_class) {
      for (int pass = 0; pass < (label_first ? 2 : 1); ++pass) {
        for (const FlowchartEdge* e = diagram->edges; e; e = e->next) {
          if (label_first && (e->label != NULL) != (pass == 0))
            continue;
          if (flowchart_first_edge_scope(diagram->subgraphs, e))
            continue;
          const int pair_of_subgraphs =
              flowchart_is_subgraph_id(diagram->subgraphs, e->from) &&
              flowchart_is_subgraph_id(diagram->subgraphs, e->to);
          if (pair_of_subgraphs != (phase == 0))
            continue;
          if (promote_single_outgoing &&
              flowchart_root_single_outgoing(diagram, e) != source_class)
            continue;
          if (!flowchart_json_append_edge(links_arr, e)) return 0;
        }
      }
    }
  }
  return 1;
}

char *flowchart_to_json_ex(const FlowchartDiagram *diagram, FlowchartJsonFormat format) {
  if (!diagram || (format != FLOWCHART_JSON_LEGACY && format != FLOWCHART_JSON_SOURCE_V2)) return NULL;
  json_value_t* root = json_create_object();
  if (!root) return NULL;
  if ((format == FLOWCHART_JSON_SOURCE_V2 &&
       !flowchart_json_put(root, "schema", json_create_string("stun.flowchart/2"))) ||
      !flowchart_json_put(root, "direction", json_create_string(diagram->direction ? diagram->direction : "TB")))
    goto fail;
  json_value_t* nodes = flowchart_json_object(root, "nodes");
  if (!nodes) goto fail;
  for (const FlowchartNode* n = diagram->nodes; n; n = n->next) {
    if (!n->id) continue;
    json_value_t* node = flowchart_json_object(nodes, n->id);
    if (!node || !flowchart_json_put(node, "id", json_create_string(n->id)) ||
        !flowchart_json_put(node, "shape", json_create_string(map_shape(n->shape))) ||
        (n->label && strcmp(n->label, n->id) != 0 && !flowchart_json_text(node, "text", n->label))) goto fail;
  }
  json_value_t* links = flowchart_json_array(root, "links");
  if (!links) goto fail;
  if (format == FLOWCHART_JSON_SOURCE_V2) {
    size_t ordinal = 0;
    for (const FlowchartEdge* e = diagram->edges; e; e = e->next, ++ordinal)
      if (!flowchart_json_v2_edge(links, e, ordinal)) goto fail;
  } else if (!flowchart_json_append_legacy_projection(links, diagram)) goto fail;
  json_value_t* subgraphs = flowchart_json_array(root, "subgraphs");
  if (!subgraphs || !serialize_subgraph_recursive(subgraphs, diagram, diagram->subgraphs)) goto fail;
  json_value_t* class_defs = flowchart_json_object(root, "classDefs");
  if (!class_defs) goto fail;
  for (const FlowchartClassDef* d = diagram->class_defs; d; d = d->next) {
    if (!d->id) continue;
    json_value_t* item = flowchart_json_object(class_defs, d->id);
    if (!item || !flowchart_json_put(item, "id", json_create_string(d->id)) ||
        !flowchart_json_styles(item, d->styles)) goto fail;
  }
  json_value_t* classes = flowchart_json_object(root, "classes");
  if (!classes) goto fail;
  for (const FlowchartClass* c = diagram->classes; c; c = c->next) {
    if (!c->node_id || !c->class_name) continue;
    json_value_t* names = json_object_get(classes, c->node_id);
    if (!names) names = flowchart_json_array(classes, c->node_id);
    if (!names) goto fail;
    int duplicate = 0;
    for (size_t i = 0; i < json_array_size(names); ++i) {
      json_value_t* old = json_array_get(names, i);
      if (old && json_type(old) == JSON_STRING && strcmp(json_string(old), c->class_name) == 0) {
        duplicate = 1;
        break;
      }
    }
    if (!duplicate && !flowchart_json_push(names, json_create_string(c->class_name))) goto fail;
  }
  json_value_t* clicks = flowchart_json_array(root, "clicks");
  if (!clicks) goto fail;
  for (const FlowchartClick* c = diagram->clicks; c; c = c->next) {
    if (!c->node_id || !c->target) continue;
    json_value_t* item = json_create_object();
    if (!item) goto fail;
    if (!flowchart_json_put(item, c->is_href ? "href" : "callback", json_create_string(c->target)) ||
        !flowchart_json_put(item, "nodeId", json_create_string(c->node_id))) {
      json_free(item);
      goto fail;
    }
    if (!flowchart_json_push(clicks, item)) goto fail;
  }
  json_value_t* link_styles = flowchart_json_array(root, "linkStyles");
  if (!link_styles) goto fail;
  for (const FlowchartLinkStyle* s = diagram->link_styles; s; s = s->next) {
    if (!s->index) continue;
    json_value_t* item = json_create_object();
    if (!item) goto fail;
    char* end = NULL;
    const long index = strtol(s->index, &end, 10);
    json_value_t* index_value = end && *end == '\0' && end != s->index
        ? json_create_number((double)index) : json_create_string(s->index);
    if (!flowchart_json_put(item, "index", index_value) || !flowchart_json_styles(item, s->styles)) {
      json_free(item);
      goto fail;
    }
    if (!flowchart_json_push(link_styles, item)) goto fail;
  }
  if (!flowchart_json_put(root, "type", json_create_string("flowchart"))) goto fail;
  size_t len;
  char* output = json_serialize_pretty(root, &len);
  json_free(root);
  return output;
fail:
  json_free(root);
  return NULL;
}

char *flowchart_to_json(FlowchartDiagram *diagram) {
  return flowchart_to_json_ex(diagram, FLOWCHART_JSON_LEGACY);
}

#endif /* !STUN_FLOWCHART_PARSER_ONLY */
