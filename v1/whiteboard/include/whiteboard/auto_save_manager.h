/**
 * \file auto_save_manager.h
 * \brief Auto-save manager for whiteboard sessions
 */

#pragma once

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>

namespace whiteboard {

class ModernWhiteboardApp;

using json = nlohmann::json;

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

} // namespace whiteboard
