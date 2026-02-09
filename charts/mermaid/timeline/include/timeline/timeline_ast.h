#ifndef TIMELINE_AST_H
#define TIMELINE_AST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TimelineEvent {
    char* text;
    struct TimelineEvent* next;
} TimelineEvent;

typedef struct TimelinePeriod {
    char* title;
    TimelineEvent* events;
    struct TimelineEvent* last_event;
    struct TimelinePeriod* next;
} TimelinePeriod;

typedef struct TimelineSection {
    char* title;
    TimelinePeriod* periods;
    TimelinePeriod* last_period;
    struct TimelineSection* next;
} TimelineSection;

typedef struct TimelineDiagram {
    char* title;
    char* accTitle;
    char* accDescr;
    TimelineSection* sections;
    TimelineSection* last_section;
} TimelineDiagram;

typedef struct {
    TimelineDiagram* diagram;
    TimelineSection* current_section;
    TimelinePeriod* current_period;
    int error_count;
    char* error_message;
} TimelineParserContext;

TimelineDiagram* timeline_create_diagram();
void timeline_free_diagram(TimelineDiagram* d);

void timeline_set_title(TimelineParserContext* ctx, const char* title);
void timeline_set_acc_title(TimelineParserContext* ctx, const char* title);
void timeline_set_acc_descr(TimelineParserContext* ctx, const char* descr);

void timeline_add_section(TimelineParserContext* ctx, const char* title);
void timeline_add_period(TimelineParserContext* ctx, const char* title);
void timeline_add_event(TimelineParserContext* ctx, const char* text);

char* timeline_to_json(const TimelineDiagram* d);

#ifdef __cplusplus
}
#endif

#endif // TIMELINE_AST_H
