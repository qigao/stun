/*
 * TUI Terminal Implementation
 */

#include "tui_terminal_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define STDOUT_FILENO 1
#else
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <signal.h>
#endif

/* ============================================================================
 * Platform-specific state
 * ============================================================================ */

#ifndef _WIN32
static volatile sig_atomic_t g_resize_pending = 0;
static void handle_sigwinch(int sig) {
    (void)sig;
    g_resize_pending = 1;
}

/* Keep the signal flag private to this translation unit. */
bool tui_terminal_consume_resize_pending(void) {
    if (!g_resize_pending) return false;
    g_resize_pending = 0;
    return true;
}
#endif


/* ============================================================================
 * Output buffer helpers
 * ============================================================================ */

static void output_clear(tui_terminal_t* term) {
    term->output_len = 0;
}

static bool output_ensure(tui_terminal_t* term, size_t need) {
    if (term->output_len > term->output_limit ||
        need > term->output_limit - term->output_len) {
        term->last_error = TUI_TERMINAL_ERROR_OUTPUT_LIMIT;
        return false;
    }

    const size_t required = term->output_len + need;
    if (required <= term->output_cap) return true;

    size_t new_cap = term->output_cap;
    while (new_cap < required) {
        if (new_cap > term->output_limit / 2u) {
            new_cap = term->output_limit;
        } else {
            new_cap *= 2u;
        }
    }

    char* new_buf = (char*)term->output_realloc(term->output, new_cap);
    if (!new_buf) {
        term->last_error = TUI_TERMINAL_ERROR_OUTPUT_ALLOCATION;
        return false;
    }

    term->output = new_buf;
    term->output_cap = new_cap;
    return true;
}

static bool output_append(tui_terminal_t* term, const char* str) {
    size_t len = strlen(str);
    if (!output_ensure(term, len)) return false;
    memcpy(term->output + term->output_len, str, len);
    term->output_len += len;
    return true;
}

/* UTF-8 encode */
static bool output_append_utf8(tui_terminal_t* term, uint32_t cp) {
    if (!output_ensure(term, 4u)) return false;
    char* p = term->output + term->output_len;
    
    if (cp < 0x80) {
        *p++ = (char)cp;
        term->output_len += 1;
    } else if (cp < 0x800) {
        *p++ = (char)(0xC0 | (cp >> 6));
        *p++ = (char)(0x80 | (cp & 0x3F));
        term->output_len += 2;
    } else if (cp < 0x10000) {
        *p++ = (char)(0xE0 | (cp >> 12));
        *p++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *p++ = (char)(0x80 | (cp & 0x3F));
        term->output_len += 3;
    } else {
        *p++ = (char)(0xF0 | (cp >> 18));
        *p++ = (char)(0x80 | ((cp >> 12) & 0x3F));
        *p++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *p++ = (char)(0x80 | (cp & 0x3F));
        term->output_len += 4;
    }
    return true;
}

/* ============================================================================
 * Raw I/O
 * ============================================================================ */

