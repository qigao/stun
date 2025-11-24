#include "whiteboard/modern_whiteboard_app.h"
#include "whiteboard/clipboard_manager.h"
#include "whiteboard/export_manager.h"
#include "whiteboard/properties_panel_module.h"
#include "whiteboard/canvas/inline_text_editor.h"
#include "whiteboard/ddf/ddf_document.h"
#include "whiteboard/ddf/shape_layer.h"
#include "whiteboard/ddf/data_layer.h"
#include <fmtlog.h>
#include <nanogui.h>
#include <nanogui/keys.h>
#include <sstream>
#include <cmath>
#include <limits>
#include <memory>

namespace whiteboard {

ModernWhiteboardApp::ModernWhiteboardApp()
    : Screen(Vector2i(1400, 900), "Modern Whiteboard - Collaborative Design") {
  set_background(Color(250, 250, 250, 255));

  // Initialize Native File Dialog
  NFD_Init();

  // Create the shared document model (MVC)
  m_document = new WhiteboardDocument();

  // Initialize SVG Shape Library
  logi("ModernWhiteboardApp: Initializing SVG Shape Library");
  m_shape_library = new SVGShapeLibrary();
  bool library_loaded = m_shape_library->load_library("shapes/library.json");
  if (library_loaded) {
    logi("ModernWhiteboardApp: Shape library loaded successfully");
  } else {
    loge("ModernWhiteboardApp: Failed to load shape library!");
  }

  // Register app as document observer for floating toolbar updates
  m_document->add_observer(this);

  // Create Canvas MVC triad
  m_canvas_view = new CanvasView(this, m_document);
  m_canvas_view->set_background_color(Color(245, 245, 245, 255));
  m_canvas_controller = new CanvasController(m_document, m_canvas_view);
  m_canvas_view->set_controller(m_canvas_controller);
  m_canvas_controller->set_shape_library(m_shape_library);

  // Create Toolbar MVC triad
  m_toolbar_view = new ToolbarView(this, m_document);
  m_toolbar_controller = new ToolbarController(m_document, m_toolbar_view);
  m_toolbar_view->set_controller(m_toolbar_controller);
  m_toolbar_view->set_visible(false); // Hidden, using legacy toolbar module

  // Create Layers Panel MVC triad
  m_layers_view = new LayersView(this, m_document);
  m_layers_controller = new LayersController(m_document, m_layers_view);
  m_layers_view->set_controller(m_layers_controller);
  m_layers_view->set_visible(true); // Now using MVC component

  // Create Properties Panel MVC triad (currently not used - using PropertiesPanelModule instead)
  m_properties_view = new PropertiesView(this, m_document);
  // Note: PropertiesController for PropertiesPanelModule is created in create_properties_panel()
  m_properties_view->set_visible(
      false); // Hidden - using PropertiesPanelModule for visual properties

  // Create Text Panel MVC triad
  m_text_view = new TextView(this, m_document);
  m_text_controller = new TextController(m_document, m_text_view);
  m_text_view->set_controller(m_text_controller);
  m_text_view->set_visible(false); // Hidden, using legacy text module

  // Create Shape Panel Module (custom-drawn, like text panel)
  logi("ModernWhiteboardApp: Creating Shape Panel Module");
  m_shape_panel = new ShapePanelModule(this, m_shape_library);
  m_shape_panel->set_position(Vector2i(100, 100));
  m_shape_panel->set_visible(false); // Hidden by default, toggle with menu
  logi("ModernWhiteboardApp: Shape Panel Module created at position (100, 100)");

  // Set callback for shape selection
  m_shape_panel->set_shape_callback([this](const std::string &shape_id) {
    try {
      logi("ModernWhiteboardApp: Shape callback triggered for '{}'", shape_id);
      if (!m_shape_library) {
        loge("ModernWhiteboardApp: Shape library is null!");
        return;
      }

      if (!m_document) {
        loge("ModernWhiteboardApp: Document is null!");
        return;
      }

      // Use drag-and-drop system: switch to SVGShape tool and set pending shape
      m_document->set_current_tool(Tool::SVGShape);
      m_document->set_pending_svg_shape(shape_id);
      
      logi("ModernWhiteboardApp: Ready to place shape '{}' - move mouse over canvas and click", shape_id);
    } catch (const std::exception &e) {
      loge("ModernWhiteboardApp: Exception in shape callback: {}", e.what());
    } catch (...) {
      loge("ModernWhiteboardApp: Unknown exception in shape callback!");
    }
  });

  create_menu_toolbar();
  create_left_sidebar();
  create_floating_panels();
  create_zoom_controls();
  create_properties_panel();
  create_floating_toolbar();

  // Callbacks are now handled by observer pattern in MVC components

  m_auto_save_manager = new AutoSaveManager(this);
  if (m_auto_save_manager->has_auto_save_data())
    show_restore_prompt();
  m_auto_save_manager->start();

  m_saving_indicator = new Label(this, "Saving...", "sans", 14);
  m_saving_indicator->set_color(Color(100, 100, 100, 255));
  m_saving_indicator->set_visible(false);

  // Create help panel
  m_help_panel = new HelpPanelModule(this, m_document);

  // Create export dialog
  m_export_dialog = new ExportDialog(this, m_document);
  m_export_dialog->set_export_callback(
      [this](ExportDialog::Format format, ExportDialog::Scope scope, int quality) {
        handle_export(format, scope, quality);
      });

  // Create context menu
  m_context_menu = new ContextMenuModule(this);

  // Set right-click callback on canvas
  m_canvas_view->set_right_click_callback(
      [this](const nanogui::Vector2i &pos) { show_context_menu(pos); });

  update_layout();
  perform_layout(nvg_context());
}

ModernWhiteboardApp::~ModernWhiteboardApp() {
  // Clean up toast notifications
  for (auto *toast : m_toast_stack) {
    delete toast;
  }
  m_toast_stack.clear();

  // Unregister observers before deleting controllers
  if (m_document && m_properties_panel) {
    m_document->remove_observer(m_properties_panel);
  }

  delete m_auto_save_manager;
  m_auto_save_manager = nullptr;

  delete m_canvas_controller;
  m_canvas_controller = nullptr;

  delete m_toolbar_controller;
  m_toolbar_controller = nullptr;

  delete m_layers_controller;
  m_layers_controller = nullptr;

  delete m_properties_controller;
  m_properties_controller = nullptr;

  delete m_text_controller;
  m_text_controller = nullptr;

  delete m_shape_library;
  m_shape_library = nullptr;

  // Quit Native File Dialog
  NFD_Quit();

  // Views and document are deleted by parent widget cleanup
}

void ModernWhiteboardApp::set_saving_indicator_visible(bool visible) {
  if (m_saving_indicator)
    m_saving_indicator->set_visible(visible);
}

bool ModernWhiteboardApp::resize_event(const Vector2i &size) {
  bool handled = Screen::resize_event(size);
  update_layout();
  perform_layout(nvg_context());
  return handled;
}

void ModernWhiteboardApp::create_menu_toolbar() {
  m_menu_toolbar = new MenuToolbarModule(this);
  m_menu_toolbar->clear_items();

  // File operations
  m_menu_toolbar->add_menu_item("New", FA_FILE, [this]() {
    if (m_document) {
      // Clear all strokes
      m_document->remove_strokes(std::vector<int>());
      // Actually, we need to remove all strokes, let me fix this properly
      std::vector<int> all_indices;
      for (size_t i = 0; i < m_document->get_strokes().size(); ++i) {
        all_indices.push_back(static_cast<int>(i));
      }
      if (!all_indices.empty()) {
        m_document->remove_strokes(all_indices);
      }
    }
    update_layers_panel();
    update_properties_panel();
  });

  m_menu_toolbar->add_menu_item("Open", FA_FOLDER_OPEN, [this]() { open_file(); });

  m_menu_toolbar->add_menu_item("Save", FA_SAVE, [this]() { save_file(); });

  m_menu_toolbar->add_menu_item("Save As", FA_SAVE, [this]() { save_file_as(); });

  m_menu_toolbar->add_menu_item("Import DDF", FA_FILE_IMPORT, [this]() { import_ddf_file(); });

  m_menu_toolbar->add_separator();

  // Edit operations
  m_menu_toolbar->add_menu_item("Undo", FA_UNDO, [this]() {
    if (m_document) {
      m_document->undo();
    }
    update_layers_panel();
    update_properties_panel();
  });

  m_menu_toolbar->add_menu_item("Redo", FA_REDO, [this]() {
    if (m_document) {
      m_document->redo();
    }
    update_layers_panel();
    update_properties_panel();
  });

  m_menu_toolbar->add_separator();

  // View operations
  m_menu_toolbar->add_menu_item("Search", FA_SEARCH, [this]() {
    if (!m_search_bar)
      return;
    m_search_bar->set_visible(!m_search_bar->visible());
    update_layout();
  });

  m_menu_toolbar->add_menu_item("Shape Library", FA_SHAPES, [this]() { toggle_shape_library(); });

  m_menu_toolbar->add_menu_item("Export", FA_DOWNLOAD, [this]() { show_export_dialog(); });

  m_menu_toolbar->add_menu_item("Dark Mode", FA_MOON, [this]() {
    if (m_document) {
      bool new_mode = !m_document->get_dark_mode();
      m_document->set_dark_mode(new_mode);
      show_toast(new_mode ? "Dark mode enabled" : "Light mode enabled",
                 ToastNotification::Type::Info);
    }
  });

  m_menu_toolbar->add_menu_item("Snap to Grid", FA_TH, [this]() {
    if (m_document) {
      bool new_state = !m_document->get_snap_enabled();
      m_document->set_snap_enabled(new_state);
      show_toast(new_state ? "Snap to grid enabled" : "Snap to grid disabled",
                 ToastNotification::Type::Info);
    }
  });

  m_menu_toolbar->add_menu_item("Show Grid", FA_BORDER_ALL, [this]() {
    if (m_document) {
      bool new_state = !m_document->get_grid_visible();
      m_document->set_grid_visible(new_state);
      // Toast removed - was causing widget tree corruption during draw cycle
    }
  });

  m_menu_toolbar->add_menu_item("Show Guides", FA_RULER_COMBINED, [this]() {
    if (m_document) {
      bool new_state = !m_document->get_guides_visible();
      m_document->set_guides_visible(new_state);
      // Toast removed - was causing widget tree corruption during draw cycle
    }
  });

  m_menu_toolbar->add_separator();

  m_menu_toolbar->add_menu_item("Settings", FA_COG, [this]() {
    show_toast("Settings dialog coming soon", ToastNotification::Type::Info);
  });
  m_menu_toolbar->add_menu_item("Help", FA_QUESTION_CIRCLE, [this]() { toggle_help_panel(); });

  m_menu_toolbar->layout_items();

  // Update widget size to match calculated preferred size
  Vector2i pref_size = m_menu_toolbar->preferred_size(nvg_context());
  m_menu_toolbar->set_size(pref_size);
}

void ModernWhiteboardApp::create_left_sidebar() {
  m_left_sidebar = new ToolbarPanelModule(this, Orientation::Vertical);

  // Map toolbar buttons to canvas tools
  m_left_sidebar->set_tool_callback([this](int button_id) {
    // Button mapping:
    // 0: lock -> Lock/Unlock selected shapes
    // 1: hand -> Pan
    // 2: cursor -> Select
    // 3: square -> Rectangle
    // 4: diamond -> Rectangle (rotated)
    // 5: circle -> Circle
    // 6: arrow -> Arrow
    // 7: line -> Line
    // 8: pen -> Pen
    // 9: text -> Text
    // 10: image -> Image
    // 11: rotate (not implemented - use Ctrl+R)
    // 12: tree (not implemented)

    switch (button_id) {
    case 0: // Lock/Unlock selected shapes
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          auto strokes = m_document->get_strokes();
          // Check if first selected is locked to determine action
          bool is_locked = strokes[selected[0]].locked;
          for (int idx : selected) {
            m_document->set_stroke_locked(idx, !is_locked);
          }
          show_toast(is_locked ? "Unlocked " + std::to_string(selected.size()) + " shape" +
                                     (selected.size() == 1 ? "" : "s")
                               : "Locked " + std::to_string(selected.size()) + " shape" +
                                     (selected.size() == 1 ? "" : "s"),
                     ToastNotification::Type::Info);
        } else {
          show_toast("Select shapes to lock/unlock", ToastNotification::Type::Warning);
        }
      }
      break;
    case 1:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Pan);
      break;
    case 2:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Select);
      break;
    case 3:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Rectangle);
      break;
    case 4:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Diamond);
      break;
    case 5:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Circle);
      break;
    case 6:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Arrow);
      break;
    case 7:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Line);
      break;
    case 8:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Pen);
      break;
    case 9:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Text);
      break;
    case 10:
      if (m_toolbar_controller)
        m_toolbar_controller->select_tool(Tool::Image);
      break;
    default:
      break; // Lock, rotate, tree not implemented
    }
  });

  // Perform layout to calculate size based on children
  m_left_sidebar->perform_layout(nvg_context());

  // Position it at the top left
  m_left_sidebar->set_position(Vector2i(20, 100));

  // Set initial tool to match the default selected button (cursor/Select)
  if (m_toolbar_controller) {
    m_toolbar_controller->select_tool(Tool::Select);
  }
}

