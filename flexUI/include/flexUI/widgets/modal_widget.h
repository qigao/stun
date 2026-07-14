/*
 * flexUI - ModalWidget
 *
 * Dialog overlay - 使用 RenderCommandList 渲染
 */

#ifndef FLEXUI_MODAL_WIDGET_H
#define FLEXUI_MODAL_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../render_command.h"
#include "../shapes.h"
#include <string>
#include <functional>

namespace flexUI {

class ModalWidget : public Widget {
public:
  enum class Side { Center, Left, Right, Top, Bottom };

  ModalWidget(const std::string& title = "");

  void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "ModalWidget"; }

  const std::string& title() const { return title_; }
  void set_title(const std::string& t) {
    title_ = t;
    sync_host_semantics();
    dirty_ = true;
  }

  void open();
  void close();
  bool is_open() const { return open_; }

  bool close_on_overlay() const { return close_on_overlay_; }
  void set_close_on_overlay(bool v) { close_on_overlay_ = v; }

  bool show_close_button() const { return show_close_; }
  void set_show_close_button(bool v) { show_close_ = v; dirty_ = true; }

  Side side() const { return side_; }
  void set_side(Side side) {
    side_ = side;
    sync_host_semantics();
    dirty_ = true;
  }

  using CloseCallback = std::function<void()>;
  void set_close_callback(CloseCallback cb) { on_close_ = std::move(cb); }

private:
  struct DialogBounds {
    float x;
    float y;
    float w;
    float h;
  };

  void emit_overlay_background(RenderCommandList& commands, const Element& elem);
  void render_dialog(RenderCommandList& commands, const Element& elem);
  void render_header(RenderCommandList& commands, const Element& elem,
                     float dialog_x, float dialog_y, float dialog_w);
  DialogBounds current_dialog_bounds(const Element& elem) const;
  static const char* side_name(Side side);
  void sync_host_semantics() override;

  std::string title_;
  bool open_ = false;
  bool close_on_overlay_ = true;
  bool show_close_ = true;
  float opacity_ = 0;
  Side side_ = Side::Center;
  CloseCallback on_close_;
};

} // namespace flexUI

#endif // FLEXUI_MODAL_WIDGET_H
