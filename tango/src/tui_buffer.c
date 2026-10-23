/*
 * TUI Buffer Implementation
 */

#include "tui.h"
#include <salts_unicode.h>
#include <stdlib.h>
#include <string.h>

static tui_cell_t blank_like(const tui_cell_t* cell) {
    return tui_cell_styled(' ', cell->fg, cell->bg, cell->style);
}

static void erase_cell_span(tui_buffer_t* buf, int x, int y) {
    if (!tui_buffer_in_bounds(buf, x, y)) return;

    tui_cell_t* cell = tui_buffer_at(buf, x, y);
    if (cell->span == TUI_CELL_SPAN_WIDE) {
        const tui_cell_t blank = blank_like(cell);
        *cell = blank;
        tui_buffer_mark_dirty(buf, x, y);
        if (tui_buffer_in_bounds(buf, x + 1, y)) {
            *tui_buffer_at(buf, x + 1, y) = blank;
            tui_buffer_mark_dirty(buf, x + 1, y);
        }
    } else if (cell->span == TUI_CELL_SPAN_CONTINUATION) {
        const tui_cell_t blank = blank_like(cell);
        *cell = blank;
        tui_buffer_mark_dirty(buf, x, y);
        if (tui_buffer_in_bounds(buf, x - 1, y)) {
            *tui_buffer_at(buf, x - 1, y) = blank;
            tui_buffer_mark_dirty(buf, x - 1, y);
        }
    }
}

static int classify_grapheme(vstr cluster, uint32_t* first_scalar) {
    size_t cursor = 0u;
    bool has_base = false;
    bool have_first = false;
    bool has_emoji = false;
    bool has_vs16 = false;
    bool wide = false;

    while (cursor < cluster.len) {
        salts_unicode_scalar scalar;
        const salts_unicode_status status =
            salts_unicode_utf8_next(cluster, &cursor, &scalar);
        if (status != SALTS_UNICODE_OK) return -1;
        if (!have_first) {
            if (first_scalar) *first_scalar = scalar.value;
            have_first = true;
        }

        if (scalar.value < 0x20u ||
            (scalar.value >= 0x7fu && scalar.value <= 0x9fu)) {
            continue;
        }

        salts_unicode_grapheme_break break_class;
        if (salts_unicode_grapheme_break_class(scalar.value, &break_class) !=
            SALTS_UNICODE_OK) return -1;
        if (break_class != SALTS_UNICODE_GRAPHEME_EXTEND &&
            break_class != SALTS_UNICODE_GRAPHEME_ZWJ &&
            break_class != SALTS_UNICODE_GRAPHEME_SPACING_MARK &&
            break_class != SALTS_UNICODE_GRAPHEME_PREPEND) {
            has_base = true;
        }

        has_emoji = has_emoji ||
            (scalar.properties & SALTS_UNICODE_PROPERTY_EMOJI) != 0u;
        has_vs16 = has_vs16 || scalar.value == 0xfe0fu;
        wide = wide ||
            (scalar.properties & SALTS_UNICODE_PROPERTY_EMOJI_PRESENTATION) != 0u;

        salts_unicode_line_break line_class;
        if (salts_unicode_line_break_class(scalar.value, &line_class) !=
            SALTS_UNICODE_OK) return -1;
        wide = wide || line_class == SALTS_UNICODE_LINE_BREAK_ID ||
            line_class == SALTS_UNICODE_LINE_BREAK_CJ ||
            line_class == SALTS_UNICODE_LINE_BREAK_H2 ||
            line_class == SALTS_UNICODE_LINE_BREAK_H3 ||
            line_class == SALTS_UNICODE_LINE_BREAK_JL ||
            line_class == SALTS_UNICODE_LINE_BREAK_JV ||
            line_class == SALTS_UNICODE_LINE_BREAK_JT;
    }

    if (has_emoji && has_vs16) wide = true;
    if (wide) return 2;
    return has_base ? 1 : 0;
}

int tui_grapheme_width(const char* utf8, size_t len) {
    if ((!utf8 && len != 0u) || len == 0u) return -1;
    const vstr input = vstr_from_buf(utf8, len);
    size_t cursor = 0u;
    vstr cluster;
    if (salts_unicode_grapheme_next(input, &cursor, &cluster) !=
            SALTS_UNICODE_OK ||
        cursor != len || cluster.len != len) return -1;
    return classify_grapheme(cluster, NULL);
}

/* ============================================================================
 * Buffer Lifecycle
 * ============================================================================ */

