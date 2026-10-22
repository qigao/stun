#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "tinytest.h"
#include "xychart/xychart_ast.h"
#include "json_parser.h"

#ifndef REQUIRE
#define REQUIRE(cond) do { if (!(cond)) { check(0, #cond); return; } } while (0)
#endif

// Forward declarations
extern XYDiagram* xychart_parse(const char* input);
extern void xychart_free_diagram(XYDiagram* diagram);
extern char* xychart_to_json(XYDiagram* diagram);

// --- Helpers ---

static char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }

    char* buf = (char*)malloc(size + 1);
    if (fread(buf, 1, size, f) != (size_t)size) {
        free(buf); fclose(f); return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static char* load_golden_file(const char* case_name, const char* suffix, const char* ext) {
    char path[512];
    if (suffix && strlen(suffix) > 0) {
        snprintf(path, sizeof(path), "%s/%s.%s.%s", GOLDEN_DIR, case_name, suffix, ext);
    } else {
        snprintf(path, sizeof(path), "%s/%s.%s", GOLDEN_DIR, case_name, ext);
    }
    return read_file(path);
}

static int same_text(const char* a, const char* b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

/* The checked-in renderer oracle uses label/values/min/max; the existing
 * public exporter uses name/data/rangeMin/rangeMax and explicit defaults.
 * Compare oracle facts to the AST, preserving both public JSON and fixtures. */
static void check_axis_oracle(const XYAxis* axis, const json_value_t* oracle) {
    check(same_text(axis->title, json_get_string(oracle, "title")), "axis title");
    const json_value_t* min = json_object_get(oracle, "min");
    const json_value_t* max = json_object_get(oracle, "max");
    check(axis->has_range == (min != NULL && max != NULL), "axis range presence");
    if (min && max) {
        check(fabs(axis->range_min - json_number(min)) < 0.0001, "axis minimum");
        check(fabs(axis->range_max - json_number(max)) < 0.0001, "axis maximum");
    }
    const json_value_t* categories = json_object_get(oracle, "categories");
    check((size_t)axis->categories.count == json_array_size(categories), "category count");
    for (int i = 0; i < axis->categories.count; ++i) {
        check(same_text(axis->categories.items[i], json_string(json_array_get(categories, (size_t)i))),
              "category text");
    }
}

static void check_xy_oracle(const XYDiagram* diagram, const char* expected) {
    json_value_t* oracle = json_parse(expected, strlen(expected));
    check(oracle != NULL, "required XYChart oracle is valid JSON");
    if (!oracle) return;
    check(same_text(json_get_string(oracle, "type"), "xychart"), "oracle chart type");
    check(same_text(diagram->title, json_get_string(oracle, "title")), "chart title");
    check(same_text(diagram->accTitle, json_get_string(oracle, "accTitle")), "accessibility title");
    check(same_text(diagram->accDescr, json_get_string(oracle, "accDescription")), "accessibility description");
    const char* orientation = json_get_string(oracle, "orientation");
    check(diagram->orientation == (orientation && strcmp(orientation, "horizontal") == 0
          ? XY_ORIENTATION_HORIZONTAL : XY_ORIENTATION_VERTICAL), "orientation");
    check_axis_oracle(&diagram->xAxis, json_object_get(oracle, "xAxis"));
    check_axis_oracle(&diagram->yAxis, json_object_get(oracle, "yAxis"));
    const json_value_t* series = json_object_get(oracle, "series");
    const XYSeries* current = diagram->series;
    size_t count = 0;
    for (; current; current = current->next, ++count) {
        const json_value_t* item = json_array_get(series, count);
        check(item != NULL, "series has oracle entry");
        if (!item) continue;
        check(same_text(current->name, json_get_string(item, "label")), "series label");
        check(same_text(current->type == XY_SERIES_LINE ? "line" : "bar",
                        json_get_string(item, "type")), "series type");
        const json_value_t* values = json_object_get(item, "values");
        check((size_t)current->data.count == json_array_size(values), "series value count");
        for (int i = 0; i < current->data.count; ++i) {
            const json_value_t* value = json_array_get(values, (size_t)i);
            check(value && json_type(value) == JSON_NUMBER &&
                  fabs(current->data.items[i] - json_number(value)) < 0.0001,
                  "series value");
        }
    }
    check(count == json_array_size(series), "series count");
    json_free(oracle);
}

#ifdef check_double_eq
#undef check_double_eq
#endif
#define check_double_eq(a, b) do { \
    double diff = (a) - (b); \
    if (diff < 0) diff = -diff; \
    if (diff > 0.0001) { \
        printf("Expected %f, got %f\n", (double)(b), (double)(a)); \
        REQUIRE(0 && "Double mismatch"); \
    } \
} while(0)

static void check_xy_fixture(const char* name, const char* form, int has_json) {
    char* input = load_golden_file(name, form, "mmd");
    check(input != NULL, "required XYChart fixture exists");
    if (!input) return;
    XYDiagram* diagram = xychart_parse(input);
    check(diagram != NULL, "XYChart fixture parses");
    if (diagram) {
        char* actual = xychart_to_json(diagram);
        check(actual != NULL, "XYChart fixture exports JSON");
        if (has_json) {
            char* expected = load_golden_file(name, "", "json");
            check(expected != NULL, "required XYChart JSON golden exists");
            if (actual && expected) {
                check_xy_oracle(diagram, expected);
            }
            free(expected);
        } else {
            json_value_t* parsed = actual ? json_parse(actual, strlen(actual)) : NULL;
            check(parsed != NULL, "XYChart source fixture exports valid JSON");
            json_free(parsed);
        }
        json_serialize_free(actual);
        xychart_free_diagram(diagram);
    }
    free(input);
}

spec("xychart_parser") {
    describe("parsing basics") {
        it("should parse simple xychart with categories") {
             const char* input = 
                "xychart-beta\n"
                "title \"Sample Chart\"\n"
                "x-axis \"Categories\" [\"A\", \"B\", \"C\"]\n"
                "y-axis \"Revenue\" 0 --> 100\n"
                "line \"Sales\" [10, 20.5, 30]\n"
                "bar \"Profit\" [5, 12, 18]\n";
             
             XYDiagram* diagram = xychart_parse(input);
             REQUIRE(diagram != NULL);
             check_true(diagram->title != NULL && strcmp(diagram->title, "Sample Chart") == 0);
             
             REQUIRE(diagram->orientation == XY_ORIENTATION_VERTICAL);
             
             check_true(diagram->xAxis.title != NULL && strcmp(diagram->xAxis.title, "Categories") == 0);
             REQUIRE(diagram->xAxis.categories.count == 3);
             check_true(diagram->xAxis.categories.items[0] != NULL && strcmp(diagram->xAxis.categories.items[0], "A") == 0);
             check_true(diagram->xAxis.categories.items[1] != NULL && strcmp(diagram->xAxis.categories.items[1], "B") == 0);
             check_true(diagram->xAxis.categories.items[2] != NULL && strcmp(diagram->xAxis.categories.items[2], "C") == 0);
             
             check_true(diagram->yAxis.title != NULL && strcmp(diagram->yAxis.title, "Revenue") == 0);
             REQUIRE(diagram->yAxis.has_range == true);
             check_double_eq(diagram->yAxis.range_min, 0.0);
             check_double_eq(diagram->yAxis.range_max, 100.0);
             
             REQUIRE(diagram->series != NULL);
             XYSeries* s1 = diagram->series;
             REQUIRE(s1->type == XY_SERIES_LINE);
             check_true(s1->name != NULL && strcmp(s1->name, "Sales") == 0);
             REQUIRE(s1->data.count == 3);
             check_double_eq(s1->data.items[0], 10.0);
             check_double_eq(s1->data.items[1], 20.5);
             check_double_eq(s1->data.items[2], 30.0);
             
             XYSeries* s2 = s1->next;
             REQUIRE(s2 != NULL);
             REQUIRE(s2->type == XY_SERIES_BAR);
             check_true(s2->name != NULL && strcmp(s2->name, "Profit") == 0);
             REQUIRE(s2->data.count == 3);
             check_double_eq(s2->data.items[0], 5.0);
             check_double_eq(s2->data.items[1], 12.0);
             check_double_eq(s2->data.items[2], 18.0);
             
             xychart_free_diagram(diagram);
        }

        it("should parse xychart with horizontal orientation") {
             const char* input = 
                "xychart-beta horizontal\n"
                "x-axis 10 --> 50\n";
             
             XYDiagram* diagram = xychart_parse(input);
             REQUIRE(diagram != NULL);
             REQUIRE(diagram->orientation == XY_ORIENTATION_HORIZONTAL);
             REQUIRE(diagram->xAxis.has_range == true);
             check_double_eq(diagram->xAxis.range_min, 10.0);
             check_double_eq(diagram->xAxis.range_max, 50.0);
             
             xychart_free_diagram(diagram);
        }

        it("should evaluate numeric expressions with MIR") {
             const char* input =
                "xychart-beta\n"
                "x-axis 5*2 --> 40+10\n"
                "y-axis 0 --> 100/2\n"
                "line \"Series\" [2*3, 8+2, 25/5]\n";

             XYDiagram* diagram = xychart_parse(input);
             if (!diagram) {
                 check(0, "diagram parsed");
                 return;
             }
             check(diagram->xAxis.has_range == true, "x-axis range parsed");
             check(diagram->yAxis.has_range == true, "y-axis range parsed");
             check_double_eq(diagram->xAxis.range_min, 10.0);
             check_double_eq(diagram->xAxis.range_max, 50.0);
             check_double_eq(diagram->yAxis.range_max, 50.0);

             if (!diagram->series) {
                 check(0, "series parsed");
                 xychart_free_diagram(diagram);
                 return;
             }
             check(diagram->series->data.count == 3, "series data count");
             check_double_eq(diagram->series->data.items[0], 6.0);
             check_double_eq(diagram->series->data.items[1], 10.0);
             check_double_eq(diagram->series->data.items[2], 5.0);

             xychart_free_diagram(diagram);
        }
    }

    describe("source fixtures") {
        const char* cases[] = {"basic", "simple"};
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            it(cases[i]) {
                check_xy_fixture(cases[i], "input", 0);
            }
        }
    }

    describe("checked-in semantic JSON oracles on every platform") {
        const char* cases[] = {
            "render-simple", "render-complex", "render-titles", "render-axes"};
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            it(cases[i]) {
                check_xy_fixture(cases[i], "", 1);
            }
        }
    }
}
