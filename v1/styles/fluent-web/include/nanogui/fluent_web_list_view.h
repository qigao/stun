#pragma once

#include <nanogui/widget.h>
#include <nanogui/vector.h>

#include <functional>
#include <string>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class FluentWebTheme;

class FluentWebListView;
class FluentWebGridView;

/**
 * Fluent-styled list row supporting primary/secondary text, icons, and selection.
 */
class NANOGUI_EXPORT FluentWebListItem : public Widget {
public:
  enum class Layout { OneLine, TwoLine };

  FluentWebListItem(FluentWebListView *owner, const std::string &primary = "",
                    const std::string &secondary = "", Layout layout = Layout::OneLine);

  void set_primary_text(const std::string &text);
  const std::string &primary_text() const { return m_primary_text; }

  void set_secondary_text(const std::string &text);
  const std::string &secondary_text() const { return m_secondary_text; }

  void set_meta_text(const std::string &text);
  const std::string &meta_text() const { return m_meta_text; }

  void set_leading_icon(int icon) { m_leading_icon = icon; }
  int leading_icon() const { return m_leading_icon; }

  void set_trailing_icon(int icon) { m_trailing_icon = icon; }
  int trailing_icon() const { return m_trailing_icon; }

  void set_layout(Layout layout);
  Layout layout() const { return m_layout; }

  void set_selected(bool selected);
  bool selected() const { return m_selected; }

  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_enter_event(const Vector2i &p, bool enter) override;

  void apply_metrics();

private:
  FluentWebListView *m_owner;
  std::string m_primary_text;
  std::string m_secondary_text;
  std::string m_meta_text;
  int m_leading_icon;
  int m_trailing_icon;
  Layout m_layout;
  bool m_selected;
  bool m_hovered;
};

/**
 * Fluent 2 list container with selection management and data callbacks.
 */
class NANOGUI_EXPORT FluentWebListView : public Widget {
public:
  enum class SelectionMode { None, Single, Multiple };

  explicit FluentWebListView(Widget *parent);

  FluentWebListItem *add_item(const std::string &primary,
                              const std::string &secondary = "",
                              FluentWebListItem::Layout layout = FluentWebListItem::Layout::OneLine);
  void clear_items();
  void set_item_count(size_t count);
  void set_item_provider(const std::function<void(int, FluentWebListItem *)> &provider);

  void set_selection_mode(SelectionMode mode);
  SelectionMode selection_mode() const { return m_selection_mode; }

  void set_selection_callback(const std::function<void(const std::vector<int> &)> &cb);
  void set_activation_callback(const std::function<void(int)> &cb);

  const std::vector<int> &selected_indices() const { return m_selected_indices; }
  void set_selected(int index, bool selected);
  void clear_selection();

  bool keyboard_event(int key, int scancode, int action, int modifiers) override;
  bool focus_event(bool focused) override;
  void set_theme(Theme *theme) override;
  void draw(NVGcontext *ctx) override;

  int item_horizontal_padding() const { return m_item_horizontal_padding; }
  int item_vertical_padding() const { return m_item_vertical_padding; }
  float primary_font_size() const { return m_primary_font_size; }
  float secondary_font_size() const { return m_secondary_font_size; }
  float primary_line_height() const { return m_primary_line_height; }
  float secondary_line_height() const { return m_secondary_line_height; }
  int leading_icon_size() const { return m_icon_size; }
  const Color &primary_text_color() const { return m_primary_color; }
  const Color &secondary_text_color() const { return m_secondary_color; }
  const Color &meta_text_color() const { return m_meta_color; }
  const Color &hover_background_color() const { return m_hover_background; }
  const Color &selected_background_color() const { return m_selected_background; }
  const Color &selected_border_color() const { return m_selected_border; }

  void notify_item_pressed(FluentWebListItem *item, int modifiers);

protected:
  void refresh_metrics();
  void rebuild_layout();
  void refresh_items_from_provider();
  void update_focus_highlight();
  int index_for_item(const FluentWebListItem *item) const;
  void select_single(int index);
  void toggle_selection(int index);

  std::vector<FluentWebListItem *> m_items;
  std::vector<int> m_selected_indices;
  SelectionMode m_selection_mode;
  int m_active_index;
  int m_focus_index;

  std::function<void(const std::vector<int> &)> m_selection_callback;
  std::function<void(int)> m_activation_callback;
  std::function<void(int, FluentWebListItem *)> m_item_provider;

