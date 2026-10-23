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
    DWORD orig_out_mode;
    DWORD orig_in_mode;
    UINT orig_output_cp;
    UINT orig_input_cp;
    bool out_mode_changed;
    bool in_mode_changed;
    bool output_cp_changed;
    bool input_cp_changed;
#else
    struct termios orig_termios;
    struct sigaction orig_sigwinch;
    bool terminal_mode_changed;
    bool sigwinch_installed;
#endif
};

#ifndef _WIN32
/* Consume a pending SIGWINCH notification on the terminal owner thread. */
bool tui_terminal_consume_resize_pending(void);
#endif

#endif /* TANGO_TUI_TERMINAL_INTERNAL_H */
