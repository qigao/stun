#include "md_extension.h"

namespace md {

ExtensionRegistry& ExtensionRegistry::instance() {
    static ExtensionRegistry registry;
    return registry;
}

void ExtensionRegistry::register_handler(const std::string& language, BlockHandler handler) {
    handlers_[language] = std::move(handler);
}

BlockHandler* ExtensionRegistry::find_handler(const std::string& language) {
    auto it = handlers_.find(language);
    return it != handlers_.end() ? &it->second : nullptr;
}

bool ExtensionRegistry::has_handler(const std::string& language) const {
    return handlers_.find(language) != handlers_.end();
}

std::optional<BlockResult> ExtensionRegistry::process(const std::string& language, std::string_view content) {
    auto* handler = find_handler(language);
    if (!handler) return std::nullopt;
    return (*handler)(content);
}

} // namespace md
