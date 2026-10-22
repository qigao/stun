#include "flowchart/flowchart_parser_wrapper.h"
#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*JsonEqual)(const char*, const char*);

static char* read_contract(const char* filename) {
  FILE* f = fopen(filename, "rb");
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  const long n = ftell(f);
  if (n <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
  char* bytes = (char*)malloc((size_t)n + 1);
  if (!bytes) { fclose(f); return NULL; }
  const size_t used = fread(bytes, 1, (size_t)n, f);
  const int error = ferror(f);
  fclose(f);
  if (error || used != (size_t)n) { free(bytes); return NULL; }
  bytes[used] = '\0';
  return bytes;
}

static int compare_case(const char* name, const char* source, const json_value_t* expected,
                        int links_only, JsonEqual equal) {
  FlowchartDiagram* diagram = NULL;
  char* actual = NULL;
  char* wanted = NULL;
  json_value_t* parsed = NULL;
  char* comparison = NULL;
  int ok = 0;
  if (!source || !expected) goto done;
  const FlowchartParseStatus status = flowchart_parse_ex(source, &diagram);
  if (status != FLOWCHART_PARSE_COMPLETE || !diagram) {
    fprintf(stderr, "JSON v2 parse status %d: %s\n", (int)status, flowchart_get_last_error());
    goto done;
  }
  actual = flowchart_to_json_ex(diagram, FLOWCHART_JSON_SOURCE_V2);
  flowchart_diagram_free(diagram); diagram = NULL; // Export buffer is independently owned.
  if (!actual) goto done;
  wanted = json_serialize_pretty(expected, NULL);
  if (!wanted) goto done;
  if (links_only) {
    parsed = json_parse(actual, strlen(actual));
    const char* schema = parsed ? json_get_string(parsed, "schema") : NULL;
    json_value_t* links = parsed ? json_object_get(parsed, "links") : NULL;
    if (!schema || strcmp(schema, "stun.flowchart/2") != 0 || !links || json_type(links) != JSON_ARRAY)
      goto done;
    comparison = json_serialize_pretty(links, NULL);
    if (!comparison) goto done;
  }
  ok = equal(links_only ? comparison : actual, wanted);
done:
  if (!ok) fprintf(stderr, "JSON v2 contract failed: %s\nexpected: %s\nactual: %s\n",
                   name, wanted ? wanted : "<missing contract>", actual ? actual : "<parse/export refused>");
  json_serialize_free(comparison);
  json_serialize_free(wanted);
  json_serialize_free(actual);
  json_free(parsed);
  flowchart_diagram_free(diagram);
  return ok;
}

int flowchart_test_json_v2(JsonEqual equal) {
  char path[1024];
  snprintf(path, sizeof(path), "%s", JSON_V2_CONTRACT);
  char* bytes = read_contract(path);
  json_value_t* contract = bytes ? json_parse(bytes, strlen(bytes)) : NULL;
  free(bytes);
  const char* schema = contract ? json_get_string(contract, "schema") : NULL;
  json_value_t* cases = contract ? json_object_get(contract, "cases") : NULL;
  json_value_t* regressions = contract ? json_object_get(contract, "regressions") : NULL;
  int ok = schema && strcmp(schema, "stun-flowchart-json-v2-test/1") == 0 &&
           cases && json_type(cases) == JSON_OBJECT && json_object_size(cases) == 95 &&
           regressions && json_type(regressions) == JSON_OBJECT && json_object_size(regressions) == 6;
  if (!ok) fprintf(stderr, "Missing/incomplete/unsupported JSON v2 test contract\n");
  size_t graphs = 0, groups = 0;
  for (size_t i = 0; ok && i < json_object_size(cases); ++i) {
    const char* name = json_object_key(cases, i);
    json_value_t* entry = json_object_value(cases, i);
    snprintf(path, sizeof(path), "%s/%s.input.mmd", GOLDEN_DIR, name);
    char* source = read_contract(path);
    ok = compare_case(name, source, json_object_get(entry, "expected"), 0, equal);
    free(source);
    ++graphs;
  }
  for (size_t i = 0; ok && i < json_object_size(regressions); ++i) {
    const char* name = json_object_key(regressions, i);
    json_value_t* entry = json_object_value(regressions, i);
    ok = compare_case(name, json_get_string(entry, "source"),
                      json_object_get(entry, "expectedLinks"), 1, equal);
    ++groups;
  }
  json_free(contract);

  FlowchartDiagram* diagram = NULL;
  if (flowchart_parse_ex("flowchart LR\nA --> B\n", &diagram) != FLOWCHART_PARSE_COMPLETE || !diagram) ok = 0;
  if (diagram) {
    for (int format = -1; format <= 3; ++format) {
      if (format == FLOWCHART_JSON_LEGACY || format == FLOWCHART_JSON_SOURCE_V2) continue;
      char* invalid = flowchart_to_json_ex(diagram, (FlowchartJsonFormat)format);
      if (invalid) ok = 0;
      json_serialize_free(invalid);
    }
    char* a = flowchart_to_json(diagram);
    char* b = flowchart_to_json_ex(diagram, FLOWCHART_JSON_LEGACY);
    if (!a || !b || strcmp(a, b) != 0) ok = 0;
    json_serialize_free(a); json_serialize_free(b);
    char* saved = diagram->edges->arrow_type;
    diagram->edges->arrow_type = NULL;
    char* invalid = flowchart_to_json_ex(diagram, FLOWCHART_JSON_SOURCE_V2);
    diagram->edges->arrow_type = saved;
    if (invalid) ok = 0;
    json_serialize_free(invalid);
  }
  flowchart_diagram_free(diagram);
  char* null_input = flowchart_to_json_ex(NULL, FLOWCHART_JSON_SOURCE_V2);
  if (null_input) ok = 0;
  json_serialize_free(null_input);
  printf("JSON v2: %zu complete graph contracts, %zu edge groups; strict ordered comparison\n", graphs, groups);
  return ok;
}
