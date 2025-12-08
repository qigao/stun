#include <flexui/avatar.h>
#include <cssbox_internal.h>
namespace flexui {

Avatar::Avatar(cssboxRenderer* renderer, const std::string& id, const std::string& initials,
               const AvatarStyle& style)
    : Widget(renderer, id, "avatar"), initials_(initials), style_(style) {
    // Set text_content for cssbox to render initials
    element()->text_content = initials;
}

void Avatar::draw(NVGcontext* vg) {
    // REMOVED: cssbox renders background + text_content
    // Style with CSS: .avatar { border-radius: 50%; background: blue; text-align: center; }
}

} // namespace flexui
