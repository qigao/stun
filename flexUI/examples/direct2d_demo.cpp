#ifdef _WIN32

#include <windows.h>
#include <windowsx.h>

#include <d2d1.h>
#include <wrl/client.h>

#include <backends/d2d/init.h>
#include "host_media_bridge.h"
#include "win32_ime_helper.h"
#include <flexUI/host_bridge.h>
#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/progressbar_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include "renderer_capability_label.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

namespace {

constexpr const char* kCss = R"(
#root {
    width: 100%;
    height: 100%;
    display: flex;
    flex-direction: column;
    gap: 14px;
    padding: 24px;
    background-color: #101418;
}

#hero {
    display: flex;
    flex-direction: column;
    gap: 8px;
    padding: 20px;
    background-color: #172026;
    border-radius: 10px;
}

#title { font-size: 26px; color: #d8f3ff; }
#subtitle { font-size: 13px; color: #8fb8c9; }
#backend-note { font-size: 12px; color: #6f94a2; }

#controls {
    display: flex;
    flex-direction: row;
    gap: 10px;
    align-items: center;
}

#search { width: 260px; height: 36px; }
#editor { width: 100%; height: 110px; }
#progress { width: 100%; height: 18px; }

.btn-ghost {
    background-color: #1f313a;
    color: #d8f3ff;
}
)";

bool load_demo_fonts() {
    bool ok = false;
    ok |= flex::d2d_backend::load_font("Arial", "C:/Windows/Fonts/arial.ttf");
    ok |= flex::d2d_backend::load_font("Consolas", "C:/Windows/Fonts/consola.ttf");
    ok |= flex::d2d_backend::load_font("Segoe UI", "C:/Windows/Fonts/segoeui.ttf");
    return ok;
}

flexUI::KeyCode vk_to_keycode(WPARAM vk) {
    switch (vk) {
        case VK_LEFT: return flexUI::KeyCode::Left;
        case VK_RIGHT: return flexUI::KeyCode::Right;
        case VK_UP: return flexUI::KeyCode::Up;
        case VK_DOWN: return flexUI::KeyCode::Down;
        case VK_HOME: return flexUI::KeyCode::Home;
        case VK_END: return flexUI::KeyCode::End;
        case VK_PRIOR: return flexUI::KeyCode::PageUp;
        case VK_NEXT: return flexUI::KeyCode::PageDown;
        case VK_BACK: return flexUI::KeyCode::Backspace;
        case VK_DELETE: return flexUI::KeyCode::Delete;
        case VK_RETURN: return flexUI::KeyCode::Enter;
        case VK_TAB: return flexUI::KeyCode::Tab;
        case VK_ESCAPE: return flexUI::KeyCode::Escape;
        case VK_SPACE: return flexUI::KeyCode::Space;
        default:
            if (vk >= 'A' && vk <= 'Z') return static_cast<flexUI::KeyCode>(vk);
            if (vk >= '0' && vk <= '9') return static_cast<flexUI::KeyCode>(vk);
            return flexUI::KeyCode::Unknown;
    }
}

int current_mods() {
    int result = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) result |= static_cast<int>(flexUI::KeyMod::Shift);
    if (GetKeyState(VK_CONTROL) & 0x8000) result |= static_cast<int>(flexUI::KeyMod::Control);
    if (GetKeyState(VK_MENU) & 0x8000) result |= static_cast<int>(flexUI::KeyMod::Alt);
    return result;
}

