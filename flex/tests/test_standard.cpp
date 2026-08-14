/*
 * Flex standard trace tests
 *
 * Verifies a canonical render/layout/animation scene through a normalized
 * backend-neutral draw trace. The goal is to lock down observable runtime
 * behavior without depending on backend-specific pixels.
 */

#include "flex.h"
#if defined(FLEX_HAS_OPENGL)
#include "backends/opengl/init.h"
#include <glad/glad.h>
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "backends/d2d/init.h"
#endif
#include "backends/tui/init.h"
#include "tinytest.h"
#include "test_render_trace.h"
#include "tui.h"

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <d2d1.h>
#include <d2d1helper.h>
#include <wincodec.h>
#include <wrl/client.h>
#endif

using namespace flex;
using namespace flex::test_support;

namespace {

Scene::RawPtr build_standard_scene(ArenaAllocator& arena) {
    auto scene = Scene::create(320.0f, 160.0f, arena);

    auto row = Group::create(arena);
    row->set_id("row");
    row->set_position(10.0f, 20.0f);
    row->set_layout_width(140.0f);
    row->set_layout_height(40.0f);
    row->set_layout(LayoutMode::Flex);
    row->set_gap(8.0f);
    row->set_padding(4.0f);
    scene->add_child(row);

    auto first = Shape::create(arena);
    first->set_id("first");
    first->set_rect(20.0f, 10.0f);
    first->set_layout_width(20.0f);
    first->set_layout_height(10.0f);
    first->set_fill(Color{1.0f, 0.2f, 0.2f, 1.0f});
    row->add_child(first);

    auto second = Shape::create(arena);
    second->set_id("second");
    second->set_rect(30.0f, 12.0f);
    second->set_layout_width(30.0f);
    second->set_layout_height(12.0f);
    second->set_opacity(0.5f);
    second->set_fill(Color{0.2f, 0.8f, 0.4f, 1.0f});
    row->add_child(second);

    auto label = Text::create(arena);
    label->set_id("label");
    label->set_position(200.0f, 30.0f);
    label->set_content("Flex");
    label->set_font_family("sans-serif");
    label->set_font_size(16.0f);
    label->set_color(Color{1.0f, 1.0f, 1.0f, 1.0f});
    scene->add_child(label);

    auto marker = Shape::create(arena);
    marker->set_id("marker");
    marker->set_position(180.0f, 12.0f);
    marker->set_rect(10.0f, 10.0f);
    marker->set_layout_width(10.0f);
    marker->set_layout_height(10.0f);
    marker->set_fill(Color{0.2f, 0.4f, 1.0f, 1.0f});
    scene->add_child(marker);

    return scene;
}

Scene::RawPtr build_clip_scene(ArenaAllocator& arena) {
    auto scene = Scene::create(160.0f, 80.0f, arena);

    auto clipper = Group::create(arena);
    clipper->set_id("clipper");
    clipper->set_position(8.0f, 16.0f);
    clipper->set_layout_width(32.0f);
    clipper->set_layout_height(16.0f);
    clipper->set_clip(true);
    scene->add_child(clipper);

    auto inside = Shape::create(arena);
    inside->set_id("inside");
    inside->set_rect(16.0f, 16.0f);
    inside->set_fill(Color{1.0f, 0.2f, 0.2f, 1.0f});
    clipper->add_child(inside);

    auto outside = Shape::create(arena);
    outside->set_id("outside");
    outside->set_position(40.0f, 0.0f);
    outside->set_rect(16.0f, 16.0f);
    outside->set_fill(Color{0.2f, 0.8f, 0.4f, 1.0f});
    clipper->add_child(outside);

    return scene;
}

Scene::RawPtr build_text_opacity_scene(ArenaAllocator& arena) {
    auto scene = Scene::create(240.0f, 120.0f, arena);

    auto overlay = Group::create(arena);
    overlay->set_id("overlay");
    overlay->set_position(100.0f, 40.0f);
    overlay->set_opacity(0.5f);
    scene->add_child(overlay);

    auto label = Text::create(arena);
    label->set_id("overlayLabel");
    label->set_position(50.0f, 10.0f);
    label->set_content("ABCD");
    label->set_font_family("sans-serif");
    label->set_font_size(20.0f);
    label->set_text_align(TextAlign::Center);
    label->set_opacity(0.5f);
    label->set_color(Color{1.0f, 1.0f, 1.0f, 1.0f});
    overlay->add_child(label);

    return scene;
}

Scene::RawPtr build_svg_scene(ArenaAllocator& arena,
                              const char* source = "nanovg_badge.svg") {
    auto scene = Scene::create(200.0f, 120.0f, arena);

    auto icon = Svg::create(arena);
    icon->set_id("badge");
    icon->set_position(24.0f, 36.0f);
    icon->set_layout_size(80.0f, 60.0f);
    icon->set_opacity(0.5f);
    icon->set_src(source);
    scene->add_child(icon);

    return scene;
}

const char* inline_svg_markup() {
    return "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'><rect width='10' height='10' rx='2' fill='#ff6600'/></svg>";
}

const char* svg_scene_source() {
    return R"(
scene SvgTrace {
    width: 200
    height: 120

    svg badge {
        x: 24
        y: 36
        width: 80
        height: 60
        opacity: 0.5
        src: "nanovg_badge.svg"
    }

    svg inlineBadge {
        x: 120
        y: 18
        width: 40
        height: 30
        data: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'><rect width='10' height='10' rx='2' fill='#ff6600'/></svg>"
    }
}
)";
}

