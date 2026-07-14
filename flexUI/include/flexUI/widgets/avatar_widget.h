/*
 * flexUI - AvatarWidget
 *
 * Avatar display using Group/Shape composition system.
 */

#ifndef FLEXUI_AVATAR_WIDGET_H
#define FLEXUI_AVATAR_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../render_command.h"
#include "../shapes.h"
#include <cstdint>
#include <string>
#include <vector>

namespace flexUI {

/**
 * AvatarWidget - User avatar with fallback to initials
 *
 * Structure:
 *   Group (root)
 *   ├── CircleShape or RectShape (background)
 *   ├── avatar image (draw_image/draw_svg when backend supports it)
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
    void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
    bool handle_event(const Event& event, Element& elem) override;
    void update(float delta_ms, Element& elem) override;
    bool needs_frame_update(const Element& elem) const override {
        (void)elem;
        return false;
    }
    bool state_affects_paint(Symbol state) const override {
        (void)state;
        return false;
    }
    const char* type_name() const override { return "AvatarWidget"; }
    bool paints_host_box() const override { return true; }

    // Properties
    const std::string& name() const { return name_; }
    void set_name(const std::string& name);

    const std::string& image_url() const { return image_url_; }
    void set_image_url(const std::string& url);

    Status status() const { return status_; }
    void set_status(Status status) { status_ = status; sync_host_semantics(); invalidate_render_cache(); }

    AvatarShape avatar_shape() const { return avatar_shape_; }
    void set_avatar_shape(AvatarShape shape) { avatar_shape_ = shape; sync_host_semantics(); invalidate_render_cache(); }

private:
    void sync_host_semantics() override;
    void rebuild_shapes(const Element& elem);
    void update_shapes(const Element& elem);
    std::string get_initials() const;
    bool render_cache_matches(const Element& elem,
                              const flex::RendererCapabilities& caps) const;
    void update_render_cache_key(const Element& elem,
                                 const flex::RendererCapabilities& caps);
    void invalidate_render_cache();

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

    bool render_cache_valid_ = false;
    bool cached_raster_images_ = false;
    bool cached_svg_images_ = false;
    uint64_t cached_style_signature_ = 0;
    int cached_status_ = 0;
    int cached_avatar_shape_ = 0;
    float cached_width_ = 0.0f;
    float cached_height_ = 0.0f;
    float cached_opacity_ = 1.0f;
    int cached_font_weight_ = 0;
    std::string cached_name_;
    std::string cached_image_url_;
    std::string cached_font_family_;
    std::string cached_avatar_size_;
    std::string cached_avatar_bg_;
    std::string cached_avatar_text_;
    std::string cached_avatar_border_;
    std::string cached_avatar_border_width_;
    std::string cached_status_size_;
    std::string cached_status_online_;
    std::string cached_status_offline_;
    std::string cached_status_away_;
    std::string cached_status_busy_;
    std::vector<RenderCommand> render_cache_;
};

} // namespace flexUI

#endif // FLEXUI_AVATAR_WIDGET_H