void ModernWhiteboardApp::create_floating_panels() {
  // Legacy search bar (kept for search functionality)
  m_search_bar = new SearchBar(this, m_document);
  m_search_bar->set_visible(false);
}

void ModernWhiteboardApp::create_zoom_controls() {
  m_zoom_panel = new ZoomPanelModule(this);
  m_zoom_panel->set_zoom_callback([this](float action) {
    if (m_document) {
      float current_zoom = m_document->get_zoom();
      if (action < 0) {
        // Zoom out
        float new_zoom = current_zoom / 1.2f;
        m_document->set_zoom(std::max(0.25f, new_zoom));
      } else if (action > 0) {
        // Zoom in
        float new_zoom = current_zoom * 1.2f;
        m_document->set_zoom(std::min(6.0f, new_zoom));
      } else {
        // Reset zoom
        m_document->set_zoom(1.0f);
      }
      m_zoom_panel->set_zoom_level(m_document->get_zoom());
    }
  });

  // Perform layout to calculate size
  m_zoom_panel->perform_layout(nvg_context());

  m_zoom_panel->set_visible(true);
}

void ModernWhiteboardApp::create_properties_panel() {
  m_properties_panel = new PropertiesPanelModule(this);
  m_properties_panel->set_visible(false); // Hidden by default, shown on Ctrl+Click
  
  // Create controller
  m_properties_controller = new PropertiesController(m_document, m_properties_panel);
  
  // Connect controller to view
  m_properties_panel->set_controller(m_properties_controller);
  m_properties_panel->set_document(m_document);
  
  // Register as observer
  m_document->add_observer(m_properties_panel);
}

void ModernWhiteboardApp::create_floating_toolbar() {
  m_floating_toolbar = new FloatingToolbar(this);
  m_floating_toolbar->set_visible(false);

  // Connect alignment callbacks
  m_floating_toolbar->set_align_left_callback([this]() {
    if (m_canvas_controller) {
      m_canvas_controller->align_selection_left();
      show_toast("Aligned left", ToastNotification::Type::Info);
    }
  });

  m_floating_toolbar->set_align_right_callback([this]() {
    if (m_canvas_controller) {
      m_canvas_controller->align_selection_right();
      show_toast("Aligned right", ToastNotification::Type::Info);
    }
  });

  m_floating_toolbar->set_align_top_callback([this]() {
    if (m_canvas_controller) {
      m_canvas_controller->align_selection_top();
      show_toast("Aligned top", ToastNotification::Type::Info);
    }
  });

  m_floating_toolbar->set_align_bottom_callback([this]() {
    logi("Align bottom button clicked");
    if (m_canvas_controller) {
      m_canvas_controller->align_selection_bottom();
      show_toast("Aligned bottom", ToastNotification::Type::Info);
      logi("Align bottom complete");
    } else {
      loge("Canvas controller is null!");
    }
  });

  // Connect group/ungroup callbacks
  m_floating_toolbar->set_group_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (selected.size() > 1) {
        auto strokes = m_document->get_strokes();
        int max_group_id = -1;
        for (const auto &stroke : strokes) {
          if (stroke.group_id > max_group_id) {
            max_group_id = stroke.group_id;
          }
        }
        int new_group_id = max_group_id + 1;

        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            Stroke updated = strokes[idx];
            updated.group_id = new_group_id;
            m_document->update_stroke(idx, updated);
          }
        }
        show_toast("Grouped " + std::to_string(selected.size()) + " shapes",
                   ToastNotification::Type::Success);
      }
    }
  });

  m_floating_toolbar->set_ungroup_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      auto strokes = m_document->get_strokes();
      for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          if (strokes[idx].group_id >= 0) {
            Stroke updated = strokes[idx];
            updated.group_id = -1;
            m_document->update_stroke(idx, updated);
          }
        }
      }
      show_toast("Ungrouped " + std::to_string(selected.size()) + " shape" +
                     (selected.size() == 1 ? "" : "s"),
                 ToastNotification::Type::Info);
    }
  });

  // Connect duplicate callback
  m_floating_toolbar->set_duplicate_callback([this]() {
    logi("Duplicate button clicked");
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      logi("   Selected indices: {}", selected.size());
      if (!selected.empty()) {
        auto strokes = m_document->get_strokes();
        std::vector<int> new_indices;

        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            const Stroke &original = strokes[idx];
            logi("   Duplicating stroke {}: tool={}, name='{}', svg_data={} bytes", idx,
                 static_cast<int>(original.tool), original.name, original.svg_data.size());
            Stroke duplicate = original;
            for (auto &pt : duplicate.points) {
              pt.x += 20.0f;
              pt.y += 20.0f;
            }

            logi("   Adding duplicate...");
            m_document->add_stroke(duplicate);
            new_indices.push_back(static_cast<int>(m_document->get_strokes().size()) - 1);

            logi("   Duplicate added");
          }
        }

        logi("   Setting selection to {} new strokes", new_indices.size());
        m_document->set_selection(new_indices);
        show_toast("Duplicated " + std::to_string(selected.size()) + " shape" +
                       (selected.size() == 1 ? "" : "s"),
                   ToastNotification::Type::Success);

        logi("Duplicate complete");
      }
    }
  });

  // Connect layer order callbacks
  m_floating_toolbar->set_bring_forward_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (!selected.empty()) {
        std::sort(selected.begin(), selected.end(), std::greater<int>());
        for (int idx : selected) {
          int target = std::min(idx + 1, static_cast<int>(m_document->get_strokes().size()) - 1);
          if (idx != target) {
            m_document->reorder_stroke(idx, target);
          }
        }
        show_toast("Brought forward", ToastNotification::Type::Info);
      }
    }
  });

  m_floating_toolbar->set_send_backward_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (!selected.empty()) {
        std::sort(selected.begin(), selected.end());
        for (int idx : selected) {
          int target = std::max(idx - 1, 0);
          if (idx != target) {
            m_document->reorder_stroke(idx, target);
          }
        }
        show_toast("Sent backward", ToastNotification::Type::Info);
      }
    }
  });

  // Connect delete callback
  m_floating_toolbar->set_delete_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (!selected.empty()) {
        m_document->remove_strokes(selected);
        show_toast("Deleted " + std::to_string(selected.size()) + " shape" +
                       (selected.size() == 1 ? "" : "s"),
                   ToastNotification::Type::Warning);
      }
    }
  });
}

