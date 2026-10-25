/*
 * TUI Input Implementation
 */

#include "tui_terminal_internal.h"
#include <salts_unicode.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>
#endif

/* Forward declaration of terminal struct */


/* ============================================================================
 * Mouse Control
 * ============================================================================ */

bool tui_terminal_enable_mouse(tui_terminal_t* term) {
    if (!term) return false;
    if (term->mouse_enabled) return true;
#ifndef _WIN32
    if (!term->write_output(term, "\033[?1006h\033[?1003h", 16u)) {
        term->last_error = TUI_TERMINAL_ERROR_OUTPUT_IO;
        return false;
    }
#endif
    term->mouse_enabled = true;
    return true;
}

bool tui_terminal_disable_mouse(tui_terminal_t* term) {
    if (!term) return false;
    if (!term->mouse_enabled) return true;
#ifndef _WIN32
    if (!term->write_output(term, "\033[?1003l\033[?1006l", 16u)) {
        term->last_error = TUI_TERMINAL_ERROR_OUTPUT_IO;
        return false;
    }
#endif
    term->mouse_enabled = false;
    return true;
}

/* ============================================================================
 * Input Polling
 * ============================================================================ */

#ifdef _WIN32

/* Private deterministic decoder shared by the actual ReadConsoleInputW
 * path and a C11 conformance test. The lifetime of a pending UTF-16 lead
 * surrogate belongs to this terminal, never to a process-global static.
 * Surrogate KEYUP records are intentionally ignored without clearing it:
 * Windows emoji/Unicode injection may interleave down/up for each unit.
 */
