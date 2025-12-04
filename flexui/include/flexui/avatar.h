#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>

namespace flexui {

struct AvatarStyle {
    NVGcolor bgColor = nvgRGB(158, 158, 158);
    NVGcolor textColor = nvgRGB(255, 255, 255);
    float fontSize = 16;
};

class Avatar : public Widget {
public:
    Avatar(cssboxRenderer* renderer, const std::string& id, const std::string& initials,
           const AvatarStyle& style = AvatarStyle());

    void draw(NVGcontext* vg) override;
    void setInitials(const std::string& initials) { initials_ = initials; }
    const std::string& getInitials() const { return initials_; }
    void setAvatarStyle(const AvatarStyle& style) { style_ = style; }

private:
    std::string initials_;
    AvatarStyle style_;
};

} // namespace flexui