void ModernWhiteboardApp::update_floating_toolbar() {
  if (!m_floating_toolbar || !m_document) {
    return;
  }

  auto selected = m_document->get_selected_indices();

  if (selected.empty()) {
    m_floating_toolbar->hide();
    return;
  }

  // Check if any selected shapes are grouped
  bool can_ungroup = false;
  auto strokes = m_document->get_strokes();
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      if (strokes[idx].group_id >= 0) {
        can_ungroup = true;
        break;
      }
    }
  }

  // Calculate bounds of selection
  float min_x = std::numeric_limits<float>::max();
  float min_y = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_y = std::numeric_limits<float>::lowest();

  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float x1, y1, x2, y2;
      strokes[idx].get_bounds(x1, y1, x2, y2);
      min_x = std::min(min_x, x1);
      min_y = std::min(min_y, y1);
      max_x = std::max(max_x, x2);
      max_y = std::max(max_y, y2);
    }
  }

  // Calculate center and size of selection
  float center_x = (min_x + max_x) / 2.0f;
  float center_y = (min_y + max_y) / 2.0f;
  float width = max_x - min_x;
  float height = max_y - min_y;
  float diagonal = std::sqrt(width * width + height * height);
  
  // Use golden ratio (0.618) for distance from shape
  // Position at 45 degrees (top-right) from center
  const float GOLDEN_RATIO = 0.618f;
  const float ANGLE_45_DEG = 3.14159265f / 4.0f; // 45 degrees in radians
  
  float distance = diagonal * GOLDEN_RATIO;
  float offset_x = distance * std::cos(ANGLE_45_DEG);
  float offset_y = -distance * std::sin(ANGLE_45_DEG); // Negative for upward
  
  nanogui::Vector2f canvas_pos(center_x + offset_x, center_y + offset_y);
  nanogui::Vector2f screen_pos = m_canvas_view->canvas_to_global(canvas_pos);

  // Show toolbar
  m_floating_toolbar->show_at(
      nanogui::Vector2i(static_cast<int>(screen_pos.x()), static_cast<int>(screen_pos.y())),
      true, // can_group
      can_ungroup, static_cast<int>(selected.size()));
}

void ModernWhiteboardApp::update_properties_panel() {
  // PropertiesPanelModule is self-contained and doesn't need updates from canvas
  // It manages its own state through button interactions
}

void ModernWhiteboardApp::update_layers_panel() {
  // No longer needed - MVC LayersView updates automatically via observer pattern
}

void ModernWhiteboardApp::show_restore_prompt() {
  auto *dialog =
      new MessageDialog(this, MessageDialog::Type::Question, "Restore Session",
                        "A previous auto-save was found. Restore it?", "Restore", "Discard", true);
  dialog->set_callback([this](int choice) {
    if (!m_auto_save_manager)
      return;
    if (choice == 0) {
      m_auto_save_manager->load_from_local_storage();
      update_layers_panel();
      update_properties_panel();
    } else {
      m_auto_save_manager->clear_auto_save();
    }
  });
}

void ModernWhiteboardApp::update_layout() {
  // Menu toolbar positioned at top
  if (m_menu_toolbar) {
    m_menu_toolbar->set_position(Vector2i(20, 20));
  }

  // Left sidebar positioned below menu toolbar
  if (m_left_sidebar) {
    int menu_offset = m_menu_toolbar ? 80 : 0;
    m_left_sidebar->set_position(Vector2i(20, 20 + menu_offset));
  }

  // Canvas takes full width and height
  if (m_canvas_view) {
    m_canvas_view->set_position(Vector2i(0, 0));
    m_canvas_view->set_fixed_size(Vector2i(width(), height()));
  }

  // Position MVC LayersView on the right side
  if (m_layers_view) {
    m_layers_view->set_position(Vector2i(width() - 240, 20));
  }

  if (m_properties_panel) {
    m_properties_panel->set_position(Vector2i(width() - 390, 260));
  }

  if (m_zoom_panel) {
    m_zoom_panel->set_position(Vector2i(width() / 2 - 120, height() - 112));
  }

  if (m_search_bar) {
    m_search_bar->set_position(Vector2i(20, height() - 240));
  }

  if (m_saving_indicator) {
    m_saving_indicator->set_position(Vector2i(width() - 150, 30));
  }
}

