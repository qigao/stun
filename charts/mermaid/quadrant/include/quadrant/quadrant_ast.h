#ifndef QUADRANT_AST_H
#define QUADRANT_AST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct QuadrantPoint {
    char* text;
    char* className;
    double x;
    double y;
    struct QuadrantPoint* next;
} QuadrantPoint;

typedef struct QuadrantDiagram {
    char* title;
    char* accTitle;
    char* accDescr;
    
    char* xAxisLeft;
    char* xAxisRight;
    char* yAxisBottom;
    char* yAxisTop;
    
    char* quadrant1Text;
    char* quadrant2Text;
    char* quadrant3Text;
    char* quadrant4Text;
    
    QuadrantPoint* points;
} QuadrantDiagram;

typedef struct {
    QuadrantDiagram* diagram;
    int error_count;
    char* error_message;
} QuadrantParserContext;

QuadrantDiagram* quadrant_create_diagram();
void quadrant_free_diagram(QuadrantDiagram* d);

void quadrant_set_title(QuadrantParserContext* ctx, const char* title);
void quadrant_set_acc_title(QuadrantParserContext* ctx, const char* title);
void quadrant_set_acc_descr(QuadrantParserContext* ctx, const char* descr);

void quadrant_set_x_axis_left(QuadrantParserContext* ctx, const char* text);
void quadrant_set_x_axis_right(QuadrantParserContext* ctx, const char* text);
void quadrant_set_y_axis_bottom(QuadrantParserContext* ctx, const char* text);
void quadrant_set_y_axis_top(QuadrantParserContext* ctx, const char* text);

void quadrant_set_quadrant_1_text(QuadrantParserContext* ctx, const char* text);
void quadrant_set_quadrant_2_text(QuadrantParserContext* ctx, const char* text);
void quadrant_set_quadrant_3_text(QuadrantParserContext* ctx, const char* text);
void quadrant_set_quadrant_4_text(QuadrantParserContext* ctx, const char* text);

void quadrant_add_point(QuadrantParserContext* ctx, const char* text, const char* className, double x, double y);

#ifdef __cplusplus
}
#endif

#endif // QUADRANT_AST_H
