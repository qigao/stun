#ifndef FLEXUI_EXAMPLES_HOST_MEDIA_BRIDGE_H
#define FLEXUI_EXAMPLES_HOST_MEDIA_BRIDGE_H

#include <flexUI/box.h>
#include <flexUI/host_bridge.h>

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace flexui_examples::media {

inline flexUI::MediaEnvironment current_environment() {
  flexUI::MediaEnvironment env;

#ifdef _WIN32
  BOOL animations_enabled = TRUE;
  if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations_enabled,
                            0)) {
    env.prefers_reduced_motion = animations_enabled == FALSE;
  }

  HIGHCONTRASTW high_contrast{};
  high_contrast.cbSize = sizeof(high_contrast);
  if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(high_contrast),
                            &high_contrast, 0)) {
    env.forced_colors_active =
        (high_contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    if (env.forced_colors_active) {
      env.contrast_preference = flexUI::ContrastPreference::More;
    }
  }

  DWORD apps_use_light_theme = 1;
  DWORD value_size = sizeof(apps_use_light_theme);
  if (RegGetValueW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                   L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr,
                   &apps_use_light_theme, &value_size) == ERROR_SUCCESS) {
    env.prefers_dark_scheme = apps_use_light_theme == 0;
  }
#endif

  return env;
}

inline void sync_environment(flexUI::Box* box) {
  if (!box) return;
  box->set_media_environment(current_environment());
}

#ifdef _WIN32
inline bool apply_window_dark_mode(HWND hwnd, bool enabled) {
  if (!hwnd) return false;

  using DwmSetWindowAttributeFn =
      HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
  static auto dwmapi = LoadLibraryW(L"dwmapi.dll");
  static auto set_window_attribute = reinterpret_cast<DwmSetWindowAttributeFn>(
      dwmapi ? GetProcAddress(dwmapi, "DwmSetWindowAttribute") : nullptr);
  if (!set_window_attribute) {
    return false;
  }

  const BOOL use_dark = enabled ? TRUE : FALSE;
  HRESULT hr = set_window_attribute(hwnd, 20, &use_dark, sizeof(use_dark));
  if (FAILED(hr)) {
    hr = set_window_attribute(hwnd, 19, &use_dark, sizeof(use_dark));
  }
  return SUCCEEDED(hr);
}

inline void sync_window_color_scheme(HWND hwnd, flexUI::Box* box,
                                     std::string& current_scheme) {
  if (!hwnd || !box) return;

  const std::string desired =
      flexUI::host::requested_document_color_scheme(box, current_scheme);
  if (desired.empty() || desired == current_scheme) return;

  current_scheme = desired;
  apply_window_dark_mode(
      hwnd, flexUI::host::should_prefer_dark_document_color_scheme(box));
}
#endif

}  // namespace flexui_examples::media

#endif  // FLEXUI_EXAMPLES_HOST_MEDIA_BRIDGE_H
