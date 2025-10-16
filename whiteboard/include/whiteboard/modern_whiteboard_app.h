/**
 * \file modern_whiteboard_app.h
 * \brief High-level application shell for the modern whiteboard experience.
 */

#pragma once

#include "whiteboard/auto_save_manager.h"
#include "whiteboard/canvas/canvas_controller.h"
#include "whiteboard/canvas/canvas_view.h"
#include "whiteboard/common.h"
#include "whiteboard/menu_toolbar_module.h"
#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/panels/layers_controller.h"
#include "whiteboard/panels/layers_view.h"
#include "whiteboard/panels/properties_controller.h"
#include "whiteboard/panels/properties_view.h"
#include "whiteboard/panels/text_controller.h"
#include "whiteboard/panels/text_view.h"
#include "whiteboard/properties_panel_module.h"
#include "whiteboard/search_bar.h"
#include "whiteboard/template_gallery.h"
#include "whiteboard/text_panel_module.h"
#include "whiteboard/toolbar/toolbar_controller.h"
#include "whiteboard/toolbar/toolbar_view.h"
#include "whiteboard/toolbar_panel_module.h"
#include "whiteboard/zoom_panel_module.h"

namespace whiteboard {

class ModernWhiteboardApp;

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
  void create_menu_toolbar();
  void create_left_sidebar();
  void create_floating_panels();
  void create_zoom_controls();
  void create_properties_panel();
  void create_text_panel();
  void update_properties_panel();
  void update_layers_panel();
  void show_restore_prompt();
  void update_layout();

  // MVC Architecture
  WhiteboardDocument *m_document = nullptr;
  CanvasView *m_canvas_view = nullptr;
  CanvasController *m_canvas_controller = nullptr;
  ToolbarView *m_toolbar_view = nullptr;
  ToolbarController *m_toolbar_controller = nullptr;
  LayersView *m_layers_view = nullptr;
  LayersController *m_layers_controller = nullptr;
  PropertiesView *m_properties_view = nullptr;
  PropertiesController *m_properties_controller = nullptr;
  TextView *m_text_view = nullptr;
  TextController *m_text_controller = nullptr;

  // Services and Legacy Modules (kept for compatibility)
  AutoSaveManager *m_auto_save_manager = nullptr;
  PropertiesPanelModule *m_properties_panel = nullptr;
  SearchBar *m_search_bar = nullptr;
  TemplateGallery *m_template_gallery = nullptr;
  MenuToolbarModule *m_menu_toolbar = nullptr;
  ToolbarPanelModule *m_left_sidebar = nullptr;
  TextPanelModule *m_text_panel = nullptr;
  ZoomPanelModule *m_zoom_panel = nullptr;
  Label *m_saving_indicator = nullptr;
  std::vector<Button *> m_tool_buttons;

  std::vector<CanvasPage> m_pages;
  int m_current_page = 0;
};

} // namespace whiteboard
