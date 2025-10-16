#include "whiteboard/auto_save_manager.h"
#include "whiteboard/modern_whiteboard_app.h"

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

void AutoSaveManager::stop() { 
  m_auto_save_enabled = false; 
}

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
  if (!m_app)
    return;
    
  try {
    // Get document and serialize it
    auto* document = m_app->get_document();
    if (!document)
      return;
      
    json j = document->to_json();
    
    // Add auto-save metadata
    j["autosave"] = true;
    j["autosave_timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    j["current_page"] = m_app->get_current_page();

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
  if (!m_app)
    return;
    
  try {
    std::ifstream file(get_auto_save_path());
    if (!file.is_open())
      return;

    json j;
    file >> j;
    file.close();

    // Load document from auto-save
    auto* document = m_app->get_document();
    if (document && j.contains("strokes")) {
      document->from_json(j);
    }
    
    m_app->set_saving_indicator_visible(false);
  } catch (const std::exception &e) {
    std::cerr << "Error loading auto-save: " << e.what() << std::endl;
  }
}

void AutoSaveManager::clear_auto_save() { 
  std::remove(get_auto_save_path().c_str()); 
}

} // namespace whiteboard
