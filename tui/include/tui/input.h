/*
 * TUI - Terminal User Interface Library
 * 
 * Input: Keyboard and mouse input handling.
 */

#pragma once

#include <optional>
#include <cstdint>

#ifdef _WIN32 
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <csignal>
#include <atomic>

// Global signal flag for SIGWINCH (Unix only)
inline std::atomic<bool> g_resize_pending{false};
inline void handle_winch(int) { g_resize_pending = true; }
#endif

namespace tui {

// Special keys
enum class Key : int {
    None = 0,
    
    // ASCII printable (32-126) use their values directly
    
    // Control keys
    Enter = 13,
    Tab = 9,
    Backspace = 127,
    Escape = 27,
    Space = 32,
    
    // Arrow keys (256+)
    Up = 256,
    Down,
    Left,
    Right,
    
    // Navigation
    Home,
    End,
    PageUp,
    PageDown,
    Insert,
    Delete,
    
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    
    // Mouse
    MouseLeft,
    MouseRight,
    MouseMiddle,
    MouseRelease,
    MouseWheelUp,
    MouseWheelDown,
};

// Modifier flags
enum class Mod : uint8_t {
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2,
};

constexpr Mod operator|(Mod a, Mod b) {
    return static_cast<Mod>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

constexpr bool has_mod(Mod m, Mod flag) {
    return (static_cast<uint8_t>(m) & static_cast<uint8_t>(flag)) != 0;
}

// Input event
struct Event {
    enum Type { KeyPress, MousePress, MouseRelease, MouseMove, Resize } type;
    
    Key key = Key::None;
    Mod mod = Mod::None;
    char32_t ch = 0;  // Unicode character (if printable)
    
    int x = 0, y = 0;  // Mouse position (for mouse events)
    
    bool is_key() const { return type == KeyPress; }
    bool is_mouse() const { return type == MousePress || type == MouseRelease || type == MouseMove; }
    bool is_resize() const { return type == Resize; }
    
    // Check for specific key
    bool is(Key k) const { return type == KeyPress && key == k; }
    bool is(char c) const { return type == KeyPress && ch == static_cast<char32_t>(c); }
    
    // Check for Ctrl+key
    bool is_ctrl(char c) const {
        return type == KeyPress && has_mod(mod, Mod::Ctrl) && 
               (ch == static_cast<char32_t>(c) || ch == static_cast<char32_t>(c - 'a' + 1));
    }
};

class Input {
public:
    Input() {
#ifndef _WIN32
        struct sigaction sa;
        sa.sa_handler = handle_winch;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGWINCH, &sa, nullptr);
#endif
    }
    
    void enable_mouse() {
        mouse_enabled_ = true;
#ifdef _WIN32
        // Mouse already enabled via console mode
#else
        // SGR mouse mode
        ::write(STDOUT_FILENO, "\033[?1006h", 8);  // SGR extended
        ::write(STDOUT_FILENO, "\033[?1003h", 8);  // All motion
#endif
    }
    
    void disable_mouse() {
        mouse_enabled_ = false;
#ifndef _WIN32
        ::write(STDOUT_FILENO, "\033[?1003l", 8);
        ::write(STDOUT_FILENO, "\033[?1006l", 8);
#endif
    }
    
    // Poll for input (non-blocking)
    std::optional<Event> poll() {
#ifdef _WIN32
        return poll_windows();
#else
        return poll_unix();
#endif
    }
    
    // Wait for input (blocking)
    Event wait() {
        while (true) {
            if (auto e = poll()) return *e;
#ifdef _WIN32
            Sleep(10);
#else
            usleep(10000);
#endif
        }
    }

private:
#ifdef _WIN32
    std::optional<Event> poll_windows() {
        HANDLE h_in = GetStdHandle(STD_INPUT_HANDLE);
        DWORD num_events = 0;
        GetNumberOfConsoleInputEvents(h_in, &num_events);
        if (num_events == 0) return std::nullopt;
        
        INPUT_RECORD rec;
        DWORD read;
        if (!ReadConsoleInputW(h_in, &rec, 1, &read) || read == 0) {
            return std::nullopt;
        }
        
        Event e;
        
        if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
            e.type = Event::KeyPress;
            e.ch = rec.Event.KeyEvent.uChar.UnicodeChar;
            
            DWORD state = rec.Event.KeyEvent.dwControlKeyState;
            if (state & SHIFT_PRESSED) e.mod = e.mod | Mod::Shift;
            if (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) e.mod = e.mod | Mod::Ctrl;
            if (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) e.mod = e.mod | Mod::Alt;
            
            switch (rec.Event.KeyEvent.wVirtualKeyCode) {
                case VK_UP: e.key = Key::Up; break;
                case VK_DOWN: e.key = Key::Down; break;
                case VK_LEFT: e.key = Key::Left; break;
                case VK_RIGHT: e.key = Key::Right; break;
                case VK_HOME: e.key = Key::Home; break;
                case VK_END: e.key = Key::End; break;
                case VK_PRIOR: e.key = Key::PageUp; break;
                case VK_NEXT: e.key = Key::PageDown; break;
                case VK_INSERT: e.key = Key::Insert; break;
                case VK_DELETE: e.key = Key::Delete; break;
                case VK_F1: e.key = Key::F1; break;
                case VK_F2: e.key = Key::F2; break;
                case VK_F3: e.key = Key::F3; break;
                case VK_F4: e.key = Key::F4; break;
                case VK_F5: e.key = Key::F5; break;
                case VK_F6: e.key = Key::F6; break;
                case VK_F7: e.key = Key::F7; break;
                case VK_F8: e.key = Key::F8; break;
                case VK_F9: e.key = Key::F9; break;
                case VK_F10: e.key = Key::F10; break;
                case VK_F11: e.key = Key::F11; break;
                case VK_F12: e.key = Key::F12; break;
                case VK_RETURN: e.key = Key::Enter; e.ch = '\n'; break;
                case VK_TAB: e.key = Key::Tab; e.ch = '\t'; break;
                case VK_BACK: e.key = Key::Backspace; break;
                case VK_ESCAPE: e.key = Key::Escape; break;
                default:
                    if (e.ch >= 32 && e.ch < 127) {
                        e.key = static_cast<Key>(e.ch);
                    }
                    break;
            }
            return e;
        }
        
        if (rec.EventType == MOUSE_EVENT && mouse_enabled_) {
            auto& me = rec.Event.MouseEvent;
            e.x = me.dwMousePosition.X;
            e.y = me.dwMousePosition.Y;
            
            if (me.dwEventFlags == 0) {  // Button press/release
                if (me.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                    e.type = Event::MousePress;
                    e.key = Key::MouseLeft;
                } else if (me.dwButtonState & RIGHTMOST_BUTTON_PRESSED) {
                    e.type = Event::MousePress;
                    e.key = Key::MouseRight;
                } else {
                    e.type = Event::MouseRelease;
                    e.key = Key::MouseRelease;
                }
                return e;
            } else if (me.dwEventFlags == MOUSE_MOVED) {
                e.type = Event::MouseMove;
                return e;
            } else if (me.dwEventFlags == MOUSE_WHEELED) {
                e.type = Event::MousePress;
                e.key = (short)HIWORD(me.dwButtonState) > 0 ? Key::MouseWheelUp : Key::MouseWheelDown;
                return e;
            }
        }
        
        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            e.type = Event::Resize;
            return e;
        }
        