static bool write_raw(tui_terminal_t* term, const char* data, size_t len) {
#ifdef _WIN32
    size_t offset = 0u;
    while (offset < len) {
        DWORD written = 0u;
        const DWORD chunk = (DWORD)(len - offset);
        if (!WriteConsoleA(term->h_out, data + offset, chunk, &written, NULL) ||
            written == 0u) return false;
        offset += (size_t)written;
    }
#else
    (void)term;
    size_t offset = 0u;
    while (offset < len) {
        const ssize_t written = write(STDOUT_FILENO, data + offset, len - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return false;
        offset += (size_t)written;
    }
#endif
    return true;
}

static bool write_raw_str(tui_terminal_t* term, const char* str) {
    return term->write_output(term, str, strlen(str));
}

static bool flush_output(tui_terminal_t* term) {
    if (fflush(stdout) != 0) {
        term->last_error = TUI_TERMINAL_ERROR_OUTPUT_IO;
        return false;
    }
    if (term->output_len > 0u &&
        !term->write_output(term, term->output, term->output_len)) {
        term->last_error = TUI_TERMINAL_ERROR_OUTPUT_IO;
        return false;
    }
    term->output_len = 0u;
    return true;
}

/* ============================================================================
 * Color quantization (for 256-color mode)
 * ============================================================================ */

static int quantize_256(tui_color_t c) {
    int q_r = c.r, q_g = c.g, q_b = c.b;
    
    /* Snap to 6x6x6 cube */
    int snap_r = (q_r < 48) ? 0 : (q_r < 115) ? 95 : (q_r < 155) ? 135 : (q_r < 195) ? 175 : (q_r < 235) ? 215 : 255;
    int snap_g = (q_g < 48) ? 0 : (q_g < 115) ? 95 : (q_g < 155) ? 135 : (q_g < 195) ? 175 : (q_g < 235) ? 215 : 255;
    int snap_b = (q_b < 48) ? 0 : (q_b < 115) ? 95 : (q_b < 155) ? 135 : (q_b < 195) ? 175 : (q_b < 235) ? 215 : 255;
    
    int map_val_r = (snap_r == 0) ? 0 : (snap_r - 55) / 40;
    int map_val_g = (snap_g == 0) ? 0 : (snap_g - 55) / 40;
    int map_val_b = (snap_b == 0) ? 0 : (snap_b - 55) / 40;
    
    return 16 + 36 * map_val_r + 6 * map_val_g + map_val_b;
}

/* ============================================================================
 * Terminal Lifecycle
 * ============================================================================ */

#ifdef _WIN32
static bool platform_leave(tui_terminal_t* term) {
    bool ok = true;
    if (term->in_mode_changed) {
        if (SetConsoleMode(term->h_in, term->orig_in_mode)) {
            term->in_mode_changed = false;
        } else {
            ok = false;
        }
    }
    if (term->out_mode_changed) {
        if (SetConsoleMode(term->h_out, term->orig_out_mode)) {
            term->out_mode_changed = false;
        } else {
            ok = false;
        }
    }
    if (term->input_cp_changed) {
        if (SetConsoleCP(term->orig_input_cp)) {
            term->input_cp_changed = false;
        } else {
            ok = false;
        }
    }
    if (term->output_cp_changed) {
        if (SetConsoleOutputCP(term->orig_output_cp)) {
            term->output_cp_changed = false;
        } else {
            ok = false;
        }
    }
    return ok;
}

static bool platform_enter(tui_terminal_t* term) {
    term->h_out = GetStdHandle(STD_OUTPUT_HANDLE);
    term->h_in = GetStdHandle(STD_INPUT_HANDLE);
    if (!term->h_out || term->h_out == INVALID_HANDLE_VALUE ||
        !term->h_in || term->h_in == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(term->h_out, &term->orig_out_mode) ||
        !GetConsoleMode(term->h_in, &term->orig_in_mode)) return false;

    term->orig_output_cp = GetConsoleOutputCP();
    term->orig_input_cp = GetConsoleCP();
    if (!term->orig_output_cp || !term->orig_input_cp) return false;

    if (term->orig_output_cp != CP_UTF8) {
        if (!SetConsoleOutputCP(CP_UTF8)) return false;
        term->output_cp_changed = true;
    }
    if (term->orig_input_cp != CP_UTF8) {
        if (!SetConsoleCP(CP_UTF8)) goto failed;
        term->input_cp_changed = true;
    }

    const DWORD out_mode = term->orig_out_mode |
        ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    const DWORD in_mode = (term->orig_in_mode | ENABLE_MOUSE_INPUT |
        ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE;
    if (!SetConsoleMode(term->h_out, out_mode)) goto failed;
    term->out_mode_changed = true;
    if (!SetConsoleMode(term->h_in, in_mode)) goto failed;
    term->in_mode_changed = true;
    return true;

failed:
    if (!platform_leave(term)) term->platform_active = true;
    return false;
}
#else
static bool platform_leave(tui_terminal_t* term) {
    bool ok = true;
    if (term->sigwinch_installed) {
        if (sigaction(SIGWINCH, &term->orig_sigwinch, NULL) == 0) {
            term->sigwinch_installed = false;
        } else {
            ok = false;
        }
    }
    if (term->terminal_mode_changed) {
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term->orig_termios) == 0) {
            term->terminal_mode_changed = false;
        } else {
            ok = false;
        }
    }
    return ok;
}

