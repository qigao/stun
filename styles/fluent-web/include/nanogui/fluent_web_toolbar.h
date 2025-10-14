#pragma once

#include <nanogui/widget.h>
#include <nanogui/fluent_web_button.h>

#include <functional>
#include <string>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class FluentWebTheme;

/**
 * Fluent 2 command bar / toolbar with grouped actions and overflow.
 */
class NANOGUI_EXPORT FluentWebToolbar : public Widget {
public:
  struct Action {
    std::string label;
    int icon;
    std::function<void()> callback;
    bool enabled;
    bool primary;

    Action(const std::string &lbl, int ic = 0, const std::function<void()> &cb = nullptr,
           bool en = true, bool prim = false)
        : label(lbl), icon(ic), callback(cb), enabled(en), primary(prim) {}
  };

  explicit FluentWebToolbar(Widget *parent);

  void add_action(const std::string &label, int icon = 0,
                  const std::function<void()> &callback = nullptr, bool enabled = true,
                  bool primary = false);
  void add_separator();
  void clear_actions();

  void set_overflow_threshold(int threshold) { m_overflow_threshold = threshold; }
  int overflow_threshold() const { return m_overflow_threshold; }

  void set_theme(Theme *theme) override;
  void perform_layout(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;

protected:
  void refresh_tokens();
  void rebuild_buttons();

  std::vector<Action> m_actions;
  std::vector<Widget *> m_buttons;
  int m_overflow_threshold;

  Color m_background;
  Color m_border_color;
  int m_padding;
  int m_spacing;
  int m_corner_radius;
};

NAMESPACE_END(nanogui)