const char* standard_scene_source() {
    return R"(
scene StandardTrace {
    width: 320
    height: 160

    group row {
        x: 10
        y: 20
        width: 140
        height: 40
        layout: flex
        gap: 8
        padding: 4

        rect first {
            width: 20
            height: 10
            fill: #ff3333
        }

        rect second {
            width: 30
            height: 12
            opacity: 0.5
            fill: #33cc66
        }
    }

    text label {
        x: 200
        y: 30
        content: "Flex"
        fontSize: 16
        fontFamily: "sans-serif"
        color: #ffffff
    }

    rect marker {
        x: 180
        y: 12
        width: 10
        height: 10
        fill: #3366ff
    }
}

anim "markerMove" {
    duration: 1
    track "#marker/x" {
        keyframe 0 -> 180
        keyframe 1 -> 220
    }
}
)";
}

std::vector<std::string> expected_standard_trace(float marker_x = 180.0f) {
    return {
        "draw_rect 14.000 24.000 20.000 10.000 alpha=1.000",
        "draw_rect 42.000 24.000 30.000 12.000 alpha=0.500",
        "draw_text 200.000 30.000 16.000 alpha=1.000 text=Flex|sans-serif",
        "draw_rect " + format_trace_float(marker_x) + " 12.000 10.000 10.000 alpha=1.000",
    };
}

std::vector<std::string> expected_clip_trace() {
    return {
        "clip_rect 8.000 16.000 32.000 16.000",
        "draw_rect 8.000 16.000 16.000 16.000 alpha=1.000",
        "draw_rect 48.000 16.000 16.000 16.000 alpha=1.000",
    };
}

std::vector<std::string> expected_text_opacity_trace() {
    return {
        "draw_text 128.000 50.000 20.000 alpha=0.250 text=ABCD|sans-serif",
    };
}

std::vector<std::string> expected_svg_trace() {
    return {
        "draw_svg 24.000 36.000 80.000 60.000 alpha=0.500 svg=nanovg_badge.svg",
        std::string("draw_svg_data 120.000 18.000 40.000 30.000 alpha=1.000 svg=") + inline_svg_markup(),
    };
}

std::vector<std::string> render_trace(Scene::RawPtr scene) {
    RecordingRenderer renderer;
    scene->render(renderer);
    return stable_trace(renderer);
}

Definition::SharedPtr load_definition_checked(const char* source) {
    auto definition = Definition::load(source);
    check(definition != nullptr);
    if (!definition) {
        return nullptr;
    }
    check_false(definition->has_error());
    if (definition->has_error()) {
        return nullptr;
    }
    check(definition->scene() != nullptr);
    if (!definition->scene()) {
        return nullptr;
    }
    return definition;
}

Instance::SharedPtr create_instance_checked(Definition::SharedPtr definition) {
    auto instance = Instance::create(std::move(definition));
    check(instance != nullptr);
    return instance;
}

void render_scene_to_backend(Scene::RawPtr scene, Renderer& renderer) {
    renderer.begin_frame(scene->width(), scene->height(), 1.0f);
    renderer.clear(Color{0.0f, 0.0f, 0.0f, 1.0f});
    scene->render(renderer);
    renderer.end_frame();
}

struct TuiTerminalGuard {
    tui_terminal_t* term = nullptr;

    TuiTerminalGuard() = default;
    TuiTerminalGuard(const TuiTerminalGuard&) = delete;
    TuiTerminalGuard& operator=(const TuiTerminalGuard&) = delete;

    TuiTerminalGuard(TuiTerminalGuard&& other) noexcept : term(other.term) {
        other.term = nullptr;
    }

    TuiTerminalGuard& operator=(TuiTerminalGuard&& other) noexcept {
        if (this != &other) {
            if (term) {
                tui_terminal_cleanup(term);
                tui_terminal_destroy(term);
            }
            term = other.term;
            other.term = nullptr;
        }
        return *this;
    }

    ~TuiTerminalGuard() {
        if (term) {
            tui_terminal_cleanup(term);
            tui_terminal_destroy(term);
        }
    }
};