bool tui_terminal_input_record(tui_terminal_t* term,
                               const INPUT_RECORD* record,
                               tui_event_t* event) {
    if (!term || !record || !event) return false;
    memset(event, 0, sizeof(*event));

    if (record->EventType == KEY_EVENT) {
        const KEY_EVENT_RECORD* key = &record->Event.KeyEvent;
        if (!key->bKeyDown) return false;
        const uint32_t unit = (uint32_t)key->uChar.UnicodeChar;
        if (unit >= 0xd800u && unit <= 0xdbffu) {
            term->pending_utf16_high = (uint16_t)unit;
            return false;
        }
        if (unit >= 0xdc00u && unit <= 0xdfffu) {
            const uint32_t high = term->pending_utf16_high;
            term->pending_utf16_high = 0u;
            if (high < 0xd800u || high > 0xdbffu) return false;
            event->ch = 0x10000u +
                ((high - 0xd800u) << 10) + (unit - 0xdc00u);
        } else {
            term->pending_utf16_high = 0u;
            event->ch = unit;
        }
        if (event->ch == 0u && key->wVirtualKeyCode == 0u) return false;
        event->type = TUI_EVENT_KEY;
        const DWORD state = key->dwControlKeyState;
        if (state & SHIFT_PRESSED) event->mod |= TUI_MOD_SHIFT;
        if (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) event->mod |= TUI_MOD_CTRL;
        if (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) event->mod |= TUI_MOD_ALT;
        
        switch (key->wVirtualKeyCode) {
            case VK_UP: event->key = TUI_KEY_UP; break;
            case VK_DOWN: event->key = TUI_KEY_DOWN; break;
            case VK_LEFT: event->key = TUI_KEY_LEFT; break;
            case VK_RIGHT: event->key = TUI_KEY_RIGHT; break;
            case VK_HOME: event->key = TUI_KEY_HOME; break;
            case VK_END: event->key = TUI_KEY_END; break;
            case VK_PRIOR: event->key = TUI_KEY_PAGE_UP; break;
            case VK_NEXT: event->key = TUI_KEY_PAGE_DOWN; break;
            case VK_INSERT: event->key = TUI_KEY_INSERT; break;
            case VK_DELETE: event->key = TUI_KEY_DELETE; break;
            case VK_F1: event->key = TUI_KEY_F1; break;
            case VK_F2: event->key = TUI_KEY_F2; break;
            case VK_F3: event->key = TUI_KEY_F3; break;
            case VK_F4: event->key = TUI_KEY_F4; break;
            case VK_F5: event->key = TUI_KEY_F5; break;
            case VK_F6: event->key = TUI_KEY_F6; break;
            case VK_F7: event->key = TUI_KEY_F7; break;
            case VK_F8: event->key = TUI_KEY_F8; break;
            case VK_F9: event->key = TUI_KEY_F9; break;
            case VK_F10: event->key = TUI_KEY_F10; break;
            case VK_F11: event->key = TUI_KEY_F11; break;
            case VK_F12: event->key = TUI_KEY_F12; break;
            case VK_RETURN: event->key = TUI_KEY_ENTER; event->ch = '\n'; break;
            case VK_TAB: event->key = TUI_KEY_TAB; event->ch = '\t'; break;
            case VK_BACK: event->key = TUI_KEY_BACKSPACE; break;
            case VK_ESCAPE: event->key = TUI_KEY_ESCAPE; break;
            default:
                if (event->ch >= 32 && event->ch < 127) {
                    event->key = (tui_key_t)event->ch;
                }
                break;
        }
        return true;
    }
    
    if (record->EventType == MOUSE_EVENT && term->mouse_enabled) {
        const MOUSE_EVENT_RECORD* me = &record->Event.MouseEvent;
        event->x = me->dwMousePosition.X;
        event->y = me->dwMousePosition.Y;
        
        /* Track button state for press/release detection */
        DWORD* last_button_state = &term->last_mouse_button_state;
        
        if (me->dwEventFlags == 0) {
            /* Button state changed */
            DWORD pressed = me->dwButtonState & ~(*last_button_state);
            DWORD released = (*last_button_state) & ~me->dwButtonState;
            *last_button_state = me->dwButtonState;
            
            if (pressed & FROM_LEFT_1ST_BUTTON_PRESSED) {
                event->type = TUI_EVENT_MOUSE_PRESS;
                event->key = TUI_KEY_MOUSE_LEFT;
                return true;
            } else if (pressed & RIGHTMOST_BUTTON_PRESSED) {
                event->type = TUI_EVENT_MOUSE_PRESS;
                event->key = TUI_KEY_MOUSE_RIGHT;
                return true;
            } else if (released & FROM_LEFT_1ST_BUTTON_PRESSED) {
                event->type = TUI_EVENT_MOUSE_RELEASE;
                event->key = TUI_KEY_MOUSE_LEFT;
                return true;
            } else if (released & RIGHTMOST_BUTTON_PRESSED) {
                event->type = TUI_EVENT_MOUSE_RELEASE;
                event->key = TUI_KEY_MOUSE_RIGHT;
                return true;
            }
            /* No actual button change, skip this event */
            return false;
        } else if (me->dwEventFlags == MOUSE_MOVED) {
            event->type = TUI_EVENT_MOUSE_MOVE;
            return true;
        } else if (me->dwEventFlags == MOUSE_WHEELED) {
            event->type = TUI_EVENT_MOUSE_PRESS;
            event->key = ((short)HIWORD(me->dwButtonState) > 0) ? TUI_KEY_MOUSE_WHEEL_UP : TUI_KEY_MOUSE_WHEEL_DOWN;
            return true;
        }
    }
    
    if (record->EventType == WINDOW_BUFFER_SIZE_EVENT) {
        event->type = TUI_EVENT_RESIZE;
        return true;
    }
    
    return false;
}


bool tui_terminal_poll(tui_terminal_t* term, tui_event_t* event) {
    if (!term || !event) return false;
    DWORD num_events = 0u;
    if (!GetNumberOfConsoleInputEvents(term->h_in, &num_events) ||
        num_events == 0u) return false;

    INPUT_RECORD record;
    DWORD read_count = 0u;
    if (!ReadConsoleInputW(term->h_in, &record, 1u, &read_count) ||
        read_count != 1u) return false;
    return tui_terminal_input_record(term, &record, event);
}