bool ModernWhiteboardApp::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (Screen::keyboard_event(key, scancode, action, modifiers))
    return true;

  // Handle keyboard shortcuts
  if (action == NANOGUI_KEY_PRESS || action == GLFW_REPEAT) {
    // Ctrl+C - Copy
    if (key == NANOGUI_KEY_C && NANOGUI_HAS_CTRL(modifiers)) {
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          ClipboardManager::copy(m_document->get_strokes(), selected);
          show_toast("Copied " + std::to_string(selected.size()) + " shape" +
                         (selected.size() == 1 ? "" : "s"),
                     ToastNotification::Type::Success);
        }
      }
      return true;
    }

    // Ctrl+X - Cut
    if (key == NANOGUI_KEY_X && NANOGUI_HAS_CTRL(modifiers)) {
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          // Copy to clipboard first
          ClipboardManager::copy(m_document->get_strokes(), selected);
          // Then remove from document
          m_document->remove_strokes(selected);
          show_toast("Cut " + std::to_string(selected.size()) + " shape" +
                         (selected.size() == 1 ? "" : "s"),
                     ToastNotification::Type::Success);
        }
      }
      return true;
    }

    // Ctrl+V - Paste
    if (key == NANOGUI_KEY_V && NANOGUI_HAS_CTRL(modifiers)) {
      if (m_document && ClipboardManager::has_content()) {
        auto pasted = ClipboardManager::paste();
        std::vector<int> new_indices;
        for (const auto &stroke : pasted) {
          m_document->add_stroke(stroke);
          new_indices.push_back(static_cast<int>(m_document->get_strokes().size()) - 1);
        }
        m_document->set_selection(new_indices);
        show_toast("Pasted " + std::to_string(pasted.size()) + " shape" +
                       (pasted.size() == 1 ? "" : "s"),
                   ToastNotification::Type::Info);
      }
      return true;
    }

    // Ctrl+D - Duplicate
    if (key == NANOGUI_KEY_D && NANOGUI_HAS_CTRL(modifiers)) {
      logi("Ctrl+D pressed - Duplicate");
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          auto strokes = m_document->get_strokes();
          std::vector<int> new_indices;
          for (int idx : selected) {
            if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
              Stroke duplicate = strokes[idx];
              // Offset by 20 pixels
              for (auto &pt : duplicate.points) {
                pt.x += 20.0f;
                pt.y += 20.0f;
              }
              m_document->add_stroke(duplicate);
              new_indices.push_back(static_cast<int>(m_document->get_strokes().size()) - 1);
            }
          }
          m_document->set_selection(new_indices);
          show_toast("Duplicated " + std::to_string(selected.size()) + " shape" +
                         (selected.size() == 1 ? "" : "s"),
                     ToastNotification::Type::Success);
        }
      }
      return true;
    }

    // Ctrl+G - Group / Ctrl+Shift+G - Ungroup
    if (key == NANOGUI_KEY_G && NANOGUI_HAS_CTRL(modifiers)) {
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          if (NANOGUI_HAS_SHIFT(modifiers)) {
            // Ungroup
            auto strokes = m_document->get_strokes();
            for (int idx : selected) {
              if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
                if (strokes[idx].group_id >= 0) {
                  Stroke updated = strokes[idx];
                  updated.group_id = -1;
                  m_document->update_stroke(idx, updated);
                }
              }
            }
            // Toast removed - causes crash during draw cycle
          } else {
            // Group
            if (selected.size() > 1) {
              // Find next available group ID
              auto strokes = m_document->get_strokes();
              int max_group_id = -1;
              for (const auto &stroke : strokes) {
                if (stroke.group_id > max_group_id) {
                  max_group_id = stroke.group_id;
                }
              }
              int new_group_id = max_group_id + 1;

              // Assign group ID to all selected strokes
              for (int idx : selected) {
                if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
                  Stroke updated = strokes[idx];
                  updated.group_id = new_group_id;
                  m_document->update_stroke(idx, updated);
                }
              }
              // Toast removed - causes crash during draw cycle
            }
          }
        }
      }
      return true;
    }

    // Ctrl+] - Bring Forward / Ctrl+Shift+] - Bring to Front
    if (key == NANOGUI_KEY_RIGHTBRACKET && NANOGUI_HAS_CTRL(modifiers)) {
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          if (NANOGUI_HAS_SHIFT(modifiers)) {
            // Bring to front
            std::sort(selected.begin(), selected.end(), std::greater<int>());
            for (int idx : selected) {
              int target = static_cast<int>(m_document->get_strokes().size()) - 1;
              if (idx != target) {
                m_document->reorder_stroke(idx, target);
              }
            }
            // Toast removed - causes crash during draw cycle
          } else {
            // Bring forward one layer
            std::sort(selected.begin(), selected.end(), std::greater<int>());
            for (int idx : selected) {
              int target =
                  std::min(idx + 1, static_cast<int>(m_document->get_strokes().size()) - 1);
              if (idx != target) {
                m_document->reorder_stroke(idx, target);
              }
            }
            // Toast removed - causes crash during draw cycle
          }
        }
      }
      return true;
    }

    // Ctrl+[ - Send Backward / Ctrl+Shift+[ - Send to Back
    if (key == NANOGUI_KEY_LEFTBRACKET && NANOGUI_HAS_CTRL(modifiers)) {
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          if (NANOGUI_HAS_SHIFT(modifiers)) {
            // Send to back
            std::sort(selected.begin(), selected.end());
            for (int idx : selected) {
              if (idx != 0) {
                m_document->reorder_stroke(idx, 0);
              }
            }
            // Toast removed - causes crash during draw cycle
          } else {
            // Send backward one layer
            std::sort(selected.begin(), selected.end());
            for (int idx : selected) {
              int target = std::max(idx - 1, 0);
              if (idx != target) {
                m_document->reorder_stroke(idx, target);
              }
            }
            // Toast removed - causes crash during draw cycle
          }
        }
      }
      return true;
    }

    // Ctrl+A - Select All
    if (key == NANOGUI_KEY_A && NANOGUI_HAS_CTRL(modifiers)) {
      if (m_document) {
        std::vector<int> all_indices;
        for (size_t i = 0; i < m_document->get_strokes().size(); ++i) {
          all_indices.push_back(static_cast<int>(i));
        }
        m_document->set_selection(all_indices);
        if (!all_indices.empty()) {
          // Toast removed - causes crash during draw cycle
        }
      }
      return true;
    }

    // Escape - Clear Selection
    if (key == NANOGUI_KEY_ESCAPE) {
      if (m_document) {
        m_document->clear_selection();
      }
      return true;
    }

    // Delete/Backspace - Delete Selected
    if (key == NANOGUI_KEY_DELETE || key == NANOGUI_KEY_BACKSPACE) {
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          m_document->remove_strokes(selected);
          // Toast removed - causes crash during draw cycle
        }
      }
      return true;
    }

    // Arrow Keys - Nudge Selection
    if (key == NANOGUI_KEY_LEFT || key == NANOGUI_KEY_RIGHT || key == NANOGUI_KEY_UP ||
        key == NANOGUI_KEY_DOWN) {
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          // Shift = 10px nudge, otherwise 1px
          float nudge = NANOGUI_HAS_SHIFT(modifiers) ? 10.0f : 1.0f;
          auto strokes = m_document->get_strokes();

          for (int idx : selected) {
            if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
              Stroke updated = strokes[idx];

              // Nudge all points
              for (auto &pt : updated.points) {
                if (key == NANOGUI_KEY_LEFT)
                  pt.x -= nudge;
                if (key == NANOGUI_KEY_RIGHT)
                  pt.x += nudge;
                if (key == NANOGUI_KEY_UP)
                  pt.y -= nudge;
                if (key == NANOGUI_KEY_DOWN)
                  pt.y += nudge;
              }

              m_document->update_stroke(idx, updated);
            }
          }
        }
      }
      return true; // Always consume arrow keys to prevent scrolling
    }

    // Ctrl+R - Rotate 90° / Ctrl+Shift+R - Rotate -90°
    if (key == NANOGUI_KEY_R && NANOGUI_HAS_CTRL(modifiers)) {
      if (m_document) {
        auto selected = m_document->get_selected_indices();
        if (!selected.empty()) {
          auto strokes = m_document->get_strokes();
          float angle = NANOGUI_HAS_SHIFT(modifiers) ? -M_PI / 2.0f : M_PI / 2.0f;

          for (int idx : selected) {
            if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
              Stroke updated = strokes[idx];
              updated.rotation += angle;
              m_document->update_stroke(idx, updated);
            }
          }

          std::string direction = NANOGUI_HAS_SHIFT(modifiers) ? "counter-clockwise" : "clockwise";
          show_toast("Rotated 90° " + direction, ToastNotification::Type::Info);
        }
      }
      return true;
    }

    // Ctrl+Shift+; - Toggle Snap to Grid
    if (key == NANOGUI_KEY_SEMICOLON && NANOGUI_HAS_CTRL(modifiers) &&
        NANOGUI_HAS_SHIFT(modifiers)) {
      if (m_document) {
        bool new_state = !m_document->get_snap_enabled();
        m_document->set_snap_enabled(new_state);
        show_toast(new_state ? "Snap to grid enabled" : "Snap to grid disabled",
                   ToastNotification::Type::Info);
      }
      return true;
    }

    // F1 or Ctrl+/ - Toggle Help Panel
    if (key == NANOGUI_KEY_F1 || (key == NANOGUI_KEY_SLASH && NANOGUI_HAS_CTRL(modifiers))) {
      toggle_help_panel();
      return true;
    }

    // Ctrl+O - Open
    if (key == NANOGUI_KEY_O && NANOGUI_HAS_CTRL(modifiers)) {
      open_file();
      return true;
    }
    // Ctrl+S - Save
    if (key == NANOGUI_KEY_S && NANOGUI_HAS_CTRL(modifiers)) {
      // Shift+Ctrl+S - Save As
      if (NANOGUI_HAS_SHIFT(modifiers)) {
        save_file_as();
      } else {
        save_file();
      }
      return true;
    }
  }

  return false;
}

bool ModernWhiteboardApp::drop_event(const std::vector<std::string> &filenames) {
  if (filenames.empty() || !m_canvas_view || !m_document) {
    return false;
  }

  // Get drop position (center of viewport)
  nanogui::Vector2i viewport_center(m_canvas_view->width() / 2, m_canvas_view->height() / 2);
  nanogui::Vector2f canvas_pos = m_canvas_view->local_to_canvas(viewport_center);

  int imported_count = 0;
  int failed_count = 0;
  float offset = 0.0f;

  for (const auto &file_path : filenames) {
    // Apply offset for multiple files to avoid overlap
    nanogui::Vector2f pos(canvas_pos.x() + offset, canvas_pos.y() + offset);

    bool success = false;

    if (CanvasView::is_image_file(file_path)) {
      // Import as image
      success = m_canvas_view->import_image(file_path, pos);
      if (success) {
        imported_count++;
        offset += 30.0f; // Offset next file
      } else {
        failed_count++;
      }
    } else if (CanvasView::is_svg_file(file_path)) {
      // Import as SVG shape
      success = m_canvas_view->import_svg_shape(file_path, pos);
      if (success) {
        imported_count++;
        offset += 30.0f; // Offset next file
      } else {
        failed_count++;
      }
    } else {
      // Unsupported file type
      failed_count++;
      logw("ModernWhiteboardApp: Unsupported file type: {}", file_path);
    }
  }

  // Show toast notification
  if (imported_count > 0) {
    show_toast("Imported " + std::to_string(imported_count) + " file" +
                   (imported_count == 1 ? "" : "s"),
               ToastNotification::Type::Success);
  }

  if (failed_count > 0) {
    show_toast("Failed to import " + std::to_string(failed_count) + " file" +
                   (failed_count == 1 ? "" : "s"),
               ToastNotification::Type::Error);
  }

  return imported_count > 0;
}

void ModernWhiteboardApp::open_file() {
  nfdchar_t *out_path = nullptr;
  nfdfilteritem_t filter_item[1] = {{"Whiteboard Files", "whiteboard"}};

  nfdopendialogu8args_t args = {0};
  args.filterList = filter_item;
  args.filterCount = 1;
  args.defaultPath = m_last_directory.empty() ? nullptr : m_last_directory.c_str();

  nfdresult_t result = NFD_OpenDialogU8_With(&out_path, &args);

  if (result == NFD_OKAY) {
    std::string file_path(out_path);
    NFD_FreePathU8(out_path);

    // Load the file
    if (m_document && m_document->load_from_file(file_path)) {
      m_current_file_path = file_path;

      // Extract directory for next time
      size_t last_slash = file_path.find_last_of("/\\");
      if (last_slash != std::string::npos) {
        m_last_directory = file_path.substr(0, last_slash);
      }

      // Update UI
      update_layers_panel();
      update_properties_panel();

      // Show success message
      std::string filename = file_path.substr(file_path.find_last_of("/\\") + 1);
      show_toast("Opened " + filename, ToastNotification::Type::Success);
    } else {
      // Show error toast
      show_toast("Failed to open file", ToastNotification::Type::Error);
    }
  } else if (result == NFD_ERROR) {
    std::cerr << "NFD Error: " << NFD_GetError() << std::endl;
  }
  // NFD_CANCEL - user cancelled, do nothing
}

void ModernWhiteboardApp::save_file() {
  if (m_current_file_path.empty()) {
    // No file path set, use Save As
    save_file_as();
    return;
  }

  // Save to current file
  if (m_document && m_document->save_to_file(m_current_file_path)) {
    // Show success toast
    std::string filename = m_current_file_path.substr(m_current_file_path.find_last_of("/\\") + 1);
    show_toast("Saved " + filename, ToastNotification::Type::Success);
  } else {
    // Show error toast
    show_toast("Failed to save file", ToastNotification::Type::Error);
  }
}