        return std::nullopt;
    }
#else
    std::optional<Event> poll_unix() {
        if (g_resize_pending.exchange(false)) {
            Event e;
            e.type = Event::Resize;
            return e;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        
        struct timeval tv = {0, 0};
        if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0) {
            // Check again in case signal arrived (EINTR)
            if (g_resize_pending.exchange(false)) {
                Event e;
                e.type = Event::Resize;
                return e;
            }
            return std::nullopt;
        }
        
        char buf[32];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) return std::nullopt;
        
        Event e;
        e.type = Event::KeyPress;
        
        // Parse escape sequences
        if (n == 1) {
            e.ch = static_cast<unsigned char>(buf[0]);
            if (e.ch < 32) {
                // Control character
                if (e.ch == 27) e.key = Key::Escape;
                else if (e.ch == 13 || e.ch == 10) { e.key = Key::Enter; e.ch = '\n'; }
                else if (e.ch == 9) { e.key = Key::Tab; e.ch = '\t'; }
                else {
                    e.mod = Mod::Ctrl;
                    e.ch = e.ch + 'a' - 1;
                }
            } else if (e.ch == 127) {
                e.key = Key::Backspace;
            } else {
                e.key = static_cast<Key>(e.ch);
            }
            return e;
        }
        
        // CSI sequences
        if (buf[0] == '\033' && buf[1] == '[') {
            // Arrow keys, etc.
            if (n == 3) {
                switch (buf[2]) {
                    case 'A': e.key = Key::Up; return e;
                    case 'B': e.key = Key::Down; return e;
                    case 'C': e.key = Key::Right; return e;
                    case 'D': e.key = Key::Left; return e;
                    case 'H': e.key = Key::Home; return e;
                    case 'F': e.key = Key::End; return e;
                }
            }
            
            // SGR mouse: \033[<btn;x;y[Mm]
            if (buf[2] == '<' && mouse_enabled_) {
                int btn = 0, x = 0, y = 0;
                char type = 0;
                if (std::sscanf(buf + 3, "%d;%d;%d%c", &btn, &x, &y, &type) == 4) {
                    e.x = x - 1;
                    e.y = y - 1;
                    e.type = (type == 'M') ? Event::MousePress : Event::MouseRelease;
                    
                    int button = btn & 0x03;
                    if (btn & 64) {
                        e.key = (btn & 1) ? Key::MouseWheelDown : Key::MouseWheelUp;
                    } else if (btn & 32) {
                        e.type = Event::MouseMove;
                    } else {
                        switch (button) {
                            case 0: e.key = Key::MouseLeft; break;
                            case 1: e.key = Key::MouseMiddle; break;
                            case 2: e.key = Key::MouseRight; break;
                            default: e.key = Key::MouseRelease; break;
                        }
                    }
                    return e;
                }
            }
            
            // Function keys, etc.
            int num = 0;
            if (std::sscanf(buf + 2, "%d~", &num) == 1) {
                switch (num) {
                    case 1: e.key = Key::Home; return e;
                    case 2: e.key = Key::Insert; return e;
                    case 3: e.key = Key::Delete; return e;
                    case 4: e.key = Key::End; return e;
                    case 5: e.key = Key::PageUp; return e;
                    case 6: e.key = Key::PageDown; return e;
                    case 15: e.key = Key::F5; return e;
                    case 17: e.key = Key::F6; return e;
                    case 18: e.key = Key::F7; return e;
                    case 19: e.key = Key::F8; return e;
                    case 20: e.key = Key::F9; return e;
                    case 21: e.key = Key::F10; return e;
                    case 23: e.key = Key::F11; return e;
                    case 24: e.key = Key::F12; return e;
                }
            }
        }
        
        // SS3 sequences (F1-F4)
        if (buf[0] == '\033' && buf[1] == 'O' && n == 3) {
            switch (buf[2]) {
                case 'P': e.key = Key::F1; return e;
                case 'Q': e.key = Key::F2; return e;
                case 'R': e.key = Key::F3; return e;
                case 'S': e.key = Key::F4; return e;
            }
        }
        
        return std::nullopt;
    }
#endif
    
    bool mouse_enabled_ = false;
};

} // namespace tui
