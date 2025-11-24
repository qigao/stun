/**
 * \file modern_whiteboard_app.h
 * \brief High-level application shell for the modern whiteboard experience.
 */

#pragma once

#include "whiteboard/auto_save_manager.h"
#include "whiteboard/canvas/canvas_controller.h"
#include "whiteboard/canvas/canvas_view.h"
#include "whiteboard/common.h"
#include "whiteboard/floating_toolbar.h"
#include "whiteboard/menu_toolbar_module.h"
#include "whiteboard/model/document_observer.h"
#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/panels/layers_controller.h"
#include "whiteboard/panels/layers_view.h"
#include "whiteboard/panels/properties_controller.h"
#include "whiteboard/panels/properties_view.h"
#include "whiteboard/panels/text_controller.h"
#include "whiteboard/panels/text_view.h"
#include "whiteboard/properties_panel_module.h"
#include "whiteboard/search_bar.h"
#include "whiteboard/shape_panel_module.h"
#include "whiteboard/svg/svg_shape_library.h"
#include "whiteboard/toolbar/toolbar_controller.h"
#include "whiteboard/toolbar/toolbar_view.h"
#include "whiteboard/toolbar_panel_module.h"
#include "whiteboard/types.h"
#include "whiteboard/ui/context_menu_module.h"
#include "whiteboard/ui/export_dialog.h"
#include "whiteboard/ui/help_panel_module.h"
#include "whiteboard/ui/toast_notification.h"
#include "whiteboard/zoom_panel_module.h"

#include <stack>
#include <string>
#include <vector>

namespace whiteboard {

class ModernWhiteboardApp;

/**
 * \class ModernWhiteboardApp
 * \brief Top-level window combining the canvas, tool panels, and async services.
 *
 * The application wraps NanoGUI's `Screen` to provide a multi-panel whiteboard
 * UI with layer management, search, and background export services. It also
 * brokers access to undo/redo stacks, guide snaps, and changes in active tools.
 */
class ModernWhiteboardApp : public Screen, public IDocumentObserver {
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
  WhiteboardDocument *get_document() { return m_document; }
  void set_saving_indicator_visible(bool visible);

  // IDocumentObserver interface
  void on_strokes_changed() override {}
  void on_selection_changed() override;
  void on_tool_changed() override {}
  void on_properties_changed() override {}

protected:
  bool resize_event(const Vector2i &size) override;
  bool keyboard_event(int key, int scancode, int action, int modifiers) override;
  bool drop_event(const std::vector<std::string> &filenames) override;

private:
  void create_menu_toolbar();
  void create_left_sidebar();
  void create_floating_panels();
  void create_zoom_controls();
  void create_properties_panel();
  void create_floating_toolbar();
  void update_floating_toolbar();
  void update_properties_panel();
  void update_layers_panel();
  void show_restore_prompt();
  void update_layout();

  // File operations
  void open_file();
  void save_file();
  void save_file_as();
  void import_ddf_file();
  void load_ddf_from_path(const std::string& filepath);
  void toggle_shape_library();
  void show_export_dialog();
  void handle_export(ExportDialog::Format format, ExportDialog::Scope scope, int quality);

  // Toast notifications
  void show_toast(const std::string &message,
                  ToastNotification::Type type = ToastNotification::Type::Info);

  // Help panel
  void toggle_help_panel();

  // Context menu
  void show_context_menu(const nanogui::Vector2i &pos);
  void add_svg_line(int stroke_index, const std::string &param_name, const std::string &default_value);
  void remove_svg_line(int stroke_index, const std::string &param_name, int line_index);
  void edit_svg_line(int stroke_index, const std::string &param_name, int line_index);
  void delete_svg_line(int stroke_index, const std::string &param_name, int line_index);
  void insert_svg_line_above(int stroke_index, const std::string &param_name, int line_index);
  void insert_svg_line_below(int stroke_index, const std::string &param_name, int line_index);
  void duplicate_svg_line(int stroke_index, const std::string &param_name, int line_index);

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
  ShapePanelModule *m_shape_panel = nullptr;
  FloatingToolbar *m_floating_toolbar = nullptr;

  // SVG Shape Library
  SVGShapeLibrary *m_shape_library = nullptr;

  // Services and Legacy Modules (kept for compatibility)
  AutoSaveManager *m_auto_save_manager = nullptr;
  PropertiesPanelModule *m_properties_panel = nullptr;
  SearchBar *m_search_bar = nullptr;
  MenuToolbarModule *m_menu_toolbar = nullptr;
  ToolbarPanelModule *m_left_sidebar = nullptr;
  ZoomPanelModule *m_zoom_panel = nullptr;
  Label *m_saving_indicator = nullptr;
  std::vector<Button *> m_tool_buttons;

  std::vector<CanvasPage> m_pages;
  int m_current_page = 0;

  // File management
  std::string m_current_file_path;
  std::string m_last_directory;

  // Toast notifications
  std::vector<ToastNotification *> m_toast_stack;

  // Help panel
  HelpPanelModule *m_help_panel = nullptr;

  // Export dialog
  ExportDialog *m_export_dialog = nullptr;

  // Context menu
  ContextMenuModule *m_context_menu = nullptr;
};

} // namespace whiteboard