TuiTerminalGuard make_tui_terminal() {
    TuiTerminalGuard guard;
    guard.term = tui_terminal_create();
    check(guard.term != nullptr);
    if (!guard.term) {
        return guard;
    }
    bool initialized = tui_terminal_init(guard.term);
    check(initialized);
    if (!initialized) {
        return guard;
    }
    return guard;
}

std::unique_ptr<Renderer> create_tui_test_renderer(tui_terminal_t* term) {
    flex::tui_backend::register_backend();
    auto renderer = flex::create_renderer({RendererBackend::TUI, term});
    check(renderer != nullptr);
    return renderer;
}

const tui_cell_t* tui_cell_at_const(tui_terminal_t* term, int x, int y) {
    tui_buffer_t* buffer = tui_terminal_buffer(term);
    return buffer ? tui_buffer_at_const(buffer, x, y) : nullptr;
}

#if defined(FLEX_HAS_OPENGL) && defined(_WIN32)

LRESULT CALLBACK opengl_test_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

ATOM ensure_opengl_test_window_class() {
    static ATOM atom = []() -> ATOM {
        WNDCLASSA window_class{};
        window_class.style = CS_OWNDC;
        window_class.lpfnWndProc = opengl_test_window_proc;
        window_class.hInstance = GetModuleHandleA(nullptr);
        window_class.lpszClassName = "FlexOpenGLStandardTestWindow";
        return RegisterClassA(&window_class);
    }();
    return atom;
}

bool load_opengl_test_font() {
    if (flex::opengl_backend::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
        return true;
    }
    return flex::opengl_backend::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
}

struct OpenGLSurfaceGuard {
    HWND hwnd = nullptr;
    HDC device_context = nullptr;
    HGLRC gl_context = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;

    OpenGLSurfaceGuard() = default;
    OpenGLSurfaceGuard(const OpenGLSurfaceGuard&) = delete;
    OpenGLSurfaceGuard& operator=(const OpenGLSurfaceGuard&) = delete;

    OpenGLSurfaceGuard(OpenGLSurfaceGuard&& other) noexcept
        : hwnd(other.hwnd),
          device_context(other.device_context),
          gl_context(other.gl_context),
          width(other.width),
          height(other.height) {
        other.hwnd = nullptr;
        other.device_context = nullptr;
        other.gl_context = nullptr;
        other.width = 0;
        other.height = 0;
    }

    OpenGLSurfaceGuard& operator=(OpenGLSurfaceGuard&& other) noexcept {
        if (this != &other) {
            if (gl_context) {
                wglMakeCurrent(nullptr, nullptr);
                wglDeleteContext(gl_context);
            }
            if (device_context && hwnd) {
                ReleaseDC(hwnd, device_context);
            }
            if (hwnd) {
                DestroyWindow(hwnd);
            }

            hwnd = other.hwnd;
            device_context = other.device_context;
            gl_context = other.gl_context;
            width = other.width;
            height = other.height;

            other.hwnd = nullptr;
            other.device_context = nullptr;
            other.gl_context = nullptr;
            other.width = 0;
            other.height = 0;
        }
        return *this;
    }

    ~OpenGLSurfaceGuard() {
        if (gl_context) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(gl_context);
        }
        if (device_context && hwnd) {
            ReleaseDC(hwnd, device_context);
        }
        if (hwnd) {
            DestroyWindow(hwnd);
        }
    }
};

