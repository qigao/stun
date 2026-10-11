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
#include <termios.h>
#endif

struct tui_terminal {
    bool initialized;
    int width;
    int height;
    tui_color_mode_t color_mode;

    tui_buffer_t *front;
    tui_buffer_t *back;

    char *output;
    size_t output_cap;
    size_t output_len;

    bool mouse_enabled;

#ifdef _WIN32
    HANDLE h_out;
    HANDLE h_in;
    DWORD orig_out_mode;
    DWORD orig_in_mode;
#else
    struct termios orig_termios;
#endif
};

#ifndef _WIN32
/* Consume a pending SIGWINCH notification on the terminal owner thread. */
bool tui_terminal_consume_resize_pending(void);
#endif

#endif /* TANGO_TUI_TERMINAL_INTERNAL_H */
