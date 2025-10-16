#include "whiteboard/modern_whiteboard_app.h"

#include <nanogui.h>
#include <nanogui/fluent_web_theme.h>

namespace whiteboard {

ModernWhiteboardApp::ModernWhiteboardApp()
    : Screen(Vector2i(1400, 900), "Modern Whiteboard - Collaborative Design") {
  m_theme = new FluentWebTheme(nvg_context());
  set_theme(m_theme);
  set_background(Color(250, 250, 250, 255));

  TemplateLibrary::instance().initialize(nvg_context());

  // Create the shared document model (MVC)
  m_document = new WhiteboardDocument();

  // Create Canvas MVC triad
  m_canvas_view = new CanvasView(this, m_document);
  m_canvas_view->set_background_color(Color(245, 245, 245, 255));
  m_canvas_controller = new CanvasController(m_document, m_canvas_view);
  m_canvas_view->set_controller(m_canvas_controller);

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

  // Create Properties Panel MVC triad
  m_properties_view = new PropertiesView(this, m_document);
  m_properties_controller = new PropertiesController(m_document, m_properties_view);
  m_properties_view->set_controller(m_properties_controller);
  m_properties_view->set_visible(false); // Hidden, using legacy properties module

  // Create Text Panel MVC triad
  m_text_view = new TextView(this, m_document);
  m_text_controller = new TextController(m_document, m_text_view);
  m_text_view->set_controller(m_text_controller);
  m_text_view->set_visible(false); // Hidden, using legacy text module

  create_menu_toolbar();
  create_left_sidebar();
  create_floating_panels();
  create_zoom_controls();
  create_properties_panel();
  create_text_panel();

  // Callbacks are now handled by observer pattern in MVC components

  m_auto_save_manager = new AutoSaveManager(this);
  if (m_auto_save_manager->has_auto_save_data())
    show_restore_prompt();
  m_auto_save_manager->start();

  m_saving_indicator = new Label(this, "Saving...", "sans", 14);
  m_saving_indicator->set_color(Color(100, 100, 100, 255));
  m_saving_indicator->set_visible(false);

  update_layout();
  perform_layout(nvg_context());
}

ModernWhiteboardApp::~ModernWhiteboardApp() {
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

  m_menu_toolbar->add_menu_item("Save", FA_SAVE, [this]() {
    if (m_auto_save_manager)
      m_auto_save_manager->save_now();
  });

  m_menu_toolbar->add_menu_item("Templates", FA_IMAGES, [this]() {
    if (!m_template_gallery) {
      m_template_gallery = new TemplateGallery(this, m_document, [this](const Template &) {
        update_layers_panel();
        update_properties_panel();
      });
    }
    if (m_template_gallery) {
      m_template_gallery->refresh_templates();
      m_template_gallery->center();
      m_template_gallery->set_visible(true);
    }
  });

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

  m_menu_toolbar->add_menu_item("Export", FA_DOWNLOAD, []() {});

  m_menu_toolbar->add_separator();

  m_menu_toolbar->add_menu_item("Settings", FA_COG, []() {});
  m_menu_toolbar->add_menu_item("Help", FA_QUESTION_CIRCLE, []() {});

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
    // 0: lock (not implemented)
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
    // 11: rotate (not implemented)
    // 12: tree (not implemented)

    switch (button_id) {
    case 1:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Pan);
      break;
    case 2:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Select);
      break;
    case 3:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Rectangle);
      break;
    case 4:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Diamond);
      break;
    case 5:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Circle);
      break;
    case 6:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Arrow);
      break;
    case 7:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Line);
      break;
    case 8:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Pen);
      break;
    case 9:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Text);
      break;
    case 10:
      if (m_toolbar_controller) m_toolbar_controller->select_tool(Tool::Image);
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
  m_properties_panel->set_visible(true);
}

void ModernWhiteboardApp::create_text_panel() {
  m_text_panel = new TextPanelModule(this);

  // Connect color callback
  m_text_panel->set_color_callback([this](NVGcolor color) {
    if (m_document) {
      m_document->set_stroke_color(Color(color.r * 255, color.g * 255, color.b * 255, color.a * 255));
    }
  });

  // Connect font size callback
  m_text_panel->set_font_size_callback([this](float size) {
    if (m_text_controller) {
      m_text_controller->change_font_size(size);
    }
  });

  // Connect alignment callback
  m_text_panel->set_align_callback([this](int align) {
    if (m_text_controller) {
      m_text_controller->set_alignment(align);
    }
  });

  // Connect font face callback
  m_text_panel->set_font_face_callback([this](const std::string &face) {
    if (m_text_controller) {
      m_text_controller->change_font_face(face);
    }
  });

  m_text_panel->set_visible(true);
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

  if (m_text_panel) {
    // Position text panel on the left side, below the toolbar
    m_text_panel->set_position(Vector2i(110, 20));
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

} // namespace whiteboard
