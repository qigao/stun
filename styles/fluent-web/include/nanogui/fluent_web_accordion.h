#pragma once

#include <nanogui/widget.h>
#include <nanogui/fluent_web_button.h>
#include <nanogui/vector.h>
#include <string>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class FluentWebTheme;
class Label;

/**
 * Fluent-styled accordion hosting expandable sections.
 */
class NANOGUI_EXPORT FluentWebAccordion : public Widget {
public:
  struct Section {
    FluentWebButton *header_button;
    Widget *body_container;
    bool expanded;
  };

  friend class AccordionSectionBody;

  explicit FluentWebAccordion(Widget *parent);

  /// Adds a new section with the given title. Returns the body container to populate.
  Widget *add_section(const std::string &title, bool expanded = false);

  /// Removes all sections and clears the accordion.
  void clear_sections();

  /// Expands the section at the given index, collapsing others if single expand is enabled.
  void expand_section(size_t index, bool expand);

  /// When enabled (default), expanding one section collapses the rest.
  void set_single_expand(bool value) { m_single_expand = value; }
  bool single_expand() const { return m_single_expand; }

  /// Override to update Fluent spacing/colors when theme changes.
  void set_theme(Theme *theme) override;

  /// Redraws headers/bodies with current states.
  void perform_layout(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  void refresh_tokens();
  void update_spacing();
  void update_section_chrome(Section &section);

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;

  std::vector<Section> m_sections;
  bool m_single_expand;

  // Cached Fluent tokens
  float m_header_font_size;
  float m_body_font_size;
  Color m_header_color;
  Color m_header_hover_color;
  Color m_header_pressed_color;
  Color m_header_foreground;
  Color m_body_background;
  Color m_body_foreground;
  Color m_body_border_color;

  int m_header_padding;
  int m_section_spacing;
  int m_body_padding;
  int m_body_spacing;
};

NAMESPACE_END(nanogui)
