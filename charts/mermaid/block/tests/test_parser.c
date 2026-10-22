#include "tinytest.h"
#include "block/block_ast.h"
#include "json_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef REQUIRE
#define REQUIRE(cond) do { if (!(cond)) { check(0, #cond); return; } } while (0)
#endif

extern BlockDiagram* block_parse(const char* input);
extern void block_free(BlockDiagram* diagram);
extern char* block_to_json(BlockDiagram* diagram);

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

/* These checked-in cases have source/normalized Mermaid, not JSON oracles.
 * Validate both forms explicitly on every platform without inventing goldens. */
static void check_block_fixture(const char* name, const char* form) {
    char* input = load_golden_file(name, form, "mmd");
    check(input != NULL, "required Block fixture exists");
    if (!input) return;
    BlockDiagram* diagram = block_parse(input);
    check(diagram != NULL, "Block fixture parses");
    if (diagram) {
        char* actual = block_to_json(diagram);
        json_value_t* parsed = actual ? json_parse(actual, strlen(actual)) : NULL;
        check(parsed != NULL, "Block fixture exports valid JSON");
        json_free(parsed);
        json_serialize_free(actual);
        block_free(diagram);
    }
    free(input);
}

spec("block_parser") {
    describe("parsing basics") {
        it("should sanity check manual input") {
            const char* input = "block-beta\n  columns 2\n  A\n  B\n";
            BlockDiagram* d = block_parse(input);
            REQUIRE(d != NULL);
            block_free(d);
        }
    }

    describe("checked-in source and normalized fixtures") {
        const char* cases[] = {"basic", "columns", "nested", "with-edges"};
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            it(cases[i]) {
                check_block_fixture(cases[i], "input");
                check_block_fixture(cases[i], "output");
            }
        }
    }
}
