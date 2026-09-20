#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gantt/gantt_ast.h"
#include "gantt_parser_gen.h"
#include "json_parser.h"

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
    if (!input) return NULL;
    GanttParserContext ctx;
    ctx.diagram = (GanttDiagram*)malloc(sizeof(GanttDiagram));
    if (!ctx.diagram) return NULL;
    memset(ctx.diagram, 0, sizeof(GanttDiagram));
    ctx.error_count = 0;
    ctx.error_message = NULL;
    ctx.current_section = NULL;

    void* parser = GanttParserAlloc(malloc);
    if (!parser) {
        gantt_diagram_free(ctx.diagram);
        return NULL;
    }
    
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

    json_value_t* root = json_create_object();
    
    // Add keys in alphabetical order
    if (diagram->axis_format) {
        json_object_set_string(root, "axisFormat", diagram->axis_format);
    }
    json_object_add(root, "clickEvents", json_create_array());
    json_object_set_string(root, "dateFormat", diagram->date_format ? diagram->date_format : "");
    if (diagram->excludes) {
        json_object_set_string(root, "excludes", diagram->excludes);
    }
    json_object_set_bool(root, "inclusiveEndDates", diagram->inclusive_end_dates ? true : false);
    
    json_value_t* sections_arr = json_create_array();
    GanttSection* s = diagram->sections;
    while (s) {
        json_value_t* s_obj = json_create_object();
        json_object_set_string(s_obj, "name", s->name ? s->name : "");
        
        json_value_t* tasks_arr = json_create_array();
        GanttTask* t = s->tasks;
        while (t) {
            json_value_t* t_obj = json_create_object();
            json_object_set_string(t_obj, "end", t->end ? t->end : "");
            json_object_set_string(t_obj, "id", t->id ? t->id : "");
            json_object_set_string(t_obj, "name", t->name ? t->name : "");
            
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
            json_object_set_string(t_obj, "rawData", raw);
            
            json_object_set_string(t_obj, "section", s->name ? s->name : "");
            json_object_set_string(t_obj, "start", t->start ? t->start : "");

            if (status_str) {
                json_object_set_string(t_obj, "status", status_str);
            }
            
            json_array_add(tasks_arr, t_obj);
            t = t->next;
        }
        json_object_add(s_obj, "tasks", tasks_arr);
        json_array_add(sections_arr, s_obj);
        s = s->next;
    }
    json_object_add(root, "sections", sections_arr);
    
    json_object_add(root, "tasks", json_create_array());
    json_object_set_string(root, "title", diagram->title ? diagram->title : "");
    json_object_set_bool(root, "topAxis", diagram->top_axis ? true : false);
    json_object_set_string(root, "type", "gantt");

    size_t len;
    char* str = json_serialize_pretty(root, &len);
    json_free(&root);
    return str;
}
