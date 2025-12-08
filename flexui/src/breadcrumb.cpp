#include <flexui/breadcrumb.h>
#include <cssbox_internal.h>
namespace flexui {

Breadcrumb::Breadcrumb(cssboxRenderer* renderer, const std::string& id,
                       const std::vector<std::string>& items, const BreadcrumbStyle& style)
    : Widget(renderer, id, "breadcrumb"), items_(items), style_(style) {
    // Set text_content for layout engine to calculate intrinsic size
    std::string content;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) content += " / ";
        content += items[i];
    }
    element()->text_content = content;
}

void Breadcrumb::draw(NVGcontext* vg) {
    // REMOVED: cssbox renders text_content (set in constructor as "Home / Products / Details")
    // Note: loses per-item coloring, use CSS for styling
}

} // namespace flexui