tui_buffer_t* tui_buffer_create(int width, int height) {
    tui_buffer_t* buf = (tui_buffer_t*)calloc(1, sizeof(tui_buffer_t));
    if (!buf) return NULL;
    
    size_t count = (size_t)width * (size_t)height;
    buf->cells = (tui_cell_t*)malloc(count * sizeof(tui_cell_t));
    if (!buf->cells) {
        free(buf);
        return NULL;
    }
    
    buf->width = width;
    buf->height = height;
    buf->capacity = count;
    
    tui_buffer_clear(buf);
    tui_buffer_reset_dirty(buf);
    
    return buf;
}

void tui_buffer_destroy(tui_buffer_t* buf) {
    if (!buf) return;
    free(buf->cells);
    free(buf);
}

void tui_buffer_resize(tui_buffer_t* buf, int width, int height) {
    if (!buf) return;
    
    size_t count = (size_t)width * (size_t)height;
    if (count > buf->capacity) {
        tui_cell_t* new_cells = (tui_cell_t*)realloc(buf->cells, count * sizeof(tui_cell_t));
        if (!new_cells) return;
        buf->cells = new_cells;
        buf->capacity = count;
    }
    
    buf->width = width;
    buf->height = height;
    tui_buffer_clear(buf);
    tui_buffer_reset_dirty(buf);
}

/* ============================================================================
 * Cell Access
 * ============================================================================ */

bool tui_buffer_in_bounds(const tui_buffer_t* buf, int x, int y) {
    return x >= 0 && x < buf->width && y >= 0 && y < buf->height;
}

tui_cell_t* tui_buffer_at(tui_buffer_t* buf, int x, int y) {
    return &buf->cells[y * buf->width + x];
}

const tui_cell_t* tui_buffer_at_const(const tui_buffer_t* buf, int x, int y) {
    return &buf->cells[y * buf->width + x];
}

/* ============================================================================
 * Dirty Tracking
 * ============================================================================ */

void tui_buffer_reset_dirty(tui_buffer_t* buf) {
    buf->dirty_min_x = buf->width;
    buf->dirty_min_y = buf->height;
    buf->dirty_max_x = -1;
    buf->dirty_max_y = -1;
}

void tui_buffer_mark_dirty(tui_buffer_t* buf, int x, int y) {
    if (!tui_buffer_in_bounds(buf, x, y)) return;
    if (x < buf->dirty_min_x) buf->dirty_min_x = x;
    if (y < buf->dirty_min_y) buf->dirty_min_y = y;
    if (x > buf->dirty_max_x) buf->dirty_max_x = x;
    if (y > buf->dirty_max_y) buf->dirty_max_y = y;
}

void tui_buffer_mark_all_dirty(tui_buffer_t* buf) {
    if (buf->width > 0 && buf->height > 0) {
        buf->dirty_min_x = 0;
        buf->dirty_min_y = 0;
        buf->dirty_max_x = buf->width - 1;
        buf->dirty_max_y = buf->height - 1;
    }
}

bool tui_buffer_is_dirty(const tui_buffer_t* buf) {
    return buf->dirty_max_x >= buf->dirty_min_x && buf->dirty_max_y >= buf->dirty_min_y;
}

/* ============================================================================
 * Drawing Primitives
 * ============================================================================ */

void tui_buffer_clear(tui_buffer_t* buf) {
    tui_cell_t def = TUI_CELL_DEFAULT;
    size_t count = (size_t)buf->width * (size_t)buf->height;
    for (size_t i = 0; i < count; i++) {
        buf->cells[i] = def;
    }
    tui_buffer_mark_all_dirty(buf);
}

void tui_buffer_clear_color(tui_buffer_t* buf, tui_color_t bg) {
    tui_cell_t cell = tui_cell(' ', TUI_WHITE, bg);
    size_t count = (size_t)buf->width * (size_t)buf->height;
    for (size_t i = 0; i < count; i++) {
        buf->cells[i] = cell;
    }
    tui_buffer_mark_all_dirty(buf);
}

void tui_buffer_set(tui_buffer_t* buf, int x, int y, tui_cell_t cell) {
    if (!tui_buffer_in_bounds(buf, x, y) ||
        cell.span == TUI_CELL_SPAN_CONTINUATION ||
        cell.span > TUI_CELL_SPAN_CONTINUATION ||
        cell.grapheme_len > TUI_CELL_GRAPHEME_CAPACITY) return;
    if (cell.span == TUI_CELL_SPAN_WIDE &&
        !tui_buffer_in_bounds(buf, x + 1, y)) return;

    erase_cell_span(buf, x, y);
    if (cell.span == TUI_CELL_SPAN_WIDE) erase_cell_span(buf, x + 1, y);
    *tui_buffer_at(buf, x, y) = cell;
    tui_buffer_mark_dirty(buf, x, y);
    if (cell.span == TUI_CELL_SPAN_WIDE) {
        tui_cell_t continuation = tui_cell_styled(0u, cell.fg, cell.bg,
                                                  cell.style);
        continuation.span = TUI_CELL_SPAN_CONTINUATION;
        *tui_buffer_at(buf, x + 1, y) = continuation;
        tui_buffer_mark_dirty(buf, x + 1, y);
    }
}

