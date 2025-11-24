#pragma once

#include "flexui/node.h" // Inherit from FlexNode

#include <SDL3/SDL.h> // Required for SDL_Event
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace flexui {

struct FlexMenuItem {
  std::string id;
  std::string label;
  std::string icon;
  bool separator = false;
  bool disabled = false;
  std::function<void()> on_click;
};

struct FlexMenuBinding {
  std::string menu_id;
  std::string trigger_id; // Element that opens the menu
  std::vector<FlexMenuItem> items;
  bool open = false;
};

class FlexMenu : public FlexNode {
public:
  void registerMenu(FlexMenuBinding binding);

  void openMenu(const std::string &menu_id);
  void closeMenu(const std::string &menu_id);
  void toggleMenu(const std::string &menu_id);
  bool isOpen(const std::string &menu_id) const;

  void handleEvent(const SDL_Event &event) override;
  void onDocumentAttached(FlexDocument *document) override; // Add onDocumentAttached

private:
  struct MenuState {
    FlexMenuBinding binding;
    bool open = false;
  };

  MenuState *hitTestTrigger(float x, float y);
  MenuState *hitTestMenuItem(float x, float y, int &item_index);
  void updateMenuState(MenuState &state);

  std::unordered_map<std::string, MenuState> m_menus;
};

} // namespace flexui
