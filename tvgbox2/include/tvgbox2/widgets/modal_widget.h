/*
 * tvgbox2 - ModalWidget
 *
 * Dialog overlay
 */

#ifndef TVGBOX2_MODAL_WIDGET_H
#define TVGBOX2_MODAL_WIDGET_H

#include "../widget.h"
#include <string>
#include <functional>

namespace tvgbox2 {

class ModalWidget : public Widget {
public:
  ModalWidget(const std::string& title = "");

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "ModalWidget"; }

  const std::string& title() const { return title_; }
  void set_title(const std::string& t) { title_ = t; dirty_ = true; }

  void open();
  void close();
  bool is_open() const { return open_; }

  bool close_on_overlay() const { return close_on_overlay_; }
  void set_close_on_overlay(bool v) { close_on_overlay_ = v; }

  bool show_close_button() const { return show_close_; }
  void set_show_close_button(bool v) { show_close_ = v; dirty_ = true; }

  using CloseCallback = std::function<void()>;
  void set_close_callback(CloseCallback cb) { on_close_ = std::move(cb); }

private:
  void render_overlay(tvg::Scene* scene, const Element& elem);
  void render_dialog(tvg::Scene* scene, const Element& elem);
  void render_header(tvg::Scene* scene, const Element& elem, float dialog_x, float dialog_y, float dialog_w);
  void render_close_button(tvg::Scene* scene, float x, float y);

  std::string title_;
  bool open_ = false;
  bool close_on_overlay_ = true;
  bool show_close_ = true;
  float opacity_ = 0;
  CloseCallback on_close_;
};

} // namespace tvgbox2

#endif
