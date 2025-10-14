#include "whiteboard/modern_whiteboard_app.h"

#include <nanogui.h>
#include <nanogui/fluent_web_theme.h>

#ifdef _WIN32
  #include <direct.h>
#else
  #include <sys/stat.h>
  #include <sys/types.h>
#endif

namespace whiteboard {

AutoSaveManager::AutoSaveManager(ModernWhiteboardApp *app)
    : m_app(app), m_has_unsaved_changes(false), m_is_saving(false), m_auto_save_enabled(false) {
  m_last_change_time = std::chrono::steady_clock::now();
}

void AutoSaveManager::start() {
  m_auto_save_enabled = true;
  m_last_change_time = std::chrono::steady_clock::now();
}

void AutoSaveManager::stop() { m_auto_save_enabled = false; }

void AutoSaveManager::mark_changed() {
  if (!m_auto_save_enabled)
    return;
  m_has_unsaved_changes = true;
  m_last_change_time = std::chrono::steady_clock::now();
}

void AutoSaveManager::update() {
  if (!m_auto_save_enabled || !m_has_unsaved_changes || m_is_saving)
    return;

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_change_time);
  if (elapsed.count() >= 30)
    save_now();
}

std::string AutoSaveManager::get_auto_save_path() {
#ifdef _WIN32
  const char *appdata = std::getenv("LOCALAPPDATA");
  if (appdata)
    return std::string(appdata) + "\\ModernWhiteboard\\autosave.json";
  return "autosave.json";
#else
  const char *home = std::getenv("HOME");
  if (home)
    return std::string(home) + "/.local/share/ModernWhiteboard/autosave.json";
  return "autosave.json";
#endif
}

void AutoSaveManager::save_to_local_storage() {
  try {
    json j;
    j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    j["current_page"] = m_app ? m_app->get_current_page() : 0;

    std::string path = get_auto_save_path();
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
      std::string dir = path.substr(0, pos);
#ifdef _WIN32
      _mkdir(dir.c_str());
#else
      mkdir(dir.c_str(), 0755);
#endif
    }

    std::ofstream file(path);
    if (file.is_open()) {
      file << j.dump(2);
      file.close();
    }
  } catch (const std::exception &e) {
    std::cerr << "Auto-save error: " << e.what() << std::endl;
  }
}

void AutoSaveManager::save_now() {
  if (m_is_saving)
    return;

  m_is_saving = true;
  if (m_app)
    m_app->set_saving_indicator_visible(true);

  save_to_local_storage();

  m_has_unsaved_changes = false;
  m_is_saving = false;
  if (m_app)
    m_app->set_saving_indicator_visible(false);
}

bool AutoSaveManager::has_auto_save_data() {
  std::ifstream file(get_auto_save_path());
  return file.good();
}

void AutoSaveManager::load_from_local_storage() {
  try {
    std::ifstream file(get_auto_save_path());
    if (!file.is_open() || !m_app)
      return;

    json j;
    file >> j;
    file.close();

    // Minimal restore: just log the presence of an auto-save.
    if (j.contains("current_page")) {
      m_app->set_saving_indicator_visible(false);
    }
  } catch (const std::exception &e) {
    std::cerr << "Error loading auto-save: " << e.what() << std::endl;
  }
}

void AutoSaveManager::clear_auto_save() {
  std::remove(get_auto_save_path().c_str());
}

