/*
 * tvgbox2 - TabsWidget
 *
 * Tab navigation - 使用 flex::Renderer 渲染
 */

#ifndef TVGBOX2_TABS_WIDGET_H
#define TVGBOX2_TABS_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <vector>
#include <functional>

namespace tvgbox2 {

class TabsWidget : public Widget {
public:
  struct Tab {
    std::string label;
    std::string id;
    bool disabled = false;
  };

  TabsWidget();

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "TabsWidget"; }

  void add_tab(const std::string& label, const std::string& id, bool disabled = false);
  void remove_tab(const std::string& id);
  void clear_tabs();
  const std::vector<Tab>& tabs() const { return tabs_; }

  int active_index() const { return active_index_; }
  void set_active_index(int index);
  const std::string& active_id() const;
  void set_active_id(const std::string& id);

  using ChangeCallback = std::function<void(int index, const std::string& id)>;
  void set_change_callback(ChangeCallback cb) { on_change_ = std::move(cb); }

private:
  void render_tabs(flex::Renderer& r, const Element& elem);
  void render_indicator(flex::Renderer& r, const Element& elem);
  float get_tab_width(const Tab& tab, float font_size) const;

  std::vector<Tab> tabs_;
  int active_index_ = 0;
  int hover_index_ = -1;
  float indicator_x_ = 0;
  float indicator_width_ = 0;
  float target_indicator_x_ = 0;
  float target_indicator_width_ = 0;
  ChangeCallback on_change_;
};

} // namespace tvgbox2

#endif // TVGBOX2_TABS_WIDGET_H
