#include "tinytest.h"
#include "sequence/sequence_ast.h"
#include "turbo_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

// Forward declaration of the wrapper function
extern SequenceDiagram* sequence_parse(const char* input);
extern void sequence_diagram_free(SequenceDiagram* diagram);
extern char* sequence_to_json(SequenceDiagram* diagram);

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

static int json_equal(const char* a, const char* b) {
    json_value_t *ja = NULL, *jb = NULL;
    if (turbo_parse_json((const uint8_t*)a, strlen(a), &ja) != 0) return 0;
    if (turbo_parse_json((const uint8_t*)b, strlen(b), &jb) != 0) {
        turbo_free_json(&ja);
        return 0;
    }

    char *sa = turbo_json_serialize(ja, NULL);
    char *sb = turbo_json_serialize(jb, NULL);
    int eq = (sa && sb && strcmp(sa, sb) == 0);
    turbo_json_serialize_free(sa);
    turbo_json_serialize_free(sb);
    turbo_free_json(&ja);
    turbo_free_json(&jb);
    return eq;
}

spec("sequence_parser") {
    describe("parsing basics") {
        it("should parse a simple sequence diagram") {
            const char* input = 
                "sequenceDiagram\n"
                "  Alice ->> Bob: Hello Bob, how are you?\n"
                "  Bob -->> Alice: I am good thanks!\n";

            SequenceDiagram* diagram = sequence_parse(input);
            REQUIRE(diagram != NULL);
            
            // Participants are prepended
            REQUIRE(diagram->participants != NULL);
            // Bob should be first if prepended
            check_str_eq(diagram->participants->id, "Bob");
            check_str_eq(diagram->participants->next->id, "Alice");
            
            SequenceStatement* s = diagram->statements;
            REQUIRE(s != NULL);
            check_int_eq(s->type, SEQ_STMT_SIGNAL);
            check_str_eq(s->data.signal.from, "Alice");
            check_str_eq(s->data.signal.to, "Bob");
            check_str_eq(s->data.signal.message, "Hello Bob, how are you?");
            check_int_eq(s->data.signal.signal_type, SEQ_SIGNAL_SOLID);
            
            s = s->next;
            REQUIRE(s != NULL);
            check_int_eq(s->type, SEQ_STMT_SIGNAL);
            check_str_eq(s->data.signal.from, "Bob");
            check_str_eq(s->data.signal.to, "Alice");
            check_str_eq(s->data.signal.message, "I am good thanks!");
            check_int_eq(s->data.signal.signal_type, SEQ_SIGNAL_DOTTED);
            
            sequence_diagram_free(diagram);
        }

        it("should parse participant and actor declarations") {
            const char* input = 
                "sequenceDiagram\n"
                "  participant A as Alice\n"
                "  actor B as Bob\n"
                "  A -> B: Hi\n";

            SequenceDiagram* diagram = sequence_parse(input);
            REQUIRE(diagram != NULL);
            
            SequenceParticipant* p = diagram->participants;
            REQUIRE(p != NULL);
            // B as Bob
            check_str_eq(p->id, "B");
            check_str_eq(p->label, "Bob");
            check_str_eq(p->type, "actor");
            
            p = p->next;
            REQUIRE(p != NULL);
            // A as Alice
            check_str_eq(p->id, "A");
            check_str_eq(p->label, "Alice");
            check_str_eq(p->type, "participant");
            
            sequence_diagram_free(diagram);
        }
    }

    describe("notes and activations") {
        it("should parse notes and activations") {
            const char* input = 
                "sequenceDiagram\n"
                "  Alice -> Bob: Hi\n"
                "  activate Bob\n"
                "  note left of Alice: Alice is thinking\n"
                "  Bob -> Alice: Bye\n"
                "  deactivate Bob\n";

            SequenceDiagram* diagram = sequence_parse(input);
            REQUIRE(diagram != NULL);
            
            SequenceStatement* s = diagram->statements;
            // 1: signal
            s = s->next; // 2: activate
            REQUIRE(s != NULL);
            check_int_eq(s->type, SEQ_STMT_ACTIVATE);
            check_str_eq(s->data.activation.actor, "Bob");
            
            s = s->next; // 3: note
            REQUIRE(s != NULL);
            check_int_eq(s->type, SEQ_STMT_NOTE);
            check_str_eq(s->data.note.actor, "Alice");
            check_str_eq(s->data.note.text, "Alice is thinking");
            check_int_eq(s->data.note.placement, SEQ_NOTE_LEFT_OF);
            
            sequence_diagram_free(diagram);
        }
    }

    describe("golden tests") {
#ifdef _WIN32
        char search_path[1024];
        snprintf(search_path, sizeof(search_path), "%s/*.input.mmd", GOLDEN_DIR);

        WIN32_FIND_DATAA find_data;
        HANDLE hFind = FindFirstFileA(search_path, &find_data);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                const char* filename = find_data.cFileName;
                const char* ext = strstr(filename, ".input.mmd");
                if (!ext) continue;

                size_t name_len = ext - filename;
                if (name_len >= 128) continue;

                char case_name[128];
                strncpy(case_name, filename, name_len);
                case_name[name_len] = '\0';

                it(case_name) {
                    char* input = load_golden_file(case_name, "input", "mmd");
                    if (!input) input = load_golden_file(case_name, "output", "mmd");

                    REQUIRE(input != NULL);

                    SequenceDiagram* diagram = sequence_parse(input);
                    if (!diagram) {
                         // Check if we expected a failure? 
                         // For now assume all golden inputs should parse.
                         check(0, "Failed to parse diagram");
                    } else {
                        char* actual = sequence_to_json(diagram);
                        REQUIRE(actual != NULL);

                        char* expected = load_golden_file(case_name, "output", "json");
                        // Only compare if we have an .output.json. 
                        // Generic .json might be in a different format.

                        if (expected) {
                            if (!json_equal(actual, expected)) {
                                printf("JSON mismatch for case: %s\n", case_name);
                                check(0, "JSON mismatch");
                            } else {
                                check(1, "JSON match");
                            }
                            free(expected);
                        } else {
                            check(1, "Parse successful and serialization worked (no golden json)");
                        }

                        turbo_json_serialize_free(actual);
                        sequence_diagram_free(diagram);
                    }
                    if (input) free(input);
                }
            } while (FindNextFileA(hFind, &find_data));
            FindClose(hFind);
        }
#endif
    }
}
