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
    bool wrap_enabled_ = true;
    unsigned wrap_mode_changes_ = 0u;
    unsigned bottom_auto_wraps_ = 0u;
    unsigned bottom_explicit_newlines_ = 0u;
    unsigned all_auto_wraps_ = 0u;
    unsigned auto_wraps_into_footer_ = 0u;
    unsigned explicit_newlines_into_footer_ = 0u;
    int last_auto_wrap_row_ = -1;
    unsigned screen_erase_full_ = 0u;
    int scroll_top_ = 0, scroll_bottom_ = 23;
    unsigned scroll_region_commands_ = 0u;
    unsigned scroll_region_linefeeds_ = 0u;

    void clear() {
        cells_.assign(static_cast<size_t>(rows_),
                      std::string(static_cast<size_t>(cols_), ' '));
        x_ = y_ = 0;
        scroll_top_ = 0;
        scroll_bottom_ = rows_ - 1;
    }

    void newline(bool implicit_wrap = false) {
        if (implicit_wrap) {
            ++all_auto_wraps_;
            last_auto_wrap_row_ = y_;
            if (y_ == rows_ - 3 && scroll_bottom_ > y_)
                ++auto_wraps_into_footer_;
        } else if (y_ == rows_ - 3 && scroll_bottom_ > y_) {
            ++explicit_newlines_into_footer_;
        }
        if (y_ >= scroll_top_ && y_ == scroll_bottom_) {
            // DECSTBM: LF on the bottom margin scrolls ONLY the bounded
            // region, never the fixed header/footer outside its borders.
            if (scroll_top_ != 0 || scroll_bottom_ != rows_ - 1)
                ++scroll_region_linefeeds_;
            else if (implicit_wrap) ++bottom_auto_wraps_;
            else ++bottom_explicit_newlines_;
            for (int row = scroll_top_; row < scroll_bottom_; ++row)
                cells_[static_cast<size_t>(row)] =
                    cells_[static_cast<size_t>(row + 1)];
            cells_[static_cast<size_t>(scroll_bottom_)].assign(
                static_cast<size_t>(cols_), ' ');
        } else if (y_ + 1 < rows_) {
            ++y_;
        } else {
            // LF outside any partial scrolling margins at viewport bottom.
            if (implicit_wrap) ++bottom_auto_wraps_;
            else ++bottom_explicit_newlines_;
            cells_.erase(cells_.begin());
            cells_.push_back(std::string(static_cast<size_t>(cols_), ' '));
        }
    }

    void put(char ch, int width = 1) {
        if (x_ >= cols_) {
            if (wrap_enabled_) {
                x_ = 0;
                newline(true);
            } else {
                // DECAWM off: printing again in the final column overwrites
                // that cell; it must not scroll a fixed footer out of view.
                x_ = cols_ - 1;
            }
        }
        if (x_ >= 0 && y_ >= 0 && y_ < rows_ && x_ < cols_) {
            auto& line = cells_[static_cast<size_t>(y_)];
            line[static_cast<size_t>(x_)] = ch;
            if (width == 2 && x_ + 1 < cols_)
                line[static_cast<size_t>(x_ + 1)] = ' ';
        }
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
        } else if (csi_ == "?7" && (code == 'h' || code == 'l')) {
            // Standard DEC automatic line-wrap. Windows Console mode
            // changes may be reflected in the ConPTY VT stream.
            wrap_enabled_ = code == 'h';
            ++wrap_mode_changes_;
        } else if (code == 'r') {
            // DECSTBM defaults to the whole viewport when omitted. ConPTY
            // can scroll the body while keeping title/status rows untouched.
            const int top = std::clamp(number(0, 1) - 1, 0, rows_ - 1);
            const int bottom = std::clamp(
                number(1, rows_) - 1, 0, rows_ - 1);
            if (top < bottom) {
                scroll_top_ = top;
                scroll_bottom_ = bottom;
                ++scroll_region_commands_;
                x_ = y_ = 0;
            }
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
        } else if (code == 'J') {
            // ED 3 erases SAVED scrollback, not visible viewport cells.
            // ED 0/1/2 erase only the corresponding visible region and
            // never move the cursor (unlike alternate-screen entry).
            if (csi_ != "3") {
                if (csi_ == "2") ++screen_erase_full_;
                const bool before = csi_ == "1";
                const bool all = csi_ == "2";
                for (int row = 0; row < rows_; ++row) {
                    int first = 0, end = cols_;
                    if (!all) {
                        if (before) {
                            if (row > y_) continue;
                            if (row == y_) end = std::min(cols_, x_ + 1);
                        } else {
                            if (row < y_) continue;
                            if (row == y_) first = std::min(cols_, x_);
                        }
                    }
                    if (end > first)
                        cells_[static_cast<size_t>(row)].replace(
                            static_cast<size_t>(first),
                            static_cast<size_t>(end - first),
                            static_cast<size_t>(end - first), ' ');
                }
            }
        } else if (code == 'X') {
            // ECH: erase N cells at cursor without moving it. ConPTY
            // normalizes native WriteConsole output into CSI n X runs.
            if (y_ >= 0 && y_ < rows_ && x_ >= 0 && x_ < cols_) {
                const int count = std::min(number(0), cols_ - x_);
                if (count > 0)
                    cells_[static_cast<size_t>(y_)].replace(
                        static_cast<size_t>(x_), static_cast<size_t>(count),
                        static_cast<size_t>(count), ' ');
            }
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
        // ConPTY resizes the presentation grid; the existing overlapping
        // cells are not all necessarily re-emitted as VT output. Clearing
        // the oracle here manufactures a blank screen that the actual
        // terminal has not displayed. Preserve the top-left overlap and
        // require the child to update the dimension-bearing status itself.
        std::vector<std::string> updated(
            static_cast<size_t>(rows),
            std::string(static_cast<size_t>(columns), ' '));
        const int kept_rows = std::min(rows_, rows);
        const int kept_cols = std::min(cols_, columns);
        for (int row = 0; row < kept_rows; ++row) {
            updated[static_cast<size_t>(row)].replace(
                0, static_cast<size_t>(kept_cols),
                cells_[static_cast<size_t>(row)], 0,
                static_cast<size_t>(kept_cols));
        }
        const bool full_region = scroll_top_ == 0 &&
                                 scroll_bottom_ == rows_ - 1;
        rows_ = rows;
        cols_ = columns;
        cells_.swap(updated);
        scroll_top_ = std::clamp(scroll_top_, 0, rows_ - 1);
        scroll_bottom_ = full_region ? rows_ - 1 :
            std::clamp(scroll_bottom_, scroll_top_, rows_ - 1);
        x_ = std::clamp(x_, 0, cols_ - 1);
        y_ = std::clamp(y_, 0, rows_ - 1);
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
                    newline(false);
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
    bool contains(const char* token) const {
        for (const auto& line : cells_) {
            if (line.find(token) != std::string::npos) return true;
        }
        return false;
    }
    bool header() const { return contains("FlexUI + Tango"); }
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
    unsigned auto_wraps_into_footer() const {
        return auto_wraps_into_footer_;
    }
    unsigned bottom_auto_wraps() const { return bottom_auto_wraps_; }
    unsigned scroll_region_commands() const {
        return scroll_region_commands_;
    }
    unsigned scroll_region_linefeeds() const {
        return scroll_region_linefeeds_;
    }
    std::string diagnostic() const {
        std::string preview;
        for (const auto& line : cells_) {
            if (line.find_first_not_of(' ') == std::string::npos) continue;
            if (!preview.empty()) preview += " / ";
            preview += line.substr(0, 78);
            if (preview.size() > 300) break;
        }
        return "size=" + std::to_string(cols_) + "x" +
               std::to_string(rows_) + ", cursor=" +
               std::to_string(x_) + "," + std::to_string(y_) +
               ", parser=" + std::to_string(static_cast<int>(parse_)) +
               ", csi='" + csi_ + "', wrap=" +
               std::to_string(wrap_enabled_ ? 1 : 0) +
               ", wrap-mode-changes=" + std::to_string(wrap_mode_changes_) +
               ", bottom-auto-wraps=" + std::to_string(bottom_auto_wraps_) +
               ", bottom-explicit-newlines=" +
               std::to_string(bottom_explicit_newlines_) +
               ", auto-wraps-all=" + std::to_string(all_auto_wraps_) +
               ", auto-wraps-into-footer=" +
               std::to_string(auto_wraps_into_footer_) +
               ", explicit-newlines-into-footer=" +
               std::to_string(explicit_newlines_into_footer_) +
               ", last-auto-wrap-row=" +
               std::to_string(last_auto_wrap_row_) +
               ", full-screen-erases=" + std::to_string(screen_erase_full_) +
               ", margin-top=" + std::to_string(scroll_top_) +
               ", margin-bottom=" + std::to_string(scroll_bottom_) +
               ", margin-commands=" + std::to_string(scroll_region_commands_) +
               ", margin-linefeeds=" + std::to_string(scroll_region_linefeeds_) +
               ", lower='" + cells_[static_cast<size_t>(rows_ - 2)] +
               "', bottom='" + cells_[static_cast<size_t>(rows_ - 1)] +
               "', entered-alt=" + std::to_string(entered_alt_ ? 1 : 0) +
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
    std::string recent_vt_;
    std::atomic<bool> output_limit_hit_{false};
    std::atomic<bool> reader_done_{false};
    std::atomic<DWORD> reader_error_{0};
    std::string phase_ = "initialize";
    std::string probe_report_path_;
    std::string frame_report_path_;

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
            recent_vt_.append(chunk, static_cast<size_t>(read));
            if (recent_vt_.size() > 2048)
                recent_vt_.erase(0, recent_vt_.size() - 2048);
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
        if (!probe_report_path_.empty()) (void)DeleteFileA(
            probe_report_path_.c_str());
        if (!frame_report_path_.empty()) (void)DeleteFileA(
            frame_report_path_.c_str());
    }
    std::string last_phase() const { return phase_; }
    std::string diagnostic() {
        std::lock_guard<std::mutex> guard(mutex_);
        std::string independent_report;
        if (!probe_report_path_.empty()) {
            HANDLE f = CreateFileA(probe_report_path_.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f != INVALID_HANDLE_VALUE) {
                char content[512]{};
                DWORD got = 0;
                if (ReadFile(f, content, sizeof(content) - 1, &got, nullptr))
                    independent_report.assign(content, got);
                CloseHandle(f);
            }
        }
        std::string owner_cell_frames;
        if (!frame_report_path_.empty()) {
            HANDLE report = CreateFileA(frame_report_path_.c_str(),
                GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (report != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER file_size{};
                if (GetFileSizeEx(report, &file_size)) {
                    if (file_size.QuadPart > 1800)
                        (void)SetFilePointer(report, -1800, nullptr, FILE_END);
                    char latest[2048]{};
                    DWORD got = 0;
                    if (ReadFile(report, latest, sizeof(latest) - 1,
                                 &got, nullptr))
                        owner_cell_frames.assign(latest, got);
                }
                CloseHandle(report);
                std::replace(owner_cell_frames.begin(),
                             owner_cell_frames.end(), '\n', ';');
            }
        }
        DWORD child_exit = 999;
        if (process_.hProcess) (void)GetExitCodeProcess(
            process_.hProcess, &child_exit);
        std::string escaped_vt;
        const size_t start = recent_vt_.size() > 512 ?
            recent_vt_.size() - 512 : 0;
        for (size_t i = start; i < recent_vt_.size(); ++i) {
            const unsigned char ch =
                static_cast<unsigned char>(recent_vt_[i]);
            if (ch >= 0x20 && ch < 0x7f && ch != '\\' && ch != '\'') {
                escaped_vt.push_back(static_cast<char>(ch));
            } else {
                char hex[8]{};
                (void)std::snprintf(hex, sizeof(hex), "\\x%02X", ch);
                escaped_vt += hex;
            }
        }
        return display_.diagnostic() + ", recent_vt='" + escaped_vt +
            "', output_bytes=" +
            std::to_string(output_count_) + ", reader_done=" +
            std::to_string(reader_done_ ? 1 : 0) + ", reader_error=" +
            std::to_string(reader_error_.load()) + ", child_exit=" +
            std::to_string(child_exit) + ", output_limit=" +
            std::to_string(output_limit_hit_ ? 1 : 0) +
            ", child_console_info='" + independent_report +
            "', owner_cell_frames='" + owner_cell_frames + "'";
    }
    bool setup(const char* executable, bool probe = false,
               bool frame_diagnostic = true) {
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
        // For PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, lpValue is the HPCON
        // value itself (not the address of the variable holding HPCON).
        // Match Microsoft's EchoCon sample and node-pty's ConPTY launcher.
        const BOOL attr_ok = UpdateProcThreadAttribute(attribute_list, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            pseudo_, sizeof(HPCON), nullptr, nullptr);
        if (!probe && frame_diagnostic) {
            char directory[MAX_PATH]{};
            char filename[MAX_PATH]{};
            const DWORD path_size = GetTempPathA(MAX_PATH, directory);
            if (path_size == 0 || path_size >= MAX_PATH ||
                !GetTempFileNameA(directory, "STF", 0, filename))
                return false;
            frame_report_path_ = filename;
        }
        if (probe) {
            char directory[MAX_PATH]{};
            char filename[MAX_PATH]{};
            const DWORD path_size = GetTempPathA(MAX_PATH, directory);
            if (path_size == 0 || path_size >= MAX_PATH ||
                !GetTempFileNameA(directory, "STP", 0, filename))
                return false;
            probe_report_path_ = filename;
        }
        const auto wide = utf8_to_wide(executable);
        BOOL started = FALSE;
        if (attr_ok && !wide.empty()) {
            std::wstring command = L"\"" + wide + L"\"";
            if (probe) {
                command += L" --probe-child \"";
                command += utf8_to_wide(probe_report_path_.c_str());
                command += L"\"";
            } else if (frame_diagnostic) {
                command += L" --frame-report \"";
                command += utf8_to_wide(frame_report_path_.c_str());
                command += L"\"";
            }
            std::vector<wchar_t> cmd(command.begin(), command.end());
            cmd.push_back(L'\0');
            STARTUPINFOEXW startup{};
            startup.StartupInfo.cb = sizeof(startup);
            startup.lpAttributeList = attribute_list;

            // Console children spawned from redirected CI/CTest processes
            // can be given duplicated parent std handles by Windows even
            // when bInheritHandles is FALSE. STARTF_USESTDHANDLES with
            // explicitly null handles prevents that implicit duplication,
            // allowing ConPTY to create its own CONIN$/CONOUT$ handles.
            // Unlike temporarily changing GetStdHandle, this is isolated to
            // this CreateProcessW and safe around the output reader thread.
            startup.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
            startup.StartupInfo.hStdInput = nullptr;
            startup.StartupInfo.hStdOutput = nullptr;
            startup.StartupInfo.hStdError = nullptr;
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
    bool run_probe(const char* executable) {
        if (!setup(executable, true)) return false;
        if (!await("independent ConPTY console and raw file output", 6000,
            [](const Screen& s) {
                return s.contains("STUN_CONPTY_HOST_PROBE") &&
                       s.contains("STUN_CONPTY_FILE_PROBE");
            }, true)) return false;
        if (WaitForSingleObject(process_.hProcess, 2000) != WAIT_OBJECT_0)
            return false;
        DWORD code = 999;
        return GetExitCodeProcess(process_.hProcess, &code) && code == 0;
    }
    bool run(const char *executable, bool frame_diagnostic = true) {
        if (!setup(executable, false, frame_diagnostic)) return false;
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
        // Do not classify the ConPTY integration as stable after only the
        // first six streamed messages. The same unmodified demo must finish
        // all 48 messages while the host remains at 52x16, and the terminal
        // must STILL show both fixed chrome and the settled progress marker.
        // This catches long-session scrolling, stale diff and render races.
        if (!await("long-session 48th message settled and visible at 52x16",
                   30000, [](const Screen &s) {
                       return s.header() && s.mode("tail", 52, 16) &&
                              s.count() == 12u &&
                              s.status().find(" | settled=48 |") !=
                                  std::string::npos;
                   })) return false;
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
    if (argc == 2 && std::strcmp(argv[1], "--oracle-selftest") == 0) {
        Screen screen;
        auto feed = [&](const std::string& bytes) {
            screen.feed(bytes.data(), bytes.size());
        };
        feed("\x1b[?1049h\x1b[1;1HOLD-BROKEN-HEADER");
        feed("\x1b[1;1H\x1b[32X\x1b[1;1HFlexUI + Tango");
        feed("\x1b[24;1HRetained 6/12 | tail | 80x24 |");
        if (!screen.header() || !screen.mode("tail", 80, 24) ||
            screen.count() != 6u) return 31;
        screen.resize(52, 16);
        feed("\x1b[16;1H\x1b[52X\x1b[16;1HRetained 6/12 | tail | 52x16 |");
        if (!screen.header() || !screen.mode("tail", 52, 16) ||
            screen.count() != 6u) return 32;
        // Autowrap-off at bottom-right and scrollback erasure are separate
        // from viewport erasure. Neither may evict the fixed header/footer.
        Screen no_wrap;
        const std::string setup =
            "\x1b[?1049h\x1b[?7l\x1b[1;1HFlexUI + Tango"
            "\x1b[24;1HRetained 12/12 | tail | 80x24 | settled=48 |";
        no_wrap.feed(setup.data(), setup.size());
        const std::string edge = "\x1b[24;80HXY\x1b[3J";
        no_wrap.feed(edge.data(), edge.size());
        if (!no_wrap.header() || !no_wrap.mode("tail", 80, 24) ||
            no_wrap.count() != 12u) return 33;
        // A last-column PRINT on the row before the footer (with DECAWM
        // enabled) can overwrite the footer without reaching the bottom
        // row, so bottom-only scroll counters are insufficient.
        Screen wrap_near_footer;
        const std::string sequence =
            "\x1b[?1049h\x1b[22;80Hxy";
        wrap_near_footer.feed(sequence.data(), sequence.size());
        if (wrap_near_footer.auto_wraps_into_footer() != 1u ||
            wrap_near_footer.bottom_auto_wraps() != 0u) return 34;
        Screen protected_footer;
        const std::string protected_sequence =
            "\x1b[?1049h\x1b[1;1HFlexUI + Tango"
            "\x1b[23;1HRetained 12/12 | tail | 80x24 |"
            "\x1b[2;22r\x1b[22;1Hbody\r\nnext body line";
        protected_footer.feed(
            protected_sequence.data(), protected_sequence.size());
        if (!protected_footer.header() ||
            !protected_footer.mode("tail", 80, 24) ||
            protected_footer.scroll_region_commands() != 1u ||
            protected_footer.scroll_region_linefeeds() != 1u)
            return 35;
        std::puts("Windows ConPTY VT ECH/autowrap/scroll-region oracle passed");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--probe-child") == 0) {
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (!output || output == INVALID_HANDLE_VALUE ||
            !GetConsoleMode(output, &mode)) return 21;
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (!GetConsoleScreenBufferInfo(output, &info)) return 23;
        const int width = info.srWindow.Right - info.srWindow.Left + 1;
        const int height = info.srWindow.Bottom - info.srWindow.Top + 1;
        DWORD console_pids[64]{};
        const DWORD nconsole = GetConsoleProcessList(console_pids, 64);
        const HANDLE f = CreateFileA(argv[2], GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            char report[256]{};
            const int count = std::snprintf(report, sizeof(report),
                "observed_window=%dx%d buffer=%dx%d mode=%lu console_pids=%lu pid=%lu",
                width, height, static_cast<int>(info.dwSize.X),
                static_cast<int>(info.dwSize.Y),
                static_cast<unsigned long>(mode),
                static_cast<unsigned long>(nconsole),
                static_cast<unsigned long>(GetCurrentProcessId()));
            if (count > 0 && count < static_cast<int>(sizeof(report))) {
                DWORD wrote = 0;
                (void)WriteFile(f, report, static_cast<DWORD>(count),
                                &wrote, nullptr);
            }
            CloseHandle(f);
        }
        // Test the two actual output paths *before* rejecting mismatched
        // geometry. This tells whether the child has another console or the
        // requested ConPTY simply reports a different window rectangle.
        static constexpr char marker[] = "STUN_CONPTY_HOST_PROBE\r\n";
        DWORD written = 0;
        if (!WriteConsoleA(output, marker, sizeof(marker) - 1u,
                           &written, nullptr) ||
            written != sizeof(marker) - 1u) return 22;
        static constexpr char raw[] = "STUN_CONPTY_FILE_PROBE\r\n";
        written = 0;
        if (!WriteFile(output, raw, sizeof(raw) - 1u,
                       &written, nullptr) ||
            written != sizeof(raw) - 1u) return 25;
        return width == 80 && height == 24 ? 0 : 24;
    }
    if (argc == 2 && std::strcmp(argv[1], "--selftest") == 0) {
        ConptyAcceptance probe;
        if (!probe.run_probe(argv[0])) {
            std::fprintf(stderr, "Independent ConPTY host probe FAILED: %s, "
                "phase='%s'\n", probe.diagnostic().c_str(),
                probe.last_phase().c_str());
            return 6;
        }
        std::puts("Independent ConPTY host transport passed");
        return 0;
    }
    // The plain test runs the exact ordinary demo invocation, with no
    // diagnostic file writes that could mask owner/ConPTY scheduling bugs.
    // The separate default mode retains bounded owner-cell evidence for
    // diagnosing a failure. Both enforce the same real host resize and
    // onscreen header/footer restoration predicates.
    const bool plain = argc == 3 &&
        std::strcmp(argv[1], "--plain") == 0;
    if (argc != 2 && !plain) return 2;
    ConptyAcceptance harness;
    if (!harness.run(argv[plain ? 2 : 1], !plain)) {
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
