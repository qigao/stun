#ifndef GANTT_AST_H
#define GANTT_AST_H

#include <stdlib.h>

typedef enum {
    GANTT_STATUS_NONE = 0,
    GANTT_STATUS_DONE = 1 << 0,
    GANTT_STATUS_ACTIVE = 1 << 1,
    GANTT_STATUS_CRIT = 1 << 2,
    GANTT_STATUS_MILESTONE = 1 << 3
} GanttTaskStatus;

typedef struct GanttTask {
    char* name;
    int status; // Bitmask of GanttTaskStatus
    char* id;
    char* start; // "2023-01-01" or "after a1"
    char* end;   // "2023-01-31" or "30d"
    struct GanttTask* next;
} GanttTask;

typedef struct GanttSection {
    char* name;
    GanttTask* tasks;
    struct GanttSection* next;
} GanttSection;

typedef struct GanttDiagram {
    char* title;
    char* date_format;
    char* axis_format;
    char* excludes;
    int inclusive_end_dates;
    int top_axis;
    GanttSection* sections;
} GanttDiagram;

typedef struct {
    GanttDiagram* diagram;
    int error_count;
    char* error_message;
    GanttSection* current_section;
} GanttParserContext;

#endif // GANTT_AST_H
