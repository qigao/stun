#pragma once

#include "flexui/document.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace flexui {

struct FlexButtonProps {
  std::string id;
  std::string parent_id;
  std::string text = "Button";
  std::vector<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
};

struct FlexTextInputProps {
  std::string id;
  std::string parent_id;
  std::string text;
  std::string placeholder;
  std::vector<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
};

struct FlexToggleProps {
  std::string id;
  std::string parent_id;
  std::string handle_id;
  bool initial_on = true;
  std::vector<std::string> track_classes;
  std::vector<std::string> handle_classes;
  std::unordered_map<std::string, std::string> track_styles;
  std::unordered_map<std::string, std::string> handle_styles;
};

struct FlexCheckboxProps {
  std::string id;
  std::string parent_id;
  bool checked = false;
  std::vector<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
};

struct FlexSliderProps {
  std::string track_id;
  std::string parent_id;
  std::string fill_id;
  std::string thumb_id;
  float left = 0.f;
  float top = 0.f;
  float width = 300.f;
  float height = 8.f;
  float thumb_size = 24.f;
  float value = 0.5f;
  std::vector<std::string> track_classes;
  std::vector<std::string> fill_classes;
  std::vector<std::string> thumb_classes;
  std::unordered_map<std::string, std::string> track_styles;
  std::unordered_map<std::string, std::string> fill_styles;
  std::unordered_map<std::string, std::string> thumb_styles;
};

struct FlexRadioProps {
  std::string id;
  std::string parent_id;
  std::vector<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
};

struct FlexDropdownOptionProps {
  std::string id;
  std::string text;
  std::vector<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
  std::string value;
};

struct FlexDropdownProps {
  std::string id;
  std::string parent_id;
  std::string display_id;
  std::string button_id;
  std::string menu_id;
  std::string placeholder = "Select";
  std::vector<FlexDropdownOptionProps> options;
  std::vector<std::string> container_classes;
  std::unordered_map<std::string, std::string> container_styles;
};

class FlexWidgetFactory {
public:
  static std::string createButton(FlexDocument &document,
                                  const FlexButtonProps &props);

  static std::string createTextInput(FlexDocument &document,
                                     const FlexTextInputProps &props);

  static std::string createToggle(FlexDocument &document,
                                  const FlexToggleProps &props);

  static std::string createCheckbox(FlexDocument &document,
                                    const FlexCheckboxProps &props);

  static std::string createSlider(FlexDocument &document,
                                  const FlexSliderProps &props);

  static std::string createRadio(FlexDocument &document,
                                 const FlexRadioProps &props);

  static std::string createDropdown(FlexDocument &document,
                                    const FlexDropdownProps &props);
};

} // namespace flexui