std::string utf8_from_codepoint(unsigned int codepoint) {
    std::string utf8;
    if (codepoint <= 0x7F) {
        utf8.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        utf8.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        utf8.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        utf8.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return utf8;
}

struct DemoApp {
    HWND hwnd = nullptr;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> render_target;
    std::unique_ptr<flex::Renderer> renderer;
    std::unique_ptr<flexUI::Box> box;
    flexUI::Element* progress_elem = nullptr;
    flexUI::Element* status_elem = nullptr;
    flexUI::LabelWidget* status_label = nullptr;
    float progress = 18.0f;
    std::chrono::steady_clock::time_point last_frame = std::chrono::steady_clock::now();
    wchar_t pending_high_surrogate = 0;
    UINT dpi = USER_DEFAULT_SCREEN_DPI;
    float dpi_scale = 1.0f;
    std::string current_cursor = "default";
    std::string current_color_scheme = "normal";
    HCURSOR native_cursor = nullptr;
};

UINT current_window_dpi(HWND hwnd) {
    if (!hwnd) return USER_DEFAULT_SCREEN_DPI;
    return GetDpiForWindow(hwnd);
}

void sync_native_capture(DemoApp& app) {
    if (!app.box || !app.hwnd) return;
    flexui_examples::sync_mouse_capture(
        app.box.get(),
        GetCapture() == app.hwnd,
        [&app]() { SetCapture(app.hwnd); },
        []() { ReleaseCapture(); });
}

HCURSOR load_native_cursor(const std::string& cursor_name) {
    if (cursor_name == "text" || cursor_name == "vertical-text") {
        return LoadCursor(nullptr, IDC_IBEAM);
    }
    if (cursor_name == "pointer") {
        return LoadCursor(nullptr, IDC_HAND);
    }
    if (cursor_name == "crosshair") {
        return LoadCursor(nullptr, IDC_CROSS);
    }
    if (cursor_name == "ew-resize" || cursor_name == "col-resize" ||
        cursor_name == "e-resize" || cursor_name == "w-resize") {
        return LoadCursor(nullptr, IDC_SIZEWE);
    }
    if (cursor_name == "ns-resize" || cursor_name == "row-resize" ||
        cursor_name == "n-resize" || cursor_name == "s-resize") {
        return LoadCursor(nullptr, IDC_SIZENS);
    }
    return LoadCursor(nullptr, IDC_ARROW);
}

void sync_native_cursor(DemoApp& app) {
    if (!app.hwnd) return;
    flexUI::host::sync_cursor(app.box.get(), app.current_cursor,
                              [&app](const std::string& desired) {
                                  app.current_cursor = desired;
                                  app.native_cursor = load_native_cursor(desired);
                              });
    SetCursor(app.native_cursor ? app.native_cursor : LoadCursor(nullptr, IDC_ARROW));
}

DemoApp* app_from_window(HWND hwnd) {
    return reinterpret_cast<DemoApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

void build_ui(DemoApp& app) {
    app.box = std::make_unique<flexUI::Box>(app.renderer.get());
    flexui_examples::media::sync_environment(app.box.get());
    RECT client{};
    GetClientRect(app.hwnd, &client);
    app.box->set_viewport(static_cast<float>(client.right - client.left),
                          static_cast<float>(client.bottom - client.top));
    app.box->load_css(kCss);

    auto* root = app.box->create("div", "root");
    app.box->set_root(root);

    auto* hero = app.box->create("div", "hero");
    root->append(hero);
    hero->append(app.box->create_widget<flexUI::LabelWidget>("div", "title", "flexUI on Direct2D"));
    hero->append(app.box->create_widget<flexUI::LabelWidget>(
        "div", "subtitle", "Minimal Win32 + Direct2D host with flexUI input, pointer and progress."));
    hero->append(app.box->create_widget<flexUI::LabelWidget>(
        "div", "backend-note",
        std::string("Direct2D ") + flexui_examples::renderer_capability_label(app.box->renderer_capabilities())));

    auto* controls = app.box->create("div", "controls");
    root->append(controls);

    auto* button = app.box->create_widget<flexUI::ButtonWidget>("button", "", "Trigger");
    controls->append(button);

    auto* ghost = app.box->create_widget<flexUI::ButtonWidget>("button", "", "Secondary");
    ghost->add_class("btn-ghost");
    controls->append(ghost);

    auto* search = app.box->create_widget<flexUI::InputWidget>("input", "search");
    static_cast<flexUI::InputWidget*>(search->widget)->set_placeholder("Type in Direct2D...");
    controls->append(search);

    app.status_elem = app.box->create_widget<flexUI::LabelWidget>("div", "", "Ready");
    app.status_label = static_cast<flexUI::LabelWidget*>(app.status_elem->widget);
    controls->append(app.status_elem);

    button->on_click([&app]() {
        app.status_label->set_text("Action fired");
        app.status_elem->mark_paint_dirty();
    });

    auto* editor = app.box->create_widget<flexUI::TextAreaWidget>("div", "editor");
    static_cast<flexUI::TextAreaWidget*>(editor->widget)->set_placeholder("Textarea on Direct2D...");
    root->append(editor);

    app.progress_elem = app.box->create_widget<flexUI::ProgressBarWidget>("div", "progress");
    static_cast<flexUI::ProgressBarWidget*>(app.progress_elem->widget)->set_value(app.progress);
    root->append(app.progress_elem);
    app.box->update();
    flexui_examples::media::sync_window_color_scheme(app.hwnd, app.box.get(),
                                                     app.current_color_scheme);
}

HRESULT ensure_render_target(DemoApp& app) {
    if (app.render_target) return S_OK;

    RECT client{};
    GetClientRect(app.hwnd, &client);
    const UINT width = static_cast<UINT>(std::max<LONG>(client.right - client.left, 0));
    const UINT height = static_cast<UINT>(std::max<LONG>(client.bottom - client.top, 0));
    if (width == 0 || height == 0) return S_FALSE;

    flex::d2d_backend::init();
    if (!flex::d2d_backend::g_d2d_factory) return E_FAIL;

    HRESULT hr = flex::d2d_backend::g_d2d_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN),
            static_cast<float>(app.dpi),
            static_cast<float>(app.dpi)),
        D2D1::HwndRenderTargetProperties(app.hwnd, D2D1::SizeU(width, height)),
        app.render_target.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;

    app.renderer = flex::d2d_backend::create_renderer(app.render_target.Get());
    if (!app.renderer) {
        app.render_target.Reset();
        return E_FAIL;
    }

    build_ui(app);
    return S_OK;
}