void tui_buffer_set_char(tui_buffer_t* buf, int x, int y, uint32_t ch) {
    if (tui_buffer_in_bounds(buf, x, y)) {
        const tui_cell_t* old = tui_buffer_at_const(buf, x, y);
        tui_buffer_set(buf, x, y,
                       tui_cell_styled(ch, old->fg, old->bg, old->style));
    }
}

bool tui_buffer_set_grapheme(tui_buffer_t* buf, int x, int y,
                             const char* utf8, size_t len,
                             tui_color_t fg, tui_color_t bg,
                             uint8_t style) {
    if (!buf || !utf8 || len == 0u ||
        len > TUI_CELL_GRAPHEME_CAPACITY ||
        !tui_buffer_in_bounds(buf, x, y)) return false;

    const vstr input = vstr_from_buf(utf8, len);
    size_t cursor = 0u;
    vstr cluster;
    if (salts_unicode_grapheme_next(input, &cursor, &cluster) !=
            SALTS_UNICODE_OK ||
        cursor != len || cluster.len != len) return false;

    uint32_t first = 0u;
    const int width = classify_grapheme(cluster, &first);
    if (width < 0 || width > 2) return false;
    if (width == 0) return true;
    if (width == 2 && !tui_buffer_in_bounds(buf, x + 1, y)) return false;

    tui_cell_t cell = tui_cell_styled(first, fg, bg, style);
    cell.span = width == 2 ? TUI_CELL_SPAN_WIDE : TUI_CELL_SPAN_SINGLE;
    cell.grapheme_len = (uint8_t)len;
    memcpy(cell.grapheme, utf8, len);
    tui_buffer_set(buf, x, y, cell);
    return true;
}

bool tui_buffer_text(tui_buffer_t* buf, int x, int y, const char* str,
                     tui_color_t fg, tui_color_t bg) {
    if (!buf || !str) return false;
    const vstr input = vstr_from_cstr(str);
    size_t cursor = 0u;
    vstr cluster;

    /* Validate the complete input and owned-cell bounds before publishing any
     * part of it.  Horizontal clipping is a successful drawing operation. */
    while (cursor < input.len) {
        if (salts_unicode_grapheme_next(input, &cursor, &cluster) !=
                SALTS_UNICODE_OK ||
            cluster.len > TUI_CELL_GRAPHEME_CAPACITY ||
            classify_grapheme(cluster, NULL) < 0) return false;
    }

    cursor = 0u;
    while (cursor < input.len && x < buf->width) {
        if (salts_unicode_grapheme_next(input, &cursor, &cluster) !=
            SALTS_UNICODE_OK) return false;
        const int width = classify_grapheme(cluster, NULL);
        if (width == 0) continue;
        if (x < 0) {
            x += width;
            continue;
        }
        if (width > buf->width - x) break;
        if (!tui_buffer_set_grapheme(buf, x, y, cluster.data, cluster.len,
                                     fg, bg, TUI_STYLE_NONE)) return false;
        x += width;
    }
    return true;
}

void tui_buffer_fill(tui_buffer_t* buf, int x, int y, int w, int h, tui_cell_t cell) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (tui_buffer_in_bounds(buf, x + i, y + j)) {
                tui_buffer_set(buf, x + i, y + j, cell);
            }
        }
    }
    tui_buffer_mark_dirty(buf, x, y);
    tui_buffer_mark_dirty(buf, x + w - 1, y + h - 1);
}

void tui_buffer_fill_color(tui_buffer_t* buf, int x, int y, int w, int h, tui_color_t bg) {
    tui_cell_t cell = tui_cell(' ', TUI_WHITE, bg);
    tui_buffer_fill(buf, x, y, w, h, cell);
}

/* ============================================================================
 * Lines
 * ============================================================================ */

void tui_buffer_hline(tui_buffer_t* buf, int x, int y, int len, uint32_t ch, tui_color_t fg, tui_color_t bg) {
    tui_cell_t cell = tui_cell(ch, fg, bg);
    for (int i = 0; i < len && x + i < buf->width; i++) {
        tui_buffer_set(buf, x + i, y, cell);
    }
}

void tui_buffer_vline(tui_buffer_t* buf, int x, int y, int len, uint32_t ch, tui_color_t fg, tui_color_t bg) {
    tui_cell_t cell = tui_cell(ch, fg, bg);
    for (int i = 0; i < len && y + i < buf->height; i++) {
        tui_buffer_set(buf, x, y + i, cell);
    }
}

