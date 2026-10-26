/*
 * Tango terminal private state shared by terminal and input implementations.
 * Not part of the public C API.
 */
#ifndef TANGO_TUI_TERMINAL_INTERNAL_H
#define TANGO_TUI_TERMINAL_INTERNAL_H

#include "tui.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <termios.h>
#define TUI_TERMINAL_PASTE_LIMIT (1024u * 1024u)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct tui_terminal;
typedef bool (*tui_terminal_platform_fn)(struct tui_terminal *);
typedef bool (*tui_terminal_write_fn)(struct tui_terminal *, const char *,
                                      size_t);

struct tui_terminal {
    bool initialized;
    bool platform_active;
    int width;
    int height;
    tui_color_mode_t color_mode;

    tui_buffer_t *front;
    tui_buffer_t *back;

    char *output;
    size_t output_cap;
    size_t output_len;
    size_t output_limit;
    void *(*output_realloc)(void *, size_t);
    tui_terminal_platform_fn platform_enter;
    tui_terminal_platform_fn platform_leave;
    tui_terminal_write_fn write_output;
    tui_terminal_error_t last_error;

    bool mouse_enabled;

#ifdef _WIN32
    HANDLE h_out;
    HANDLE h_in;
    /* Owned read/write handle to the actual active VT screen buffer.
     * GetStdHandle(STD_OUTPUT_HANDLE) may still reference the original
     * screen after ?1049h. Both dimensions and glyph output must target
     * the SAME buffer. Private state; no public ABI or second UI owner. */
    HANDLE h_active_out;
    DWORD orig_active_out_mode;
    DWORD orig_out_mode;
    DWORD orig_in_mode;
    /* Owner-local Win32 INPUT_RECORD decode state. Never shared across
     * independent terminals or producer threads. */
    uint16_t pending_utf16_high;
    DWORD last_mouse_button_state;
    UINT orig_output_cp;
    UINT orig_input_cp;
    bool active_out_mode_changed;
    bool out_mode_changed;
    bool in_mode_changed;
    bool output_cp_changed;
    bool input_cp_changed;
#else
    struct termios orig_termios;
    struct sigaction orig_sigwinch;
    bool terminal_mode_changed;
    bool sigwinch_installed;

    /* Bytes not yet decoded into an event.  This is intentionally a byte
     * queue: Unicode scalar/grapheme decoding belongs to Salts::Unicode. */
    unsigned char input_bytes[64];
    size_t input_len;
    size_t paste_bytes;
    bool paste_active;
    bool paste_discarding;
#endif

#if defined(STUN_TUI_PTY_RENDER_TRACE_CI) && !defined(_WIN32)
    /* Keep this test-only tail AFTER all shared private fields so regular
     * headless consumers see unchanged layout and offsets. */
    unsigned long long ci_render_epoch;
    unsigned ci_render_reports;
#endif
};

/* Private pre-frame resize and invalidation. UI Owner invokes this before
 * any view callback; terminal_render keeps it as an idempotent safeguard. */
bool tui_terminal_prepare_frame(tui_terminal_t *term);

#if defined(STUN_TUI_PTY_RENDER_TRACE_CI) && !defined(_WIN32)
/* Private conformance-only hook: the demo arms exactly its most recent
 * on_key epoch after the Box-owned back buffer is updated. The native C11
 * terminal reports write/commit outcome independently after render(). */
void tui_terminal_ci_arm_next_render(tui_terminal_t *term,
                                     unsigned long long epoch);
#endif

#ifdef _WIN32
/* Private deterministic translation of an actual Win32 input record. */
bool tui_terminal_input_record(tui_terminal_t* term,
                               const INPUT_RECORD* record,
                               tui_event_t* event);
#else
/* Consume a pending SIGWINCH notification on the terminal owner thread. */
bool tui_terminal_consume_resize_pending(void);

/* Private incremental-decoder seams used by the real-library tests. */
bool tui_terminal_input_push(tui_terminal_t *term, const void *data,
                             size_t size);
bool tui_terminal_input_next(tui_terminal_t *term, tui_event_t *event);
bool tui_terminal_input_flush(tui_terminal_t *term, tui_event_t *event);
/* Private POSIX select-result boundary, used by production poll and C11
 * signal-interruption conformance. Not a public Tango header or ABI. */
bool tui_terminal_input_after_wait(tui_terminal_t *term,
                                   tui_event_t *event,
                                   int select_result, int select_error);
#endif

#ifdef __cplusplus
}
#endif

#endif /* TANGO_TUI_TERMINAL_INTERNAL_H */
