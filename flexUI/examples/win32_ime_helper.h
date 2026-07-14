#pragma once

#include "host_input_bridge.h"

#include <string>
#ifdef _WIN32
#include <cstring>
#include <vector>
#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif

namespace flexui_examples::win32_ime {

#ifdef _WIN32

inline void enable_ime(HWND hwnd) {
    if (!hwnd) return;

    HIMC himc = ImmGetContext(hwnd);
    if (!himc) {
        himc = ImmCreateContext();
        ImmAssociateContext(hwnd, himc);
    } else {
        ImmReleaseContext(hwnd, himc);
    }
}

inline WNDPROC subclass_window(HWND hwnd, void* user_data, WNDPROC wnd_proc) {
    if (!hwnd || !wnd_proc) return nullptr;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(user_data));
    return reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wnd_proc)));
}

inline void restore_window_proc(HWND hwnd, WNDPROC original_wnd_proc) {
    if (!hwnd || !original_wnd_proc) return;
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_wnd_proc));
}

inline std::string utf8_from_wide(const wchar_t* text, int len) {
    if (!text || len <= 0) return {};

    const int utf8_len = WideCharToMultiByte(CP_UTF8, 0, text, len, nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 0) return {};

    std::string utf8(static_cast<size_t>(utf8_len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, len, utf8.data(), utf8_len, nullptr, nullptr);
    return utf8;
}

inline void update_ime_position(HWND hwnd, float content_scale_y, int x, int y) {
    if (!hwnd) return;

    HIMC himc = ImmGetContext(hwnd);
    if (!himc) return;

    COMPOSITIONFORM cf{};
    cf.dwStyle = CFS_POINT;
    cf.ptCurrentPos.x = x;
    cf.ptCurrentPos.y = y;
    ImmSetCompositionWindow(himc, &cf);

    CANDIDATEFORM caf{};
    caf.dwIndex = 0;
    caf.dwStyle = CFS_CANDIDATEPOS;
    caf.ptCurrentPos.x = x;
    caf.ptCurrentPos.y = y;
    ImmSetCandidateWindow(himc, &caf);

    LOGFONTA lf{};
    lf.lfHeight = static_cast<LONG>(-20.0f * content_scale_y);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    strcpy_s(lf.lfFaceName, "Microsoft YaHei");
    ImmSetCompositionFontA(himc, &lf);

    ImmReleaseContext(hwnd, himc);
}

inline void sync_ime_caret(HWND hwnd, flexUI::Box* box, float content_scale_y) {
    if (!hwnd || !box) return;

    float x = 0.0f;
    float y = 0.0f;
    if (!flexui_examples::focused_text_input_caret_anchor(box, x, y)) return;

    update_ime_position(hwnd, content_scale_y, static_cast<int>(x), static_cast<int>(y));
}

template <typename StartFn, typename UpdateFn, typename EndFn>
inline bool handle_ime_message(HWND hwnd,
                               UINT msg,
                               LPARAM& lp,
                               StartFn&& on_composition_start,
                               UpdateFn&& on_composition_update,
                               EndFn&& on_composition_end) {
    switch (msg) {
        case WM_IME_SETCONTEXT:
            lp |= ISC_SHOWUIALL;
            return false;

        case WM_IME_STARTCOMPOSITION: {
            HIMC himc = ImmGetContext(hwnd);
            if (himc) {
                ImmSetOpenStatus(himc, TRUE);
                ImmReleaseContext(hwnd, himc);
            }
            on_composition_start();
            return true;
        }

        case WM_IME_COMPOSITION: {
            HIMC himc = ImmGetContext(hwnd);
            if (himc) {
                if (lp & GCS_COMPSTR) {
                    const int len = ImmGetCompositionStringW(himc, GCS_COMPSTR, nullptr, 0);
                    if (len > 0) {
                        std::vector<wchar_t> buf(static_cast<size_t>(len / sizeof(wchar_t)) + 1, 0);
                        ImmGetCompositionStringW(himc, GCS_COMPSTR, buf.data(), len);
                        on_composition_update(
                            utf8_from_wide(buf.data(), len / static_cast<int>(sizeof(wchar_t))));
                    } else {
                        on_composition_update("");
                    }
                }
                ImmReleaseContext(hwnd, himc);
            }
            return true;
        }

        case WM_IME_ENDCOMPOSITION:
            on_composition_end();
            return true;

        default:
            return false;
    }
}

inline bool handle_ime_for_box(HWND hwnd,
                               UINT msg,
                               LPARAM& lp,
                               flexUI::Box* box,
                               float content_scale_y) {
    return handle_ime_message(
        hwnd,
        msg,
        lp,
        [hwnd, box, content_scale_y]() {
            flexui_examples::dispatch_composition_start_if_focused(box);
            sync_ime_caret(hwnd, box, content_scale_y);
        },
        [hwnd, box, content_scale_y](const std::string& text) {
            flexui_examples::dispatch_composition_update_if_focused(box, text);
            sync_ime_caret(hwnd, box, content_scale_y);
        },
        [box]() { flexui_examples::dispatch_composition_end_if_focused(box); });
}

#endif

} // namespace flexui_examples::win32_ime
