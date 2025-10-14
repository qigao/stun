#pragma once

#include <nanogui/widget.h>

#include <functional>
#include <string>

NAMESPACE_BEGIN(nanogui)

class FluentWebTheme;

/**
 * Fluent 2 persona chip with avatar, name, and presence indicator.
 */
class NANOGUI_EXPORT FluentWebPersona : public Widget {
public:
  enum class Size { Small, Medium, Large };
  enum class Presence { None, Available, Busy, Away, Offline, DoNotDisturb };

  explicit FluentWebPersona(Widget *parent, const std::string &name = "",
                            const std::string &secondary = "");

  void set_name(const std::string &name);
  const std::string &name() const { return m_name; }

  void set_secondary_text(const std::string &text);
  const std::string &secondary_text() const { return m_secondary_text; }

  void set_initials(const std::string &initials);
  const std::string &initials() const { return m_initials; }

  void set_size(Size size);
  Size size() const { return m_persona_size; }

  void set_presence(Presence presence);
  Presence presence() const { return m_presence; }

  void set_clickable(bool clickable) { m_clickable = clickable; }
  bool clickable() const { return m_clickable; }

  void set_callback(const std::function<void()> &cb) { m_callback = cb; }

  void set_theme(Theme *theme) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_enter_event(const Vector2i &p, bool enter) override;

protected:
  void refresh_tokens();
  Color presence_color() const;
  int avatar_size() const;

  std::string m_name;
  std::string m_secondary_text;
  std::string m_initials;
  Size m_persona_size;
  Presence m_presence;
  bool m_clickable;
  bool m_hovered;
  std::function<void()> m_callback;

  Color m_avatar_background;
  Color m_avatar_text;
  Color m_name_color;
  Color m_secondary_color;
  Color m_hover_background;
  Color m_presence_available;
  Color m_presence_busy;
  Color m_presence_away;
  Color m_presence_offline;
  Color m_presence_dnd;
  int m_padding;
  int m_spacing;
  float m_name_font_size;
  float m_secondary_font_size;
};

NAMESPACE_END(nanogui)