OpenGLSurfaceGuard make_opengl_surface(uint32_t width, uint32_t height) {
    OpenGLSurfaceGuard guard;
    guard.width = width;
    guard.height = height;

    flex::opengl_backend::init();
    const ATOM atom = ensure_opengl_test_window_class();
    check(atom != 0);
    if (!atom) {
        return guard;
    }

    constexpr DWORD window_style = WS_POPUP;
    RECT window_rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    const BOOL adjusted = AdjustWindowRectEx(&window_rect, window_style, FALSE, 0);
    check(adjusted == TRUE);
    if (!adjusted) {
        return guard;
    }

    guard.hwnd = CreateWindowExA(0,
                                 "FlexOpenGLStandardTestWindow",
                                 "Flex OpenGL Standard Test",
                                 window_style,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 window_rect.right - window_rect.left,
                                 window_rect.bottom - window_rect.top,
                                 nullptr,
                                 nullptr,
                                 GetModuleHandleA(nullptr),
                                 nullptr);
    check(guard.hwnd != nullptr);
    if (!guard.hwnd) {
        return guard;
    }

    RECT client_rect{};
    const BOOL got_client_rect = GetClientRect(guard.hwnd, &client_rect);
    check(got_client_rect == TRUE);
    check(client_rect.right == static_cast<LONG>(width));
    check(client_rect.bottom == static_cast<LONG>(height));
    if (!got_client_rect || client_rect.right != static_cast<LONG>(width) ||
        client_rect.bottom != static_cast<LONG>(height)) {
        return guard;
    }

    guard.device_context = GetDC(guard.hwnd);
    check(guard.device_context != nullptr);
    if (!guard.device_context) {
        return guard;
    }

    PIXELFORMATDESCRIPTOR pixel_format{};
    pixel_format.nSize = sizeof(pixel_format);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 32;
    pixel_format.cAlphaBits = 8;
    pixel_format.cDepthBits = 24;
    pixel_format.cStencilBits = 8;
    pixel_format.iLayerType = PFD_MAIN_PLANE;

    const int pixel_format_index = ChoosePixelFormat(guard.device_context, &pixel_format);
    check(pixel_format_index != 0);
    if (!pixel_format_index) {
        return guard;
    }

    BOOL set_pixel_result = SetPixelFormat(guard.device_context, pixel_format_index, &pixel_format);
    check(set_pixel_result == TRUE);
    if (!set_pixel_result) {
        return guard;
    }

    guard.gl_context = wglCreateContext(guard.device_context);
    check(guard.gl_context != nullptr);
    if (!guard.gl_context) {
        return guard;
    }

    BOOL make_current_result = wglMakeCurrent(guard.device_context, guard.gl_context);
    check(make_current_result == TRUE);
    if (!make_current_result) {
        return guard;
    }

    int glad_result = gladLoadGL();
    check(glad_result != 0);
    if (!glad_result) {
        return guard;
    }

    load_opengl_test_font();
    return guard;
}

bool surface_ready(const OpenGLSurfaceGuard& guard) {
    return guard.gl_context != nullptr;
}

uint32_t opengl_pixel_at(const OpenGLSurfaceGuard& guard, uint32_t x, uint32_t y) {
    if (!guard.gl_context || x >= guard.width || y >= guard.height) {
        return 0;
    }

    std::array<unsigned char, 4> rgba{0, 0, 0, 0};
    glFinish();
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(static_cast<GLint>(x),
                 static_cast<GLint>(guard.height - 1 - y),
                 1,
                 1,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 rgba.data());

    return (static_cast<uint32_t>(rgba[0]) << 24) | (static_cast<uint32_t>(rgba[1]) << 16) |
           (static_cast<uint32_t>(rgba[2]) << 8) | static_cast<uint32_t>(rgba[3]);
}

std::unique_ptr<Renderer> create_opengl_test_renderer() {
    flex::opengl_backend::register_backend();
    flex::opengl_backend::OpenGLCanvas canvas;
    canvas.get_proc_address = [](void*, const char* name) {
        PROC address = wglGetProcAddress(name);
        const auto raw = reinterpret_cast<std::intptr_t>(address);
        if (address && raw != 1 && raw != 2 && raw != 3 && raw != -1) {
            return reinterpret_cast<flex::opengl_backend::OpenGLProcAddress>(address);
        }
        HMODULE module = GetModuleHandleA("opengl32.dll");
        return reinterpret_cast<flex::opengl_backend::OpenGLProcAddress>(
            module ? GetProcAddress(module, name) : nullptr);
    };
    auto renderer = flex::opengl_backend::create_renderer(&canvas);
    check(renderer != nullptr);
    return renderer;
}

#endif

#ifdef _WIN32

struct D2DTargetGuard {
    Microsoft::WRL::ComPtr<IWICBitmap> bitmap;
    Microsoft::WRL::ComPtr<ID2D1RenderTarget> target;
    uint32_t width = 0;
    uint32_t height = 0;
};

bool load_d2d_test_font() {
    if (flex::d2d_backend::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
        return true;
    }
    return flex::d2d_backend::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
}

D2DTargetGuard make_d2d_target(uint32_t width, uint32_t height) {
    D2DTargetGuard guard;
    flex::d2d_backend::init();
    check(flex::d2d_backend::g_d2d_factory != nullptr);
    check(flex::d2d_backend::g_wic_factory != nullptr);
    if (!flex::d2d_backend::g_d2d_factory || !flex::d2d_backend::g_wic_factory) {
        return guard;
    }

    load_d2d_test_font();

    HRESULT bitmap_result = flex::d2d_backend::g_wic_factory->CreateBitmap(
        width,
        height,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapCacheOnLoad,
        guard.bitmap.GetAddressOf());
    check(bitmap_result == S_OK);
    if (FAILED(bitmap_result)) {
        return guard;
    }

    HRESULT target_result = flex::d2d_backend::g_d2d_factory->CreateWicBitmapRenderTarget(
        guard.bitmap.Get(),
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        guard.target.GetAddressOf());
    check(target_result == S_OK);
    if (FAILED(target_result)) {
        guard.bitmap.Reset();
        return guard;
    }

    guard.width = width;
    guard.height = height;
    return guard;
}

