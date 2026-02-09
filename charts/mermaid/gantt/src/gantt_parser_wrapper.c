#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gantt/gantt_ast.h"
#include "gantt_parser_gen.h"
#include "turbo_parser.h"

// Lexer and Parser functions (generated)
void *GanttParserAlloc(void *(*mallocProc)(size_t));
void GanttParser(void *yyp, int yymajor, void* yyminor, GanttParserContext *ctx);
void GanttParserFree(void *p, void (*freeProc)(void*));

typedef struct {
    const char *start;
    const char *cursor;
    const char *limit;
    const char *marker;
    int line;
} Scanner;

void gantt_scan(Scanner *s, void *parser, GanttParserContext *ctx);

static void free_tasks(GanttTask* t) {
    while (t) {
        GanttTask* next = t->next;
        free(t->name);
        free(t->id);
        free(t->start);
        free(t->end);
        free(t);
        t = next;
    }
}

static void free_sections(GanttSection* s) {
    while (s) {
        GanttSection* next = s->next;
        free(s->name);
        free_tasks(s->tasks);
        free(s);
        s = next;
    }
}

void gantt_diagram_free(GanttDiagram* diagram) {
    if (!diagram) return;
    free(diagram->title);
    free(diagram->date_format);
    free(diagram->axis_format);
    free_sections(diagram->sections);
    free(diagram);
}

GanttDiagram* gantt_parse(const char* input) {
    GanttParserContext ctx;
    ctx.diagram = (GanttDiagram*)malloc(sizeof(GanttDiagram));
    memset(ctx.diagram, 0, sizeof(GanttDiagram));
    ctx.error_count = 0;
    ctx.error_message = NULL;
    ctx.current_section = NULL;

    void* parser = GanttParserAlloc(malloc);
    
    Scanner s;
    s.start = input;
    s.cursor = input;
    s.limit = input + strlen(input);
    s.marker = NULL;
    s.line = 1;

    gantt_scan(&s, parser, &ctx);
    
    GanttParser(parser, GANTT_NL, NULL, &ctx);
    GanttParser(parser, 0, NULL, &ctx);
    GanttParserFree(parser, free);

    if (ctx.error_count > 0) {
        gantt_diagram_free(ctx.diagram);
        return NULL;
    }

    return ctx.diagram;
}

char* gantt_to_json(GanttDiagram* diagram) {
    if (!diagram) return NULL;

    json_value_t* root = turbo_json_create_object();
    
    // Add keys in alphabetical order
    if (diagram->axis_format) {
        turbo_json_object_set_string(root, "axisFormat", diagram->axis_format);
    }
    turbo_json_object_add(root, "clickEvents", turbo_json_create_array());
    turbo_json_object_set_string(root, "dateFormat", diagram->date_format ? diagram->date_format : "");
    if (diagram->excludes) {
        turbo_json_object_set_string(root, "excludes", diagram->excludes);
    }
    turbo_json_object_set_bool(root, "inclusiveEndDates", diagram->inclusive_end_dates ? true : false);
    
    json_value_t* sections_arr = turbo_json_create_array();
    GanttSection* s = diagram->sections;
    while (s) {
        json_value_t* s_obj = turbo_json_create_object();
        turbo_json_object_set_string(s_obj, "name", s->name ? s->name : "");
        
        json_value_t* tasks_arr = turbo_json_create_array();
        GanttTask* t = s->tasks;
        while (t) {
            json_value_t* t_obj = turbo_json_create_object();
            turbo_json_object_set_string(t_obj, "end", t->end ? t->end : "");
            turbo_json_object_set_string(t_obj, "id", t->id ? t->id : "");
            turbo_json_object_set_string(t_obj, "name", t->name ? t->name : "");
            
            // Build status string and rawData
            const char* status_str = NULL;
            if (t->status & GANTT_STATUS_DONE) status_str = "done";
            else if (t->status & GANTT_STATUS_ACTIVE) status_str = "active";
            else if (t->status & GANTT_STATUS_CRIT) status_str = "crit";
            else if (t->status & GANTT_STATUS_MILESTONE) status_str = "milestone";

            char raw[256];
            if (status_str) {
                snprintf(raw, sizeof(raw), ": %s, %s, %s, %s", status_str, t->id ? t->id : "", t->start ? t->start : "", t->end ? t->end : "");
            } else {
                snprintf(raw, sizeof(raw), ": %s, %s, %s", t->id ? t->id : "", t->start ? t->start : "", t->end ? t->end : "");
            }
            turbo_json_object_set_string(t_obj, "rawData", raw);
            
            turbo_json_object_set_string(t_obj, "section", s->name ? s->name : "");
            turbo_json_object_set_string(t_obj, "start", t->start ? t->start : "");

            if (status_str) {
                turbo_json_object_set_string(t_obj, "status", status_str);
            }
            
            turbo_json_array_add(tasks_arr, t_obj);
            t = t->next;
        }
        turbo_json_object_add(s_obj, "tasks", tasks_arr);
        turbo_json_array_add(sections_arr, s_obj);
        s = s->next;
    }
    turbo_json_object_add(root, "sections", sections_arr);
    
    turbo_json_object_add(root, "tasks", turbo_json_create_array());
    turbo_json_object_set_string(root, "title", diagram->title ? diagram->title : "");
    turbo_json_object_set_bool(root, "topAxis", diagram->top_axis ? true : false);
    turbo_json_object_set_string(root, "type", "gantt");

    size_t len;
    char* str = turbo_json_serialize_pretty(root, &len);
    turbo_free_json(&root);
    return str;
}
