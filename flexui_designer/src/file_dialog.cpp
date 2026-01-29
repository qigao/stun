/*
 * flexUI Designer - File Dialog Implementation
 */

#include "flexui_designer/file_dialog.h"
#include <nfd.h>

namespace flexui_designer {

void FileDialog::init() {
    NFD_Init();
}

void FileDialog::shutdown() {
    NFD_Quit();
}

std::optional<std::string> FileDialog::open_file(
    const char* title,
    const char* default_path,
    const char* filter_name,
    const char* filter_spec)
{
    nfdu8char_t* out_path = nullptr;
    nfdu8filteritem_t filter = {filter_name, filter_spec};
    
    nfdresult_t result = NFD_OpenDialogU8(
        &out_path,
        filter_name ? &filter : nullptr,
        filter_name ? 1 : 0,
        default_path);

    if (result == NFD_OKAY && out_path) {
        std::string path(out_path);
        NFD_FreePathU8(out_path);
        return path;
    }
    return std::nullopt;
}

std::optional<std::string> FileDialog::save_file(
    const char* title,
    const char* default_path,
    const char* default_name,
    const char* filter_name,
    const char* filter_spec)
{
    nfdu8char_t* out_path = nullptr;
    nfdu8filteritem_t filter = {filter_name, filter_spec};
    
    nfdresult_t result = NFD_SaveDialogU8(
        &out_path,
        filter_name ? &filter : nullptr,
        filter_name ? 1 : 0,
        default_path,
        default_name);

    if (result == NFD_OKAY && out_path) {
        std::string path(out_path);
        NFD_FreePathU8(out_path);
        return path;
    }
    return std::nullopt;
}

std::optional<std::string> FileDialog::pick_folder(const char* default_path) {
    nfdu8char_t* out_path = nullptr;
    
    nfdresult_t result = NFD_PickFolderU8(&out_path, default_path);

    if (result == NFD_OKAY && out_path) {
        std::string path(out_path);
        NFD_FreePathU8(out_path);
        return path;
    }
    return std::nullopt;
}

} // namespace flexui_designer