/* ============================================================================
 * Boxes
 * ============================================================================ */

void tui_buffer_box(tui_buffer_t* buf, int x, int y, int w, int h, tui_color_t fg, tui_color_t bg) {
    if (w < 2 || h < 2) return;
    
    /* Corners */
    tui_buffer_set(buf, x, y, tui_cell(0x250C, fg, bg));             /* ┌ */
    tui_buffer_set(buf, x + w - 1, y, tui_cell(0x2510, fg, bg));     /* ┐ */
    tui_buffer_set(buf, x, y + h - 1, tui_cell(0x2514, fg, bg));     /* └ */
    tui_buffer_set(buf, x + w - 1, y + h - 1, tui_cell(0x2518, fg, bg)); /* ┘ */
    
    /* Edges */
    tui_buffer_hline(buf, x + 1, y, w - 2, 0x2500, fg, bg);          /* ─ */
    tui_buffer_hline(buf, x + 1, y + h - 1, w - 2, 0x2500, fg, bg);
    tui_buffer_vline(buf, x, y + 1, h - 2, 0x2502, fg, bg);          /* │ */
    tui_buffer_vline(buf, x + w - 1, y + 1, h - 2, 0x2502, fg, bg);
}

void tui_buffer_box_double(tui_buffer_t* buf, int x, int y, int w, int h, tui_color_t fg, tui_color_t bg) {
    if (w < 2 || h < 2) return;
    
    tui_buffer_set(buf, x, y, tui_cell(0x2554, fg, bg));             /* ╔ */
    tui_buffer_set(buf, x + w - 1, y, tui_cell(0x2557, fg, bg));     /* ╗ */
    tui_buffer_set(buf, x, y + h - 1, tui_cell(0x255A, fg, bg));     /* ╚ */
    tui_buffer_set(buf, x + w - 1, y + h - 1, tui_cell(0x255D, fg, bg)); /* ╝ */
    
    tui_buffer_hline(buf, x + 1, y, w - 2, 0x2550, fg, bg);          /* ═ */
    tui_buffer_hline(buf, x + 1, y + h - 1, w - 2, 0x2550, fg, bg);
    tui_buffer_vline(buf, x, y + 1, h - 2, 0x2551, fg, bg);          /* ║ */
    tui_buffer_vline(buf, x + w - 1, y + 1, h - 2, 0x2551, fg, bg);
}

void tui_buffer_box_round(tui_buffer_t* buf, int x, int y, int w, int h, tui_color_t fg, tui_color_t bg) {
    if (w < 2 || h < 2) return;
    
    tui_buffer_set(buf, x, y, tui_cell(0x256D, fg, bg));             /* ╭ */
    tui_buffer_set(buf, x + w - 1, y, tui_cell(0x256E, fg, bg));     /* ╮ */
    tui_buffer_set(buf, x, y + h - 1, tui_cell(0x2570, fg, bg));     /* ╰ */
    tui_buffer_set(buf, x + w - 1, y + h - 1, tui_cell(0x256F, fg, bg)); /* ╯ */
    
    tui_buffer_hline(buf, x + 1, y, w - 2, 0x2500, fg, bg);
    tui_buffer_hline(buf, x + 1, y + h - 1, w - 2, 0x2500, fg, bg);
    tui_buffer_vline(buf, x, y + 1, h - 2, 0x2502, fg, bg);
    tui_buffer_vline(buf, x + w - 1, y + 1, h - 2, 0x2502, fg, bg);
}

void tui_buffer_box_heavy(tui_buffer_t* buf, int x, int y, int w, int h, tui_color_t fg, tui_color_t bg) {
    if (w < 2 || h < 2) return;
    
    tui_buffer_set(buf, x, y, tui_cell(0x250F, fg, bg));             /* ┏ */
    tui_buffer_set(buf, x + w - 1, y, tui_cell(0x2513, fg, bg));     /* ┓ */
    tui_buffer_set(buf, x, y + h - 1, tui_cell(0x2517, fg, bg));     /* ┗ */
    tui_buffer_set(buf, x + w - 1, y + h - 1, tui_cell(0x251B, fg, bg)); /* ┛ */
    
    tui_buffer_hline(buf, x + 1, y, w - 2, 0x2501, fg, bg);          /* ━ */
    tui_buffer_hline(buf, x + 1, y + h - 1, w - 2, 0x2501, fg, bg);
    tui_buffer_vline(buf, x, y + 1, h - 2, 0x2503, fg, bg);          /* ┃ */
    tui_buffer_vline(buf, x + w - 1, y + 1, h - 2, 0x2503, fg, bg);
}