uint32_t d2d_pixel_at(const D2DTargetGuard& guard, uint32_t x, uint32_t y) {
    if (!guard.bitmap || x >= guard.width || y >= guard.height) {
        return 0;
    }

    std::vector<uint32_t> pixels(static_cast<size_t>(guard.width) * static_cast<size_t>(guard.height), 0u);
    HRESULT copy_result = guard.bitmap->CopyPixels(
        nullptr,
        guard.width * sizeof(uint32_t),
        static_cast<UINT>(pixels.size() * sizeof(uint32_t)),
        reinterpret_cast<BYTE*>(pixels.data()));
    check(copy_result == S_OK);
    if (FAILED(copy_result)) {
        return 0;
    }

    return pixels[static_cast<size_t>(y) * guard.width + x];
}

std::unique_ptr<Renderer> create_d2d_test_renderer(ID2D1RenderTarget* render_target) {
    auto renderer = flex::d2d_backend::create_renderer(render_target);
    check(renderer != nullptr);
    return renderer;
}

#endif

#ifdef _WIN32
bool surface_ready(const D2DTargetGuard& guard) {
    return guard.target != nullptr;
}
#endif

template <typename MakeSurface, typename CreateRenderer, typename SamplePixel>
void check_standard_pixel_backend(MakeSurface make_surface, CreateRenderer create_renderer,
                                  SamplePixel sample_pixel) {
    ArenaAllocator arena(8192);
    auto scene = build_standard_scene(arena);

    auto surface = make_surface(320, 160);
    if (!surface_ready(surface)) {
        return;
    }

    auto renderer = create_renderer(surface);
    if (!renderer) {
        return;
    }

    render_scene_to_backend(scene, *renderer);

    const uint32_t background = sample_pixel(surface, 0, 0);
    const uint32_t first_rect = sample_pixel(surface, 24, 29);
    const uint32_t second_rect = sample_pixel(surface, 57, 30);
    const uint32_t marker = sample_pixel(surface, 185, 17);

    check(first_rect != background);
    check(second_rect != background);
    check(marker != background);
    check(first_rect != second_rect);
    check(first_rect != marker);
    check(second_rect != marker);
}

template <typename MakeSurface, typename CreateRenderer, typename SamplePixel>
void check_animation_pixel_backend(MakeSurface make_surface, CreateRenderer create_renderer,
                                   SamplePixel sample_pixel) {
    auto definition = load_definition_checked(standard_scene_source());
    if (!definition) {
        return;
    }

    auto instance = create_instance_checked(definition);
    if (!instance) {
        return;
    }

    auto surface = make_surface(320, 160);
    if (!surface_ready(surface)) {
        return;
    }

    auto renderer = create_renderer(surface);
    if (!renderer) {
        return;
    }

    auto* player = instance->play_animation("markerMove");
    check(player != nullptr);
    if (!player) {
        return;
    }

    instance->advance(0.5f);
    render_scene_to_backend(instance->scene(), *renderer);

    const uint32_t background = sample_pixel(surface, 0, 0);
    const uint32_t old_marker = sample_pixel(surface, 185, 17);
    const uint32_t new_marker = sample_pixel(surface, 205, 17);

    check(old_marker == background);
    check(new_marker != background);
}

template <typename MakeSurface, typename CreateRenderer, typename SamplePixel>
void check_clipped_pixel_backend(MakeSurface make_surface, CreateRenderer create_renderer,
                                 SamplePixel sample_pixel) {
    ArenaAllocator arena(4096);
    auto scene = build_clip_scene(arena);

    auto surface = make_surface(160, 80);
    if (!surface_ready(surface)) {
        return;
    }

    auto renderer = create_renderer(surface);
    if (!renderer) {
        return;
    }

    render_scene_to_backend(scene, *renderer);

    const uint32_t background = sample_pixel(surface, 0, 0);
    const uint32_t inside = sample_pixel(surface, 12, 20);
    const uint32_t outside = sample_pixel(surface, 52, 20);

    check(inside != background);
    check(outside == background);
}

template <typename MakeSurface, typename CreateRenderer, typename SamplePixel>
void check_svg_pixel_backend(MakeSurface make_surface, CreateRenderer create_renderer,
                             SamplePixel sample_pixel) {
    ArenaAllocator arena(4096);
    auto scene = build_svg_scene(arena, FLEX_TEST_ASSET_DIR "/nanovg_badge.svg");

    auto surface = make_surface(200, 120);
    if (!surface_ready(surface)) {
        return;
    }

    auto renderer = create_renderer(surface);
    if (!renderer) {
        return;
    }

    render_scene_to_backend(scene, *renderer);

    const uint32_t background = sample_pixel(surface, 0, 0);
    const uint32_t svg_pixel = sample_pixel(surface, 56, 60);

    check(svg_pixel != background);
}

