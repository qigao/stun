/*
 * flexUI Designer - File Dialog Wrapper
 *
 * Native file dialog using NFD library.
 */

#pragma once

#include <string>
#include <optional>

namespace flexui_designer {

class FileDialog {
public:
    static void init();
    static void shutdown();

    // Open file dialog
    static std::optional<std::string> open_file(
        const char* title = nullptr,
        const char* default_path = nullptr,
        const char* filter_name = nullptr,
        const char* filter_spec = nullptr);

    // Save file dialog
    static std::optional<std::string> save_file(
        const char* title = nullptr,
        const char* default_path = nullptr,
        const char* default_name = nullptr,
        const char* filter_name = nullptr,
        const char* filter_spec = nullptr);

    // Pick folder dialog
    static std::optional<std::string> pick_folder(
        const char* default_path = nullptr);
};

} // namespace flexui_designer
