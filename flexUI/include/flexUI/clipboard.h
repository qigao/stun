#ifndef FLEXUI_CLIPBOARD_H
#define FLEXUI_CLIPBOARD_H

#include <string>

namespace flexUI::clipboard {

bool write_text(const std::string& text);
std::string read_text();

} // namespace flexUI::clipboard

#endif // FLEXUI_CLIPBOARD_H