static bool platform_enter(tui_terminal_t* term) {
    if (tcgetattr(STDIN_FILENO, &term->orig_termios) != 0) return false;

    struct termios raw = term->orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return false;
    term->terminal_mode_changed = true;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGWINCH, &sa, &term->orig_sigwinch) != 0) {
        if (!platform_leave(term)) term->platform_active = true;
        return false;
    }
    term->sigwinch_installed = true;
    g_resize_pending = 0;
    return true;
}
#endif

tui_terminal_t* tui_terminal_create(void) {
    tui_terminal_t* term = (tui_terminal_t*)calloc(1, sizeof(tui_terminal_t));
    if (!term) return NULL;
    
    term->width = 80;
    term->height = 24;
    term->output_cap = 8192;
    term->output_limit = 64u * 1024u * 1024u;
    term->output_realloc = realloc;
    term->platform_enter = platform_enter;
    term->platform_leave = platform_leave;
    term->write_output = write_raw;
    term->output = (char*)malloc(term->output_cap);
    
    if (!term->output) {
        free(term);
        return NULL;
    }
    
    return term;
}

void tui_terminal_destroy(tui_terminal_t* term) {
    if (!term) return;
    (void)tui_terminal_cleanup(term);
    tui_buffer_destroy(term->front);
    tui_buffer_destroy(term->back);
    free(term->output);
    free(term);
}

bool tui_terminal_init(tui_terminal_t* term) {
    if (!term || term->initialized) return term && term->initialized;
    term->last_error = TUI_TERMINAL_ERROR_NONE;
    
    /* Auto-detect color mode if not set */
    if (term->color_mode == TUI_COLOR_AUTO) {
        const char* ct = getenv("COLORTERM");
        if (ct && (strcmp(ct, "truecolor") == 0 || strcmp(ct, "24bit") == 0)) {
            term->color_mode = TUI_COLOR_TRUECOLOR;
        } else {
            term->color_mode = TUI_COLOR_256;
        }
    }
    
    if (!term->platform_enter(term)) {
        term->last_error = TUI_TERMINAL_ERROR_PLATFORM;
        return false;
    }
    term->platform_active = true;
    
    tui_terminal_query_size(term);
    
    tui_buffer_t* front = tui_buffer_create(term->width, term->height);
    tui_buffer_t* back = tui_buffer_create(term->width, term->height);
    
    if (!front || !back) {
        tui_buffer_destroy(front);
        tui_buffer_destroy(back);
        if (term->platform_leave(term)) {
            term->platform_active = false;
            term->last_error = TUI_TERMINAL_ERROR_OUTPUT_ALLOCATION;
        } else {
            term->last_error = TUI_TERMINAL_ERROR_PLATFORM;
        }
        return false;
    }
    
    /* Mark front buffer as "invalid" to force first frame full render */
    {
        tui_cell_t invalid = {0xFFFFFFFF, {255,255,255}, {255,255,255}, 0xFF};
        size_t count = (size_t)front->width * (size_t)front->height;
        for (size_t i = 0; i < count; i++) {
            front->cells[i] = invalid;
        }
    }
    
    /* Enter alternate screen, hide cursor */
    if (fflush(stdout) != 0 ||
        !write_raw_str(term, "\033[?1049h\033[?25l\033[2J")) {
        (void)write_raw_str(term, "\033[?25h\033[?1049l\033[0m");
        const bool restored = term->platform_leave(term);
        if (restored) term->platform_active = false;
        tui_buffer_destroy(front);
        tui_buffer_destroy(back);
        term->last_error = restored ? TUI_TERMINAL_ERROR_OUTPUT_IO :
            TUI_TERMINAL_ERROR_PLATFORM;
        return false;
    }

    tui_buffer_destroy(term->front);
    tui_buffer_destroy(term->back);
    term->front = front;
    term->back = back;
    
    term->initialized = true;
    return true;
}

