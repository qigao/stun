/*
 * TUI Widgets Implementation
 */

#include "tui.h"
#include <salts_unicode.h>
#include <string.h>

static inline int tui_min_i(int a, int b) { return a < b ? a : b; }
static inline float tui_clamp_f(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ============================================================================
 * Theme
 * ============================================================================ */

tui_theme_t tui_theme_default(void) {
    tui_theme_t t = {
        .bg        = TUI_COLOR(30, 30, 30),
        .fg        = TUI_COLOR(220, 220, 220),
        .primary   = TUI_COLOR(100, 149, 237),   /* Cornflower blue */
        .secondary = TUI_COLOR(128, 128, 128),
        .success   = TUI_COLOR(50, 205, 50),
        .warning   = TUI_COLOR(255, 165, 0),
        .error     = TUI_COLOR(220, 20, 60),
        .border    = TUI_COLOR(80, 80, 80),
        .highlight = TUI_COLOR(70, 130, 180),
    };
    return t;
}

/* ============================================================================
 * Progress Bar
 * ============================================================================ */

void tui_widget_progress(tui_buffer_t* buf, int x, int y, int width, float progress, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    progress = tui_clamp_f(progress, 0.0f, 1.0f);
    int filled = (int)(progress * (float)(width - 2));
    
    /* Border */
    tui_buffer_set(buf, x, y, tui_cell('[', t.fg, t.bg));
    tui_buffer_set(buf, x + width - 1, y, tui_cell(']', t.fg, t.bg));
    
    /* Fill */
    for (int i = 1; i < width - 1; i++) {
        uint32_t ch = (i - 1 < filled) ? 0x2588 : 0x2591;  /* █ or ░ */
        tui_color_t fg = (i - 1 < filled) ? t.primary : t.secondary;
        tui_buffer_set(buf, x + i, y, tui_cell(ch, fg, t.bg));
    }
}

void tui_widget_progress_label(tui_buffer_t* buf, int x, int y, int width, float progress, const char* label, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    tui_widget_progress(buf, x, y, width, progress, &t);
    
    /* Center label */
    int len = (int)strlen(label);
    int label_x = x + (width - len) / 2;
    tui_buffer_text(buf, label_x, y, label, t.fg, t.bg);
}

/* ============================================================================
 * Button
 * ============================================================================ */

void tui_widget_button(tui_buffer_t* buf, int x, int y, int width, const char* label, bool focused, bool pressed, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    tui_color_t bg = pressed ? t.highlight : (focused ? t.primary : t.secondary);
    
    /* Background */
    for (int i = 0; i < width; i++) {
        tui_buffer_set(buf, x + i, y, tui_cell(' ', t.fg, bg));
    }
    
    /* Center label */
    int len = (int)strlen(label);
    int label_x = x + (width - len) / 2;
    tui_buffer_text(buf, label_x, y, label, t.fg, bg);
}

/* ============================================================================
 * Checkbox
 * ============================================================================ */

void tui_widget_checkbox(tui_buffer_t* buf, int x, int y, bool checked, const char* label, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    uint32_t box = checked ? 0x2611 : 0x2610;  /* ☑ or ☐ */
    tui_buffer_set(buf, x, y, tui_cell(box, t.primary, t.bg));
    tui_buffer_text(buf, x + 2, y, label, t.fg, t.bg);
}

/* ============================================================================
 * Radio
 * ============================================================================ */

void tui_widget_radio(tui_buffer_t* buf, int x, int y, bool selected, const char* label, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    uint32_t dot = selected ? 0x25C9 : 0x25CB;  /* ◉ or ○ */
    tui_buffer_set(buf, x, y, tui_cell(dot, t.primary, t.bg));
    tui_buffer_text(buf, x + 2, y, label, t.fg, t.bg);
}

/* ============================================================================
 * Toggle
 * ============================================================================ */

void tui_widget_toggle(tui_buffer_t* buf, int x, int y, bool on, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    tui_color_t bg = on ? t.success : t.secondary;
    
    /* Track (4 chars wide) */
    tui_buffer_set(buf, x, y, tui_cell('(', t.fg, t.bg));
    tui_buffer_set(buf, x + 1, y, tui_cell(on ? 0x25CF : ' ', t.fg, bg));   /* ● */
    tui_buffer_set(buf, x + 2, y, tui_cell(on ? ' ' : 0x25CF, t.fg, bg));
    tui_buffer_set(buf, x + 3, y, tui_cell(')', t.fg, t.bg));
}

/* ============================================================================
 * Slider
 * ============================================================================ */

void tui_widget_slider(tui_buffer_t* buf, int x, int y, int width, float value, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    value = tui_clamp_f(value, 0.0f, 1.0f);
    int pos = (int)(value * (float)(width - 1));
    
    for (int i = 0; i < width; i++) {
        uint32_t ch = (i == pos) ? 0x25CF : 0x2500;  /* ● or ─ */
        tui_color_t fg = (i <= pos) ? t.primary : t.secondary;
        tui_buffer_set(buf, x + i, y, tui_cell(ch, fg, t.bg));
    }
}

/* ============================================================================
 * Spinner
 * ============================================================================ */

void tui_widget_spinner(tui_buffer_t* buf, int x, int y, int frame, const tui_theme_t* theme) {
    static const uint32_t frames[] = {0x280B, 0x2819, 0x2839, 0x2838, 0x283C, 0x2834, 0x2826, 0x2827, 0x2807, 0x280F};
    /* ⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏ */
    tui_theme_t t = theme ? *theme : tui_theme_default();
    uint32_t ch = frames[frame % 10];
    tui_buffer_set(buf, x, y, tui_cell(ch, t.primary, t.bg));
}

/* ============================================================================
 * Divider
 * ============================================================================ */

void tui_widget_divider(tui_buffer_t* buf, int x, int y, int width, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    for (int i = 0; i < width; i++) {
        tui_buffer_set(buf, x + i, y, tui_cell(0x2500, t.border, t.bg));  /* ─ */
    }
}

/* ============================================================================
 * Panel
 * ============================================================================ */

void tui_widget_panel(tui_buffer_t* buf, int x, int y, int width, int height, const char* title, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    
    /* Fill background */
    tui_buffer_fill_color(buf, x, y, width, height, t.bg);
    
    /* Border */
    tui_buffer_box_round(buf, x, y, width, height, t.border, t.bg);
    
    /* Title */
    if (title && title[0]) {
        int title_x = x + 2;
        tui_buffer_set(buf, title_x - 1, y, tui_cell(' ', t.fg, t.bg));
        tui_buffer_text(buf, title_x, y, title, t.fg, t.bg);
        tui_buffer_set(buf, title_x + (int)strlen(title), y, tui_cell(' ', t.fg, t.bg));
    }
}

/* ============================================================================
 * Input
 * ============================================================================ */

void tui_widget_input(tui_buffer_t* buf, int x, int y, int width, const char* text, int cursor, bool focused, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    tui_color_t bg = focused ? TUI_COLOR(50, 50, 50) : t.bg;
    
    /* Background */
    for (int i = 0; i < width; i++) {
        tui_buffer_set(buf, x + i, y, tui_cell(' ', t.fg, bg));
    }
    
    /* Text and cursor positions are grapheme/terminal-column based even though
     * the public cursor remains a UTF-8 byte offset for compatibility. */
    const vstr input = vstr_from_cstr(text);
    size_t byte_cursor = 0u;
    int col = 0;
    int cursor_col = 0;
    bool cursor_visible = cursor == 0;
    const int inner_width = width > 2 ? width - 2 : 0;
    while (byte_cursor < input.len) {
        vstr cluster;
        if (salts_unicode_grapheme_next(input, &byte_cursor, &cluster) !=
            SALTS_UNICODE_OK) break;
        const int cluster_width = tui_grapheme_width(cluster.data, cluster.len);
        if (cluster_width < 0) break;
        if (cursor >= 0 && (size_t)cursor == byte_cursor) {
            cursor_col = col + cluster_width;
            cursor_visible = true;
        }
        if (cluster_width == 0) continue;
        if (col + cluster_width > inner_width) break;
        if (!tui_buffer_set_grapheme(buf, x + 1 + col, y,
                                     cluster.data, cluster.len,
                                     t.fg, bg, TUI_STYLE_NONE)) break;
        col += cluster_width;
    }
    
    /* Cursor */
    if (focused && cursor_visible && cursor >= 0 &&
        (size_t)cursor <= input.len &&
        cursor_col < inner_width) {
        tui_buffer_set(buf, x + 1 + cursor_col, y,
                       tui_cell(0x258F, t.primary, bg));  /* ▏ */
    }
    
    /* Border */
    tui_buffer_set(buf, x, y, tui_cell(0x2502, t.border, t.bg));  /* │ */
    tui_buffer_set(buf, x + width - 1, y, tui_cell(0x2502, t.border, t.bg));
}

/* ============================================================================
 * Badge
 * ============================================================================ */

void tui_widget_badge(tui_buffer_t* buf, int x, int y, const char* text, tui_color_t bg_color, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    int len = (int)strlen(text);
    
    tui_buffer_set(buf, x, y, tui_cell(' ', t.fg, bg_color));
    tui_buffer_text(buf, x + 1, y, text, TUI_WHITE, bg_color);
    tui_buffer_set(buf, x + len + 1, y, tui_cell(' ', t.fg, bg_color));
}

/* ============================================================================
 * Scrollbar
 * ============================================================================ */

void tui_widget_scrollbar_v(tui_buffer_t* buf, int x, int y, int height, float position, float visible_ratio, const tui_theme_t* theme) {
    tui_theme_t t = theme ? *theme : tui_theme_default();
    position = tui_clamp_f(position, 0.0f, 1.0f);
    visible_ratio = tui_clamp_f(visible_ratio, 0.0f, 1.0f);
    
    int thumb_height = height;
    if (visible_ratio < 1.0f) {
        thumb_height = (int)(height * visible_ratio);
        if (thumb_height < 1) thumb_height = 1;
    }
    int thumb_pos = (int)(position * (float)(height - thumb_height));
    
    for (int i = 0; i < height; i++) {
        bool is_thumb = (i >= thumb_pos && i < thumb_pos + thumb_height);
        uint32_t ch = is_thumb ? 0x2588 : 0x2591;  /* █ or ░ */
        tui_color_t fg = is_thumb ? t.primary : t.secondary;
        tui_buffer_set(buf, x, y + i, tui_cell(ch, fg, t.bg));
    }
}


/* ============================================================================
 * Input State
 * ============================================================================ */

void tui_input_init(tui_input_state_t* state) {
    state->text[0] = '\0';
    state->cursor = 0;
    state->len = 0;
}

void tui_input_set(tui_input_state_t* state, const char* text) {
    if (!state || !text) {
        if (state) tui_input_init(state);
        return;
    }
    const vstr input = vstr_from_cstr(text);
    size_t cursor = 0u;
    size_t accepted = 0u;
    while (cursor < input.len) {
        vstr cluster;
        if (salts_unicode_grapheme_next(input, &cursor, &cluster) !=
            SALTS_UNICODE_OK) {
            tui_input_init(state);
            return;
        }
        if (cursor > sizeof(state->text) - 1u) break;
        accepted = cursor;
    }
    memcpy(state->text, text, accepted);
    state->text[accepted] = '\0';
    state->len = (int)accepted;
    state->cursor = (int)accepted;
}

static size_t input_boundary_at_or_before(vstr input, size_t requested) {
    size_t cursor = 0u;
    size_t boundary = 0u;
    if (requested > input.len) requested = input.len;
    while (cursor < input.len) {
        vstr cluster;
        if (salts_unicode_grapheme_next(input, &cursor, &cluster) !=
            SALTS_UNICODE_OK) return boundary;
        if (cursor > requested) break;
        boundary = cursor;
    }
    return boundary;
}

static bool encode_input_scalar(uint32_t scalar, char bytes[4], size_t* size) {
    if (!size || scalar > 0x10ffffu ||
        (scalar >= 0xd800u && scalar <= 0xdfffu) ||
        scalar < 0x20u || (scalar >= 0x7fu && scalar <= 0x9fu)) return false;
    if (scalar < 0x80u) {
        bytes[0] = (char)scalar;
        *size = 1u;
    } else if (scalar < 0x800u) {
        bytes[0] = (char)(0xc0u | (scalar >> 6));
        bytes[1] = (char)(0x80u | (scalar & 0x3fu));
        *size = 2u;
    } else if (scalar < 0x10000u) {
        bytes[0] = (char)(0xe0u | (scalar >> 12));
        bytes[1] = (char)(0x80u | ((scalar >> 6) & 0x3fu));
        bytes[2] = (char)(0x80u | (scalar & 0x3fu));
        *size = 3u;
    } else {
        bytes[0] = (char)(0xf0u | (scalar >> 18));
        bytes[1] = (char)(0x80u | ((scalar >> 12) & 0x3fu));
        bytes[2] = (char)(0x80u | ((scalar >> 6) & 0x3fu));
        bytes[3] = (char)(0x80u | (scalar & 0x3fu));
        *size = 4u;
    }
    return true;
}

bool tui_input_handle(tui_input_state_t* state, const tui_event_t* event) {
    if (!state || !event ||
        (event->type != TUI_EVENT_KEY && event->type != TUI_EVENT_PASTE))
        return false;
    const bool pasted = event->type == TUI_EVENT_PASTE;
    size_t observed_len = 0u;
    while (observed_len < sizeof(state->text) &&
           state->text[observed_len] != '\0') ++observed_len;
    if (observed_len == sizeof(state->text)) {
        observed_len = sizeof(state->text) - 1u;
        state->text[observed_len] = '\0';
    }
    state->len = (int)observed_len;
    const vstr input = vstr_from_buf(state->text, (size_t)state->len);
    state->cursor = (int)input_boundary_at_or_before(
        input, state->cursor < 0 ? 0u : (size_t)state->cursor);
    
    if (!pasted && event->key == TUI_KEY_LEFT && state->cursor > 0) {
        size_t cursor = (size_t)state->cursor;
        vstr cluster;
        if (salts_unicode_grapheme_prev(input, &cursor, &cluster) ==
            SALTS_UNICODE_OK) state->cursor = (int)cursor;
        return true;
    }
    if (!pasted && event->key == TUI_KEY_RIGHT && state->cursor < state->len) {
        size_t cursor = (size_t)state->cursor;
        vstr cluster;
        if (salts_unicode_grapheme_next(input, &cursor, &cluster) ==
            SALTS_UNICODE_OK) state->cursor = (int)cursor;
        return true;
    }
    if (!pasted && event->key == TUI_KEY_HOME) {
        state->cursor = 0;
        return true;
    }
    if (!pasted && event->key == TUI_KEY_END) {
        state->cursor = state->len;
        return true;
    }
    if (!pasted && event->key == TUI_KEY_BACKSPACE && state->cursor > 0) {
        size_t start = (size_t)state->cursor;
        vstr cluster;
        if (salts_unicode_grapheme_prev(input, &start, &cluster) ==
            SALTS_UNICODE_OK) {
            memmove(state->text + start, state->text + state->cursor,
                    (size_t)(state->len - state->cursor) + 1u);
            state->len -= state->cursor - (int)start;
            state->cursor = (int)start;
        }
        return true;
    }
    if (!pasted && event->key == TUI_KEY_DELETE && state->cursor < state->len) {
        size_t end = (size_t)state->cursor;
        vstr cluster;
        if (salts_unicode_grapheme_next(input, &end, &cluster) ==
            SALTS_UNICODE_OK) {
            memmove(state->text + state->cursor, state->text + end,
                    (size_t)state->len - end + 1u);
            state->len -= (int)(end - (size_t)state->cursor);
        }
        return true;
    }
    char encoded[4];
    size_t encoded_size = 0u;
    if (encode_input_scalar(event->ch, encoded, &encoded_size) &&
        (size_t)state->len + encoded_size < sizeof(state->text)) {
        const size_t insertion_end = (size_t)state->cursor + encoded_size;
        memmove(state->text + insertion_end, state->text + state->cursor,
                (size_t)(state->len - state->cursor) + 1u);
        memcpy(state->text + state->cursor, encoded, encoded_size);
        state->len += (int)encoded_size;

        /* Insertion can merge adjacent scalars into one grapheme (combining
         * marks, ZWJ sequences).  Keep the cursor on the merged cluster end. */
        const vstr updated = vstr_from_buf(state->text, (size_t)state->len);
        size_t scan = 0u;
        while (scan < updated.len) {
            vstr cluster;
            if (salts_unicode_grapheme_next(updated, &scan, &cluster) !=
                SALTS_UNICODE_OK) return false;
            if (scan >= insertion_end) break;
        }
        state->cursor = (int)scan;
        return true;
    }
    return false;
}

/* ============================================================================
 * Drag State (Slider/Scrollbar)
 * ============================================================================ */

void tui_drag_init(tui_drag_state_t* state, float initial) {
    state->value = tui_clamp_f(initial, 0.0f, 1.0f);
    state->dragging = false;
}

bool tui_drag_handle_h(tui_drag_state_t* state, const tui_event_t* event, int x, int y, int width) {
    if (event->type == TUI_EVENT_MOUSE_PRESS && event->key == TUI_KEY_MOUSE_LEFT) {
        if (tui_rect_contains(x, y, width, 1, event->x, event->y)) {
            state->dragging = true;
            state->value = tui_clamp_f((float)(event->x - x) / (float)(width - 1), 0.0f, 1.0f);
            return true;
        }
    }
    if (event->type == TUI_EVENT_MOUSE_RELEASE) {
        state->dragging = false;
    }
    if (event->type == TUI_EVENT_MOUSE_MOVE && state->dragging) {
        state->value = tui_clamp_f((float)(event->x - x) / (float)(width - 1), 0.0f, 1.0f);
        return true;
    }
    return false;
}

bool tui_drag_handle_v(tui_drag_state_t* state, const tui_event_t* event, int x, int y, int height) {
    if (event->type == TUI_EVENT_MOUSE_PRESS && event->key == TUI_KEY_MOUSE_LEFT) {
        if (tui_rect_contains(x, y, 1, height, event->x, event->y)) {
            state->dragging = true;
            state->value = tui_clamp_f((float)(event->y - y) / (float)(height - 1), 0.0f, 1.0f);
            return true;
        }
    }
    if (event->type == TUI_EVENT_MOUSE_RELEASE) {
        state->dragging = false;
    }
    if (event->type == TUI_EVENT_MOUSE_MOVE && state->dragging) {
        state->value = tui_clamp_f((float)(event->y - y) / (float)(height - 1), 0.0f, 1.0f);
        return true;
    }
    return false;
}

/* ============================================================================
 * List State
 * ============================================================================ */

void tui_list_init(tui_list_state_t* state, int count, int visible) {
    state->selected = 0;
    state->count = count;
    state->scroll = 0;
    state->visible = visible;
}

void tui_list_ensure_visible(tui_list_state_t* state) {
    if (state->selected < state->scroll) {
        state->scroll = state->selected;
    }
    if (state->selected >= state->scroll + state->visible) {
        state->scroll = state->selected - state->visible + 1;
    }
}

bool tui_list_handle(tui_list_state_t* state, const tui_event_t* event, int x, int y, int width, int height) {
    if (event->type == TUI_EVENT_KEY) {
        if (event->key == TUI_KEY_UP && state->selected > 0) {
            state->selected--;
            tui_list_ensure_visible(state);
            return true;
        }
        if (event->key == TUI_KEY_DOWN && state->selected < state->count - 1) {
            state->selected++;
            tui_list_ensure_visible(state);
            return true;
        }
        if (event->key == TUI_KEY_PAGE_UP) {
            state->selected -= state->visible;
            if (state->selected < 0) state->selected = 0;
            tui_list_ensure_visible(state);
            return true;
        }
        if (event->key == TUI_KEY_PAGE_DOWN) {
            state->selected += state->visible;
            if (state->selected >= state->count) state->selected = state->count - 1;
            tui_list_ensure_visible(state);
            return true;
        }
        if (event->key == TUI_KEY_HOME) {
            state->selected = 0;
            tui_list_ensure_visible(state);
            return true;
        }
        if (event->key == TUI_KEY_END) {
            state->selected = state->count - 1;
            tui_list_ensure_visible(state);
            return true;
        }
    }
    if (event->type == TUI_EVENT_MOUSE_PRESS && event->key == TUI_KEY_MOUSE_LEFT) {
        if (tui_rect_contains(x, y, width, height, event->x, event->y)) {
            int clicked = state->scroll + (event->y - y);
            if (clicked >= 0 && clicked < state->count) {
                state->selected = clicked;
                return true;
            }
        }
    }
    if (event->type == TUI_EVENT_MOUSE_PRESS) {
        if (tui_rect_contains(x, y, width, height, event->x, event->y)) {
            if (event->key == TUI_KEY_MOUSE_WHEEL_UP && state->scroll > 0) {
                state->scroll--;
                return true;
            }
            if (event->key == TUI_KEY_MOUSE_WHEEL_DOWN && state->scroll < state->count - state->visible) {
                state->scroll++;
                return true;
            }
        }
    }
    return false;
}
