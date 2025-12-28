/*
 * flexUI - TabsWidget
 *
 * Tab navigation with integrated page management.
 * Pages are automatically shown/hidden based on active tab.
 */

#ifndef FLEXUI_TABS_WIDGET_H
#define FLEXUI_TABS_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <vector>
#include <functional>

namespace flexUI {

// Forward declaration
class Element;

class TabsWidget : public Widget {
public:
  struct Tab {
    std::string label;
    std::string id;
    Element* page = nullptr;  // Associated page element
    bool disabled = false;
  };

  TabsWidget();

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "TabsWidget"; }

  // Tab management
  void add_tab(const std::string& label, const std::string& id, Element* page = nullptr, bool disabled = false);
  void remove_tab(const std::string& id);
  void clear_tabs();
  const std::vector<Tab>& tabs() const { return tabs_; }

  // Page management
  void set_tab_page(int index, Element* page);
  void set_tab_page(const std::string& id, Element* page);
  Element* get_tab_page(int index) const;
  Element* get_tab_page(const std::string& id) const;

  // Active tab
  int active_index() const { return active_index_; }
  void set_active_index(int index);
  const std::string& active_id() const;
  void set_active_id(const std::string& id);

  // Callback (still available for additional logic)
  using ChangeCallback = std::function<void(int index, const std::string& id)>;
  void set_change_callback(ChangeCallback cb) { on_change_ = std::move(cb); }

private:
  void render_tabs(flex::Renderer& r, const Element& elem);
  void render_indicator(flex::Renderer& r, const Element& elem);
  float get_tab_width(const Tab& tab, float font_size) const;
  void update_page_visibility();

  std::vector<Tab> tabs_;
  int active_index_ = 0;
  int hover_index_ = -1;
  float indicator_x_ = 0;
  float indicator_width_ = 0;
  float target_indicator_x_ = 0;
  float target_indicator_width_ = 0;
  ChangeCallback on_change_;
};

} // namespace flexUI

#endif // FLEXUI_TABS_WIDGET_H