bool tui_terminal_cleanup(tui_terminal_t* term) {
    if (!term) return false;
    if (!term->initialized && !term->platform_active) return true;
    bool ok = true;
    
    if (!tui_terminal_disable_mouse(term)) ok = false;
    
    if (fflush(stdout) != 0 ||
        !write_raw_str(term, "\033[?25h\033[?1049l\033[0m")) {
        term->last_error = TUI_TERMINAL_ERROR_OUTPUT_IO;
        ok = false;
    }
    if (!term->platform_leave(term)) {
        term->last_error = TUI_TERMINAL_ERROR_PLATFORM;
        ok = false;
    } else {
        term->platform_active = false;
    }

    if (ok) {
        term->initialized = false;
        term->last_error = TUI_TERMINAL_ERROR_NONE;
    }
    return ok;
}

/* ============================================================================
 * Size
 * ============================================================================ */

int tui_terminal_width(const tui_terminal_t* term) { return term->width; }
int tui_terminal_height(const tui_terminal_t* term) { return term->height; }

void tui_terminal_set_color_mode(tui_terminal_t* term, tui_color_mode_t mode) {
    if (term) term->color_mode = mode;
}

tui_color_mode_t tui_terminal_get_color_mode(const tui_terminal_t* term) {
    return term ? term->color_mode : TUI_COLOR_AUTO;
}

void tui_terminal_query_size(tui_terminal_t* term) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(term->h_out, &csbi)) {
        term->width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        term->height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        term->width = ws.ws_col;
        term->height = ws.ws_row;
    }
#endif
    if (term->width <= 0) term->width = 80;
    if (term->height <= 0) term->height = 24;
}

/* ============================================================================
 * Buffer Access
 * ============================================================================ */

tui_buffer_t* tui_terminal_buffer(tui_terminal_t* term) {
    return term->back;
}

/* ============================================================================
 * Rendering
 * ============================================================================ */

