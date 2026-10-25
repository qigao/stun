/*
 * C11 Win32 INPUT_RECORD decoding contract. Uses the exact production
 * parser entry; a separate installed-SDK gate drives the real OS console.
 * This is not a substitute for physical IME-composition acceptance.
 */
#include "tui_terminal_internal.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "tango_win32_input_test:%d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static INPUT_RECORD key_record(BOOL down, WORD vk, WCHAR utf16, DWORD state) {
    INPUT_RECORD rec;
    memset(&rec, 0, sizeof(rec));
    rec.EventType = KEY_EVENT;
    rec.Event.KeyEvent.bKeyDown = down;
    rec.Event.KeyEvent.wVirtualKeyCode = vk;
    rec.Event.KeyEvent.wRepeatCount = 1;
    rec.Event.KeyEvent.uChar.UnicodeChar = utf16;
    rec.Event.KeyEvent.dwControlKeyState = state;
    return rec;
}

int main(void) {
    tui_terminal_t* a = tui_terminal_create();
    tui_terminal_t* b = tui_terminal_create();
    CHECK(a && b);
    tui_event_t event;

    INPUT_RECORD rec = key_record(TRUE, 'A', L'A', SHIFT_PRESSED);
    CHECK(tui_terminal_input_record(a, &rec, &event));
    CHECK(event.type == TUI_EVENT_KEY && event.ch == 'A');
    CHECK(event.key == (tui_key_t)'A' && (event.mod & TUI_MOD_SHIFT));

    /* U+1F60A (smiling face) is two UTF-16 units. A KEYUP can separate
     * the KEYDOWN halves; no surrogate may escape as an event scalar. */
    rec = key_record(TRUE, 0, (WCHAR)0xd83du, 0);
    CHECK(!tui_terminal_input_record(a, &rec, &event));
    rec = key_record(FALSE, 0, (WCHAR)0xd83du, 0);
    CHECK(!tui_terminal_input_record(a, &rec, &event));
    rec = key_record(TRUE, 0, (WCHAR)0xde0au, 0);
    CHECK(tui_terminal_input_record(a, &rec, &event));
    CHECK(event.type == TUI_EVENT_KEY && event.ch == 0x1f60au);
    rec = key_record(FALSE, 0, (WCHAR)0xde0au, 0);
    CHECK(!tui_terminal_input_record(a, &rec, &event));

    /* No cross-terminal pairing and no lone low-surrogate acceptance. */
    rec = key_record(TRUE, 0, (WCHAR)0xde0au, 0);
    CHECK(!tui_terminal_input_record(b, &rec, &event));
    CHECK(!tui_terminal_input_record(a, &rec, &event));

    /* An unrelated key must discard a pending high surrogate. */
    rec = key_record(TRUE, 0, (WCHAR)0xd83du, 0);
    CHECK(!tui_terminal_input_record(a, &rec, &event));
    rec = key_record(TRUE, 'x', L'x', 0);
    CHECK(tui_terminal_input_record(a, &rec, &event));
    CHECK(event.ch == 'x');
    rec = key_record(TRUE, 0, (WCHAR)0xde0au, 0);
    CHECK(!tui_terminal_input_record(a, &rec, &event));

    /* A newer high surrogate replaces the earlier incomplete one. */
    rec = key_record(TRUE, 0, (WCHAR)0xd800u, 0);
    CHECK(!tui_terminal_input_record(a, &rec, &event));
    rec = key_record(TRUE, 0, (WCHAR)0xd83du, 0);
    CHECK(!tui_terminal_input_record(a, &rec, &event));
    rec = key_record(TRUE, 0, (WCHAR)0xde0au, 0);
    CHECK(tui_terminal_input_record(a, &rec, &event));
    CHECK(event.ch == 0x1f60au);

    /* Mouse press state is terminal-scoped, not a static cross-owner flag. */
    a->mouse_enabled = true;
    b->mouse_enabled = true;
    memset(&rec, 0, sizeof(rec));
    rec.EventType = MOUSE_EVENT;
    rec.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
    CHECK(tui_terminal_input_record(a, &rec, &event));
    CHECK(event.type == TUI_EVENT_MOUSE_PRESS &&
          event.key == TUI_KEY_MOUSE_LEFT);
    rec.Event.MouseEvent.dwButtonState = 0u;
    CHECK(!tui_terminal_input_record(b, &rec, &event));
    CHECK(tui_terminal_input_record(a, &rec, &event));
    CHECK(event.type == TUI_EVENT_MOUSE_RELEASE &&
          event.key == TUI_KEY_MOUSE_LEFT);

    rec.Event.MouseEvent.dwEventFlags = MOUSE_WHEELED;
    rec.Event.MouseEvent.dwButtonState = (DWORD)WHEEL_DELTA << 16;
    CHECK(tui_terminal_input_record(a, &rec, &event));
    CHECK(event.key == TUI_KEY_MOUSE_WHEEL_UP);

    memset(&rec, 0, sizeof(rec));
    rec.EventType = WINDOW_BUFFER_SIZE_EVENT;
    CHECK(tui_terminal_input_record(a, &rec, &event));
    CHECK(event.type == TUI_EVENT_RESIZE);

    tui_terminal_destroy(b);
    tui_terminal_destroy(a);
    puts("Win32 UTF-16 INPUT_RECORD and per-terminal mouse decode passed");
    return 0;
}
