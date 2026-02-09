#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "journey/journey_ast.h"
#include "turbo_parser.h"

static char* copy_string(const char* s) {
    if(!s) return NULL;
    return strdup(s);
}

JourneyDiagram* journey_create_diagram() {
    JourneyDiagram* d = (JourneyDiagram*)malloc(sizeof(JourneyDiagram));
    if(d) {
        memset(d, 0, sizeof(JourneyDiagram));
    }
    return d;
}

void journey_free_diagram(JourneyDiagram* d) {
    if(!d) return;
    free(d->title);
    free(d->acc_title);
    free(d->acc_descr);
    
    JourneySection* s = d->sections;
    while(s) {
        JourneySection* next_s = s->next;
        free(s->title);
        
        JourneyTask* t = s->tasks;
        while(t) {
            JourneyTask* next_t = t->next;
            free(t->title);
            if(t->actors) {
                for(int i=0; i<t->actor_count; i++) {
                    free(t->actors[i]);
                }
                free(t->actors);
            }
            free(t);
            t = next_t;
        }
        
        free(s);
        s = next_s;
    }
    free(d);
}

void journey_set_title(JourneyDiagram* d, const char* title) {
    if(d) {
        if(d->title) free(d->title);
        d->title = copy_string(title);
    }
}

void journey_set_acc_title(JourneyDiagram* d, const char* title) {
    if(d) {
        if(d->acc_title) free(d->acc_title);
        d->acc_title = copy_string(title);
    }
}

void journey_set_acc_descr(JourneyDiagram* d, const char* descr) {
    if(d) {
        if(d->acc_descr) free(d->acc_descr);
        d->acc_descr = copy_string(descr);
    }
}

void journey_add_section(JourneyParserContext* ctx, const char* title) {
    if(ctx && ctx->diagram) {
        JourneySection* s = (JourneySection*)malloc(sizeof(JourneySection));
        memset(s, 0, sizeof(JourneySection));
        s->title = copy_string(title);
        
        // Append to list.
        if(!ctx->diagram->sections) {
            ctx->diagram->sections = s;
        } else {
            // Find last section
            JourneySection* cur = ctx->diagram->sections;
            while(cur->next) cur = cur->next;
            cur->next = s;
        }
        ctx->current_section = s;
    }
}

void journey_add_task(JourneyParserContext* ctx, const char* title, const char* data) {
    // data format: ": score: actors"
    // e.g. ": 5: Me, Cat"
    if(ctx && ctx->current_section) {
        JourneyTask* t = (JourneyTask*)malloc(sizeof(JourneyTask));
        memset(t, 0, sizeof(JourneyTask));
        t->title = copy_string(title);
        
        // Parse data
        const char *p = data;
        while(*p == ':' || *p == ' ') p++;
        
        // Parse score
        t->score = atoi(p);
        
        // Find next colon
         const char *colon = strchr(p, ':');
         if(colon) {
             const char *actors_str = colon + 1;
             int count = 1;
             const char *tmp = actors_str;
             while(*tmp) {
                 if(*tmp == ',') count++;
                 tmp++;
             }
             
             t->actors = (char**)malloc(sizeof(char*) * count);
             t->actor_count = 0;
             
             char* buf = strdup(actors_str);
             char* token = strtok(buf, ",");
             while(token) {
                 while(*token == ' ') token++;
                 t->actors[t->actor_count++] = strdup(token);
                 token = strtok(NULL, ",");
             }
             free(buf);
         }
        
        // Append to current section
        if(!ctx->current_section->tasks) {
            ctx->current_section->tasks = t;
        } else {
            JourneyTask* cur = ctx->current_section->tasks;
            while(cur->next) cur = cur->next;
            cur->next = t;
        }
    }
}

char* journey_to_json(JourneyDiagram* d) {
    if(!d) {
        json_value_t *null_val = turbo_json_create_null();
        size_t len;
        char *s = turbo_json_serialize_pretty_crlf(null_val, &len);
        turbo_free_json(null_val);
        return s;
    }

    json_value_t *root = turbo_json_create_object();

    json_value_t *sections = turbo_json_create_array();
    for(JourneySection* s = d->sections; s; s = s->next) {
        json_value_t *section = turbo_json_create_object();
        turbo_json_object_set_string(section, "name", s->title ? s->title : "");

        json_value_t *tasks = turbo_json_create_array();
        for(JourneyTask* t = s->tasks; t; t = t->next) {
            json_value_t *task = turbo_json_create_object();

            json_value_t *actors = turbo_json_create_array();
            for(int i = 0; i < t->actor_count; i++) {
                turbo_json_array_add(actors, turbo_json_create_string(t->actors[i]));
            }
            turbo_json_object_add(task, "actors", actors);
            turbo_json_object_set_string(task, "name", t->title ? t->title : "");
            turbo_json_object_set_number(task, "score", t->score);

            turbo_json_array_add(tasks, task);
        }
        turbo_json_object_add(section, "tasks", tasks);

        turbo_json_array_add(sections, section);
    }
    turbo_json_object_add(root, "sections", sections);
    turbo_json_object_set_string(root, "title", d->title ? d->title : "");
    turbo_json_object_set_string(root, "type", "journey");

    size_t out_len;
    char *json_str = turbo_json_serialize_pretty_crlf(root, &out_len);
    turbo_free_json(root);

    return json_str;
}