void discard_render_target(DemoApp& app) {
    app.box.reset();
    app.renderer.reset();
    app.render_target.Reset();
}

void resize_render_target(DemoApp& app) {
    if (!app.render_target) return;
    RECT client{};
    GetClientRect(app.hwnd, &client);
    const UINT width = static_cast<UINT>(std::max<LONG>(client.right - client.left, 0));
    const UINT height = static_cast<UINT>(std::max<LONG>(client.bottom - client.top, 0));
    if (width == 0 || height == 0) return;
    app.render_target->SetDpi(static_cast<float>(app.dpi), static_cast<float>(app.dpi));
    if (FAILED(app.render_target->Resize(D2D1::SizeU(width, height)))) {
        discard_render_target(app);
        return;
    }
    if (app.box) {
        app.box->set_viewport(static_cast<float>(width), static_cast<float>(height));
        app.box->invalidate();
    }
}

void render_frame(DemoApp& app) {
    if (FAILED(ensure_render_target(app)) || !app.renderer || !app.box) return;
    flexui_examples::media::sync_environment(app.box.get());

    const auto now = std::chrono::steady_clock::now();
    const float dt = std::chrono::duration<float>(now - app.last_frame).count();
    app.last_frame = now;

    app.progress += dt * 20.0f;
    if (app.progress > 100.0f) app.progress = 0.0f;
    if (app.progress_elem && app.progress_elem->widget) {
        static_cast<flexUI::ProgressBarWidget*>(app.progress_elem->widget)->set_value(app.progress);
        app.progress_elem->mark_paint_dirty();
    }

    app.box->update_time(dt * 1000.0f);
    app.box->update();
    sync_native_cursor(app);
    flexui_examples::media::sync_window_color_scheme(app.hwnd, app.box.get(),
                                                     app.current_color_scheme);
    flexui_examples::win32_ime::sync_ime_caret(app.hwnd, app.box.get(), app.dpi_scale);

    if (app.renderer->requires_surface_recreation()) {
        app.renderer->acknowledge_surface_recreation();
        discard_render_target(app);
    }
}

void dispatch_mouse_move(DemoApp& app, LPARAM lp) {
    if (!app.box) return;
    auto event = flexUI::Event::mouse_move(static_cast<float>(GET_X_LPARAM(lp)),
                                           static_cast<float>(GET_Y_LPARAM(lp)));
    app.box->dispatch_event(event);
    sync_native_cursor(app);
}

void dispatch_mouse_button(DemoApp& app, UINT message, LPARAM lp) {
    if (!app.box) return;
    const float x = static_cast<float>(GET_X_LPARAM(lp));
    const float y = static_cast<float>(GET_Y_LPARAM(lp));
    flexUI::MouseButton button = flexUI::MouseButton::Left;
    if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) button = flexUI::MouseButton::Right;
    if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) button = flexUI::MouseButton::Middle;
    auto event = (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
        ? flexUI::Event::mouse_down(x, y, button)
        : flexUI::Event::mouse_up(x, y, button);
    app.box->dispatch_event(event);
    sync_native_capture(app);
    sync_native_cursor(app);
}