void ModernWhiteboardApp::save_file_as() {
  nfdchar_t *out_path = nullptr;
  nfdfilteritem_t filter_item[1] = {{"Whiteboard Files", "whiteboard"}};

  nfdsavedialogu8args_t args = {0};
  args.filterList = filter_item;
  args.filterCount = 1;
  args.defaultPath = m_last_directory.empty() ? nullptr : m_last_directory.c_str();
  args.defaultName = "untitled.whiteboard";

  nfdresult_t result = NFD_SaveDialogU8_With(&out_path, &args);

  if (result == NFD_OKAY) {
    std::string file_path(out_path);
    NFD_FreePathU8(out_path);

    // Ensure .whiteboard extension
    if (file_path.length() < 11 || file_path.substr(file_path.length() - 11) != ".whiteboard") {
      file_path += ".whiteboard";
    }

    // Save the file
    if (m_document && m_document->save_to_file(file_path)) {
      m_current_file_path = file_path;

      // Extract directory for next time
      size_t last_slash = file_path.find_last_of("/\\");
      if (last_slash != std::string::npos) {
        m_last_directory = file_path.substr(0, last_slash);
      }

      // Show success toast
      std::string filename = file_path.substr(file_path.find_last_of("/\\") + 1);
      show_toast("Saved " + filename, ToastNotification::Type::Success);
    } else {
      // Show error toast
      show_toast("Failed to save file", ToastNotification::Type::Error);
    }
  } else if (result == NFD_ERROR) {
    std::cerr << "NFD Error: " << NFD_GetError() << std::endl;
  }
  // NFD_CANCEL - user cancelled, do nothing
}

void ModernWhiteboardApp::import_ddf_file() {
  logi("import_ddf_file: Opening file dialog");
  nfdchar_t *out_path = nullptr;
  nfdfilteritem_t filter_item[1] = {{"DDF Documents", "json"}};

  nfdopendialogu8args_t args = {0};
  args.filterList = filter_item;
  args.filterCount = 1;
  args.defaultPath = m_last_directory.empty() ? nullptr : m_last_directory.c_str();

  nfdresult_t result = NFD_OpenDialogU8_With(&out_path, &args);

  if (result == NFD_OKAY) {
    std::string file_path(out_path);
    NFD_FreePathU8(out_path);

    logi("import_ddf_file: User selected file: {}", file_path);
    load_ddf_from_path(file_path);

    // Extract directory for next time
    size_t last_slash = file_path.find_last_of("/\\");
    if (last_slash != std::string::npos) {
      m_last_directory = file_path.substr(0, last_slash);
    }
  } else if (result == NFD_ERROR) {
    loge("import_ddf_file: NFD Error: {}", NFD_GetError());
    std::cerr << "NFD Error: " << NFD_GetError() << std::endl;
  } else {
    logi("import_ddf_file: User cancelled");
  }
}

void ModernWhiteboardApp::load_ddf_from_path(const std::string& filepath) {
  logi("load_ddf_from_path: Loading DDF from: {}", filepath);
  try {
    // Create DDF document
    auto ddf_doc = std::make_shared<whiteboard::ddf::DDFDocument>();
    
    // Load from file
    logi("load_ddf_from_path: Calling load_from_file...");
    if (!ddf_doc->load_from_file(filepath)) {
      loge("load_ddf_from_path: Failed to load DDF file");
      show_toast("Failed to load DDF file", ToastNotification::Type::Error);
      return;
    }
    
    logi("load_ddf_from_path: File loaded successfully");
    logi("load_ddf_from_path: Validating document...");
    if (!ddf_doc->validate()) {
      auto errors = ddf_doc->get_validation_errors();
      std::string error_msg = "DDF validation errors:\n";
      for (const auto& err : errors) {
        error_msg += "- " + err + "\n";
      }
      loge("load_ddf_from_path: Validation failed:\n{}", error_msg);
      std::cerr << error_msg << std::endl;
      show_toast("DDF validation failed - see console", ToastNotification::Type::Error);
      return;
    }
    
    logi("load_ddf_from_path: Validation passed");
    auto shapes = ddf_doc->shape_layer().get_all_shapes();
    logi("DDF loaded: {} shapes, {} nodes", shapes.size(), ddf_doc->data_layer().get_all_nodes().size());
    
    if (shapes.empty()) {
      auto nodes = ddf_doc->data_layer().get_all_nodes();
      if (!nodes.empty()) {
        logi("No shapes found, generating from {} data nodes", nodes.size());
        // Use grid layout by default for flowcharts
        ddf_doc->generate_from_data("grid");
        shapes = ddf_doc->shape_layer().get_all_shapes();
        logi("Generated {} shapes from data", shapes.size());
        show_toast("Generated " + std::to_string(shapes.size()) + " shapes from data", 
                   ToastNotification::Type::Success);
      } else {
        logi("No shapes or data nodes found in DDF document");
        show_toast("DDF file has no shapes or data", ToastNotification::Type::Warning);
      }
    } else {
      logi("Using {} existing shapes from DDF file", shapes.size());
      show_toast("Loaded " + std::to_string(shapes.size()) + " shapes", 
                 ToastNotification::Type::Success);
    }
    
    // Set in canvas view
    if (m_canvas_view) {
      // Pass shape library to DDF document for SVG shape rendering
      if (m_shape_library) {
        m_canvas_view->set_svg_shape_library(m_shape_library);
      }
      m_canvas_view->set_ddf_document(ddf_doc);
      
      // Zoom to fit DDF content - use try-catch to handle any issues
      try {
        auto all_shapes = ddf_doc->shape_layer().get_all_shapes();
        if (!all_shapes.empty()) {
          float min_x = std::numeric_limits<float>::max();
          float min_y = std::numeric_limits<float>::max();
          float max_x = std::numeric_limits<float>::lowest();
          float max_y = std::numeric_limits<float>::lowest();
          
          bool found_bounds = false;
          for (const auto* shape : all_shapes) {
            if (!shape) continue;
            
            // Safely access geometry with count() checks
            auto& geom = shape->geometry;
            if (geom.count("x") > 0 && geom.count("y") > 0) {
              try {
                float x = geom.at("x");
                float y = geom.at("y");
                float w = geom.count("width") > 0 ? geom.at("width") : 0.0f;
                float h = geom.count("height") > 0 ? geom.at("height") : 0.0f;
                
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x + w);
                max_y = std::max(max_y, y + h);
                found_bounds = true;
              } catch (...) {
                // Skip shapes with invalid geometry
                continue;
              }
            }
          }
          
          // Calculate zoom to fit if we found valid bounds
          if (found_bounds) {
            logi("Found bounds: ({}, {}) to ({}, {})", min_x, min_y, max_x, max_y);
            float content_width = max_x - min_x;
            float content_height = max_y - min_y;
            float canvas_width = static_cast<float>(m_canvas_view->width());
            float canvas_height = static_cast<float>(m_canvas_view->height());
            
            if (content_width > 0 && content_height > 0 && canvas_width > 0 && canvas_height > 0) {
              float zoom_x = canvas_width / content_width;
              float zoom_y = canvas_height / content_height;
              float zoom = std::min(zoom_x, zoom_y) * 0.8f; // 80% to add margin
              
              // Set zoom and center content
              if (m_document) {
                m_document->set_zoom(zoom);
                
                float center_x = (min_x + max_x) / 2.0f;
                float center_y = (min_y + max_y) / 2.0f;
                
                nanogui::Vector2f pan_offset(
                  canvas_width / 2.0f - center_x * zoom,
                  canvas_height / 2.0f - center_y * zoom
                );
                
                m_document->set_pan_offset(pan_offset);
              }
            }
          }
        }
      } catch (const std::exception& e) {
        std::cerr << "Warning: Could not zoom to fit DDF content: " << e.what() << std::endl;
        // Continue anyway - document is loaded, just not zoomed
      }
      
      // Show success message
      std::string filename = filepath.substr(filepath.find_last_of("/\\") + 1);
      show_toast("Loaded DDF: " + filename, ToastNotification::Type::Success);
      
      std::cout << "DDF document loaded successfully: " << filepath << std::endl;
    }
    
  } catch (const std::exception& e) {
    std::cerr << "Error loading DDF: " << e.what() << std::endl;
    show_toast(std::string("Error loading DDF: ") + e.what(), ToastNotification::Type::Error);
  }
}

void ModernWhiteboardApp::toggle_shape_library() {
  if (m_shape_panel) {
    bool is_visible = m_shape_panel->visible();
    bool new_state = !is_visible;
    logi("ModernWhiteboardApp: Toggling shape panel visibility: {} -> {}",
         is_visible ? "visible" : "hidden", new_state ? "visible" : "hidden");
    m_shape_panel->set_visible(new_state);
  } else {
    loge("ModernWhiteboardApp: Shape panel is null, cannot toggle!");
  }
}

void ModernWhiteboardApp::show_toast(const std::string &message, ToastNotification::Type type) {
  // Limit maximum number of visible toasts
  const size_t MAX_TOASTS = 5;

  // Remove old toasts if we have too many
  while (m_toast_stack.size() >= MAX_TOASTS) {
    auto *old_toast = m_toast_stack.front();
    m_toast_stack.erase(m_toast_stack.begin());
    delete old_toast;
  }

  // Create new toast
  auto *toast = new ToastNotification(this);
  m_toast_stack.push_back(toast);

  // Position toast at bottom-center of screen
  int toast_width = 300;
  int toast_height = 60;
  int margin = 20;
  int stack_offset = static_cast<int>(m_toast_stack.size() - 1) * (toast_height + 10);

  int x = (width() - toast_width) / 2;
  int y = height() - margin - toast_height - stack_offset;

  toast->set_position(Vector2i(x, y));
  toast->show(message, type);
}

void ModernWhiteboardApp::toggle_help_panel() {
  if (m_help_panel) {
    if (m_help_panel->is_visible()) {
      m_help_panel->hide();
    } else {
      m_help_panel->show();
    }
  }
}

void ModernWhiteboardApp::show_export_dialog() {
  if (m_export_dialog) {
    m_export_dialog->show();
  }
}

