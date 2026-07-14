/*
 * Flex Engine - Direct2D + HWND Demo
 *
 * Minimal end-to-end example for the immediate-mode Direct2D backend.
 */

#ifdef _WIN32

#include <windows.h>

#include <d2d1.h>
#include <d2d1helper.h>
#include <wrl/client.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <flex.h>
#include "backends/d2d/init.h"

namespace {

struct DemoApp {
  HWND hwnd = nullptr;
  std::string scene_path;
  flex::Definition::SharedPtr definition;
  flex::Instance::SharedPtr instance;
  std::unique_ptr<flex::Renderer> renderer;
  Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> render_target;
  std::chrono::steady_clock::time_point last_frame = std::chrono::steady_clock::now();
};

bool load_default_font() {
  if (flex::d2d_backend::load_font("sans-serif", "C:/Windows/Fonts/segoeui.ttf")) {
    return true;
  }
  return flex::d2d_backend::load_font("sans-serif", "C:/Windows/Fonts/arial.ttf");
}

void start_demo_animations(flex::Instance& instance) {
  static const char* kAnimations[] = {
      "spin",
      "pulse",
      "bounce1",
      "bounce2",
      "bounce3",
      "bounce4",
      "progressFill",
      "squareSpin",
      "fadeWave1",
      "fadeWave2",
      "fadeWave3",
      "scalePulse",
  };

  for (const char* name : kAnimations) {
    instance.play_animation(name);
  }
}

DemoApp* app_from_window(HWND hwnd) {
  return reinterpret_cast<DemoApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
}

HRESULT ensure_render_target(DemoApp& app) {
  if (app.render_target) {
    return S_OK;
  }

  RECT client{};
  GetClientRect(app.hwnd, &client);
  const UINT width = static_cast<UINT>(std::max<LONG>(client.right - client.left, 0));
  const UINT height = static_cast<UINT>(std::max<LONG>(client.bottom - client.top, 0));
  if (width == 0 || height == 0) {
    return S_FALSE;
  }

  flex::d2d_backend::init();
  if (!flex::d2d_backend::g_d2d_factory) {
    return E_FAIL;
  }

  HRESULT hr = flex::d2d_backend::g_d2d_factory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(app.hwnd, D2D1::SizeU(width, height)),
      app.render_target.ReleaseAndGetAddressOf());
  if (FAILED(hr)) {
    return hr;
  }

  app.renderer = flex::create_renderer(
      static_cast<flex::CanvasHandle>(app.render_target.Get()));
  if (!app.renderer) {
    app.render_target.Reset();
    return E_FAIL;
  }

  return S_OK;
}

void discard_render_target(DemoApp& app) {
  app.renderer.reset();
  app.render_target.Reset();
}

void resize_render_target(DemoApp& app) {
  if (!app.render_target) {
    return;
  }

  RECT client{};
  GetClientRect(app.hwnd, &client);
  const UINT width = static_cast<UINT>(std::max<LONG>(client.right - client.left, 0));
  const UINT height = static_cast<UINT>(std::max<LONG>(client.bottom - client.top, 0));
  if (width == 0 || height == 0) {
    return;
  }

  if (FAILED(app.render_target->Resize(D2D1::SizeU(width, height)))) {
    discard_render_target(app);
  }
}

void render_frame(DemoApp& app) {
  if (!app.instance || !app.instance->scene()) {
    return;
  }

  if (FAILED(ensure_render_target(app)) || !app.renderer) {
    return;
  }

  RECT client{};
  GetClientRect(app.hwnd, &client);
  const float width = static_cast<float>(std::max<LONG>(client.right - client.left, 0));
  const float height = static_cast<float>(std::max<LONG>(client.bottom - client.top, 0));
  if (width <= 0.0f || height <= 0.0f) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  const float dt = std::chrono::duration<float>(now - app.last_frame).count();
  app.last_frame = now;

  app.instance->advance(dt);

  app.renderer->begin_frame(width, height, 1.0f);
  app.renderer->clear(app.instance->scene()->background());
  app.instance->render(*app.renderer);
  app.renderer->end_frame();
}

LRESULT CALLBACK demo_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  DemoApp* app = app_from_window(hwnd);

  switch (msg) {
    case WM_SIZE:
      if (app) {
        resize_render_target(*app);
      }
      return 0;

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

int main(int argc, char** argv) {
  const std::string scene_path = (argc > 1) ? argv[1] : "svg_showcase.flex";

  flex::d2d_backend::init();
  flex::d2d_backend::register_backend();
  load_default_font();

  auto definition = flex::Definition::load_file(scene_path.c_str());
  if (!definition || definition->has_error()) {
    std::cerr << "Failed to load " << scene_path << ": "
              << (definition ? definition->error_message() : "null definition") << '\n';
    return 1;
  }

  auto instance = flex::Instance::create(definition);
  if (!instance) {
    std::cerr << "Failed to create Flex instance\n";
    return 1;
  }
  start_demo_animations(*instance);

  HINSTANCE hinstance = GetModuleHandle(nullptr);
  const wchar_t* class_name = L"FlexDirect2DDemoWindow";
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = demo_wnd_proc;
  window_class.hInstance = hinstance;
  window_class.lpszClassName = class_name;
  window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
  window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassW(&window_class);

  std::wstring title = L"Flex Direct2D Demo - ";
  title += std::filesystem::path(scene_path).stem().wstring();

  HWND hwnd = CreateWindowExW(
      0,
      class_name,
      title.c_str(),
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      960,
      720,
      nullptr,
      nullptr,
      hinstance,
      nullptr);
  if (!hwnd) {
    std::cerr << "Window creation failed\n";
    return 1;
  }

  DemoApp app{};
  app.hwnd = hwnd;
  app.scene_path = scene_path;
  app.definition = definition;
  app.instance = std::move(instance);
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