LRESULT CALLBACK demo_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    DemoApp* app = app_from_window(hwnd);
    switch (msg) {
        case WM_SIZE:
            if (app) resize_render_target(*app);
            return 0;
        case WM_DPICHANGED:
            if (app) {
                app->dpi = HIWORD(wp);
                app->dpi_scale = static_cast<float>(app->dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
                if (const auto* suggested = reinterpret_cast<RECT*>(lp)) {
                    SetWindowPos(hwnd, nullptr,
                                 suggested->left, suggested->top,
                                 suggested->right - suggested->left,
                                 suggested->bottom - suggested->top,
                                 SWP_NOZORDER | SWP_NOACTIVATE);
                }
                resize_render_target(*app);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (app) dispatch_mouse_move(*app, lp);
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            if (app) dispatch_mouse_button(*app, msg, lp);
            return 0;
        case WM_MOUSEWHEEL:
            if (app && app->box) {
                POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                ScreenToClient(hwnd, &pt);
                const float dy = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / static_cast<float>(WHEEL_DELTA);
                auto event = flexUI::Event::mouse_wheel(static_cast<float>(pt.x), static_cast<float>(pt.y), 0.0f, dy);
                app->box->dispatch_event(event);
                sync_native_capture(*app);
                sync_native_cursor(*app);
            }
            return 0;
        case WM_KEYDOWN:
            if (app && app->box) {
                auto event = flexUI::Event::key_down(vk_to_keycode(wp), current_mods());
                app->box->dispatch_event(event);
                sync_native_cursor(*app);
            }
            return 0;
        case WM_KEYUP:
            if (app && app->box) {
                auto event = flexUI::Event::key_up(vk_to_keycode(wp), current_mods());
                app->box->dispatch_event(event);
                sync_native_cursor(*app);
            }
            return 0;
        case WM_IME_SETCONTEXT:
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_COMPOSITION:
        case WM_IME_ENDCOMPOSITION:
            if (flexui_examples::win32_ime::handle_ime_for_box(
                    hwnd,
                    msg,
                    lp,
                    app ? app->box.get() : nullptr,
                    app ? app->dpi_scale : 1.0f)) {
                return 0;
            }
            break;
        case WM_CHAR:
            if (app && app->box) {
                const wchar_t ch = static_cast<wchar_t>(wp);
                if (ch >= 0xD800 && ch <= 0xDBFF) {
                    app->pending_high_surrogate = ch;
                    return 0;
                }

                unsigned int codepoint = static_cast<unsigned int>(ch);
                if (ch >= 0xDC00 && ch <= 0xDFFF &&
                    app->pending_high_surrogate >= 0xD800 &&
                    app->pending_high_surrogate <= 0xDBFF) {
                    codepoint =
                        0x10000u +
                        ((static_cast<unsigned int>(app->pending_high_surrogate) - 0xD800u) << 10) +
                        (static_cast<unsigned int>(ch) - 0xDC00u);
                }

                app->pending_high_surrogate = 0;
                if (codepoint >= 32) {
                    flexui_examples::dispatch_text_input_if_focused(
                        app->box.get(), utf8_from_codepoint(codepoint));
                }
            }
            return 0;
        case WM_KILLFOCUS:
            if (app && app->box) {
                flexui_examples::clear_focus_and_capture(app->box.get());
                sync_native_capture(*app);
                sync_native_cursor(*app);
            }
            return 0;
        case WM_CAPTURECHANGED:
            if (app && app->box && reinterpret_cast<HWND>(lp) != hwnd) {
                flexui_examples::clear_mouse_capture(app->box.get());
                sync_native_cursor(*app);
            }
            return 0;
        case WM_SETCURSOR:
            if (app && LOWORD(lp) == HTCLIENT) {
                sync_native_cursor(*app);
                return TRUE;
            }
            break;
        case WM_PAINT:
            if (app) {
                PAINTSTRUCT ps{};
                BeginPaint(hwnd, &ps);
                render_frame(*app);
                EndPaint(hwnd, &ps);
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

} // namespace

int main() {
    flex::d2d_backend::init();
    flex::d2d_backend::register_backend();
    load_demo_fonts();

    HINSTANCE hinstance = GetModuleHandle(nullptr);
    const wchar_t* class_name = L"FlexUIDirect2DDemoWindow";
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = demo_wnd_proc;
    window_class.hInstance = hinstance;
    window_class.lpszClassName = class_name;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&window_class);

    HWND hwnd = CreateWindowExW(0, class_name, L"flexUI Direct2D Demo",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                1100, 760, nullptr, nullptr, hinstance, nullptr);
    if (!hwnd) {
        std::cerr << "Window creation failed\n";
        return 1;
    }

    DemoApp app{};
    app.hwnd = hwnd;
    app.dpi = current_window_dpi(hwnd);
    app.dpi_scale = static_cast<float>(app.dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&app));

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    app.last_frame = std::chrono::steady_clock::now();

    MSG msg{};
    for (;;) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                discard_render_target(app);
                flex::d2d_backend::shutdown();
                return static_cast<int>(msg.wParam);
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        render_frame(app);
        Sleep(16);
    }
}

#else

#include <iostream>

int main() {
    std::cerr << "Direct2D demo is only available on Windows.\n";
    return 1;
}

#endif