void ModernWhiteboardApp::show_context_menu(const nanogui::Vector2i &pos) {
  if (!m_context_menu || !m_document) {
    return;
  }
  
  // Right-click shows floating toolbar for selected shapes
  update_floating_toolbar();

  // Check if there's an active inline editor with a clicked line
  InlineTextEditor* active_editor = InlineTextEditor::get_active_editor();
  if (active_editor) {
    int clicked_line = active_editor->get_clicked_line_index();
    if (clicked_line >= 0) {
      // Show line-specific context menu
      std::vector<MenuItem> items;
      
      // Get the stroke index and parameter name from the editor
      const auto &selected = m_document->get_selected_indices();
      if (!selected.empty()) {
        int stroke_index = selected[0];
        const auto &strokes = m_document->get_strokes();
        if (stroke_index >= 0 && stroke_index < static_cast<int>(strokes.size())) {
          const Stroke &stroke = strokes[stroke_index];
          
          // Find which parameter is being edited
          // We need to get this from the editor, but for now we'll check common parameters
          std::string param_name;
          for (const auto &param : stroke.svg_parameters) {
            if (param.first == "methods" || param.first == "attributes" || 
                param.first == "properties" || param.first == "operations") {
              // Check if this parameter has multiple lines
              if (param.second.find('\n') != std::string::npos || !param.second.empty()) {
                param_name = param.first;
                break;
              }
            }
          }
          
          if (!param_name.empty()) {
            // Add line-specific menu items
            items.push_back(MenuItem("Edit this line", "", FA_EDIT, 
              [this, stroke_index, param_name, clicked_line]() {
                edit_svg_line(stroke_index, param_name, clicked_line);
              }));
            
            items.push_back(MenuItem("Delete this line", "", FA_TRASH, 
              [this, stroke_index, param_name, clicked_line]() {
                delete_svg_line(stroke_index, param_name, clicked_line);
              }));
            
            items.push_back(MenuItem::Separator());
            
            items.push_back(MenuItem("Insert line above", "", FA_ARROW_UP, 
              [this, stroke_index, param_name, clicked_line]() {
                insert_svg_line_above(stroke_index, param_name, clicked_line);
              }));
            
            items.push_back(MenuItem("Insert line below", "", FA_ARROW_DOWN, 
              [this, stroke_index, param_name, clicked_line]() {
                insert_svg_line_below(stroke_index, param_name, clicked_line);
              }));
            
            items.push_back(MenuItem::Separator());
            
            items.push_back(MenuItem("Duplicate this line", "", FA_CLONE, 
              [this, stroke_index, param_name, clicked_line]() {
                duplicate_svg_line(stroke_index, param_name, clicked_line);
              }));
            
            m_context_menu->show_at(pos, items);
            return;
          }
        }
      }
    }
  }

  const auto &selected = m_document->get_selected_indices();
  if (selected.empty()) {
    return; // No selection, no context menu
  }

  std::vector<MenuItem> items;

  // Check if right-clicked on an SVG shape
  const auto &strokes = m_document->get_strokes();
  bool is_svg_shape = false;
  int svg_stroke_index = -1;
  
  if (!selected.empty()) {
    svg_stroke_index = selected[0];
    if (svg_stroke_index >= 0 && svg_stroke_index < static_cast<int>(strokes.size())) {
      const Stroke &stroke = strokes[svg_stroke_index];
      is_svg_shape = (stroke.tool == Tool::SVGShape);
    }
  }

  // Show different menu for SVG shapes
  if (is_svg_shape) {
    const Stroke &svg_stroke = strokes[svg_stroke_index];
    
    // Dynamically build menu based on shape's text parameters
    bool has_add_items = false;
    bool has_remove_items = false;
    
    // Check for multi-line text parameters
    for (const auto &param : svg_stroke.svg_parameters) {
      const std::string &param_name = param.first;
      
      // Check if this is a multi-line parameter (methods, attributes, properties, operations, etc.)
      if (param_name == "methods" || param_name == "attributes" || 
          param_name == "properties" || param_name == "operations") {
        
        // Capitalize first letter for display
        std::string display_name = param_name;
        if (!display_name.empty()) {
          display_name[0] = std::toupper(display_name[0]);
          // Remove trailing 's' for singular form
          if (display_name.back() == 's') {
            display_name.pop_back();
          }
        }
        
        // Add "Add X" menu item
        items.push_back(MenuItem("Add " + display_name, "", FA_PLUS, 
          [this, svg_stroke_index, param_name]() {
            add_svg_line(svg_stroke_index, param_name, "");
          }));
        has_add_items = true;
      }
    }
    
    if (has_add_items) {
      items.push_back(MenuItem::Separator());
    }
    
    // Add "Remove X" items for parameters that have content
    for (const auto &param : svg_stroke.svg_parameters) {
      const std::string &param_name = param.first;
      
      if (param_name == "methods" || param_name == "attributes" || 
          param_name == "properties" || param_name == "operations") {
        
        // Only show remove if there's content
        if (!param.second.empty()) {
          std::string display_name = param_name;
          if (!display_name.empty()) {
            display_name[0] = std::toupper(display_name[0]);
            if (display_name.back() == 's') {
              display_name.pop_back();
            }
          }
          
          items.push_back(MenuItem("Remove " + display_name, "", FA_MINUS, 
            [this, svg_stroke_index, param_name]() {
              remove_svg_line(svg_stroke_index, param_name, -1);
            }));
          has_remove_items = true;
        }
      }
    }
    
    // Only show menu if there are items
    if (has_add_items || has_remove_items) {
      m_context_menu->show_at(pos, items);
    }
    return;
  }

  // Cut
  items.push_back(MenuItem("Cut", "Ctrl+X", FA_CUT, [this]() {
    const auto &strokes = m_document->get_strokes();
    const auto &selected = m_document->get_selected_indices();
    ClipboardManager::cut(const_cast<std::vector<Stroke> &>(strokes),
                          const_cast<std::vector<int> &>(selected));
    m_document->remove_strokes(selected);
    show_toast("Cut " + std::to_string(selected.size()) + " shape(s)",
               ToastNotification::Type::Success);
  }));

  // Copy
  items.push_back(MenuItem("Copy", "Ctrl+C", FA_COPY, [this]() {
    const auto &strokes = m_document->get_strokes();
    const auto &selected = m_document->get_selected_indices();
    ClipboardManager::copy(strokes, selected);
    show_toast("Copied " + std::to_string(selected.size()) + " shape(s)",
               ToastNotification::Type::Success);
  }));

  // Paste
  bool has_clipboard = ClipboardManager::has_content();
  items.push_back(MenuItem(
      "Paste", "Ctrl+V", FA_PASTE,
      [this]() {
        auto pasted = ClipboardManager::paste();
        std::vector<int> new_indices;
        for (const auto &stroke : pasted) {
          m_document->add_stroke(stroke);
          new_indices.push_back(static_cast<int>(m_document->get_strokes().size()) - 1);
        }
        m_document->set_selection(new_indices);
        show_toast("Pasted " + std::to_string(pasted.size()) + " shape(s)",
                   ToastNotification::Type::Success);
      },
      has_clipboard));

  // Delete
  items.push_back(MenuItem("Delete", "Del", FA_TRASH, [this]() {
    const auto &selected = m_document->get_selected_indices();
    int count = static_cast<int>(selected.size());
    m_document->remove_strokes(selected);
    show_toast("Deleted " + std::to_string(count) + " shape(s)", ToastNotification::Type::Success);
  }));

  items.push_back(MenuItem::Separator());

  // Duplicate
  items.push_back(MenuItem("Duplicate", "Ctrl+D", FA_CLONE, [this]() {
    const auto &strokes = m_document->get_strokes();
    const auto &selected = m_document->get_selected_indices();
    std::vector<int> new_indices;

    for (int idx : selected) {
      if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
        Stroke duplicate = strokes[idx];
        duplicate.move(20.0f, 20.0f);
        m_document->add_stroke(duplicate);
        new_indices.push_back(static_cast<int>(m_document->get_strokes().size()) - 1);
      }
    }

    m_document->set_selection(new_indices);
    show_toast("Duplicated " + std::to_string(new_indices.size()) + " shape(s)",
               ToastNotification::Type::Success);
  }));

  items.push_back(MenuItem::Separator());

  // Group/Ungroup
  bool has_grouped = false;
  bool has_ungrouped = false;
  // strokes already declared above
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      if (strokes[idx].group_id >= 0) {
        has_grouped = true;
      } else {
        has_ungrouped = true;
      }
    }
  }

  if (has_ungrouped && selected.size() > 1) {
    items.push_back(MenuItem("Group", "Ctrl+G", FA_OBJECT_GROUP, [this]() {
      const auto &strokes = m_document->get_strokes();
      const auto &selected = m_document->get_selected_indices();

      // Find next available group ID
      int max_group_id = -1;
      for (const auto &stroke : strokes) {
        max_group_id = std::max(max_group_id, stroke.group_id);
      }
      int new_group_id = max_group_id + 1;

      // Assign group ID to selected strokes
      for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          Stroke modified = strokes[idx];
          modified.group_id = new_group_id;
          m_document->update_stroke(idx, modified);
        }
      }

      show_toast("Grouped " + std::to_string(selected.size()) + " shape(s)",
                 ToastNotification::Type::Success);
    }));
  }

  if (has_grouped) {
    items.push_back(MenuItem("Ungroup", "Ctrl+Shift+G", FA_OBJECT_UNGROUP, [this]() {
      const auto &strokes = m_document->get_strokes();
      const auto &selected = m_document->get_selected_indices();

      for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          Stroke modified = strokes[idx];
          modified.group_id = -1;
          m_document->update_stroke(idx, modified);
        }
      }

      show_toast("Ungrouped " + std::to_string(selected.size()) + " shape(s)",
                 ToastNotification::Type::Success);
    }));
  }

  items.push_back(MenuItem::Separator());

  // Layer ordering
  items.push_back(MenuItem("Bring to Front", "Ctrl+Shift+]", FA_ARROW_UP, [this]() {
    const auto &selected = m_document->get_selected_indices();
    if (!selected.empty()) {
      int idx = selected[0];
      int target = static_cast<int>(m_document->get_strokes().size()) - 1;
      m_document->reorder_stroke(idx, target);
      show_toast("Brought to front", ToastNotification::Type::Success);
    }
  }));

  items.push_back(MenuItem("Bring Forward", "Ctrl+]", FA_ANGLE_UP, [this]() {
    const auto &selected = m_document->get_selected_indices();
    if (!selected.empty()) {
      int idx = selected[0];
      int target = std::min(idx + 1, static_cast<int>(m_document->get_strokes().size()) - 1);
      if (target != idx) {
        m_document->reorder_stroke(idx, target);
        show_toast("Brought forward", ToastNotification::Type::Success);
      }
    }
  }));

  items.push_back(MenuItem("Send Backward", "Ctrl+[", FA_ANGLE_DOWN, [this]() {
    const auto &selected = m_document->get_selected_indices();
    if (!selected.empty()) {
      int idx = selected[0];
      int target = std::max(idx - 1, 0);
      if (target != idx) {
        m_document->reorder_stroke(idx, target);
        show_toast("Sent backward", ToastNotification::Type::Success);
      }
    }
  }));

  items.push_back(MenuItem("Send to Back", "Ctrl+Shift+[", FA_ARROW_DOWN, [this]() {
    const auto &selected = m_document->get_selected_indices();
    if (!selected.empty()) {
      int idx = selected[0];
      m_document->reorder_stroke(idx, 0);
      show_toast("Sent to back", ToastNotification::Type::Success);
    }
  }));

  items.push_back(MenuItem::Separator());

  // Lock/Unlock
  bool has_locked = false;
  bool has_unlocked = false;
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      if (strokes[idx].locked) {
        has_locked = true;
      } else {
        has_unlocked = true;
      }
    }
  }

  if (has_unlocked) {
    items.push_back(MenuItem("Lock", "", FA_LOCK, [this]() {
      const auto &strokes = m_document->get_strokes();
      const auto &selected = m_document->get_selected_indices();

      for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          m_document->set_stroke_locked(idx, true);
        }
      }

      show_toast("Locked " + std::to_string(selected.size()) + " shape(s)",
                 ToastNotification::Type::Success);
    }));
  }

  if (has_locked) {
    items.push_back(MenuItem("Unlock", "", FA_UNLOCK, [this]() {
      const auto &strokes = m_document->get_strokes();
      const auto &selected = m_document->get_selected_indices();

      for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          m_document->set_stroke_locked(idx, false);
        }
      }

      show_toast("Unlocked " + std::to_string(selected.size()) + " shape(s)",
                 ToastNotification::Type::Success);
    }));
  }

  // Hide/Show
  bool has_visible = false;
  bool has_hidden = false;
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      if (strokes[idx].visible) {
        has_visible = true;
      } else {
        has_hidden = true;
      }
    }
  }

  if (has_visible) {
    items.push_back(MenuItem("Hide", "", FA_EYE_SLASH, [this]() {
      const auto &strokes = m_document->get_strokes();
      const auto &selected = m_document->get_selected_indices();

      for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          m_document->set_stroke_visible(idx, false);
        }
      }

      show_toast("Hidden " + std::to_string(selected.size()) + " shape(s)",
                 ToastNotification::Type::Success);
    }));
  }

  if (has_hidden) {
    items.push_back(MenuItem("Show", "", FA_EYE, [this]() {
      const auto &strokes = m_document->get_strokes();
      const auto &selected = m_document->get_selected_indices();

      for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          m_document->set_stroke_visible(idx, true);
        }
      }

      show_toast("Shown " + std::to_string(selected.size()) + " shape(s)",
                 ToastNotification::Type::Success);
    }));
  }

  // Show the context menu
  m_context_menu->show_at(pos, items);
}

