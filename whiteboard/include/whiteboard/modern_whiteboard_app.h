/**
 * \file modern_whiteboard_app.h
 * \brief High-level application shell for the modern whiteboard experience.
 */

#pragma once

#include "whiteboard/common.h"
#include "whiteboard/layers_panel.h"
#include "whiteboard/modern_canvas.h"
#include "whiteboard/properties_panel.h"
#include "whiteboard/search_bar.h"
#include "whiteboard/template_gallery.h"

namespace whiteboard {

class ModernWhiteboardApp;

/**
 * \class AutoSaveManager
 * \brief Coordinates background persistence for whiteboard sessions.
 *
 * The manager owns a lightweight timer loop that snapshots the active canvas
 * to local storage whenever content changes. It exposes hooks so UI code can
 * start or suspend auto-save, force an immediate save, and restore sessions
 * after crashes.
 */
class AutoSaveManager {
public:
  explicit AutoSaveManager(ModernWhiteboardApp *app);

  void start();
  void stop();
  void mark_changed();
  void update();
  void save_now();
  bool has_auto_save_data();
  void load_from_local_storage();
  void clear_auto_save();
  bool is_saving() const { return m_is_saving; }

private:
  std::string get_auto_save_path();
  void save_to_local_storage();

  ModernWhiteboardApp *m_app;
  std::chrono::steady_clock::time_point m_last_change_time;
  bool m_has_unsaved_changes;
  bool m_is_saving;
  bool m_auto_save_enabled;
};

/**
 * \class ModernWhiteboardApp
 * \brief Top-level window combining the canvas, tool panels, and async services.
 *
 * The application wraps NanoGUI's `Screen` to provide a multi-panel whiteboard
 * UI with layer management, template galleries, search, and background export
 * services. It also brokers access to undo/redo stacks, guide snaps, and
 * changes in active tools.
 */
class ModernWhiteboardApp : public Screen {
public:
  /**
   * \struct CanvasPage
   * \brief Snapshot of the whiteboard state for a single tab/page.
   *
   * The structure tracks stroke data, undo/redo stacks, viewport transforms,
   * and various sidebar visibility toggles so that switching pages restores an
   * identical editing context.
   */
  struct CanvasPage {
    std::vector<Stroke> strokes;
    std::stack<std::vector<Stroke>> undo_stack;
    std::stack<std::vector<Stroke>> redo_stack;
    float zoom = 1.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;
    Color current_color = Color(255, 100, 100, 255);
    Color fill_color = Color(255, 182, 193, 255);
    float stroke_width = 3.0f;
    Tool current_tool = Tool::Pen;
    bool left_sidebar_visible = true;
    bool right_panel_visible = true;
    int left_sidebar_width = 70;
    int right_panel_width = 60;
    bool snap_to_grid_enabled = false;
    std::vector<Guide> guides;
    std::string name;
    bool exists = false;
  };

  ModernWhiteboardApp();
  ~ModernWhiteboardApp() override;

  std::vector<CanvasPage> &get_pages() { return m_pages; }
  int get_current_page() const { return m_current_page; }
  void set_saving_indicator_visible(bool visible);

protected:
  bool resize_event(const Vector2i &size) override;

private:
  void create_top_toolbar();
  void create_left_sidebar();
  void create_floating_panels();
  void create_zoom_controls();
  void create_properties_panel();
  void update_properties_panel();
  void update_layers_panel();
  void show_restore_prompt();
  void update_layout();

  ModernCanvas *m_canvas = nullptr;
  AutoSaveManager *m_auto_save_manager = nullptr;
  PropertiesPanel *m_properties_panel = nullptr;
  LayersPanel *m_layers_panel = nullptr;
  SearchBar *m_search_bar = nullptr;
  TemplateGallery *m_template_gallery = nullptr;
  Widget *m_top_toolbar = nullptr;
  Widget *m_left_sidebar = nullptr;
  Window *m_zoom_window = nullptr;
  Label *m_saving_indicator = nullptr;
  std::vector<Button *> m_tool_buttons;

  std::vector<CanvasPage> m_pages;
  int m_current_page = 0;
};

} // namespace whiteboard