bool tui_terminal_render(tui_terminal_t* term) {
    if (!term || !term->initialized || !term->front || !term->back) {
        if (term) term->last_error = TUI_TERMINAL_ERROR_INVALID_STATE;
        return false;
    }
    term->last_error = TUI_TERMINAL_ERROR_NONE;
    
    tui_buffer_t* back = term->back;
    tui_buffer_t* front = term->front;
    
    /* Resize if needed */
    if (back->width != term->width || back->height != term->height) {
        tui_buffer_resize(back, term->width, term->height);
        tui_buffer_resize(front, term->width, term->height);
        if (back->width != term->width || back->height != term->height ||
            front->width != term->width || front->height != term->height) {
            term->last_error = TUI_TERMINAL_ERROR_OUTPUT_ALLOCATION;
            return false;
        }
        /* Force full redraw */
        tui_buffer_clear(front);
        tui_buffer_mark_all_dirty(back);
    }
    
    if (!tui_buffer_is_dirty(back)) return true;
    
    output_clear(term);
    if (!output_append(term, "\033[0m")) goto output_failed;
    
    tui_color_t last_fg = TUI_WHITE;
    tui_color_t last_bg = TUI_BLACK;
    uint8_t last_style = TUI_STYLE_NONE;
    int last_x = -1000, last_y = -1000;
    
    int min_x = back->dirty_min_x;
    int min_y = back->dirty_min_y;
    int max_x = back->dirty_max_x;
    int max_y = back->dirty_max_y;
    
    /* Clamp */
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= term->width) max_x = term->width - 1;
    if (max_y >= term->height) max_y = term->height - 1;
    
    char buf[64];
    
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            const tui_cell_t* cell = tui_buffer_at_const(back, x, y);
            const tui_cell_t* prev = tui_buffer_at_const(front, x, y);
            
            if (tui_cell_eq(cell, prev)) continue;
            
            /* Move cursor if not sequential */
            if (last_x != x - 1 || last_y != y) {
                snprintf(buf, sizeof(buf), "\033[%d;%dH", y + 1, x + 1);
                if (!output_append(term, buf)) goto output_failed;
            }
            
            /* Update style */
            if (cell->style != last_style) {
                if (!output_append(term, "\033[0m")) goto output_failed;
                last_fg = TUI_WHITE;
                last_bg = TUI_BLACK;
                
                if ((cell->style & TUI_STYLE_BOLD) && !output_append(term, "\033[1m")) goto output_failed;
                if ((cell->style & TUI_STYLE_DIM) && !output_append(term, "\033[2m")) goto output_failed;
                if ((cell->style & TUI_STYLE_ITALIC) && !output_append(term, "\033[3m")) goto output_failed;
                if ((cell->style & TUI_STYLE_UNDERLINE) && !output_append(term, "\033[4m")) goto output_failed;
                if ((cell->style & TUI_STYLE_BLINK) && !output_append(term, "\033[5m")) goto output_failed;
                if ((cell->style & TUI_STYLE_REVERSE) && !output_append(term, "\033[7m")) goto output_failed;
                if ((cell->style & TUI_STYLE_STRIKE) && !output_append(term, "\033[9m")) goto output_failed;
                
                last_style = cell->style;
            }
            
            /* Update colors */
            if (!tui_color_eq(cell->fg, last_fg)) {
                if (term->color_mode == TUI_COLOR_TRUECOLOR) {
                    snprintf(buf, sizeof(buf), "\033[38;2;%d;%d;%dm", cell->fg.r, cell->fg.g, cell->fg.b);
                } else {
                    snprintf(buf, sizeof(buf), "\033[38;5;%dm", quantize_256(cell->fg));
                }
                if (!output_append(term, buf)) goto output_failed;
                last_fg = cell->fg;
            }
            
            if (!tui_color_eq(cell->bg, last_bg)) {
                if (term->color_mode == TUI_COLOR_TRUECOLOR) {
                    snprintf(buf, sizeof(buf), "\033[48;2;%d;%d;%dm", cell->bg.r, cell->bg.g, cell->bg.b);
                } else {
                    snprintf(buf, sizeof(buf), "\033[48;5;%dm", quantize_256(cell->bg));
                }
                if (!output_append(term, buf)) goto output_failed;
                last_bg = cell->bg;
            }
            
            /* Output character */
            if (cell->ch != 0) {
                if (!output_append_utf8(term, cell->ch)) goto output_failed;
            }
            
            last_x = x;
            last_y = y;
            
        }
    }

    if (!flush_output(term)) goto output_failed;

    /* Commit the rendered rectangle only after output construction and write
       both succeed. Failure leaves the previous front and dirty back intact. */
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            *tui_buffer_at(front, x, y) = *tui_buffer_at_const(back, x, y);
        }
    }
    tui_buffer_reset_dirty(back);
    return true;

output_failed:
    output_clear(term);
    return false;
}

tui_terminal_error_t tui_terminal_last_error(const tui_terminal_t* term) {
    return term ? term->last_error : TUI_TERMINAL_ERROR_INVALID_STATE;
}

void tui_terminal_refresh(tui_terminal_t* term) {
    if (!term || !term->initialized) return;
    /* Mark front buffer as invalid to force full redraw on next render */
    tui_cell_t invalid = {0xFFFFFFFF, {255,255,255}, {255,255,255}, 0xFF};
    size_t count = (size_t)term->front->width * (size_t)term->front->height;
    for (size_t i = 0; i < count; i++) {
        term->front->cells[i] = invalid;
    }
    tui_buffer_mark_all_dirty(term->back);
    /* Don't render here - let the main loop call render after view() updates back buffer */
}

void tui_terminal_clear(tui_terminal_t* term) {
    if (term && term->back) tui_buffer_clear(term->back);
}

void tui_terminal_clear_color(tui_terminal_t* term, tui_color_t bg) {
    if (term && term->back) tui_buffer_clear_color(term->back, bg);
}

/* ============================================================================
 * Cursor
 * ============================================================================ */

void tui_terminal_show_cursor(tui_terminal_t* term) {
    write_raw_str(term, "\033[?25h");
    fflush(stdout);
}

void tui_terminal_hide_cursor(tui_terminal_t* term) {
    write_raw_str(term, "\033[?25l");
    fflush(stdout);
}

void tui_terminal_move_cursor(tui_terminal_t* term, int x, int y) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\033[%d;%dH", y + 1, x + 1);
    write_raw_str(term, buf);
    fflush(stdout);
}
