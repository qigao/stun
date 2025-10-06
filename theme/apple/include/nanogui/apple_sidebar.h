/*
    nanogui/apple_sidebar.h -- Apple HIG sidebar navigation (macOS)

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>
#include <vector>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleSidebar apple_sidebar.h nanogui/apple_sidebar.h
 *
 * \brief Apple HIG sidebar for macOS-style navigation
 */
class NANOGUI_EXPORT AppleSidebar : public Widget {
public:
  struct Section {
    std::string title;
    std::vector<std::string> items;
    std::vector<int> icons;
    
    Section(const std::string &title = "") : title(title) {}
  };

  AppleSidebar(Widget *parent);

  void add_section(const std::string &title);
  void add_item(const std::string &label, int icon = 0);
  
  int selected_index() const { return m_selected_index; }
  void set_selected_index(int index);

  bool collapsed() const { return m_collapsed; }
  void set_collapsed(bool collapsed);

  void set_callback(const std::function<void(int, int)> &callback) { 
    m_callback = callback; 
  }

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  std::pair<int, int> item_at_position(const Vector2i &p) const;

  std::vector<Section> m_sections;
  int m_selected_section;
  int m_selected_index;
  bool m_collapsed;
  float m_item_height;
  std::function<void(int, int)> m_callback;
};

NAMESPACE_END(nanogui)
