#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "timeline/timeline_ast.h"
#include "turbo_parser.h"

static char* copy_string(const char* s) {
    if(!s) return NULL;
    return strdup(s);
}

TimelineDiagram* timeline_create_diagram() {
    TimelineDiagram* d = (TimelineDiagram*)malloc(sizeof(TimelineDiagram));
    if(d) {
        memset(d, 0, sizeof(TimelineDiagram));
    }
    return d;
}

static void free_events(TimelineEvent* e) {
    while(e) {
        TimelineEvent* next = e->next;
        free(e->text);
        free(e);
        e = next;
    }
}

static void free_periods(TimelinePeriod* p) {
    while(p) {
        TimelinePeriod* next = p->next;
        free(p->title);
        free_events(p->events);
        free(p);
        p = next;
    }
}

void timeline_free_diagram(TimelineDiagram* d) {
    if(!d) return;
    free(d->title);
    free(d->accTitle);
    free(d->accDescr);
    TimelineSection* s = d->sections;
    while(s) {
        TimelineSection* next = s->next;
        free(s->title);
        free_periods(s->periods);
        free(s);
        s = next;
    }
    free(d);
}

void timeline_set_title(TimelineParserContext* ctx, const char* title) {
    if(ctx && ctx->diagram) {
        if(ctx->diagram->title) free(ctx->diagram->title);
        ctx->diagram->title = copy_string(title);
    }
}

void timeline_set_acc_title(TimelineParserContext* ctx, const char* title) {
    if(ctx && ctx->diagram) {
        if(ctx->diagram->accTitle) free(ctx->diagram->accTitle);
        ctx->diagram->accTitle = copy_string(title);
    }
}

void timeline_set_acc_descr(TimelineParserContext* ctx, const char* descr) {
    if(ctx && ctx->diagram) {
        if(ctx->diagram->accDescr) free(ctx->diagram->accDescr);
        ctx->diagram->accDescr = copy_string(descr);
    }
}

void timeline_add_section(TimelineParserContext* ctx, const char* title) {
    if(!ctx || !ctx->diagram) return;
    TimelineSection* s = (TimelineSection*)malloc(sizeof(TimelineSection));
    if(s) {
        memset(s, 0, sizeof(TimelineSection));
        s->title = copy_string(title);
        if(!ctx->diagram->sections) {
            ctx->diagram->sections = s;
        } else {
            ctx->diagram->last_section->next = s;
        }
        ctx->diagram->last_section = s;
        ctx->current_section = s;
        ctx->current_period = NULL;
    }
}

void timeline_add_period(TimelineParserContext* ctx, const char* title) {
    if(!ctx || !ctx->diagram) return;
    
    // If no section yet, create a default anonymous section
    if(!ctx->current_section) {
        timeline_add_section(ctx, "");
    }
    
    TimelinePeriod* p = (TimelinePeriod*)malloc(sizeof(TimelinePeriod));
    if(p) {
        memset(p, 0, sizeof(TimelinePeriod));
        p->title = copy_string(title);
        if(!ctx->current_section->periods) {
            ctx->current_section->periods = p;
        } else {
            ctx->current_section->last_period->next = p;
        }
        ctx->current_section->last_period = p;
        ctx->current_period = p;
    }
}

void timeline_add_event(TimelineParserContext* ctx, const char* text) {
    if(!ctx || !ctx->current_period) return;
    
    TimelineEvent* e = (TimelineEvent*)malloc(sizeof(TimelineEvent));
    if(e) {
        memset(e, 0, sizeof(TimelineEvent));
        e->text = copy_string(text);
        if(!ctx->current_period->events) {
            ctx->current_period->events = e;
        } else {
            ctx->current_period->last_event->next = e;
        }
        ctx->current_period->last_event = e;
    }
}

char* timeline_to_json(const TimelineDiagram* d) {
    if(!d) {
        json_value_t *null_val = turbo_json_create_null();
        size_t len;
        char *s = turbo_json_serialize_pretty_crlf(null_val, &len);
        turbo_free_json(null_val);
        return s;
    }

    json_value_t *root = turbo_json_create_object();

    json_value_t *sections = turbo_json_create_array();
    for(const TimelineSection* s = d->sections; s; s = s->next) {
        json_value_t *section = turbo_json_create_object();
        turbo_json_object_set_string(section, "name", s->title ? s->title : "");

        json_value_t *periods = turbo_json_create_array();
        for(const TimelinePeriod* p = s->periods; p; p = p->next) {
            json_value_t *period = turbo_json_create_object();

            json_value_t *events = turbo_json_create_array();
            for(const TimelineEvent* e = p->events; e; e = e->next) {
                json_value_t *event = turbo_json_create_object();
                turbo_json_object_set_string(event, "text", e->text ? e->text : "");
                turbo_json_array_add(events, event);
            }
            turbo_json_object_add(period, "events", events);
            turbo_json_object_set_string(period, "name", p->title ? p->title : "");

            turbo_json_array_add(periods, period);
        }
        turbo_json_object_add(section, "periods", periods);

        turbo_json_array_add(sections, section);
    }
    turbo_json_object_add(root, "sections", sections);

    if(d->title)
        turbo_json_object_set_string(root, "title", d->title);
    else
        turbo_json_object_set_null(root, "title");

    turbo_json_object_set_string(root, "type", "timeline");

    size_t out_len;
    char *json_str = turbo_json_serialize_pretty_crlf(root, &out_len);
    turbo_free_json(root);

    return json_str;
}
