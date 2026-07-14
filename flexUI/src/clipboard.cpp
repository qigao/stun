#include <flexUI/clipboard.h>

#include <mutex>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace flexUI::clipboard {
namespace {

std::mutex g_clipboard_mutex;
std::string g_fallback_text;

#ifdef _WIN32
std::wstring utf16_from_utf8(const std::string& text) {
  if (text.empty()) return {};

  const int wide_len =
      MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (wide_len <= 0) return {};

  std::wstring wide(static_cast<size_t>(wide_len), L'\0');
  MultiByteToWideChar(
      CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), wide_len);
  return wide;
}

std::string utf8_from_utf16(const wchar_t* text, int length) {
  if (!text || length <= 0) return {};

  const int utf8_len =
      WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
  if (utf8_len <= 0) return {};

  std::string utf8(static_cast<size_t>(utf8_len), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, length, utf8.data(), utf8_len, nullptr, nullptr);
  return utf8;
}

bool write_system_text(const std::string& text) {
  const std::wstring wide = utf16_from_utf8(text);
  if (wide.empty() && !text.empty()) return false;

  if (!OpenClipboard(nullptr)) return false;

  struct ClipboardCloser {
    ~ClipboardCloser() { CloseClipboard(); }
  } closer;

  if (!EmptyClipboard()) return false;

  const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
  HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!handle) return false;

  void* memory = GlobalLock(handle);
  if (!memory) {
    GlobalFree(handle);
    return false;
  }

  memcpy(memory, wide.c_str(), bytes);
  GlobalUnlock(handle);

  if (!SetClipboardData(CF_UNICODETEXT, handle)) {
    GlobalFree(handle);
    return false;
  }

  return true;
}

std::string read_system_text() {
  if (!OpenClipboard(nullptr)) return {};

  struct ClipboardCloser {
    ~ClipboardCloser() { CloseClipboard(); }
  } closer;

  HANDLE handle = GetClipboardData(CF_UNICODETEXT);
  if (!handle) return {};

  const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(handle));
  if (!wide) return {};

  const std::wstring wide_text(wide);
  GlobalUnlock(handle);
  return utf8_from_utf16(wide_text.c_str(), static_cast<int>(wide_text.size()));
}
#endif

} // namespace

bool write_text(const std::string& text) {
  {
    std::lock_guard<std::mutex> lock(g_clipboard_mutex);
    g_fallback_text = text;
  }

#ifdef _WIN32
  write_system_text(text);
#endif
  return true;
}

std::string read_text() {
#ifdef _WIN32
  const std::string system_text = read_system_text();
  if (!system_text.empty()) {
    std::lock_guard<std::mutex> lock(g_clipboard_mutex);
    g_fallback_text = system_text;
    return system_text;
  }
#endif

  std::lock_guard<std::mutex> lock(g_clipboard_mutex);
  return g_fallback_text;
}

} // namespace flexUI::clipboard
