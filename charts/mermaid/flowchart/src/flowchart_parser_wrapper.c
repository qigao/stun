#include "flowchart/flowchart_parser_wrapper.h"
#include "flowchart_parser_gen.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
  const char *start;
  const char *cursor;
  const char *limit;
  const char *marker;
  int line;
} Scanner;

extern void flowchart_scan(Scanner *s, void *parser, FlowchartParserContext *ctx);

void *FlowchartParserAlloc(void *(*mallocProc)(size_t));
void FlowchartParser(void *yyp, int yymajor, void *yyminor, FlowchartParserContext *ctx);
void FlowchartParserFree(void *p, void (*freeProc)(void*));

void flowchart_diagram_free(FlowchartDiagram *diagram);

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

static FlowchartNodeRef *reverse_node_refs(FlowchartNodeRef *head) {
  FlowchartNodeRef *result = NULL;
  while (head) {
    FlowchartNodeRef *next = head->next;
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
    head->node_refs = reverse_node_refs(head->node_refs);
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
    free(e->from);
    free(e->to);
    free(e->label);
    free(e->arrow_type);
    free(e);
    e = next;
  }
  FlowchartSubGraph *sg = diagram->subgraphs;
  while (sg) {
    FlowchartSubGraph *next = sg->next;
    free_subgraph(sg);
    sg = next;
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
    return "trapezoid";
  case FC_SHAPE_INV_TRAPEZOID:
    return "inv_trapezoid";
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
  if (strstr(arrow, "---") || strstr(arrow, "-.-"))
    return "arrow_open";
  return "arrow_point";
}

static void serialize_subgraph_recursive(json_value_t *parent_arr, FlowchartSubGraph *sg) {
  int count = 0;
  while (sg) {
    if (++count > 1000) {
      printf("Infinite loop detected in subgraphs!\n");
      break;
    }
    // printf("Serializing subgraph: %s\n", sg->id);
    json_value_t *sg_det = json_create_object();
    json_object_set_string(sg_det, "id", sg->id ? sg->id : "");

    // Nodes
    json_value_t *nodes_arr = json_create_array();
    FlowchartNodeRef *nr = sg->node_refs;
    while (nr) {
      if (nr->id)
        json_array_add(nodes_arr, json_create_string(nr->id));
      nr = nr->next;
    }
    json_object_add(sg_det, "nodes", nodes_arr);

    // Title
    json_value_t *title_obj = json_create_object();
    json_object_set_string(title_obj, "text", sg->label ? sg->label : (sg->id ? sg->id : ""));
    json_object_set_string(title_obj, "type", "text");
    json_object_add(sg_det, "title", title_obj);

    // Only add subgraphs key if there are nested ones
    if (sg->children) {
      json_value_t *children_arr = json_create_array();
      serialize_subgraph_recursive(children_arr, sg->children);
      json_object_add(sg_det, "subgraphs", children_arr);
    }

    json_array_add(parent_arr, sg_det);
    sg = sg->next;
  }
}

char *flowchart_to_json(FlowchartDiagram *diagram) {
  if (!diagram)
    return NULL;

  json_value_t *root = json_create_object();
  json_object_set_string(root, "direction", diagram->direction ? diagram->direction : "TB");

  // Nodes
  json_value_t *nodes_obj = json_create_object();
  FlowchartNode *n = diagram->nodes;
  while (n) {
    if (n->id) {
      json_value_t *n_det = json_create_object();
      json_object_set_string(n_det, "id", n->id);
      json_object_set_string(n_det, "shape", map_shape(n->shape));

      if (n->label && strcmp(n->label, n->id) != 0) {
        json_value_t *text_obj = json_create_object();
        json_object_set_string(text_obj, "text", n->label);
        json_object_set_string(text_obj, "type", "text");
        json_object_add(n_det, "text", text_obj);
      }

      json_object_add(nodes_obj, n->id, n_det);
    }
    n = n->next;
  }
  json_object_add(root, "nodes", nodes_obj);

  // Edges
  json_value_t *links_arr = json_create_array();
  for (int labelled_pass = 1; labelled_pass >= 0; --labelled_pass) {
    FlowchartEdge *e = diagram->edges;
    while (e) {
      if ((e->label != NULL) != labelled_pass) {
        e = e->next;
        continue;
      }
    json_value_t *e_det = json_create_object();
    json_object_set_number(e_det, "length", 1);
    json_object_set_string(e_det, "source", e->from ? e->from : "");
    const char *stroke = "normal";
    if (e->arrow_type && strstr(e->arrow_type, "==="))
      stroke = "thick";
    if (e->arrow_type && strstr(e->arrow_type, "-.-"))
      stroke = "dotted";
    json_object_set_string(e_det, "stroke", stroke);
    json_object_set_string(e_det, "target", e->to ? e->to : "");
    json_object_set_string(e_det, "type", map_arrow(e->arrow_type));

    if (e->label) {
      json_value_t *text_obj = json_create_object();
      json_object_set_string(text_obj, "text", e->label);
      json_object_set_string(text_obj, "type", "text");
      json_object_add(e_det, "text", text_obj);
    }
    json_array_add(links_arr, e_det);
    e = e->next;
    }
  }
  json_object_add(root, "links", links_arr);

  // Subgraphs
  json_value_t *subgraphs_arr = json_create_array();
  serialize_subgraph_recursive(subgraphs_arr, diagram->subgraphs);
  json_object_add(root, "subgraphs", subgraphs_arr);

  // Meta / Styles
  json_object_add(root, "classDefs", json_create_object());
  json_object_add(root, "classes", json_create_object());
  json_object_add(root, "clicks", json_create_array());
  json_object_add(root, "linkStyles", json_create_array());
  json_object_set_string(root, "type", "flowchart");

  size_t len;
  char *str = json_serialize_pretty(root, &len);
  json_free(&root);
  return str;
}
