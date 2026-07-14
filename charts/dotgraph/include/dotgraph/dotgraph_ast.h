#ifndef DOTGRAPH_AST_H
#define DOTGRAPH_AST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Shape Enum ── */
typedef enum {
    DG_SHAPE_ELLIPSE = 0,
    DG_SHAPE_BOX,
    DG_SHAPE_CIRCLE,
    DG_SHAPE_DIAMOND,
    DG_SHAPE_RECORD,
    DG_SHAPE_PLAINTEXT,
    DG_SHAPE_DOUBLECIRCLE,
    DG_SHAPE_TRIANGLE,
    DG_SHAPE_HEXAGON,
    DG_SHAPE_PARALLELOGRAM,
    DG_SHAPE_CYLINDER,
    DG_SHAPE_NOTE,
    DG_SHAPE_COMPONENT,
    DG_SHAPE_FOLDER
} DotGraphShape;

/* ── Compass Point ── */
typedef enum {
    DG_COMPASS_NONE = 0,
    DG_COMPASS_N,
    DG_COMPASS_NE,
    DG_COMPASS_E,
    DG_COMPASS_SE,
    DG_COMPASS_S,
    DG_COMPASS_SW,
    DG_COMPASS_W,
    DG_COMPASS_NW,
    DG_COMPASS_C
} DotGraphCompass;

/* ── Rank Direction ── */
typedef enum {
    DG_RANKDIR_TB = 0,
    DG_RANKDIR_BT,
    DG_RANKDIR_LR,
    DG_RANKDIR_RL
} DotGraphRankdir;

/* ── Routing Mode ── */
typedef enum {
    DG_ROUTE_ORTHOGONAL = 0,
    DG_ROUTE_POLYLINE
} DotGraphRoutingMode;

/* ── Attribute key/value pair (linked list) ── */
typedef struct DotGraphAttr {
    char* key;
    char* value;
    struct DotGraphAttr* next;
} DotGraphAttr;

/* ── Node ── */
typedef struct DotGraphNode {
    char* id;
    char* label;
    DotGraphShape shape;
    char* color;
    char* fillcolor;
    char* fontcolor;
    char* style;
    DotGraphAttr* attrs;
    double layout_width;
    double layout_height;
    struct DotGraphNode* next;
} DotGraphNode;

/* ── Edge ── */
typedef struct DotGraphEdge {
    char* from;
    char* from_port;
    DotGraphCompass from_compass;
    char* to;
    char* to_port;
    DotGraphCompass to_compass;
    char* label;
    char* color;
    char* style;
    DotGraphAttr* attrs;
    struct DotGraphEdge* next;
} DotGraphEdge;

/* ── Node reference (for subgraph membership) ── */
typedef struct DotGraphNodeRef {
    char* id;
    struct DotGraphNodeRef* next;
} DotGraphNodeRef;

/* ── SubGraph ── */
typedef struct DotGraphSubGraph {
    char* id;
    int is_cluster;
    char* label;
    char* color;
    char* style;
    DotGraphAttr* node_defaults;
    DotGraphAttr* edge_defaults;
    DotGraphNodeRef* node_refs;
    struct DotGraphSubGraph* children;
    struct DotGraphSubGraph* next;
} DotGraphSubGraph;

/* ── Top-level Diagram ── */
typedef struct DotGraphDiagram {
    int is_directed;
    int is_strict;
    char* graph_id;
    DotGraphRankdir rankdir;
    DotGraphRoutingMode routing_mode;
    double routing_shape_buffer;
    double routing_nudging_distance;
    double routing_segment_penalty;
    double routing_angle_penalty;
    double routing_crossing_penalty;
    int routing_nudge_orthogonal_ends;
    int routing_nudge_shared_paths;
    DotGraphAttr* graph_defaults;
    DotGraphAttr* node_defaults;
    DotGraphAttr* edge_defaults;
    DotGraphNode* nodes;
    DotGraphEdge* edges;
    DotGraphSubGraph* subgraphs;
} DotGraphDiagram;

/* ── Parser Context ── */
typedef struct {
    DotGraphDiagram* diagram;
    int error_count;
    int line;
    char error_message[256];
    void* active_subgraphs;
} DotGraphParserContext;

#ifdef __cplusplus
}
#endif

#endif /* DOTGRAPH_AST_H */