void ModernWhiteboardApp::handle_export(ExportDialog::Format format, ExportDialog::Scope scope,
                                        int quality) {
  // Get the strokes to export based on scope
  std::vector<Stroke> strokes_to_export;

  switch (scope) {
  case ExportDialog::Scope::EntireCanvas:
    strokes_to_export = m_document->get_strokes();
    break;

  case ExportDialog::Scope::VisibleArea:
    // TODO: Filter strokes by visible area
    // For now, export all strokes
    strokes_to_export = m_document->get_strokes();
    show_toast("Visible area export not yet implemented, exporting all",
               ToastNotification::Type::Warning);
    break;

  case ExportDialog::Scope::SelectedShapes: {
    auto selected_indices = m_document->get_selected_indices();
    auto all_strokes = m_document->get_strokes();
    for (size_t idx : selected_indices) {
      if (idx < all_strokes.size()) {
        strokes_to_export.push_back(all_strokes[idx]);
      }
    }
    if (strokes_to_export.empty()) {
      show_toast("No shapes selected", ToastNotification::Type::Error);
      return;
    }
  } break;
  }

  // Show file save dialog
  nfdchar_t *out_path = nullptr;
  nfdfilteritem_t filter_items[3] = {
      {"PNG Image", "png"}, {"SVG Vector", "svg"}, {"PDF Document", "pdf"}};

  nfdsavedialogu8args_t args = {0};
  args.filterList = filter_items;
  args.filterCount = 3;
  args.defaultPath = m_last_directory.empty() ? nullptr : m_last_directory.c_str();

  // Set default name based on format
  const char *default_name = "export.png";
  switch (format) {
  case ExportDialog::Format::PNG:
    default_name = "export.png";
    break;
  case ExportDialog::Format::SVG:
    default_name = "export.svg";
    break;
  case ExportDialog::Format::PDF:
    default_name = "export.pdf";
    break;
  }
  args.defaultName = default_name;

  nfdresult_t result = NFD_SaveDialogU8_With(&out_path, &args);

  if (result != NFD_OKAY) {
    if (result == NFD_ERROR) {
      show_toast(std::string("File dialog error: ") + NFD_GetError(),
                 ToastNotification::Type::Error);
    }
    return; // User cancelled or error
  }

  std::string filename(out_path);
  NFD_FreePathU8(out_path);

  // Update last directory
  size_t last_slash = filename.find_last_of("/\\");
  if (last_slash != std::string::npos) {
    m_last_directory = filename.substr(0, last_slash);
  }

  // Perform the export
  bool success = false;

  try {
    switch (format) {
    case ExportDialog::Format::PNG: {
      int width = 1920 * quality;
      int height = 1080 * quality;
      success = ExportManager::export_to_png(filename, strokes_to_export, nvg_context(), false,
                                             width, height);
    } break;

    case ExportDialog::Format::SVG: {
      int width = 1920;
      int height = 1080;
      success = ExportManager::export_to_svg(filename, strokes_to_export, false, width, height);
    } break;

    case ExportDialog::Format::PDF: {
      int width = 1920;
      int height = 1080;
      success = ExportManager::export_to_pdf(filename, strokes_to_export, false, width, height);
    } break;
    }

    if (success) {
      show_toast("Exported successfully to " + filename, ToastNotification::Type::Success);
    } else {
      show_toast("Export failed", ToastNotification::Type::Error);
    }
  } catch (const std::exception &e) {
    show_toast(std::string("Export error: ") + e.what(), ToastNotification::Type::Error);
  }
}

void ModernWhiteboardApp::on_selection_changed() {
  // Single click shows properties panel only (not floating toolbar)
  // Right click will show floating toolbar (handled in show_context_menu)
  
  // Show properties panel for all shapes including SVG shapes
  if (m_properties_panel && m_document) {
    auto selected = m_document->get_selected_indices();
    if (selected.size() == 1) {
      const auto &strokes = m_document->get_strokes();
      int idx = selected[0];
      if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
        const auto &stroke = strokes[idx];
        
        // Show properties panel for all shapes
        m_properties_panel->set_visible(true);
        
        // Configure panel based on shape type
        if (stroke.tool == Tool::SVGShape) {
          // For SVG shapes, show SVG parameters tab
          m_properties_panel->set_svg_shape(true, stroke.svg_shape_id, stroke.svg_parameters);
        } else {
          // For regular shapes, show standard properties
          m_properties_panel->set_svg_shape(false);
        }
      }
    } else {
      // No selection or multiple selection - hide properties panel
      m_properties_panel->set_visible(false);
      m_properties_panel->set_svg_shape(false);
    }
  }
}

void ModernWhiteboardApp::add_svg_line(int stroke_index, const std::string &param_name, const std::string &default_value) {
  if (!m_document || !m_shape_library) {
    return;
  }

  const auto &strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(strokes.size())) {
    return;
  }

  Stroke stroke = strokes[stroke_index];
  auto it = stroke.svg_parameters.find(param_name);
  
  std::string new_line;
  if (param_name == "methods") {
    new_line = "+ newMethod(): void";
  } else if (param_name == "attributes") {
    new_line = "- newAttribute: type";
  } else {
    new_line = default_value;
  }

  if (it != stroke.svg_parameters.end()) {
    // Append to existing lines
    if (!it->second.empty()) {
      stroke.svg_parameters[param_name] = it->second + "\n" + new_line;
    } else {
      stroke.svg_parameters[param_name] = new_line;
    }
  } else {
    // Create new parameter
    stroke.svg_parameters[param_name] = new_line;
  }

  // Regenerate SVG
  stroke.svg_data = m_shape_library->generate_svg(stroke.svg_shape_id, stroke.svg_parameters);
  
  if (!stroke.svg_data.empty()) {
    m_document->update_stroke(stroke_index, stroke);
    show_toast("Added " + param_name.substr(0, param_name.length() - 1), ToastNotification::Type::Success);
  }
}

