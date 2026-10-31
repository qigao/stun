#include <stun/ui_core.h>
#include <cmeta/cleanup.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test_) do { \
    if (!(test_)) { \
        fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #test_); \
        return 1; \
    } \
} while (0)

static int test_live_cmeta_reflection(void) {
    stun_ui_runtime_state model = {0};
    CHECK(stun_ui_runtime_state_init(&model, 120.f, 40.f) == STUN_UI_OK);
    CHECK(model.revision == 1u);

    cmeta_object_ref borrowed = CMETA_OBJECT_REF_INIT;
    CHECK(stun_ui_runtime_state_borrow(&model, &borrowed) == CMETA_OK);

    cmeta_object_field_binding binding = CMETA_OBJECT_FIELD_BINDING_INIT;
    CHECK(cmeta_object_field_bind(&borrowed, "scroll_row", &binding) == CMETA_OK);

    const void *borrowed_value = NULL;
    CHECK(cmeta_object_field_read_admitted(&binding, &borrowed_value) == CMETA_OK);
    CHECK(borrowed_value != NULL && *(const int *)borrowed_value == 0);

    CHECK(stun_ui_runtime_state_set_scroll(&model, 12) == STUN_UI_OK);
    CHECK(model.revision == 2u);
    CHECK(cmeta_object_field_read_admitted(&binding, &borrowed_value) == CMETA_OK);
    CHECK(*(const int *)borrowed_value == 12);

    CHECK(stun_ui_runtime_state_set_scroll(&model, 12) == STUN_UI_OK);
    CHECK(model.revision == 2u); /* Unchanged value is not dirty. */
    CHECK(stun_ui_runtime_state_set_scroll(&model, -1) ==
          STUN_UI_INVALID_ARGUMENT);
    CHECK(model.revision == 2u && model.scroll_row == 12);

    CHECK(stun_ui_runtime_state_set_viewport(&model, 90.f, 30.f) == STUN_UI_OK);
    CHECK(model.revision == 3u);
    CHECK(stun_ui_runtime_state_set_viewport(&model, NAN, 30.f) ==
          STUN_UI_INVALID_ARGUMENT);
    CHECK(model.viewport_width == 90.f && model.revision == 3u);

    cmeta_object_field_binding width = CMETA_OBJECT_FIELD_BINDING_INIT;
    CHECK(cmeta_object_field_bind(&borrowed, "viewport_width", &width) == CMETA_OK);
    CHECK(cmeta_object_field_read_admitted(&width, &borrowed_value) == CMETA_OK);
    CHECK(*(const float *)borrowed_value == 90.f);

    const cmeta_data_desc *field_data = NULL;
    CHECK(cmeta_object_field_read(&borrowed, "focused_index", &field_data,
                                  &borrowed_value) == CMETA_OK);
    CHECK(*(const int *)borrowed_value == -1);
    CHECK(stun_ui_runtime_state_set_focus(&model, 3) == STUN_UI_OK);
    CHECK(*(const int *)borrowed_value == 3);
    CHECK(model.revision == 4u);

    cmeta_object_release(&borrowed);
    CHECK(stun_ui_runtime_state_data() != NULL);
    model.revision = UINT64_MAX;
    CHECK(stun_ui_runtime_state_set_scroll(&model, 7) ==
          STUN_UI_CAPACITY_EXCEEDED);
    CHECK(model.scroll_row == 12 && model.revision == UINT64_MAX);
    return 0;
}

static int test_layout_ir(void) {
    const stun_ui_rect rects[] = {
        { 100u, STUN_UI_PARENT_NONE, 0.f, 0.f, 80.f, 24.f },
        { 101u, 0u, 0.f, 0.f, 20.f, 24.f },
        { 102u, 0u, 20.f, 0.f, 60.f, 24.f },
    };
    stun_ui_rect_view scene = {rects, 3u};
    CHECK(stun_ui_rect_view_validate(&scene, 3u) == STUN_UI_OK);
    CHECK(stun_ui_rect_view_validate(&scene, 2u) ==
          STUN_UI_CAPACITY_EXCEEDED);

    const stun_ui_rect duplicate[] = {
        { 100u, STUN_UI_PARENT_NONE, 0.f, 0.f, 80.f, 24.f },
        { 100u, 0u, 1.f, 0.f, 5.f, 5.f },
    };
    scene = (stun_ui_rect_view){duplicate, 2u};
    CHECK(stun_ui_rect_view_validate(&scene, 2u) ==
          STUN_UI_INVALID_ARGUMENT);

    return 0;
}

/* C11 lexical RAII: cleanup is owned by the scope, not by reflection. */
typedef struct cleanup_test {
    cmeta_cleanup slots[2];
    int order[2];
    int count;
} cleanup_test;

static void release_number(void *authority, void *resource) {
    cleanup_test *ctx = (cleanup_test *)authority;
    ctx->order[ctx->count++] = *(int *)resource;
    free(resource);
}
static cmeta_status cleanup_body(void *context) {
    cleanup_test *ctx = (cleanup_test *)context;
    for (int i = 0; i < 2; ++i) {
        int *value = (int *)malloc(sizeof(int));
        if (!value) return CMETA_CAPACITY_EXCEEDED;
        *value = i + 1;
        if (cmeta_cleanup_arm(&ctx->slots[i], release_number,
                              ctx, value) != CMETA_OK) {
            free(value);
            return CMETA_BUSY;
        }
    }
    return CMETA_INVALID_ARGUMENT; /* Failed body still releases in LIFO order. */
}
static int test_scope_cleanup(void) {
    cleanup_test ctx = {0};
    CHECK(cmeta_with_cleanups(ctx.slots, 2u, cleanup_body, &ctx) ==
          CMETA_INVALID_ARGUMENT);
    CHECK(ctx.count == 2 && ctx.order[0] == 2 && ctx.order[1] == 1);
    return 0;
}

int main(void) {
    if (test_live_cmeta_reflection()) return 1;
    if (test_layout_ir()) return 1;
    if (test_scope_cleanup()) return 1;
    puts("Stun C11 UI semantic core: tests passed");
    return 0;
}
