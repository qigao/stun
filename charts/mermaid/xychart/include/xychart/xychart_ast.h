#ifndef XYCHART_AST_H
#define XYCHART_AST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef enum {
    XY_ORIENTATION_VERTICAL,
    XY_ORIENTATION_HORIZONTAL
} XYOrientation;

typedef struct {
    char** items;
    int count;
} XYStringList;

typedef struct {
    double* items;
    int count;
} XYNumberList;

typedef struct {
    char* title;
    bool has_range;
    double range_min;
    double range_max;
    XYStringList categories; // for X-axis
} XYAxis;

typedef enum {
    XY_SERIES_LINE,
    XY_SERIES_BAR
} XYSeriesType;

typedef struct XYSeries {
    XYSeriesType type;
    char* name;
    XYNumberList data;
    struct XYSeries* next;
} XYSeries;

typedef struct XYDiagram {
    char* title;
    char* accTitle;
    char* accDescr;
    XYOrientation orientation;
    XYAxis xAxis;
    XYAxis yAxis;
    XYSeries* series;
} XYDiagram;

typedef struct {
    XYDiagram* diagram;
    int error_count;
    char* error_message;
} XYParserContext;

XYDiagram* xychart_create_diagram();
void xychart_free_diagram(XYDiagram* d);

void xychart_set_orientation(XYParserContext* ctx, XYOrientation orientation);
void xychart_set_title(XYParserContext* ctx, const char* title);
void xychart_set_acc_title(XYParserContext* ctx, const char* title);
void xychart_set_acc_descr(XYParserContext* ctx, const char* descr);

void xychart_set_x_axis_title(XYParserContext* ctx, const char* title);
void xychart_set_x_axis_range(XYParserContext* ctx, double min, double max);
void xychart_set_x_axis_categories(XYParserContext* ctx, XYStringList categories);

void xychart_set_y_axis_title(XYParserContext* ctx, const char* title);
void xychart_set_y_axis_range(XYParserContext* ctx, double min, double max);

void xychart_add_series(XYParserContext* ctx, XYSeriesType type, const char* name, XYNumberList data);

#ifdef __cplusplus
}
#endif

#endif // XYCHART_AST_H