void ModernWhiteboardApp::remove_svg_line(int stroke_index, const std::string &param_name, int line_index) {
  if (!m_document || !m_shape_library) {
    return;
  }

  const auto &all_strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(all_strokes.size())) {
    return;
  }

  Stroke stroke = all_strokes[stroke_index];
  auto it = stroke.svg_parameters.find(param_name);
  
  if (it == stroke.svg_parameters.end() || it->second.empty()) {
    return; // Nothing to remove
  }

  // Split into lines
  std::vector<std::string> lines;
  std::stringstream ss(it->second);
  std::string line;
  while (std::getline(ss, line)) {
    lines.push_back(line);
  }

  if (lines.empty()) {
    return;
  }

  // Remove the last line (or specific line_index if provided)
  if (line_index >= 0 && line_index < static_cast<int>(lines.size())) {
    lines.erase(lines.begin() + line_index);
  } else if (!lines.empty()) {
    lines.pop_back();
  }

  // Rebuild the parameter
  std::string new_value;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) new_value += "\n";
    new_value += lines[i];
  }
  
  stroke.svg_parameters[param_name] = new_value;

  // Regenerate SVG
  stroke.svg_data = m_shape_library->generate_svg(stroke.svg_shape_id, stroke.svg_parameters);
  
  if (!stroke.svg_data.empty()) {
    m_document->update_stroke(stroke_index, stroke);
    show_toast("Removed " + param_name.substr(0, param_name.length() - 1), ToastNotification::Type::Success);
  }
}

void ModernWhiteboardApp::edit_svg_line(int stroke_index, const std::string &param_name, int line_index) {
  if (!m_document || !m_shape_library) {
    return;
  }

  const auto &strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(strokes.size())) {
    return;
  }

  const Stroke &stroke = strokes[stroke_index];
  auto it = stroke.svg_parameters.find(param_name);
  
  if (it == stroke.svg_parameters.end()) {
    return;
  }

  // Calculate position for the inline editor
  // Use the stroke's bounding box center as a starting point
  nanogui::Vector2i editor_pos(100, 100); // Default position
  
  // Create inline editor for the specific line
  InlineTextEditor *editor = new InlineTextEditor(
      m_canvas_view, m_document, m_shape_library, stroke_index, param_name,
      editor_pos, line_index, InlineTextEditor::EditorMode::SingleLine);
  
  editor->activate();
  
  logi("ModernWhiteboardApp: Opened editor for line {} of parameter '{}'", line_index, param_name);
}

void ModernWhiteboardApp::delete_svg_line(int stroke_index, const std::string &param_name, int line_index) {
  if (!m_document || !m_shape_library) {
    return;
  }

  // Check if there's an active inline editor - if so, use its method
  InlineTextEditor* active_editor = InlineTextEditor::get_active_editor();
  if (active_editor) {
    active_editor->delete_line(line_index);
    show_toast("Deleted line", ToastNotification::Type::Success);
    return;
  }

  const auto &all_strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(all_strokes.size())) {
    return;
  }

  Stroke stroke = all_strokes[stroke_index];
  auto it = stroke.svg_parameters.find(param_name);
  
  if (it == stroke.svg_parameters.end() || it->second.empty()) {
    return;
  }

  // Split into lines
  std::vector<std::string> lines;
  std::stringstream ss(it->second);
  std::string line;
  while (std::getline(ss, line)) {
    lines.push_back(line);
  }

  if (line_index < 0 || line_index >= static_cast<int>(lines.size())) {
    return;
  }

  // Remove the specific line
  lines.erase(lines.begin() + line_index);

  // Rebuild the parameter
  std::string new_value;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) new_value += "\n";
    new_value += lines[i];
  }
  
  stroke.svg_parameters[param_name] = new_value;

  // Regenerate SVG
  stroke.svg_data = m_shape_library->generate_svg(stroke.svg_shape_id, stroke.svg_parameters);
  
  if (!stroke.svg_data.empty()) {
    m_document->update_stroke(stroke_index, stroke);
    show_toast("Deleted line", ToastNotification::Type::Success);
  }
}

void ModernWhiteboardApp::insert_svg_line_above(int stroke_index, const std::string &param_name, int line_index) {
  if (!m_document || !m_shape_library) {
    return;
  }

  // Determine default new line content
  std::string new_line;
  if (param_name == "methods") {
    new_line = "+ newMethod(): void";
  } else if (param_name == "attributes") {
    new_line = "- newAttribute: type";
  } else {
    new_line = "";
  }

  // Check if there's an active inline editor - if so, use its method
  InlineTextEditor* active_editor = InlineTextEditor::get_active_editor();
  if (active_editor) {
    active_editor->insert_line(line_index, new_line);
    show_toast("Inserted line above", ToastNotification::Type::Success);
    return;
  }

  const auto &all_strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(all_strokes.size())) {
    return;
  }

  Stroke stroke = all_strokes[stroke_index];
  auto it = stroke.svg_parameters.find(param_name);
  
  if (it == stroke.svg_parameters.end()) {
    return;
  }

  // Split into lines
  std::vector<std::string> lines;
  std::stringstream ss(it->second);
  std::string line;
  while (std::getline(ss, line)) {
    lines.push_back(line);
  }

  // Insert above the specified line
  if (line_index >= 0 && line_index <= static_cast<int>(lines.size())) {
    lines.insert(lines.begin() + line_index, new_line);
  } else {
    lines.push_back(new_line);
  }

  // Rebuild the parameter
  std::string new_value;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) new_value += "\n";
    new_value += lines[i];
  }
  
  stroke.svg_parameters[param_name] = new_value;

  // Regenerate SVG
  stroke.svg_data = m_shape_library->generate_svg(stroke.svg_shape_id, stroke.svg_parameters);
  
  if (!stroke.svg_data.empty()) {
    m_document->update_stroke(stroke_index, stroke);
    show_toast("Inserted line above", ToastNotification::Type::Success);
  }
}

void ModernWhiteboardApp::insert_svg_line_below(int stroke_index, const std::string &param_name, int line_index) {
  if (!m_document || !m_shape_library) {
    return;
  }

  // Determine default new line content
  std::string new_line;
  if (param_name == "methods") {
    new_line = "+ newMethod(): void";
  } else if (param_name == "attributes") {
    new_line = "- newAttribute: type";
  } else {
    new_line = "";
  }

  // Check if there's an active inline editor - if so, use its method
  InlineTextEditor* active_editor = InlineTextEditor::get_active_editor();
  if (active_editor) {
    active_editor->insert_line(line_index + 1, new_line);
    show_toast("Inserted line below", ToastNotification::Type::Success);
    return;
  }

  const auto &all_strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(all_strokes.size())) {
    return;
  }

  Stroke stroke = all_strokes[stroke_index];
  auto it = stroke.svg_parameters.find(param_name);
  
  if (it == stroke.svg_parameters.end()) {
    return;
  }

  // Split into lines
  std::vector<std::string> lines;
  std::stringstream ss(it->second);
  std::string line;
  while (std::getline(ss, line)) {
    lines.push_back(line);
  }

  // Insert below the specified line
  if (line_index >= 0 && line_index < static_cast<int>(lines.size())) {
    lines.insert(lines.begin() + line_index + 1, new_line);
  } else {
    lines.push_back(new_line);
  }

  // Rebuild the parameter
  std::string new_value;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) new_value += "\n";
    new_value += lines[i];
  }
  
  stroke.svg_parameters[param_name] = new_value;

  // Regenerate SVG
  stroke.svg_data = m_shape_library->generate_svg(stroke.svg_shape_id, stroke.svg_parameters);
  
  if (!stroke.svg_data.empty()) {
    m_document->update_stroke(stroke_index, stroke);
    show_toast("Inserted line below", ToastNotification::Type::Success);
  }
}

void ModernWhiteboardApp::duplicate_svg_line(int stroke_index, const std::string &param_name, int line_index) {
  if (!m_document || !m_shape_library) {
    return;
  }

  // Check if there's an active inline editor - if so, use its method
  InlineTextEditor* active_editor = InlineTextEditor::get_active_editor();
  if (active_editor) {
    active_editor->duplicate_line(line_index);
    show_toast("Duplicated line", ToastNotification::Type::Success);
    return;
  }

  const auto &all_strokes = m_document->get_strokes();
  if (stroke_index < 0 || stroke_index >= static_cast<int>(all_strokes.size())) {
    return;
  }

  Stroke stroke = all_strokes[stroke_index];
  auto it = stroke.svg_parameters.find(param_name);
  
  if (it == stroke.svg_parameters.end() || it->second.empty()) {
    return;
  }

  // Split into lines
  std::vector<std::string> lines;
  std::stringstream ss(it->second);
  std::string line;
  while (std::getline(ss, line)) {
    lines.push_back(line);
  }

  if (line_index < 0 || line_index >= static_cast<int>(lines.size())) {
    return;
  }

  // Duplicate the line (insert copy immediately below)
  std::string line_to_duplicate = lines[line_index];
  lines.insert(lines.begin() + line_index + 1, line_to_duplicate);

  // Rebuild the parameter
  std::string new_value;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) new_value += "\n";
    new_value += lines[i];
  }
  
  stroke.svg_parameters[param_name] = new_value;

  // Regenerate SVG
  stroke.svg_data = m_shape_library->generate_svg(stroke.svg_shape_id, stroke.svg_parameters);
  
  if (!stroke.svg_data.empty()) {
    m_document->update_stroke(stroke_index, stroke);
    show_toast("Duplicated line", ToastNotification::Type::Success);
  }
}

} // namespace whiteboard
