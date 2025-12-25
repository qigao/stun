/*
 * tvgbox2 - AccordionWidget
 *
 * Collapsible sections - 使用 flex::Renderer 渲染
 */

#ifndef TVGBOX2_ACCORDION_WIDGET_H
#define TVGBOX2_ACCORDION_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <vector>
#include <functional>

namespace tvgbox2 {

class AccordionWidget : public Widget {
public:
  struct Section {
    std::string title;
    std::string id;
    bool expanded = false;
    float content_height = 0;
  };

  AccordionWidget();

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "AccordionWidget"; }

  void add_section(const std::string& title, const std::string& id, float content_height = 100.0f);
  void remove_section(const std::string& id);
  void clear_sections();

  void expand(const std::string& id);
  void collapse(const std::string& id);
  void toggle(const std::string& id);
  bool is_expanded(const std::string& id) const;

  bool allow_multiple() const { return allow_multiple_; }
  void set_allow_multiple(bool allow) { allow_multiple_ = allow; }

  using ChangeCallback = std::function<void(const std::string& id, bool expanded)>;
  void set_change_callback(ChangeCallback cb) { on_change_ = std::move(cb); }

private:
  void render_section(flex::Renderer& r, const Element& elem, Section& section, float y, size_t idx);
  void render_arrow(flex::Renderer& r, float x, float y, bool expanded);

  std::vector<Section> sections_;
  bool allow_multiple_ = false;
  int hover_index_ = -1;
  std::vector<float> animation_progress_;
  ChangeCallback on_change_;
};

} // namespace tvgbox2

#endif // TVGBOX2_ACCORDION_WIDGET_H
