#include "whiteboard/modern_whiteboard_app.h"
#include <fmtlog.h>
#include <nanogui.h>
#include <nanogui/fluent_web_theme.h>

namespace whiteboard {

ModernWhiteboardApp::ModernWhiteboardApp()
    : Screen(Vector2i(1400, 900), "Modern Whiteboard - Collaborative Design") {
  m_theme = new FluentWebTheme(nvg_context());
  set_theme(m_theme);
  set_background(Color(250, 250, 250, 255));

  // Initialize Native File Dialog
  NFD_Init();

  TemplateLibrary::instance().initialize(nvg_context());

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

      // Validate pointers
      if (!m_shape_library) {
        loge("ModernWhiteboardApp: Shape library is null!");
        return;
      }

      if (!m_document) {
        loge("ModernWhiteboardApp: Document is null!");
        return;
      }

      // Create shape at canvas center with slight offset for each new shape
      static int shape_offset_counter = 0;
      float offset = (shape_offset_counter++ % 10) * 30.0f; // Offset by 30px for each shape
      Point position = {400.0f + offset, 300.0f + offset};
      logi("ModernWhiteboardApp: Creating shape at ({}, {})", position.x, position.y);

      Stroke shape = m_shape_library->create_shape(shape_id, position);

      logi("ModernWhiteboardApp: Shape created - Name: '{}', Tool: {}, SVG data size: {}",
           shape.name, static_cast<int>(shape.tool), shape.svg_data.size());

      m_document->add_stroke(shape);
      logi("ModernWhiteboardApp: Shape added to document");

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
  create_text_panel();
  create_floating_toolbar();

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

  m_menu_toolbar->add_menu_item("Shape Library", FA_SHAPES, [this]() { toggle_shape_library(); });

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
  m_properties_panel->set_visible(true);
}

void ModernWhiteboardApp::create_text_panel() {
  m_text_panel = new TextPanelModule(this);

  // Connect color callback
  m_text_panel->set_color_callback([this](NVGcolor color) {
    if (m_document) {
      m_document->set_stroke_color(
          Color(color.r * 255, color.g * 255, color.b * 255, color.a * 255));
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

bool ModernWhiteboardApp::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (Screen::keyboard_event(key, scancode, action, modifiers))
    return true;

  // Handle keyboard shortcuts
  if (action == GLFW_PRESS || action == GLFW_REPEAT) {
    // Ctrl+O - Open
    if (key == GLFW_KEY_O && (modifiers & GLFW_MOD_CONTROL)) {
      open_file();
      return true;
    }
    // Ctrl+S - Save
    if (key == GLFW_KEY_S && (modifiers & GLFW_MOD_CONTROL)) {
      // Shift+Ctrl+S - Save As
      if (modifiers & GLFW_MOD_SHIFT) {
        save_file_as();
      } else {
        save_file();
      }
      return true;
    }
  }

  return false;
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
      std::cout << "Loaded file: " << file_path << std::endl;
    } else {
      // Show error dialog
      auto *dialog = new MessageDialog(
          this, MessageDialog::Type::Warning, "Open Failed",
          "Failed to load file. The file may be corrupted or in an invalid format.", "OK", "",
          true);
      dialog->center();
      dialog->set_visible(true);
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
    // Show saving indicator briefly
    set_saving_indicator_visible(true);

    // Hide after a short delay (this is a simple approach)
    // In a real app, you might use a timer
    std::cout << "Saved file: " << m_current_file_path << std::endl;

    // Schedule hiding the indicator
    // For now, just hide it immediately after a brief moment
    // (In production, you'd use a proper timer mechanism)
  } else {
    // Show error dialog
    auto *dialog = new MessageDialog(this, MessageDialog::Type::Warning, "Save Failed",
                                     "Failed to save file. Check that you have write permissions.",
                                     "OK", "", true);
    dialog->center();
    dialog->set_visible(true);
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

      // Show saving indicator briefly
      set_saving_indicator_visible(true);
      std::cout << "Saved file: " << file_path << std::endl;
    } else {
      // Show error dialog
      auto *dialog = new MessageDialog(
          this, MessageDialog::Type::Warning, "Save Failed",
          "Failed to save file. Check that you have write permissions.", "OK", "", true);
      dialog->center();
      dialog->set_visible(true);
    }
  } else if (result == NFD_ERROR) {
    std::cerr << "NFD Error: " << NFD_GetError() << std::endl;
  }
  // NFD_CANCEL - user cancelled, do nothing
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

void ModernWhiteboardApp::create_floating_toolbar() {
  m_floating_toolbar = new FloatingToolbar(this);
  m_floating_toolbar->set_visible(false);

  // Wire up callbacks
  m_floating_toolbar->set_delete_callback([this]() {
    if (m_document) {
      // Delete selected strokes
      auto selected = m_document->get_selected_indices();
      if (!selected.empty()) {
        m_document->remove_strokes(selected);
      }
    }
  });

  m_floating_toolbar->set_duplicate_callback([this]() {
    if (m_document) {
      // Duplicate selected strokes with offset
      auto selected = m_document->get_selected_indices();
      if (!selected.empty()) {
        auto strokes = m_document->get_strokes();
        std::vector<Stroke> duplicates;

        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            Stroke duplicate = strokes[idx];
            // Offset by 20 pixels
            for (auto &pt : duplicate.points) {
              pt.x += 20.0f;
              pt.y += 20.0f;
            }
            duplicates.push_back(duplicate);
          }
        }

        // Add duplicates and select them
        std::vector<int> new_indices;
        for (const auto &dup : duplicates) {
          m_document->add_stroke(dup);
          new_indices.push_back(static_cast<int>(m_document->get_strokes().size()) - 1);
        }
        m_document->set_selection(new_indices);
      }
    }
  });

  m_floating_toolbar->set_group_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (selected.size() > 1) {
        // Find next available group ID
        const auto &strokes = m_document->get_strokes();
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
        logi("Grouped {} shapes with ID {}", selected.size(), new_group_id);
      }
    }
  });

  m_floating_toolbar->set_ungroup_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      const auto &strokes = m_document->get_strokes();

      for (int idx : selected) {
        if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
          if (strokes[idx].group_id >= 0) {
            Stroke updated = strokes[idx];
            updated.group_id = -1;
            m_document->update_stroke(idx, updated);
          }
        }
      }
      logi("Ungrouped {} shapes", selected.size());
    }
  });

  m_floating_toolbar->set_bring_forward_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (!selected.empty()) {
        // Sort in descending order to move from back to front
        std::sort(selected.begin(), selected.end(), std::greater<int>());

        for (int idx : selected) {
          int new_idx = std::min(idx + 1, static_cast<int>(m_document->get_strokes().size()) - 1);
          if (new_idx != idx) {
            m_document->reorder_stroke(idx, new_idx);
          }
        }
        logi("Brought {} shapes forward", selected.size());
      }
    }
  });

  m_floating_toolbar->set_send_backward_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (!selected.empty()) {
        // Sort in ascending order to move from front to back
        std::sort(selected.begin(), selected.end());

        for (int idx : selected) {
          int new_idx = std::max(idx - 1, 0);
          if (new_idx != idx) {
            m_document->reorder_stroke(idx, new_idx);
          }
        }
        logi("Sent {} shapes backward", selected.size());
      }
    }
  });

  m_floating_toolbar->set_align_left_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (selected.size() > 1) {
        auto strokes = m_document->get_strokes();

        // Find leftmost position
        float min_x = std::numeric_limits<float>::max();
        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            float s_min_x, s_min_y, s_max_x, s_max_y;
            strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
            min_x = std::min(min_x, s_min_x);
          }
        }

        // Align all to leftmost
        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            float s_min_x, s_min_y, s_max_x, s_max_y;
            strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
            float offset = min_x - s_min_x;

            for (auto &pt : strokes[idx].points) {
              pt.x += offset;
            }
            m_document->update_stroke(idx, strokes[idx]);
          }
        }
        logi("Aligned {} shapes to left", selected.size());
      }
    }
  });

  m_floating_toolbar->set_align_right_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (selected.size() > 1) {
        auto strokes = m_document->get_strokes();

        // Find rightmost position
        float max_x = std::numeric_limits<float>::lowest();
        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            float s_min_x, s_min_y, s_max_x, s_max_y;
            strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
            max_x = std::max(max_x, s_max_x);
          }
        }

        // Align all to rightmost
        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            float s_min_x, s_min_y, s_max_x, s_max_y;
            strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
            float offset = max_x - s_max_x;

            for (auto &pt : strokes[idx].points) {
              pt.x += offset;
            }
            m_document->update_stroke(idx, strokes[idx]);
          }
        }
        logi("Aligned {} shapes to right", selected.size());
      }
    }
  });

  m_floating_toolbar->set_align_top_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (selected.size() > 1) {
        auto strokes = m_document->get_strokes();

        // Find topmost position
        float min_y = std::numeric_limits<float>::max();
        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            float s_min_x, s_min_y, s_max_x, s_max_y;
            strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
            min_y = std::min(min_y, s_min_y);
          }
        }

        // Align all to topmost
        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            float s_min_x, s_min_y, s_max_x, s_max_y;
            strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
            float offset = min_y - s_min_y;

            for (auto &pt : strokes[idx].points) {
              pt.y += offset;
            }
            m_document->update_stroke(idx, strokes[idx]);
          }
        }
        logi("Aligned {} shapes to top", selected.size());
      }
    }
  });

  m_floating_toolbar->set_align_bottom_callback([this]() {
    if (m_document) {
      auto selected = m_document->get_selected_indices();
      if (selected.size() > 1) {
        auto strokes = m_document->get_strokes();

        // Find bottommost position
        float max_y = std::numeric_limits<float>::lowest();
        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            float s_min_x, s_min_y, s_max_x, s_max_y;
            strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
            max_y = std::max(max_y, s_max_y);
          }
        }

        // Align all to bottommost
        for (int idx : selected) {
          if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
            float s_min_x, s_min_y, s_max_x, s_max_y;
            strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
            float offset = max_y - s_max_y;

            for (auto &pt : strokes[idx].points) {
              pt.y += offset;
            }
            m_document->update_stroke(idx, strokes[idx]);
          }
        }
        logi("Aligned {} shapes to bottom", selected.size());
      }
    }
  });

  logi("ModernWhiteboardApp: Floating toolbar created");
}

