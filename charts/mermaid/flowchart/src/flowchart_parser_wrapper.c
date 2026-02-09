#include "flowchart/flowchart_ast.h"
#include "flowchart_parser_gen.h"
#include "turbo_parser.h"
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

FlowchartDiagram *flowchart_parse(const char *input) {
  if (!input)
    return NULL;

  // extern void FlowchartParserTrace(FILE *, char *);
  // FlowchartParserTrace(stdout, "Parser >> ");

  FlowchartParserContext ctx;
  memset(&ctx, 0, sizeof(FlowchartParserContext));
  ctx.diagram = (FlowchartDiagram *)malloc(sizeof(FlowchartDiagram));
  if (!ctx.diagram)
    return NULL;
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
    return NULL;
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

  if (ctx.error_count > 0) {
    // We keep the diagram even with errors if it has nodes, but here we follow original safety
  }

  return ctx.diagram;
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
    return "rhombus";
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
    json_value_t *sg_det = turbo_json_create_object();
    turbo_json_object_set_string(sg_det, "id", sg->id ? sg->id : "");

    // Nodes
    json_value_t *nodes_arr = turbo_json_create_array();
    FlowchartNodeRef *nr = sg->node_refs;
    while (nr) {
      if (nr->id)
        turbo_json_array_add(nodes_arr, turbo_json_create_string(nr->id));
      nr = nr->next;
    }
    turbo_json_object_add(sg_det, "nodes", nodes_arr);

    // Title
    json_value_t *title_obj = turbo_json_create_object();
    turbo_json_object_set_string(title_obj, "text", sg->label ? sg->label : (sg->id ? sg->id : ""));
    turbo_json_object_set_string(title_obj, "type", "text");
    turbo_json_object_add(sg_det, "title", title_obj);

    // Only add subgraphs key if there are nested ones
    if (sg->children) {
      json_value_t *children_arr = turbo_json_create_array();
      serialize_subgraph_recursive(children_arr, sg->children);
      turbo_json_object_add(sg_det, "subgraphs", children_arr);
    }

    turbo_json_array_add(parent_arr, sg_det);
    sg = sg->next;
  }
}

char *flowchart_to_json(FlowchartDiagram *diagram) {
  if (!diagram)
    return NULL;

  json_value_t *root = turbo_json_create_object();
  turbo_json_object_set_string(root, "direction", diagram->direction ? diagram->direction : "TB");

  // Nodes
  json_value_t *nodes_obj = turbo_json_create_object();
  FlowchartNode *n = diagram->nodes;
  while (n) {
    if (n->id) {
      json_value_t *n_det = turbo_json_create_object();
      turbo_json_object_set_string(n_det, "id", n->id);
      turbo_json_object_set_string(n_det, "shape", map_shape(n->shape));

      json_value_t *text_obj = turbo_json_create_object();
      turbo_json_object_set_string(text_obj, "text", n->label ? n->label : n->id);
      turbo_json_object_set_string(text_obj, "type", "text");
      turbo_json_object_add(n_det, "text", text_obj);

      turbo_json_object_add(nodes_obj, n->id, n_det);
    }
    n = n->next;
  }
  turbo_json_object_add(root, "nodes", nodes_obj);

  // Edges
  json_value_t *links_arr = turbo_json_create_array();
  FlowchartEdge *e = diagram->edges;
  while (e) {
    json_value_t *e_det = turbo_json_create_object();
    turbo_json_object_set_number(e_det, "length", 1);
    turbo_json_object_set_string(e_det, "source", e->from ? e->from : "");
    const char *stroke = "normal";
    if (e->arrow_type && strstr(e->arrow_type, "==="))
      stroke = "thick";
    if (e->arrow_type && strstr(e->arrow_type, "-.-"))
      stroke = "dotted";
    turbo_json_object_set_string(e_det, "stroke", stroke);
    turbo_json_object_set_string(e_det, "target", e->to ? e->to : "");
    turbo_json_object_set_string(e_det, "type", map_arrow(e->arrow_type));

    if (e->label) {
      json_value_t *text_obj = turbo_json_create_object();
      turbo_json_object_set_string(text_obj, "text", e->label);
      turbo_json_object_set_string(text_obj, "type", "text");
      turbo_json_object_add(e_det, "text", text_obj);
    }
    turbo_json_array_add(links_arr, e_det);
    e = e->next;
  }
  turbo_json_object_add(root, "links", links_arr);

  // Subgraphs
  json_value_t *subgraphs_arr = turbo_json_create_array();
  serialize_subgraph_recursive(subgraphs_arr, diagram->subgraphs);
  turbo_json_object_add(root, "subgraphs", subgraphs_arr);

  // Meta / Styles
  turbo_json_object_add(root, "classDefs", turbo_json_create_object());
  turbo_json_object_add(root, "classes", turbo_json_create_object());
  turbo_json_object_add(root, "clicks", turbo_json_create_array());
  turbo_json_object_add(root, "linkStyles", turbo_json_create_array());
  turbo_json_object_set_string(root, "type", "flowchart");

  size_t len;
  char *str = turbo_json_serialize_pretty(root, &len);
  turbo_free_json(&root);
  return str;
}