ModernWhiteboardApp::ModernWhiteboardApp()
    : Screen(Vector2i(1400, 900), "Modern Whiteboard - Collaborative Design") {
  m_theme = new FluentWebTheme(nvg_context());
  set_theme(m_theme);
  set_background(Color(250, 250, 250, 255));

  TemplateLibrary::instance().initialize(nvg_context());

  m_canvas = new ModernCanvas(this);
  m_canvas->set_background_color(Color(245, 245, 245, 255));

  create_top_toolbar();
  create_left_sidebar();
  create_floating_panels();
  create_zoom_controls();
  create_properties_panel();

  if (m_canvas) {
    m_canvas->set_selection_changed_callback([this]() { update_properties_panel(); });
    m_canvas->set_strokes_changed_callback([this]() {
      update_layers_panel();
      if (m_auto_save_manager)
        m_auto_save_manager->mark_changed();
    });
  }

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

void ModernWhiteboardApp::create_top_toolbar() {
  const int toolbar_height = 60;
  m_top_toolbar = new Widget(this);
  m_top_toolbar->set_layout(
      new BoxLayout(Orientation::Horizontal, Alignment::Middle, 12, 12));
  m_top_toolbar->set_fixed_height(toolbar_height);

  auto *title = new Label(m_top_toolbar, "Modern Whiteboard", "sans-bold", 20);
  title->set_color(Color(45, 45, 45, 255));

  auto *button_container = new Widget(m_top_toolbar);
  button_container->set_layout(
      new BoxLayout(Orientation::Horizontal, Alignment::Middle, 6, 0));

  auto add_button = [button_container](const std::string &caption, int icon,
                                       const std::string &tooltip,
                                       std::function<void()> callback) {
    auto *btn = new Button(button_container, caption, icon);
    btn->set_tooltip(tooltip);
    btn->set_fixed_height(36);
    btn->set_callback(std::move(callback));
    return btn;
  };

  add_button("", FA_FILE, "New board", [this]() {
    if (!m_canvas)
      return;
    m_canvas->clear_canvas();
    update_layers_panel();
    update_properties_panel();
  });

  add_button("", FA_SAVE, "Save snapshot", [this]() {
    if (m_auto_save_manager)
      m_auto_save_manager->save_now();
  });

  add_button("", FA_UNDO, "Undo", [this]() {
    if (!m_canvas)
      return;
    m_canvas->undo();
    update_layers_panel();
    update_properties_panel();
  });

  add_button("", FA_REDO, "Redo", [this]() {
    if (!m_canvas)
      return;
    m_canvas->redo();
    update_layers_panel();
    update_properties_panel();
  });

  add_button("Templates", 0, "Open template gallery", [this]() {
    if (!m_template_gallery) {
      m_template_gallery = new TemplateGallery(
          this, m_canvas, [this](const Template &) {
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

  add_button("", FA_SEARCH, "Toggle search", [this]() {
    if (!m_search_bar)
      return;
    m_search_bar->set_visible(!m_search_bar->visible());
    update_layout();
  });
}

void ModernWhiteboardApp::create_left_sidebar() {
  m_left_sidebar = new Widget(this);
  m_left_sidebar->set_layout(
      new BoxLayout(Orientation::Vertical, Alignment::Middle, 8, 8));
  m_left_sidebar->set_fixed_width(70);

  struct ToolInfo {
    Tool tool;
    int icon;
    const char *tooltip;
  };

  std::vector<ToolInfo> tools = {
      {Tool::Select, FA_MOUSE_POINTER, "Select"},
      {Tool::Pan, FA_HAND_PAPER, "Pan"},
      {Tool::Pen, FA_PEN, "Pen"},
      {Tool::Text, FA_FONT, "Text"},
      {Tool::Sticky, FA_STICKY_NOTE, "Sticky note"},
      {Tool::Rectangle, FA_SQUARE, "Rectangle"},
      {Tool::Circle, FA_CIRCLE, "Circle"},
      {Tool::Line, FA_MINUS, "Line"},
      {Tool::Arrow, FA_ARROW_RIGHT, "Arrow"},
      {Tool::Image, FA_IMAGE, "Image"},
  };

  for (const auto &entry : tools) {
    auto *btn = new Button(m_left_sidebar, "", entry.icon);
    btn->set_tooltip(entry.tooltip);
    btn->set_flags(Button::RadioButton);
    btn->set_fixed_size(Vector2i(50, 40));
    btn->set_callback([this, entry]() {
      if (m_canvas)
        m_canvas->set_tool(entry.tool);
    });
    if (entry.tool == Tool::Pen)
      btn->set_pushed(true);
    m_tool_buttons.push_back(btn);
  }
}

void ModernWhiteboardApp::create_floating_panels() {
  m_layers_panel = new LayersPanel(this, m_canvas);
  m_layers_panel->set_visible(true);

  m_search_bar = new SearchBar(this, m_canvas);
  m_search_bar->set_visible(false);
}

void ModernWhiteboardApp::create_zoom_controls() {
  m_zoom_window = new Window(this, "Zoom");
  m_zoom_window->set_layout(
      new BoxLayout(Orientation::Horizontal, Alignment::Middle, 6, 6));
  m_zoom_window->set_fixed_size(Vector2i(200, 60));

  auto make_button = [this](Window *wnd, int icon, const std::string &tooltip,
                            std::function<void()> callback) {
    auto *btn = new Button(wnd, "", icon);
    btn->set_tooltip(tooltip);
    btn->set_fixed_size(Vector2i(48, 36));
    btn->set_callback(std::move(callback));
    return btn;
  };

  make_button(m_zoom_window, FA_SEARCH_MINUS, "Zoom out", [this]() {
    if (m_canvas)
      m_canvas->zoom_out();
  });
  make_button(m_zoom_window, FA_EXPAND, "Reset zoom", [this]() {
    if (m_canvas)
      m_canvas->reset_zoom();
  });
  make_button(m_zoom_window, FA_SEARCH_PLUS, "Zoom in", [this]() {
    if (m_canvas)
      m_canvas->zoom_in();
  });
}

void ModernWhiteboardApp::create_properties_panel() {
  m_properties_panel = new PropertiesPanel(this, m_canvas);
  update_properties_panel();
}

void ModernWhiteboardApp::update_properties_panel() {
  if (m_properties_panel && m_canvas) {
    m_properties_panel->update_for_selection(m_canvas->get_selected_indices(),
                                             m_canvas->get_strokes());
  }
}

void ModernWhiteboardApp::update_layers_panel() {
  if (m_layers_panel)
    m_layers_panel->refresh();
}

void ModernWhiteboardApp::show_restore_prompt() {
  auto *dialog = new MessageDialog(this, MessageDialog::Type::Question, "Restore Session",
                                   "A previous auto-save was found. Restore it?",
                                   "Restore", "Discard", true);
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
  const int toolbar_height = m_top_toolbar ? m_top_toolbar->fixed_height() : 0;
  const int sidebar_width = m_left_sidebar ? m_left_sidebar->fixed_width() : 0;

  if (m_top_toolbar) {
    m_top_toolbar->set_position(Vector2i(0, 0));
    m_top_toolbar->set_fixed_size(Vector2i(width(), toolbar_height));
  }

  if (m_left_sidebar) {
    m_left_sidebar->set_position(Vector2i(0, toolbar_height));
    m_left_sidebar->set_fixed_size(Vector2i(sidebar_width, height() - toolbar_height));
  }

  if (m_canvas) {
    m_canvas->set_position(Vector2i(sidebar_width, toolbar_height));
    m_canvas->set_fixed_size(Vector2i(width() - sidebar_width, height() - toolbar_height));
  }

  if (m_layers_panel) {
    m_layers_panel->set_position(Vector2i(width() - m_layers_panel->fixed_width() - 20,
                                          toolbar_height + 20));
  }

  if (m_properties_panel) {
    m_properties_panel->set_position(Vector2i(width() - m_properties_panel->fixed_width() - 20,
                                              toolbar_height + 260));
  }

  if (m_zoom_window) {
    m_zoom_window->set_position(Vector2i(width() - m_zoom_window->fixed_width() - 20,
                                         height() - m_zoom_window->fixed_height() - 20));
  }

  if (m_search_bar) {
    m_search_bar->set_position(Vector2i(sidebar_width + 20, height() - 240));
  }

  if (m_saving_indicator) {
    m_saving_indicator->set_position(Vector2i(width() - 150, 30));
  }
}

} // namespace whiteboard