void check_standard_tui_backend() {
    ArenaAllocator arena(8192);
    auto scene = build_standard_scene(arena);

    auto terminal = make_tui_terminal();
    if (!terminal.term) {
        return;
    }

    auto renderer = create_tui_test_renderer(terminal.term);
    if (!renderer) {
        return;
    }

    render_scene_to_backend(scene, *renderer);

    const tui_cell_t* first_rect = tui_cell_at_const(terminal.term, 1, 1);
    const tui_cell_t* second_rect = tui_cell_at_const(terminal.term, 5, 1);
    const tui_cell_t* marker = tui_cell_at_const(terminal.term, 22, 0);
    const tui_cell_t* letter_f = tui_cell_at_const(terminal.term, 25, 1);

    check(first_rect != nullptr);
    check(second_rect != nullptr);
    check(marker != nullptr);
    check(letter_f != nullptr);
    if (!first_rect || !second_rect || !marker || !letter_f) {
        return;
    }

    check(tui_color_eq(first_rect->bg, TUI_COLOR(255, 51, 51)));
    check(tui_color_eq(second_rect->bg, TUI_COLOR(51, 204, 102)));
    check(tui_color_eq(marker->bg, TUI_COLOR(51, 102, 255)));
    check(letter_f->ch == 'F');
    check(tui_color_eq(letter_f->fg, TUI_WHITE));
}

void check_animation_tui_backend() {
    auto definition = load_definition_checked(standard_scene_source());
    if (!definition) {
        return;
    }

    auto instance = create_instance_checked(definition);
    if (!instance) {
        return;
    }

    auto terminal = make_tui_terminal();
    if (!terminal.term) {
        return;
    }

    auto renderer = create_tui_test_renderer(terminal.term);
    if (!renderer) {
        return;
    }

    auto* player = instance->play_animation("markerMove");
    check(player != nullptr);
    if (!player) {
        return;
    }

    instance->advance(0.5f);
    render_scene_to_backend(instance->scene(), *renderer);

    const tui_cell_t* old_marker = tui_cell_at_const(terminal.term, 22, 0);
    const tui_cell_t* new_marker = tui_cell_at_const(terminal.term, 25, 0);
    check(old_marker != nullptr);
    check(new_marker != nullptr);
    if (!old_marker || !new_marker) {
        return;
    }

    check(tui_color_eq(old_marker->bg, TUI_BLACK));
    check(tui_color_eq(new_marker->bg, TUI_COLOR(51, 102, 255)));
}

void check_clipped_tui_backend() {
    ArenaAllocator arena(4096);
    auto scene = build_clip_scene(arena);

    auto terminal = make_tui_terminal();
    if (!terminal.term) {
        return;
    }

    auto renderer = create_tui_test_renderer(terminal.term);
    if (!renderer) {
        return;
    }

    render_scene_to_backend(scene, *renderer);

    const tui_cell_t* inside = tui_cell_at_const(terminal.term, 1, 1);
    const tui_cell_t* outside = tui_cell_at_const(terminal.term, 6, 1);
    check(inside != nullptr);
    check(outside != nullptr);
    if (!inside || !outside) {
        return;
    }

    check(tui_color_eq(inside->bg, TUI_COLOR(255, 51, 51)));
    check(tui_color_eq(outside->bg, TUI_BLACK));
}

void check_transform_tui_backend() {
    auto terminal = make_tui_terminal();
    if (!terminal.term) {
        return;
    }

    auto renderer = create_tui_test_renderer(terminal.term);
    if (!renderer) {
        return;
    }

    renderer->begin_frame(128.0f, 64.0f, 1.0f);
    renderer->clear(Color{0.0f, 0.0f, 0.0f, 1.0f});
    renderer->set_transform(make_translation(16.0f, 16.0f));
    renderer->draw_rect(0.0f,
                        0.0f,
                        16.0f,
                        16.0f,
                        0.0f,
                        Paint::solid(Color{1.0f, 0.2f, 0.2f, 1.0f}),
                        Paint::none(),
                        0.0f);
    renderer->end_frame();

    const tui_cell_t* origin = tui_cell_at_const(terminal.term, 0, 0);
    const tui_cell_t* translated = tui_cell_at_const(terminal.term, 2, 1);
    check(origin != nullptr);
    check(translated != nullptr);
    if (!origin || !translated) {
        return;
    }

    check(tui_color_eq(origin->bg, TUI_BLACK));
    check(tui_color_eq(translated->bg, TUI_COLOR(255, 51, 51)));
}

