#ifndef FLOWCHART_AST_H
#define FLOWCHART_AST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FC_SHAPE_RECT,          // []
    FC_SHAPE_ROUND,         // ()
    FC_SHAPE_CIRCLE,        // (()) 
    FC_SHAPE_ELLIPSE,       // (- -)
    FC_SHAPE_RHOMBUS,       // { }
    FC_SHAPE_STADIUM,       // ([ ])
    FC_SHAPE_SUBROUTINE,    // [[ ]]
    FC_SHAPE_CYLINDER,      // [( )]
    FC_SHAPE_DOUBLECIRCLE,  // ((( )))
    FC_SHAPE_TRAPEZOID,     // [/ /]
    FC_SHAPE_INV_TRAPEZOID, // [\ \]
    FC_SHAPE_HEXAGON,       // {{ }}
    FC_SHAPE_DIAMOND        // { } - same as rhombus in many contexts
} FlowchartNodeShape;

typedef enum {
    FC_LAYOUT_LEGACY = 0,
    FC_LAYOUT_PROFESSIONAL = 1
} FlowchartLayoutMode;

typedef enum {
    FC_ROUTE_ORTHOGONAL = 0,
    FC_ROUTE_POLYLINE = 1
} FlowchartRoutingMode;

typedef struct FlowchartNode {
    char* id;
    char* label;
    FlowchartNodeShape shape;
    double layout_width;
    double layout_height;
    struct FlowchartNode* next;
} FlowchartNode;

typedef struct FlowchartEdge {
    char* from;
    char* to;
    char* label;
    char* arrow_type; 
    struct FlowchartEdge* next;
} FlowchartEdge;

typedef struct FlowchartNodeRef {
    char* id;
    struct FlowchartNodeRef* next;
} FlowchartNodeRef;

typedef struct FlowchartSubGraph {
    char* id;
    char* label;
    FlowchartNodeRef* node_refs;
    struct FlowchartSubGraph* children;
    struct FlowchartSubGraph* next;
} FlowchartSubGraph;

typedef struct FlowchartSubGraphStack {
    FlowchartSubGraph* sg;
    struct FlowchartSubGraphStack* next;
} FlowchartSubGraphStack;

typedef struct FlowchartDiagram {
    char* direction; 
    FlowchartLayoutMode layout_mode;
    FlowchartRoutingMode routing_mode;
    double routing_shape_buffer;
    double routing_nudging_distance;
    double routing_segment_penalty;
    double routing_angle_penalty;
    double routing_crossing_penalty;
    int routing_nudge_orthogonal_ends;
    int routing_nudge_shared_paths;
    FlowchartNode* nodes;
    FlowchartEdge* edges;
    FlowchartSubGraph* subgraphs;
} FlowchartDiagram;

typedef struct {
    FlowchartDiagram* diagram;
    int error_count;
    char* error_message;
    void* active_subgraphs; 
} FlowchartParserContext;

#ifdef __cplusplus
}
#endif

#endif // FLOWCHART_AST_H