#else /* Unix */

typedef enum {
    TUI_INPUT_INCOMPLETE,
    TUI_INPUT_IGNORED,
    TUI_INPUT_EVENT
} tui_input_result_t;

static void input_consume(tui_terminal_t *term, size_t size) {
    if (size < term->input_len) {
        memmove(term->input_bytes, term->input_bytes + size,
                term->input_len - size);
    }
    term->input_len -= size;
}

static void input_key(unsigned char ch, tui_event_t *event) {
    event->type = TUI_EVENT_KEY;
    event->ch = ch;
    if (ch < 32u) {
        if (ch == 27u) {
            event->key = TUI_KEY_ESCAPE;
        } else if (ch == 13u || ch == 10u) {
            event->key = TUI_KEY_ENTER;
            event->ch = '\n';
        } else if (ch == 9u) {
            event->key = TUI_KEY_TAB;
            event->ch = '\t';
        } else {
            event->mod = TUI_MOD_CTRL;
            event->ch = (uint32_t)ch + (uint32_t)'a' - 1u;
        }
    } else if (ch == 127u) {
        event->key = TUI_KEY_BACKSPACE;
    } else {
        event->key = (tui_key_t)ch;
    }
}

static bool parse_decimal(const unsigned char *bytes, size_t begin,
                          size_t end, int *value) {
    int parsed = 0;
    if (begin == end) return false;
    for (size_t i = begin; i < end; ++i) {
        const unsigned char ch = bytes[i];
        if (ch < '0' || ch > '9') return false;
        const int digit = ch - '0';
        if (parsed > (INT_MAX - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    *value = parsed;
    return true;
}

static bool parse_sgr_mouse(tui_terminal_t *term, size_t size,
                            tui_event_t *event) {
    const unsigned char *bytes = term->input_bytes;
    size_t first = 3u;
    size_t second = first;
    size_t third;
    int btn;
    int x;
    int y;

    while (second < size && bytes[second] != ';') ++second;
    if (second == size) return false;
    third = second + 1u;
    while (third < size && bytes[third] != ';') ++third;
    if (third == size || size < third + 2u) return false;
    if (!parse_decimal(bytes, first, second, &btn) ||
        !parse_decimal(bytes, second + 1u, third, &x) ||
        !parse_decimal(bytes, third + 1u, size - 1u, &y) ||
        x <= 0 || y <= 0) {
        return false;
    }

    event->x = x - 1;
    event->y = y - 1;
    event->type = bytes[size - 1u] == 'M' ? TUI_EVENT_MOUSE_PRESS :
                                           TUI_EVENT_MOUSE_RELEASE;
    if (btn & 64) {
        event->key = (btn & 1) ? TUI_KEY_MOUSE_WHEEL_DOWN :
                                 TUI_KEY_MOUSE_WHEEL_UP;
    } else if (btn & 32) {
        event->type = TUI_EVENT_MOUSE_MOVE;
    } else {
        switch (btn & 0x03) {
            case 0: event->key = TUI_KEY_MOUSE_LEFT; break;
            case 1: event->key = TUI_KEY_MOUSE_MIDDLE; break;
            case 2: event->key = TUI_KEY_MOUSE_RIGHT; break;
            default: event->key = TUI_KEY_MOUSE_RELEASE; break;
        }
    }
    return true;
}

static tui_input_result_t decode_csi(tui_terminal_t *term,
                                     tui_event_t *event) {
    const unsigned char *bytes = term->input_bytes;
    size_t size = 2u;

    while (size < term->input_len &&
           !(bytes[size] >= 0x40u && bytes[size] <= 0x7eu)) {
        ++size;
    }
    if (size == term->input_len) {
        if (term->input_len == sizeof(term->input_bytes)) {
            input_consume(term, term->input_len);
            return TUI_INPUT_IGNORED;
        }
        return TUI_INPUT_INCOMPLETE;
    }
    ++size; /* Include the final byte. */

    if (size == 3u) {
        switch (bytes[2]) {
            case 'A': event->key = TUI_KEY_UP; break;
            case 'B': event->key = TUI_KEY_DOWN; break;
            case 'C': event->key = TUI_KEY_RIGHT; break;
            case 'D': event->key = TUI_KEY_LEFT; break;
            case 'H': event->key = TUI_KEY_HOME; break;
            case 'F': event->key = TUI_KEY_END; break;
            default: input_consume(term, size); return TUI_INPUT_IGNORED;
        }
        event->type = TUI_EVENT_KEY;
        input_consume(term, size);
        return TUI_INPUT_EVENT;
    }

    if (bytes[2] == '<' &&
        (bytes[size - 1u] == 'M' || bytes[size - 1u] == 'm')) {
        const bool decoded = term->mouse_enabled &&
                             parse_sgr_mouse(term, size, event);
        input_consume(term, size);
        return decoded ? TUI_INPUT_EVENT : TUI_INPUT_IGNORED;
    }

    if (bytes[size - 1u] == '~') {
        int number;
        if (parse_decimal(bytes, 2u, size - 1u, &number)) {
            if (number == 200) {
                term->paste_active = true;
                term->paste_discarding = false;
                term->paste_bytes = 0u;
                input_consume(term, size);
                return TUI_INPUT_IGNORED;
            }
            switch (number) {
                case 1: event->key = TUI_KEY_HOME; break;
                case 2: event->key = TUI_KEY_INSERT; break;
                case 3: event->key = TUI_KEY_DELETE; break;
                case 4: event->key = TUI_KEY_END; break;
                case 5: event->key = TUI_KEY_PAGE_UP; break;
                case 6: event->key = TUI_KEY_PAGE_DOWN; break;
                case 15: event->key = TUI_KEY_F5; break;
                case 17: event->key = TUI_KEY_F6; break;
                case 18: event->key = TUI_KEY_F7; break;
                case 19: event->key = TUI_KEY_F8; break;
                case 20: event->key = TUI_KEY_F9; break;
                case 21: event->key = TUI_KEY_F10; break;
                case 23: event->key = TUI_KEY_F11; break;
                case 24: event->key = TUI_KEY_F12; break;
                default: input_consume(term, size); return TUI_INPUT_IGNORED;
            }
            event->type = TUI_EVENT_KEY;
            input_consume(term, size);
            return TUI_INPUT_EVENT;
        }
    }

    input_consume(term, size);
    return TUI_INPUT_IGNORED;
}

static bool paste_accept(tui_terminal_t *term, size_t size) {
    if (term->paste_discarding) return false;
    if (term->paste_bytes > TUI_TERMINAL_PASTE_LIMIT ||
        size > TUI_TERMINAL_PASTE_LIMIT - term->paste_bytes) {
        term->paste_discarding = true;
        return false;
    }
    term->paste_bytes += size;
    return true;
}

static tui_input_result_t decode_paste(tui_terminal_t *term,
                                       tui_event_t *event) {
    static const unsigned char end_marker[] = {27u, '[', '2', '0', '1', '~'};
    const unsigned char *bytes = term->input_bytes;
    if (term->input_len == 0u) return TUI_INPUT_INCOMPLETE;

    if (bytes[0] == 27u) {
        const size_t compared = term->input_len < sizeof(end_marker) ?
            term->input_len : sizeof(end_marker);
        if (memcmp(bytes, end_marker, compared) == 0) {
            if (term->input_len < sizeof(end_marker))
                return TUI_INPUT_INCOMPLETE;
            input_consume(term, sizeof(end_marker));
            term->paste_active = false;
            term->paste_discarding = false;
            term->paste_bytes = 0u;
            return TUI_INPUT_IGNORED;
        }

        /* Escape/control payload is data from an untrusted paste, never a
         * keyboard command.  Drop ESC and preserve later printable text. */
        input_consume(term, 1u);
        (void)paste_accept(term, 1u);
        return TUI_INPUT_IGNORED;
    }

    if (term->paste_discarding) {
        input_consume(term, 1u);
        return TUI_INPUT_IGNORED;
    }

    if (bytes[0] < 0x80u) {
        size_t size = 1u;
        uint32_t scalar = bytes[0];
        if (scalar == '\r') {
            if (term->input_len == 1u) return TUI_INPUT_INCOMPLETE;
            if (bytes[1] == '\n') size = 2u;
            scalar = '\n';
        }
        input_consume(term, size);
        if (!paste_accept(term, size)) return TUI_INPUT_IGNORED;
        if ((scalar < 0x20u && scalar != '\n') || scalar == 0x7fu)
            return TUI_INPUT_IGNORED;
        event->type = TUI_EVENT_PASTE;
        event->ch = scalar;
        return TUI_INPUT_EVENT;
    }

    const size_t max = term->input_len < 4u ? term->input_len : 4u;
    for (size_t size = 1u; size <= max; ++size) {
        const vstr input = vstr_from_buf((const char *)bytes, size);
        salts_unicode_scalar scalar;
        size_t cursor = 0u;
        if (salts_unicode_utf8_next(input, &cursor, &scalar) ==
                SALTS_UNICODE_OK && cursor == size) {
            input_consume(term, size);
            if (!paste_accept(term, size) ||
                (scalar.value >= 0x80u && scalar.value <= 0x9fu))
                return TUI_INPUT_IGNORED;
            event->type = TUI_EVENT_PASTE;
            event->ch = scalar.value;
            return TUI_INPUT_EVENT;
        }
    }
    if (term->input_len >= 4u) {
        input_consume(term, 1u);
        (void)paste_accept(term, 1u);
        return TUI_INPUT_IGNORED;
    }
    return TUI_INPUT_INCOMPLETE;
}

static tui_input_result_t decode_one(tui_terminal_t *term,
                                     tui_event_t *event) {
    const unsigned char *bytes = term->input_bytes;
    if (term->input_len == 0u) return TUI_INPUT_INCOMPLETE;
    if (term->paste_active) return decode_paste(term, event);

    if (bytes[0] < 0x80u && bytes[0] != 27u) {
        input_key(bytes[0], event);
        input_consume(term, 1u);
        return TUI_INPUT_EVENT;
    }
    if (bytes[0] >= 0x80u) {
        const size_t max = term->input_len < 4u ? term->input_len : 4u;
        for (size_t size = 1u; size <= max; ++size) {
            const vstr input = vstr_from_buf((const char *)bytes, size);
            salts_unicode_scalar scalar;
            size_t cursor = 0u;
            const salts_unicode_status status =
                salts_unicode_utf8_next(input, &cursor, &scalar);
            if (status == SALTS_UNICODE_OK && cursor == size) {
                event->type = TUI_EVENT_KEY;
                event->ch = scalar.value;
                input_consume(term, size);
                return TUI_INPUT_EVENT;
            }
        }
        /* UTF-8 scalars are at most four bytes.  Once four bytes fail every
         * canonical Salts prefix scan, the leading byte is malformed rather
         * than incomplete; discard it and continue with queued input. */
        if (term->input_len >= 4u) {
            input_consume(term, 1u);
            return TUI_INPUT_IGNORED;
        }
        return TUI_INPUT_INCOMPLETE;
    }
    if (term->input_len == 1u) return TUI_INPUT_INCOMPLETE;

    if (bytes[1] == '[') return decode_csi(term, event);
    if (bytes[1] == 'O') {
        if (term->input_len < 3u) return TUI_INPUT_INCOMPLETE;
        event->type = TUI_EVENT_KEY;
        switch (bytes[2]) {
            case 'P': event->key = TUI_KEY_F1; break;
            case 'Q': event->key = TUI_KEY_F2; break;
            case 'R': event->key = TUI_KEY_F3; break;
            case 'S': event->key = TUI_KEY_F4; break;
            default: input_consume(term, 3u); return TUI_INPUT_IGNORED;
        }
        input_consume(term, 3u);
        return TUI_INPUT_EVENT;
    }

    /* Preserve the following byte for the next event. */
    input_key(bytes[0], event);
    input_consume(term, 1u);
    return TUI_INPUT_EVENT;
}

bool tui_terminal_input_push(tui_terminal_t *term, const void *data,
                             size_t size) {
    if (!term || (!data && size != 0u) ||
        size > sizeof(term->input_bytes) - term->input_len) {
        return false;
    }
    if (size != 0u) {
        memcpy(term->input_bytes + term->input_len, data, size);
        term->input_len += size;
    }
    return true;
}

bool tui_terminal_input_next(tui_terminal_t *term, tui_event_t *event) {
    if (!term || !event) return false;
    memset(event, 0, sizeof(*event));
    for (;;) {
        const tui_input_result_t result = decode_one(term, event);
        if (result == TUI_INPUT_EVENT) return true;
        if (result == TUI_INPUT_INCOMPLETE) return false;
    }
}

bool tui_terminal_input_flush(tui_terminal_t *term, tui_event_t *event) {
    if (!term || !event || term->input_len == 0u) return false;
    if (tui_terminal_input_next(term, event)) return true;
    if (term->input_len == 0u) return false;

    memset(event, 0, sizeof(*event));
    if (term->paste_active) {
        /* A timed-out paste prefix/scalar is never replayed through the key
         * decoder.  Stay in discard mode until a complete end marker arrives. */
        input_consume(term, term->input_len);
        term->paste_discarding = true;
        return false;
    }
    if (term->input_len == 1u && term->input_bytes[0] == 27u) {
        input_key(term->input_bytes[0], event);
        input_consume(term, 1u);
        return true;
    }
    if (term->input_bytes[0] == 27u) {
        /* A timed-out control sequence is untrusted as a unit.  Do not replay
         * its parameter bytes as ordinary key presses. */
        input_consume(term, term->input_len);
        return false;
    }

    /* Salts rejected an incomplete or malformed non-ASCII prefix.  Drop only
     * invalid leading bytes, preserving a later valid scalar/key if present. */
    while (term->input_len != 0u) {
        input_consume(term, 1u);
        if (tui_terminal_input_next(term, event)) return true;
    }
    return false;
}

bool tui_terminal_poll(tui_terminal_t* term, tui_event_t* event) {
    if (!term || !event) return false;

    if (tui_terminal_input_next(term, event)) return true;

    /* Check resize signal */
    if (tui_terminal_consume_resize_pending()) {
        memset(event, 0, sizeof(*event));
        event->type = TUI_EVENT_RESIZE;
        return true;
    }

    for (;;) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        /* Wait briefly only while disambiguating a partial escape sequence. */
        struct timeval tv = {0, term->input_len != 0u ? 10000 : 0};
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) {
            if (tui_terminal_consume_resize_pending()) {
                memset(event, 0, sizeof(*event));
                event->type = TUI_EVENT_RESIZE;
                return true;
            }
            return tui_terminal_input_flush(term, event);
        }

        unsigned char buf[32];
        const size_t available = sizeof(term->input_bytes) - term->input_len;
        const size_t read_size = available < sizeof(buf) ? available :
                                                            sizeof(buf);
        const ssize_t n = read(STDIN_FILENO, buf, read_size);
        if (n <= 0) return tui_terminal_input_flush(term, event);
        if (!tui_terminal_input_push(term, buf, (size_t)n)) return false;
        if (tui_terminal_input_next(term, event)) return true;
    }
}

#endif

void tui_terminal_wait(tui_terminal_t* term, tui_event_t* event) {
    while (!tui_terminal_poll(term, event)) {
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
}