void check_tui_renderer_capabilities() {
    auto terminal = make_tui_terminal();
    if (!terminal.term) {
        return;
    }

    auto renderer = create_tui_test_renderer(terminal.term);
    if (!renderer) {
        return;
    }

    const RendererCapabilities caps = renderer->capabilities();
    check(!caps.retained_mode);
    check(!caps.surface_recreation);
    check(!caps.path_drawing);
    check(!caps.raster_images);
    check(!caps.svg_images);
    check(!caps.rotation);
    check(!caps.scaling);
    check(!caps.shadow);
    check(!caps.blur);
}

#if defined(FLEX_HAS_OPENGL) && defined(_WIN32)
void check_opengl_renderer_capabilities() {
    auto surface = make_opengl_surface(64, 64);
    if (!surface_ready(surface)) {
        return;
    }

    auto renderer = create_opengl_test_renderer();
    if (!renderer) {
        return;
    }

    const RendererCapabilities caps = renderer->capabilities();
    check(!caps.retained_mode);
    check(!caps.surface_recreation);
    check(caps.path_drawing);
    check(caps.raster_images);
    check(caps.svg_images);
    check(caps.rotation);
    check(caps.scaling);
    check(caps.shadow);
    check(caps.blur);
}
#endif

#ifdef _WIN32
void check_d2d_renderer_capabilities() {
    auto surface = make_d2d_target(64, 64);
    if (!surface_ready(surface)) {
        return;
    }

    auto renderer = create_d2d_test_renderer(surface.target.Get());
    if (!renderer) {
        return;
    }

    const RendererCapabilities caps = renderer->capabilities();
    check(!caps.retained_mode);
    check(caps.surface_recreation);
    check(caps.path_drawing);
    check(caps.raster_images);
    check(caps.svg_images);
    check(caps.rotation);
    check(caps.scaling);
    check(!caps.shadow);
    check(!caps.blur);
}

void check_d2d_blur_contract() {
    auto surface = make_d2d_target(64, 64);
    if (!surface_ready(surface)) {
        return;
    }

    auto renderer = create_d2d_test_renderer(surface.target.Get());
    if (!renderer) {
        return;
    }

    renderer->set_blur(BlurFilter{});
    renderer->clear_blur();
    check_throws_as(renderer->set_blur(BlurFilter{-1.0f}), std::invalid_argument);
    check_throws_as(renderer->set_blur(BlurFilter{2.0f}), std::logic_error);
}

void check_d2d_shadow_contract() {
    auto surface = make_d2d_target(64, 64);
    if (!surface_ready(surface)) {
        return;
    }

    auto renderer = create_d2d_test_renderer(surface.target.Get());
    if (!renderer) {
        return;
    }

    renderer->set_shadow(Shadow{});
    Shadow transparent = Shadow::drop(2.0f, 2.0f, 2.0f, Color::Black);
    transparent.color.a = 0.0f;
    renderer->set_shadow(transparent);
    renderer->clear_shadow();

    Shadow invalid = Shadow::drop(1.0f, 1.0f, -1.0f, Color::Black);
    check_throws_as(renderer->set_shadow(invalid), std::invalid_argument);
    check_throws_as(
        renderer->set_shadow(Shadow::drop(2.0f, 2.0f, 3.0f, Color::Black)),
        std::logic_error);
}
#endif

} // namespace

