#ifndef JOURNEY_AST_H
#define JOURNEY_AST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct JourneyTask {
    char* title;
    int score;
    char** actors;
    int actor_count;
    struct JourneyTask* next;
} JourneyTask;

typedef struct JourneySection {
    char* title;
    JourneyTask* tasks;
    struct JourneySection* next;
} JourneySection;

typedef struct JourneyDiagram {
    char* title;
    char* acc_title;
    char* acc_descr;
    JourneySection* sections;
} JourneyDiagram;

typedef struct {
    JourneyDiagram* diagram;
    int error_count;
    char* error_message;
    JourneySection* current_section;
} JourneyParserContext;

JourneyDiagram* journey_create_diagram();
void journey_free_diagram(JourneyDiagram* d);
void journey_set_title(JourneyDiagram* d, const char* title);
void journey_set_acc_title(JourneyDiagram* d, const char* title);
void journey_set_acc_descr(JourneyDiagram* d, const char* descr);
void journey_add_section(JourneyParserContext* ctx, const char* title);
void journey_add_task(JourneyParserContext* ctx, const char* title, const char* data);
char* journey_to_json(JourneyDiagram* d);

#ifdef __cplusplus
}
#endif

#endif // JOURNEY_AST_H
