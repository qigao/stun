/*
 * Real Windows ConPTY host acceptance for the unchanged Tango/FlexUI demo.
 * Only the host is test code: no event-map injection, fake terminal buffer,
 * second Tango UI owner, or private Tango API. ResizePseudoConsole is the
 * Windows host-owned operation; SetConsoleWindowInfo is not a substitute.
 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {
class Screen {
    enum class Parse { Normal, Escape, CSI, OSC, OSCEscape };
    Parse parse_ = Parse::Normal;
    std::string csi_;
    int rows_ = 24, cols_ = 80, x_ = 0, y_ = 0;
    int utf8_continuations_ = 0;
    std::vector<std::string> cells_;
    bool entered_alt_ = false, left_alt_ = false;

    void clear() {
        cells_.assign(static_cast<size_t>(rows_),
                      std::string(static_cast<size_t>(cols_), ' '));
        x_ = y_ = 0;
    }

    void put(char ch, int width = 1) {
        if (x_ >= cols_) {
            x_ = 0;
            y_ = std::min(rows_ - 1, y_ + 1);
        }
        if (x_ >= 0 && y_ >= 0 && y_ < rows_ && x_ < cols_)
            cells_[static_cast<size_t>(y_)][static_cast<size_t>(x_)] = ch;
        x_ += width;
    }

    int number(size_t index, int fallback = 1) const {
        size_t begin = 0;
        while (index-- && begin != std::string::npos) {
            begin = csi_.find(';', begin);
            if (begin != std::string::npos) ++begin;
        }
        if (begin == std::string::npos || begin >= csi_.size() ||
            !std::isdigit(static_cast<unsigned char>(csi_[begin])))
            return fallback;
        int n = 0;
        while (begin < csi_.size() &&
               std::isdigit(static_cast<unsigned char>(csi_[begin]))) {
            n = std::min(32767, n * 10 + csi_[begin] - '0');
            ++begin;
        }
        return n == 0 ? fallback : n;
    }

    void control(char code) {
        if (csi_ == "?1049" && code == 'h') {
            entered_alt_ = true;
            clear();
        } else if (csi_ == "?1049" && code == 'l') {
            left_alt_ = true;
            clear();
        } else if (code == 'H' || code == 'f') {
            y_ = std::clamp(number(0) - 1, 0, rows_ - 1);
            x_ = std::clamp(number(1) - 1, 0, cols_ - 1);
        } else if (code == 'G') {
            x_ = std::clamp(number(0) - 1, 0, cols_ - 1);
        } else if (code == 'A') {
            y_ = std::max(0, y_ - number(0));
        } else if (code == 'B') {
            y_ = std::min(rows_ - 1, y_ + number(0));
        } else if (code == 'C') {
            x_ = std::min(cols_ - 1, x_ + number(0));
        } else if (code == 'D') {
            x_ = std::max(0, x_ - number(0));
        } else if (code == 'J' && (csi_ == "2" || csi_ == "3")) {
            clear();
        } else if (code == 'K') {
            int first = 0, last = cols_;
            if (csi_ == "1") last = std::min(cols_, x_ + 1);
            else if (csi_ != "2") first = std::min(cols_, x_);
            if (y_ >= 0 && y_ < rows_ && last > first) {
                cells_[static_cast<size_t>(y_)].replace(
                    static_cast<size_t>(first),
                    static_cast<size_t>(last - first),
                    static_cast<size_t>(last - first), ' ');
            }
        }
    }

public:
    Screen() { clear(); }
    void resize(int columns, int rows) {
        if (columns < 1 || columns > 256 || rows < 1 || rows > 120) return;
        cols_ = columns;
        rows_ = rows;
        clear();
    }
    void feed(const char *bytes, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            const unsigned char b = static_cast<unsigned char>(bytes[i]);
            switch (parse_) {
            case Parse::Escape:
                if (b == '[') { csi_.clear(); parse_ = Parse::CSI; }
                else if (b == ']') { parse_ = Parse::OSC; }
                else { parse_ = Parse::Normal; }
                break;
            case Parse::CSI:
                if (b >= 0x40 && b <= 0x7e) {
                    control(static_cast<char>(b));
                    parse_ = Parse::Normal;
                } else if (csi_.size() < 96) {
                    csi_.push_back(static_cast<char>(b));
                } else {
                    parse_ = Parse::Normal;
                }
                break;
            case Parse::OSC:
                if (b == 7) parse_ = Parse::Normal;
                else if (b == 27) parse_ = Parse::OSCEscape;
                break;
            case Parse::OSCEscape:
                parse_ = (b == '\\') ? Parse::Normal : Parse::OSC;
                break;
            case Parse::Normal:
                if (utf8_continuations_) {
                    --utf8_continuations_;
                } else if (b == 27) {
                    parse_ = Parse::Escape;
                } else if (b == '\r') {
                    x_ = 0;
                } else if (b == '\n') {
                    y_ = std::min(rows_ - 1, y_ + 1);
                    x_ = 0;
                } else if (b == '\b') {
                    x_ = std::max(0, x_ - 1);
                } else if (b >= 0x20 && b <= 0x7e) {
                    put(static_cast<char>(b));
                } else if (b >= 0xf0) {
                    put('?', 2);
                    utf8_continuations_ = 3;
                } else if (b >= 0xe0) {
                    put('?', 2);
                    utf8_continuations_ = 2;
                } else if (b >= 0xc2) {
                    put('?');
                    utf8_continuations_ = 1;
                }
                break;
            }
        }
    }
    std::string status() const {
        for (auto i = cells_.rbegin(); i != cells_.rend(); ++i) {
            auto pos = i->find("Retained ");
            if (pos != std::string::npos) return i->substr(pos);
        }
        return {};
    }
    bool header() const {
        for (const auto& line : cells_) {
            if (line.find("FlexUI + Tango") != std::string::npos) return true;
        }
        return false;
    }
    bool mode(const char *which, int cols, int rows) const {
        const auto line = status();
        const std::string expect = std::string(" | ") + which + " | " +
            std::to_string(cols) + "x" + std::to_string(rows) + " | ";
        return line.find(expect) != std::string::npos;
    }
    unsigned count() const {
        const auto line = status();
        unsigned result = 0;
        if (!line.empty()) (void)std::sscanf(line.c_str(), "Retained %u/12", &result);
        return result;
    }
    bool entered_alt() const { return entered_alt_; }
    bool left_alt() const { return left_alt_; }
    std::string diagnostic() const {
        std::string preview;
        for (const auto& line : cells_) {
            if (line.find_first_not_of(' ') == std::string::npos) continue;
            if (!preview.empty()) preview += " / ";
            preview += line.substr(0, 78);
            if (preview.size() > 300) break;
        }
        return "size=" + std::to_string(cols_) + "x" +
               std::to_string(rows_) + ", entered-alt=" +
               std::to_string(entered_alt_ ? 1 : 0) +
               ", left-alt=" + std::to_string(left_alt_ ? 1 : 0) +
               ", header=" + std::to_string(header() ? 1 : 0) +
               ", status='" + status() + "', preview='" + preview + "'";
    }
};

class ConptyAcceptance {
    HANDLE child_input_write_ = nullptr;
    HANDLE child_output_read_ = nullptr;
    HPCON pseudo_ = nullptr;
    PROCESS_INFORMATION process_{};
    std::thread reader_;
    std::mutex mutex_;
    Screen display_;
    size_t output_count_ = 0;
    std::atomic<bool> output_limit_hit_{false};
    std::atomic<bool> reader_done_{false};
    std::atomic<DWORD> reader_error_{0};
    std::string phase_ = "initialize";

    static std::wstring utf8_to_wide(const char* path) {
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                              path, -1, nullptr, 0);
        if (count <= 0) return {};
        std::wstring result(static_cast<size_t>(count), L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                 path, -1, result.data(), count)) return {};
        result.resize(static_cast<size_t>(count - 1));
        return result;
    }

    void read_output() {
        char chunk[16384];
        DWORD read = 0;
        while (ReadFile(child_output_read_, chunk, sizeof(chunk),
                        &read, nullptr) && read != 0) {
            std::lock_guard<std::mutex> guard(mutex_);
            if (output_count_ > 12u * 1024u * 1024u - read) {
                output_limit_hit_ = true;
                break;
            }
            output_count_ += read;
            display_.feed(chunk, read);
        }
        reader_error_ = GetLastError();
        reader_done_ = true;
    }
    bool write_input(const char *data, DWORD length) {
        DWORD written = 0;
        return WriteFile(child_input_write_, data, length, &written, nullptr) &&
               written == length;
    }

public:
    ~ConptyAcceptance() {
        if (process_.hProcess &&
            WaitForSingleObject(process_.hProcess, 0) == WAIT_TIMEOUT) {
            TerminateProcess(process_.hProcess, 77);
            (void)WaitForSingleObject(process_.hProcess, 2000);
        }
        if (child_input_write_) CloseHandle(child_input_write_);
        if (pseudo_) ClosePseudoConsole(pseudo_);
        if (reader_.joinable()) reader_.join();
        if (child_output_read_) CloseHandle(child_output_read_);
        if (process_.hThread) CloseHandle(process_.hThread);
        if (process_.hProcess) CloseHandle(process_.hProcess);
    }
    std::string last_phase() const { return phase_; }
    std::string diagnostic() {
        std::lock_guard<std::mutex> guard(mutex_);
        DWORD child_exit = 999;
        if (process_.hProcess) (void)GetExitCodeProcess(
            process_.hProcess, &child_exit);
        return display_.diagnostic() + ", output_bytes=" +
            std::to_string(output_count_) + ", reader_done=" +
            std::to_string(reader_done_ ? 1 : 0) + ", reader_error=" +
            std::to_string(reader_error_.load()) + ", child_exit=" +
            std::to_string(child_exit) + ", output_limit=" +
            std::to_string(output_limit_hit_ ? 1 : 0);
    }
    bool setup(const char* executable) {
        phase_ = "CreatePseudoConsole/child";
        HANDLE conpty_in = nullptr, conpty_out = nullptr;
        SECURITY_ATTRIBUTES attrs{sizeof(attrs), nullptr, TRUE};
        if (!CreatePipe(&conpty_in, &child_input_write_, &attrs, 0) ||
            !CreatePipe(&child_output_read_, &conpty_out, &attrs, 0))
            return false;
        (void)SetHandleInformation(child_input_write_, HANDLE_FLAG_INHERIT, 0);
        (void)SetHandleInformation(child_output_read_, HANDLE_FLAG_INHERIT, 0);
        const HRESULT hr = CreatePseudoConsole(COORD{80, 24},
            conpty_in, conpty_out, 0, &pseudo_);
        if (FAILED(hr)) {
            CloseHandle(conpty_in);
            CloseHandle(conpty_out);
            return false;
        }
        // Keep the device-end pipe handles open until CreateProcessW has
        // attached the client to the pseudoconsole. Closing them here can
        // leave a live child and a permanently empty ConPTY output pipe.

        SIZE_T attr_bytes = 0;
        (void)InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_bytes);
        if (!attr_bytes) return false;
        std::unique_ptr<unsigned char[]> attr_memory(new unsigned char[attr_bytes]);
        auto* attribute_list =
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attr_memory.get());
        if (!InitializeProcThreadAttributeList(attribute_list, 1, 0,
                                               &attr_bytes)) return false;
        constexpr DWORD_PTR kPseudoConsoleAttribute = 0x00020016;
        const BOOL attr_ok = UpdateProcThreadAttribute(attribute_list, 0,
            kPseudoConsoleAttribute, &pseudo_, sizeof(pseudo_), nullptr, nullptr);
        const auto wide = utf8_to_wide(executable);
        BOOL started = FALSE;
        if (attr_ok && !wide.empty()) {
            std::wstring command = L"\"" + wide + L"\"";
            std::vector<wchar_t> cmd(command.begin(), command.end());
            cmd.push_back(L'\0');
            STARTUPINFOEXW startup{};
            startup.StartupInfo.cb = sizeof(startup);
            startup.lpAttributeList = attribute_list;
            started = CreateProcessW(wide.c_str(), cmd.data(), nullptr,
                nullptr, FALSE, EXTENDED_STARTUPINFO_PRESENT, nullptr,
                nullptr, &startup.StartupInfo, &process_);
        }
        DeleteProcThreadAttributeList(attribute_list);
        // Release our copies only after CreateProcessW completes; ConPTY
        // keeps its own handles. This is the Microsoft documented lifetime.
        CloseHandle(conpty_in);
        CloseHandle(conpty_out);
        if (!started) return false;
        reader_ = std::thread([this]() { read_output(); });
        return true;
    }

    template<typename Predicate>
    bool await(const char* name, DWORD timeout_ms, Predicate expected,
               bool allow_exited = false) {
        phase_ = name;
        const ULONGLONG deadline = GetTickCount64() + timeout_ms;
        while (GetTickCount64() < deadline) {
            {
                std::lock_guard<std::mutex> guard(mutex_);
                if (expected(display_)) return true;
            }
            if (output_limit_hit_ ||
                (!allow_exited &&
                 WaitForSingleObject(process_.hProcess, 0) != WAIT_TIMEOUT))
                return false;
            Sleep(35);
        }
        return false;
    }
    bool run(const char *executable) {
        if (!setup(executable)) return false;
        if (!await("real 80x24 ConPTY terminal frame", 12000,
            [](const Screen &s) {return s.entered_alt() && s.header() &&
                                        s.mode("tail", 80, 24);})) return false;
        if (!await("at least six real ConPTY message admissions", 18000,
            [](const Screen &s) {return s.mode("tail", 80, 24) &&
                                        s.count() >= 6;})) return false;
        phase_ = "ResizePseudoConsole host-owned 52x16";
        const HRESULT resized = ResizePseudoConsole(pseudo_, COORD{52, 16});
        if (FAILED(resized)) return false;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            display_.resize(52, 16);
        }
        if (!await("visible child-owned 52x16 reflow", 10000,
            [](const Screen &s) {return s.header() &&
                                        s.mode("tail", 52, 16);})) return false;
        phase_ = "normal q and alternate-screen return";
        if (!write_input("q", 1) ||
            WaitForSingleObject(process_.hProcess, 10000) != WAIT_OBJECT_0)
            return false;
        DWORD exit_code = 999;
        if (!GetExitCodeProcess(process_.hProcess, &exit_code) || exit_code != 0)
            return false;
        if (!await("alt buffer restoration", 3000,
            [](const Screen &s) {return s.left_alt();}, true)) return false;
        return !output_limit_hit_;
    }
};

} // namespace
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    ConptyAcceptance harness;
    if (!harness.run(argv[1])) {
        std::fprintf(stderr, "Win32 ConPTY resize acceptance failed at '%s', "
            "%s, error=%lu\n",
            harness.last_phase().c_str(), harness.diagnostic().c_str(),
            static_cast<unsigned long>(GetLastError()));
        return 5;
    }
    std::puts("real Windows ConPTY accepted: host ResizePseudoConsole "
              "80x24 to 52x16, child visibly reflowed and restored ALT buffer");
    return 0;
}
