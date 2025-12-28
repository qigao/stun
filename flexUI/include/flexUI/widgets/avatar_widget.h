/*
 * flexUI - AvatarWidget
 *
 * Avatar display using Group/Shape composition system.
 */

#ifndef FLEXUI_AVATAR_WIDGET_H
#define FLEXUI_AVATAR_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>

namespace flexUI {

/**
 * AvatarWidget - User avatar with fallback to initials
 *
 * Structure:
 *   Group (root)
 *   ├── CircleShape or RectShape (background)
 *   ├── ImageShape (avatar image, if available)
 *   ├── TextShape (initials fallback)
 *   └── CircleShape (status indicator, optional)
 *
 * CSS variables:
 *   --avatar-size: "40"
 *   --avatar-bg: "r,g,b,a"
 *   --avatar-text: "r,g,b,a"
 *   --avatar-radius: "50%" or pixel value
 *   --avatar-border: "r,g,b,a"
 *   --avatar-border-width: "0"
 *   --status-color: "r,g,b,a" (for status indicator)
 *   --status-size: "12"
 */
class AvatarWidget : public Widget {
public:
    enum class Status { None, Online, Offline, Away, Busy };
    enum class AvatarShape { Circle, Square, Rounded };

    explicit AvatarWidget(const std::string& name = "", const std::string& image_url = "");

    // Widget interface
    void render(const Element& elem, Renderer& renderer) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    const char* type_name() const override { return "AvatarWidget"; }

    // Properties
    const std::string& name() const { return name_; }
    void set_name(const std::string& name);

    const std::string& image_url() const { return image_url_; }
    void set_image_url(const std::string& url);

    Status status() const { return status_; }
    void set_status(Status status) { status_ = status; dirty_ = true; }

    AvatarShape avatar_shape() const { return avatar_shape_; }
    void set_avatar_shape(AvatarShape shape) { avatar_shape_ = shape; dirty_ = true; }

private:
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);
    std::string get_initials() const;

    // Visual composition
    Group root_;
    RectShape* background_ = nullptr;
    TextShape* initials_text_ = nullptr;
    CircleShape* status_indicator_ = nullptr;

    // State
    std::string name_;
    std::string image_url_;
    Status status_ = Status::None;
    AvatarShape avatar_shape_ = AvatarShape::Circle;

    // Cached
    float cached_size_ = 0;
};

} // namespace flexUI

#endif // FLEXUI_AVATAR_WIDGET_H