void ModernWhiteboardApp::update_floating_toolbar() {
  if (!m_floating_toolbar || !m_document) {
    return;
  }

  const auto &selected = m_document->get_selected_indices();

  if (selected.empty()) {
    m_floating_toolbar->hide();
    return;
  }

  // Calculate center of selection for toolbar position
  const auto &strokes = m_document->get_strokes();
  float min_x = std::numeric_limits<float>::max();
  float min_y = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_y = std::numeric_limits<float>::lowest();

  bool can_ungroup = false;
  for (int idx : selected) {
    if (idx >= 0 && idx < static_cast<int>(strokes.size())) {
      float s_min_x, s_min_y, s_max_x, s_max_y;
      strokes[idx].get_bounds(s_min_x, s_min_y, s_max_x, s_max_y);
      min_x = std::min(min_x, s_min_x);
      min_y = std::min(min_y, s_min_y);
      max_x = std::max(max_x, s_max_x);
      max_y = std::max(max_y, s_max_y);

      if (strokes[idx].group_id >= 0) {
        can_ungroup = true;
      }
    }
  }

  // Convert to screen coordinates
  if (m_canvas_view) {
    nanogui::Vector2f center_canvas((min_x + max_x) / 2.0f, min_y);
    nanogui::Vector2f center_screen = m_canvas_view->canvas_to_global(center_canvas);

    bool can_group = selected.size() > 1;
    m_floating_toolbar->show_at(
        nanogui::Vector2i(static_cast<int>(center_screen.x()), static_cast<int>(center_screen.y())),
        can_group, can_ungroup, static_cast<int>(selected.size()));
  }
}

void ModernWhiteboardApp::on_selection_changed() { update_floating_toolbar(); }

} // namespace whiteboard