  // Cached Fluent tokens
  Color m_background;
  Color m_border_color;
  Color m_primary_color;
  Color m_secondary_color;
  Color m_meta_color;
  Color m_hover_background;
  Color m_selected_background;
  Color m_selected_border;
  Color m_focus_ring_outer;
  Color m_focus_ring_inner;
  int m_corner_radius;
  int m_container_padding;
  int m_item_spacing;
  int m_item_horizontal_padding;
  int m_item_vertical_padding;
  int m_icon_size;
  float m_primary_font_size;
  float m_secondary_font_size;
  float m_primary_line_height;
  float m_secondary_line_height;
};

/**
 * Fluent-styled grid tile supporting thumbnails and captions.
 */
class NANOGUI_EXPORT FluentWebGridItem : public Widget {
public:
  FluentWebGridItem(FluentWebGridView *owner, const std::string &title = "",
                    const std::string &subtitle = "", int icon = 0);

  void set_title(const std::string &title);
  const std::string &title() const { return m_title; }

  void set_subtitle(const std::string &subtitle);
  const std::string &subtitle() const { return m_subtitle; }

  void set_icon(int icon) { m_icon = icon; }
  int icon() const { return m_icon; }

  void set_selected(bool selected);
  bool selected() const { return m_selected; }

  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_enter_event(const Vector2i &p, bool enter) override;

  void apply_metrics();

private:
  FluentWebGridView *m_owner;
  std::string m_title;
  std::string m_subtitle;
  int m_icon;
  bool m_selected;
  bool m_hovered;
};

/**
 * Fluent 2 grid view with selectable tiles and custom data callbacks.
 */
class NANOGUI_EXPORT FluentWebGridView : public Widget {
public:
  explicit FluentWebGridView(Widget *parent);

  FluentWebGridItem *add_item(const std::string &title,
                              const std::string &subtitle = "", int icon = 0);
  void clear_items();
  void set_item_count(size_t count);
  void set_item_provider(const std::function<void(int, FluentWebGridItem *)> &provider);

  void set_columns(int columns);
  int columns() const { return m_columns; }

  void set_tile_size(const Vector2i &size);
  Vector2i tile_size() const { return m_tile_size; }

  void set_selection_mode(FluentWebListView::SelectionMode mode);
  FluentWebListView::SelectionMode selection_mode() const { return m_selection_mode; }

  void set_selection_callback(const std::function<void(const std::vector<int> &)> &cb);
  void set_activation_callback(const std::function<void(int)> &cb);

  const std::vector<int> &selected_indices() const { return m_selected_indices; }
  void set_selected(int index, bool selected);
  void clear_selection();

  void set_theme(Theme *theme) override;
  void perform_layout(NVGcontext *ctx) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  bool keyboard_event(int key, int scancode, int action, int modifiers) override;
  bool focus_event(bool focused) override;
  void draw(NVGcontext *ctx) override;

  const Color &tile_background() const { return m_tile_background; }
  const Color &tile_hover_background() const { return m_tile_hover_background; }
  const Color &tile_selected_background() const { return m_tile_selected_background; }
  const Color &tile_selected_border() const { return m_tile_selected_border; }
  const Color &title_color() const { return m_title_color; }
  const Color &subtitle_color() const { return m_subtitle_color; }
  int tile_padding() const { return m_tile_padding; }
  float title_font_size() const { return m_title_font_size; }
  float subtitle_font_size() const { return m_subtitle_font_size; }

  void notify_item_pressed(FluentWebGridItem *item, int modifiers);

protected:
  void refresh_metrics();
  void refresh_items_from_provider();
  int index_for_item(const FluentWebGridItem *item) const;
  void select_single(int index);
  void toggle_selection(int index);

  std::vector<FluentWebGridItem *> m_items;
  std::vector<int> m_selected_indices;
  FluentWebListView::SelectionMode m_selection_mode;
  int m_columns;
  Vector2i m_tile_size;
  int m_active_index;
  int m_focus_index;

  std::function<void(const std::vector<int> &)> m_selection_callback;
  std::function<void(int)> m_activation_callback;
  std::function<void(int, FluentWebGridItem *)> m_item_provider;

  // Cached Fluent tokens
  Color m_background;
  Color m_border_color;
  Color m_tile_background;
  Color m_tile_hover_background;
  Color m_tile_selected_background;
  Color m_tile_selected_border;
  Color m_title_color;
  Color m_subtitle_color;
  Color m_focus_ring_outer;
  Color m_focus_ring_inner;
  int m_corner_radius;
  int m_container_padding;
  int m_tile_spacing;
  int m_tile_padding;
  float m_title_font_size;
  float m_subtitle_font_size;
};

NAMESPACE_END(nanogui)
