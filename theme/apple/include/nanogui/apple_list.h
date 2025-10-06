/*
    nanogui/apple_list.h -- Apple HIG list component

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
    All rights reserved. Use of this source code is governed by a BSD-style license.
*/

#pragma once

#include <nanogui/widget.h>
#include <nanogui/apple_theme.h>
#include <vector>
#include <functional>

NAMESPACE_BEGIN(nanogui)

/**
 * \class AppleList apple_list.h nanogui/apple_list.h
 *
 * \brief Apple HIG list component with grouped/plain styles
 */
class NANOGUI_EXPORT AppleList : public Widget {
public:
  enum class Style {
    Plain,    ///< Plain list (iOS)
    Grouped,  ///< Grouped list with rounded corners (iOS)
    Sidebar   ///< Sidebar list (macOS)
  };

  struct Item {
    std::string text;
    std::string detail;
    int icon;
    bool has_disclosure;
    bool selected;
    std::function<void()> callback;

    Item(const std::string &text = "", const std::string &detail = "",
         int icon = 0, bool disclosure = false)
        : text(text), detail(detail), icon(icon),
          has_disclosure(disclosure), selected(false) {}
  };

  AppleList(Widget *parent, Style style = Style::Plain);

  Style style() const { return m_style; }
  void set_style(Style style) { m_style = style; }

  void add_item(const std::string &text, const std::string &detail = "",
                int icon = 0, bool disclosure = true);
  void add_item(const Item &item);
  
  void clear_items() { m_items.clear(); }
  
  const std::vector<Item> &items() const { return m_items; }
  std::vector<Item> &items() { return m_items; }

  int selected_index() const { return m_selected_index; }
  void set_selected_index(int index);

  void set_callback(const std::function<void(int)> &callback) { m_callback = callback; }

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  void draw(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;

protected:
  AppleTheme *apple_theme() const;
  int item_at_position(const Vector2i &p) const;
  void draw_item(NVGcontext *ctx, const Item &item, float x, float y, float w, float h, bool hover);

  Style m_style;
  std::vector<Item> m_items;
  int m_selected_index;
  int m_hover_index;
  std::function<void(int)> m_callback;
  float m_item_height;
};

NAMESPACE_END(nanogui)
