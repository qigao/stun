/* GNU linker interception at the real installed SDK boundary. No alternate
 * JSON implementation or production test hooks. Fail exactly one operation
 * after parsing; construction must return NULL and leave its AST reusable. */
#include "flowchart/flowchart_parser_wrapper.h"
#include "json_parser.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int armed;
static size_t operations, fail_at;
static const char* failed_kind;
static unsigned kinds;
enum { OBJECT=1, ARRAY=2, STRING=4, NUMBER=8, UINT=16, NIL=32,
       PUT=64, PUSH=128, SERIALIZE=256, MALLOC=512, CALLOC=1024 };
static int fails(unsigned kind, const char* name) {
    if (!armed) return 0;
    kinds |= kind;
    if (++operations != fail_at) return 0;
    failed_kind = name;
    return 1;
}

void* __real_malloc(size_t);
void* __real_calloc(size_t, size_t);
void* __wrap_malloc(size_t n) { return fails(MALLOC, "malloc") ? NULL : __real_malloc(n); }
void* __wrap_calloc(size_t n, size_t size) { return fails(CALLOC, "calloc") ? NULL : __real_calloc(n, size); }

#define FACTORY0(name, kind) \
    json_value_t* __real_##name(void); \
    json_value_t* __wrap_##name(void) { return fails(kind, #name) ? NULL : __real_##name(); }
FACTORY0(json_create_object, OBJECT)
FACTORY0(json_create_array, ARRAY)
FACTORY0(json_create_null, NIL)
#define FACTORY1(name, kind, type) \
    json_value_t* __real_##name(type); \
    json_value_t* __wrap_##name(type value) { return fails(kind, #name) ? NULL : __real_##name(value); }
FACTORY1(json_create_string, STRING, const char*)
FACTORY1(json_create_number, NUMBER, double)
FACTORY1(json_create_uint64, UINT, uint64_t)
bool __real_json_object_add_checked(json_value_t*, const char*, json_value_t*);
bool __wrap_json_object_add_checked(json_value_t* object, const char* key, json_value_t* value) {
    return fails(PUT, "json_object_add_checked") ? false : __real_json_object_add_checked(object, key, value);
}
bool __real_json_array_add_checked(json_value_t*, json_value_t*);
bool __wrap_json_array_add_checked(json_value_t* array, json_value_t* value) {
    return fails(PUSH, "json_array_add_checked") ? false : __real_json_array_add_checked(array, value);
}
char* __real_json_serialize_pretty(const json_value_t*, size_t*);
char* __wrap_json_serialize_pretty(const json_value_t* value, size_t* n) {
    return fails(SERIALIZE, "json_serialize_pretty") ? NULL : __real_json_serialize_pretty(value, n);
}

static int qualify(const char* source, size_t* tested) {
    FlowchartDiagram* diagram = NULL;
    if (flowchart_parse_ex(source, &diagram) != FLOWCHART_PARSE_COMPLETE || !diagram) {
        fprintf(stderr, "Fault driver source refused: %s\n", flowchart_get_last_error());
        flowchart_diagram_free(diagram);
        return 0;
    }
    int ok = 1;
    for (int f = FLOWCHART_JSON_LEGACY; ok && f <= FLOWCHART_JSON_SOURCE_V2; ++f) {
        operations = 0; fail_at = SIZE_MAX; armed = 1;
        char* baseline = flowchart_to_json_ex(diagram, (FlowchartJsonFormat)f);
        armed = 0;
        const size_t count = operations;
        if (!baseline || !count) { json_serialize_free(baseline); ok = 0; break; }
        for (size_t n = 1; ok && n <= count; ++n) {
            operations = 0; fail_at = n; failed_kind = NULL; armed = 1;
            char* output = flowchart_to_json_ex(diagram, (FlowchartJsonFormat)f);
            armed = 0;
            if (!failed_kind || output) {
                fprintf(stderr, "JSON format %d returned output after failure %zu/%zu (%s)\n",
                        f, n, count, failed_kind ? failed_kind : "not reached");
                ok = 0;
            }
            json_serialize_free(output);
            if (!ok) break;
            char* recovered = flowchart_to_json_ex(diagram, (FlowchartJsonFormat)f);
            if (!recovered || strcmp(baseline, recovered) != 0) {
                fprintf(stderr, "JSON format %d changed its AST/output after failure %zu\n", f, n);
                ok = 0;
            }
            json_serialize_free(recovered);
            ++*tested;
        }
        json_serialize_free(baseline);
    }
    flowchart_diagram_free(diagram);
    return ok;
}

int main(void) {
    const char* sources[] = {
        "flowchart TD\nsubgraph Top\nsubgraph Leaf\n"
        "A[Start] first@-->|go| B[Next]\nB --> C[End]\nend\n"
        "subgraph Declarations\nD[One]\nE[Two]\nend\nB --> D\nend\n"
        "A --> D\nA --> E\nclassDef c fill:blue,stroke:red\nclass A,B c\nclass A c\n"
        "linkStyle default stroke:blue,stroke-width:2px\nlinkStyle 1,2 stroke:red\n"
        "click A href \"https://example.test\" \"tip\"\nclick B callback \"tip\"\n"
        "first@{ curve: natural }\n",
        "flowchart LR\nsubgraph Group\nA -->|one| B\nA -->|two| B\nend\n",
        "flowchart LR\n",
    };
    size_t tested = 0;
    for (size_t i = 0; i < sizeof(sources)/sizeof(sources[0]); ++i)
        if (!qualify(sources[i], &tested)) return 1;
    if (kinds != (OBJECT | ARRAY | STRING | NUMBER | UINT | NIL | PUT | PUSH | SERIALIZE | MALLOC | CALLOC)) {
        fprintf(stderr, "Missing required allocation/insertion interception kinds: %u\n", kinds);
        return 1;
    }
    printf("JSON failure qualification: %zu one-shot failure points; both formats refuse partial output and recover identically\n", tested);
    return 0;
}