suite("flex::standard") {
    group("render trace") {
        it("matches the canonical runtime-built scene") {
            ArenaAllocator arena(8192);
            auto scene = build_standard_scene(arena);

            auto trace = render_trace(scene);
            check_eq(trace, expected_standard_trace());
        }

        it("matches the canonical trace after DSL lowering") {
            auto definition = load_definition_checked(standard_scene_source());
            if (!definition) {
                return;
            }

            auto trace = render_trace(definition->scene());
            check_eq(trace, expected_standard_trace());
        }
    }

    group("clip contract") {
        it("emits clip and child draw operations in canonical order") {
            ArenaAllocator arena(4096);
            auto scene = build_clip_scene(arena);

            auto trace = render_trace(scene);
            check_eq(trace, expected_clip_trace());
        }
    }

    group("opacity and text alignment") {
        it("applies centered text offset and stacked alpha in trace") {
            ArenaAllocator arena(4096);
            auto scene = build_text_opacity_scene(arena);

            auto trace = render_trace(scene);
            check_eq(trace, expected_text_opacity_trace());
        }
    }

    group("svg contract") {
        it("propagates image and SVG effect state to their renderer calls") {
            ArenaAllocator arena(4096);
            RecordingRenderer renderer;
            const Shadow shadow = Shadow::drop(2.0f, 3.0f, 4.0f, Color::Black);

            auto image = Image::create(arena);
            image->set_src("image.png");
            image->set_width(16.0f);
            image->set_height(8.0f);
            image->set_shadow(shadow);
            image->render(renderer);

            auto svg = Svg::create(arena);
            svg->set_data("<svg xmlns='http://www.w3.org/2000/svg' width='4' height='4'/>");
            svg->set_layout_size(4.0f, 4.0f);
            svg->set_shadow(shadow);
            svg->render(renderer);

            check_size_eq(renderer.count("set_shadow"), std::size_t{2});
            check_size_eq(renderer.count("clear_shadow"), std::size_t{2});
            check_size_eq(renderer.count("draw_image"), std::size_t{1});
            check_size_eq(renderer.count("draw_svg_data"), std::size_t{1});
        }

        it("renders file-backed svg nodes through draw_svg") {
            ArenaAllocator arena(4096);
            auto scene = build_svg_scene(arena);
            const std::vector<std::string> expected = {
                "draw_svg 24.000 36.000 80.000 60.000 alpha=0.500 svg=nanovg_badge.svg",
            };

            auto trace = render_trace(scene);
            check_eq(trace, expected);
        }

        it("lowers svg width height and inline data into renderer calls") {
            auto definition = load_definition_checked(svg_scene_source());
            if (!definition) {
                return;
            }

            auto trace = render_trace(definition->scene());
            check_eq(trace, expected_svg_trace());
        }
    }

    group("animation trace") {
        it("updates the canonical trace after timeline advance") {
            auto definition = load_definition_checked(standard_scene_source());
            if (!definition) {
                return;
            }

            auto instance = create_instance_checked(definition);
            if (!instance) {
                return;
            }

            auto* player = instance->play_animation("markerMove");
            check(player != nullptr);
            if (!player) {
                return;
            }

            instance->advance(0.5f);

            auto* marker = instance->scene()->find("marker");
            check(marker != nullptr);
            if (!marker) {
                return;
            }
            check_float_eq(marker->x(), 200.0f, 0.001f);

            RecordingRenderer renderer;
            instance->render(renderer);

            auto trace = stable_trace(renderer);
            check_eq(trace, expected_standard_trace(200.0f));
        }
    }

    group("tui backend") {
        it("projects the canonical scene into expected terminal cells") {
            check_standard_tui_backend();
        }

        it("reflects animation advance in terminal cells") {
            check_animation_tui_backend();
        }

        it("keeps fully clipped content out of terminal cells") {
            check_clipped_tui_backend();
        }

        it("applies translation passed through set_transform") {
            check_transform_tui_backend();
        }

        it("reports the expected capability matrix") {
            check_tui_renderer_capabilities();
        }
    }

#if defined(FLEX_HAS_OPENGL) && defined(_WIN32)
    group("opengl backend") {
        it("projects the canonical scene into distinct software pixels") {
            check_standard_pixel_backend(
                make_opengl_surface,
                [](auto& surface) { return create_opengl_test_renderer(); },
                opengl_pixel_at);
        }

        it("moves the marker pixels after animation advance") {
            check_animation_pixel_backend(
                make_opengl_surface,
                [](auto& surface) { return create_opengl_test_renderer(); },
                opengl_pixel_at);
        }

        it("keeps fully clipped content out of software pixels") {
            check_clipped_pixel_backend(
                make_opengl_surface,
                [](auto& surface) { return create_opengl_test_renderer(); },
                opengl_pixel_at);
        }

        it("renders svg content into software pixels") {
            check_svg_pixel_backend(
                make_opengl_surface,
                [](auto& surface) { return create_opengl_test_renderer(); },
                opengl_pixel_at);
        }

        it("reports the expected capability matrix") {
            check_opengl_renderer_capabilities();
        }
    }
#endif

#ifdef _WIN32
    group("direct2d backend") {
        it("projects the canonical scene into distinct software pixels") {
            check_standard_pixel_backend(
                make_d2d_target,
                [](auto& surface) { return create_d2d_test_renderer(surface.target.Get()); },
                d2d_pixel_at);
        }

        it("moves the marker pixels after animation advance") {
            check_animation_pixel_backend(
                make_d2d_target,
                [](auto& surface) { return create_d2d_test_renderer(surface.target.Get()); },
                d2d_pixel_at);
        }

        it("keeps fully clipped content out of software pixels") {
            check_clipped_pixel_backend(
                make_d2d_target,
                [](auto& surface) { return create_d2d_test_renderer(surface.target.Get()); },
                d2d_pixel_at);
        }

        it("renders svg content into software pixels") {
            check_svg_pixel_backend(
                make_d2d_target,
                [](auto& surface) { return create_d2d_test_renderer(surface.target.Get()); },
                d2d_pixel_at);
        }

        it("reports the expected capability matrix") {
            check_d2d_renderer_capabilities();
        }

        it("rejects unsupported nonzero blur without silently dropping it") {
            check_d2d_blur_contract();
        }

        it("rejects unsupported visible shadows without silently dropping them") {
            check_d2d_shadow_contract();
        }
    }
#endif
}
