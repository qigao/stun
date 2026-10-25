// Real Windows Console qualification of the unchanged interactive Tango/FlexUI
// demo. No mock renderer, direct event-map call, synthetic console handle or
// second application event loop. The test is a distinct supervising process.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

struct ConsoleTest {
    HANDLE in = INVALID_HANDLE_VALUE;
    HANDLE out = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION child{};
    DWORD original_in = 0, original_out = 0;
    UINT original_input_cp = 0, original_output_cp = 0;
    bool attached = false;
    std::string last_phase = "setup";
    std::wstring last_screen;

    ~ConsoleTest() {
        if (child.hProcess) {
            if (WaitForSingleObject(child.hProcess, 0) == WAIT_TIMEOUT) {
                TerminateProcess(child.hProcess, 77);
                WaitForSingleObject(child.hProcess, 2000);
            }
            CloseHandle(child.hProcess);
        }
        if (child.hThread) CloseHandle(child.hThread);
        if (in != INVALID_HANDLE_VALUE) CloseHandle(in);
        if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
        if (attached) FreeConsole();
    }

    bool setup() {
        // GitHub Actions services typically have redirected std handles and
        // no console. Create a fresh real Console in this isolated test process.
        (void)FreeConsole();
        if (!AllocConsole()) return false;
        attached = true;
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        in = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         &sa, OPEN_EXISTING, 0, nullptr);
        out = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          &sa, OPEN_EXISTING, 0, nullptr);
        if (in == INVALID_HANDLE_VALUE || out == INVALID_HANDLE_VALUE ||
            !GetConsoleMode(in, &original_in) ||
            !GetConsoleMode(out, &original_out)) return false;
        original_input_cp = GetConsoleCP();
        original_output_cp = GetConsoleOutputCP();
        if (!original_input_cp || !original_output_cp) return false;
        const wchar_t marker[] = L"STUN_CONSOLE_BASELINE";
        DWORD written = 0;
        return WriteConsoleW(out, marker, 21, &written, nullptr) &&
               written == 21;
    }

    bool spawn(const char* path) {
        last_phase = "spawn ordinary demo";
        const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                  path, -1, nullptr, 0);
        if (required <= 0) return false;
        std::wstring wide(static_cast<size_t>(required), L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                                 &wide[0], required)) return false;
        wide.resize(static_cast<size_t>(required - 1));
        std::wstring command = L"\"" + wide + L"\"";
        std::vector<wchar_t> mutable_command(command.begin(), command.end());
        mutable_command.push_back(L'\0');
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = in;
        si.hStdOutput = out;
        si.hStdError = out;
        return CreateProcessW(wide.c_str(), mutable_command.data(),
                              nullptr, nullptr, TRUE, 0,
                              nullptr, nullptr, &si, &child) != FALSE;
    }

    struct Screen {
        bool ok = false;
        bool header = false;
        bool tail = false;
        bool history = false;
        unsigned count = 0;
        int width = 0, height = 0;
    };

    Screen read_screen() {
        Screen s;
        // Reopen CONOUT$ on every probe to follow the active VT alternate
        // buffer rather than reading the pre-existing main output buffer.
        HANDLE h = CreateFileW(L"CONOUT$", GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) return s;
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (GetConsoleScreenBufferInfo(h, &info)) {
            s.width = info.srWindow.Right - info.srWindow.Left + 1;
            s.height = info.srWindow.Bottom - info.srWindow.Top + 1;
            if (s.width > 0 && s.width <= 256 &&
                s.height > 0 && s.height <= 120) {
                wchar_t line[256];
                std::wstring text;
                s.ok = true;
                for (SHORT y = info.srWindow.Top; y <= info.srWindow.Bottom; ++y) {
                    DWORD read = 0;
                    COORD p{info.srWindow.Left, y};
                    if (!ReadConsoleOutputCharacterW(
                            h, line, static_cast<DWORD>(s.width), p, &read) ||
                        read != static_cast<DWORD>(s.width)) {
                        s.ok = false;
                        break;
                    }
                    text.append(line, read);
                    text.push_back(L'\n');
                }
                if (s.ok) {
                    last_screen = text;
                    s.header = text.find(L"FlexUI + Tango") != std::wstring::npos;
                    const size_t pos = text.find(L"Retained ");
                    if (pos != std::wstring::npos) {
                        const std::wstring status =
                            text.substr(pos, text.find(L'\n', pos) - pos);
                        (void)swscanf(status.c_str(), L"Retained %u/12",
                                      &s.count);
                        const std::wstring dims = L" | " +
                            std::to_wstring(s.width) + L"x" +
                            std::to_wstring(s.height) + L" | ";
                        s.tail = status.find(L" | tail | ") != std::wstring::npos &&
                                 status.find(dims) != std::wstring::npos;
                        s.history =
                            status.find(L" | scrollback | ") != std::wstring::npos &&
                            status.find(dims) != std::wstring::npos;
                    }
                }
            }
        }
        CloseHandle(h);
        return s;
    }

    template<class Predicate>
    bool await(const char* phase, DWORD timeout, Predicate predicate) {
        last_phase = phase;
        const ULONGLONG deadline = GetTickCount64() + timeout;
        while (GetTickCount64() < deadline) {
            const Screen s = read_screen();
            if (s.ok && predicate(s)) return true;
            if (WaitForSingleObject(child.hProcess, 0) != WAIT_TIMEOUT)
                return false;
            Sleep(25);
        }
        return false;
    }

    bool send_key(WORD virtual_key, wchar_t ch = 0) {
        INPUT_RECORD record{};
        record.EventType = KEY_EVENT;
        record.Event.KeyEvent.bKeyDown = TRUE;
        record.Event.KeyEvent.wRepeatCount = 1;
        record.Event.KeyEvent.wVirtualKeyCode = virtual_key;
        record.Event.KeyEvent.uChar.UnicodeChar = ch;
        DWORD n = 0;
        return WriteConsoleInputW(in, &record, 1, &n) && n == 1;
    }

    bool send_wheel() {
        INPUT_RECORD record{};
        record.EventType = MOUSE_EVENT;
        record.Event.MouseEvent.dwMousePosition = {2, 2};
        record.Event.MouseEvent.dwEventFlags = MOUSE_WHEELED;
        record.Event.MouseEvent.dwButtonState =
            static_cast<DWORD>(static_cast<WORD>(WHEEL_DELTA)) << 16;
        DWORD n = 0;
        return WriteConsoleInputW(in, &record, 1, &n) && n == 1;
    }

    bool live_modes() const {
        DWORD im = 0, om = 0;
        return GetConsoleMode(in, &im) && GetConsoleMode(out, &om) &&
            (im & (ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT)) ==
                 (ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT) &&
            (im & ENABLE_QUICK_EDIT_MODE) == 0 &&
            (om & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0 &&
            GetConsoleCP() == CP_UTF8 && GetConsoleOutputCP() == CP_UTF8;
    }

    bool resize() {
        last_phase = "SetConsoleWindowInfo physical 52x16";
        HANDLE active = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
        if (active == INVALID_HANDLE_VALUE) return false;
        SMALL_RECT target{0, 0, 51, 15};
        const BOOL result = SetConsoleWindowInfo(active, TRUE, &target);
        CloseHandle(active);
        return result != FALSE;
    }

    bool restored() {
        last_phase = "full input/output mode, codepage and screen restoration";
        DWORD im = 0, om = 0;
        if (!GetConsoleMode(in, &im) || !GetConsoleMode(out, &om) ||
            im != original_in || om != original_out ||
            GetConsoleCP() != original_input_cp ||
            GetConsoleOutputCP() != original_output_cp)
            return false;
        // Once ?1049l restores the original OS screen buffer, the sentinel
        // written before launching the demo must become visible again.
        return last_screen.find(L"STUN_CONSOLE_BASELINE") != std::wstring::npos;
    }

    bool run(const char* exe) {
        if (!setup() || !spawn(exe)) return false;
        if (!await("real console render and Win32 mode activation", 15000,
                   [this](const Screen& s) {
                       return s.header && s.tail && s.width >= 60 &&
                              s.height >= 18 && live_modes();
                   })) return false;
        if (!send_key(VK_HOME) ||
            !await("HOME INPUT_RECORD makes visible scrollback", 6000,
                   [](const Screen& s) {return s.history;})) return false;
        if (!send_key(VK_END) ||
            !await("END INPUT_RECORD makes visible tail", 6000,
                   [](const Screen& s) {return s.tail;})) return false;
        if (!await(">= 6 actual rendered messages before wheel", 16000,
                   [](const Screen& s) {return s.tail && s.count >= 6;}))
            return false;
        if (!send_wheel() ||
            !await("MOUSE_WHEELED visibly changes scrollback", 6000,
                   [](const Screen& s) {return s.history;})) return false;
        if (!resize() ||
            !await("52x16 physical console reflow", 10000,
                   [](const Screen& s) {
                       return s.header && s.history &&
                              s.width == 52 && s.height == 16;
                   })) return false;
        last_phase = "normal q INPUT_RECORD and process exit";
        if (!send_key('Q', L'q') ||
            WaitForSingleObject(child.hProcess, 10000) != WAIT_OBJECT_0)
            return false;
        DWORD exit_code = 1;
        if (!GetExitCodeProcess(child.hProcess, &exit_code) || exit_code != 0)
            return false;
        (void)read_screen();
        return restored();
    }
};

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    ConsoleTest test;
    const bool passed = test.run(argv[1]);
    if (!passed) {
        const DWORD error = GetLastError();
        std::fprintf(stderr,
            "Win32 Console PTY-equivalent gate FAILED at '%s', OS=%lu, "
            "last frame UTF-16 cells=%zu\n",
            test.last_phase.c_str(), static_cast<unsigned long>(error),
            test.last_screen.size());
        return 4;
    }
    std::puts("real Win32 Console chat passed: KEY_EVENT Home/End, "
              "MOUSE_WHEELED, resize 52x16, mode/codepage and "
              "alternate-screen restoration");
    return 0;
}
