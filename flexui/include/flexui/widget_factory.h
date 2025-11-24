#pragma once

#include "flexui/document.h"

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace flexui {

// Simplified props, containing only ID, hierarchy, state (classes), and content.
// All layout and styling are now controlled by CSS.

struct FlexLabelProps {
  std::string id;
  std::string parent_id;
  std::unordered_set<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
  std::string text;
};

struct FlexButtonProps {
  std::string id;
  std::string parent_id;
  std::unordered_set<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
  std::string text = "Button";
};

struct FlexTextInputProps {
  std::string id;
  std::string parent_id;
  std::unordered_set<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
  std::string placeholder;
  std::string text;
};

struct FlexToggleProps {
  std::string id; // The track
  std::string parent_id;
  std::string handle_id;
  std::unordered_set<std::string> track_classes;
  std::unordered_map<std::string, std::string> track_styles;
  std::unordered_set<std::string> handle_classes;
  std::unordered_map<std::string, std::string> handle_styles;
  bool initial_on = false;
};

struct FlexCheckboxProps {
  std::string id;
  std::string parent_id;
  std::unordered_set<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
  bool checked = false;
};

struct FlexSliderProps {
  std::string parent_id;
  std::string track_id;
  std::string fill_id;
  std::string thumb_id;
  std::unordered_set<std::string> track_classes;
  std::unordered_set<std::string> fill_classes;
  std::unordered_set<std::string> thumb_classes;
  // All layout (left, top, width, height) MUST be defined in CSS, not here
  float value = 0.5f;
};

struct FlexProgressBarProps {
  std::string parent_id;
  std::string track_id;
  std::string fill_id;
  std::unordered_set<std::string> track_classes;
  std::unordered_set<std::string> fill_classes;
  // All layout (left, top, width, height) MUST be defined in CSS, not here
  float value = 0.0f;
};

struct FlexDropdownOptionProps {
  std::string id;
  std::string text;
  std::string value;
  std::unordered_set<std::string> classes;
};

struct FlexDropdownProps {
  std::string id; // The container
  std::string parent_id;
  std::unordered_set<std::string> container_classes;
  std::unordered_map<std::string, std::string> container_styles;
  std::string placeholder = "Select";
  std::vector<FlexDropdownOptionProps> options;
};

// Basic container, defaults to a div-like element.
struct FlexContainerProps {
  std::string id;
  std::string parent_id;
  std::unordered_set<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
};

// Basic grid, defaults to a div-like element.
struct FlexGridProps {
  std::string id;
  std::string parent_id;
  std::unordered_set<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
};

// Basic grid item, defaults to a div-like element.
struct FlexGridItemProps {
  std::string id;
  std::string parent_id;
  std::unordered_set<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
  std::string text;
};

// Basic flex item, defaults to a div-like element.
struct FlexItemProps {
  std::string id;
  std::string parent_id;
  std::unordered_set<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
  std::string text;
};

// Missing props from example
struct FlexRadioProps {
  std::string id;
  std::string parent_id;
  std::unordered_set<std::string> classes;
  std::unordered_map<std::string, std::string> styles;
};

struct FlexSpinnerProps {
    std::string id;
    std::string parent_id;
    std::unordered_set<std::string> classes;
    std::unordered_map<std::string, std::string> styles;
    float size = 20.0f;
};

struct FlexTooltipProps {
    std::string tooltip_id;
    std::string parent_id;
    std::string text;
};

struct FlexAlertProps {
    std::string alert_id;
    std::string parent_id;
    std::string message;
    std::string type;
    std::unordered_map<std::string, std::string> styles;
};

struct FlexAccordionItemProps {
    std::string header_id;
    std::string content_id;
    std::string title;
    std::string content_text;
};

struct FlexAccordionProps {
    std::string container_id;
    std::string parent_id;
    std::vector<FlexAccordionItemProps> items;
    std::unordered_map<std::string, std::string> styles;
};

struct FlexTabsProps {
    std::string container_id;
    std::string parent_id;
    struct Tab {
        std::string id;
        std::string panel_id;
        std::string title;
        std::unordered_set<std::string> classes;
        std::unordered_map<std::string, std::string> styles;
    };
    std::vector<Tab> tabs;
    std::unordered_map<std::string, std::string> container_styles;
};


class FlexButton; // Forward declaration
class FlexCheckbox; // Forward declaration
class FlexToggle; // Forward declaration

class FlexWidgetFactory {
public:
  // Generic node creator
  static FlexNode* createNode(FlexDocument &document,
                                  const FlexNodeDesc &desc);

  // Widget creators
  static FlexNode* createLabel(FlexDocument &document,
                                 const FlexLabelProps &props);
  static FlexButton* createButton(FlexDocument &document,
                                  const FlexButtonProps &props);
  static std::string createTextInput(FlexDocument &document,
                                     const FlexTextInputProps &props);
  static FlexToggle* createToggle(FlexDocument &document,
                                  const FlexToggleProps &props);
  static FlexCheckbox* createCheckbox(FlexDocument &document,
                                    const FlexCheckboxProps &props);
  static std::string createSlider(FlexDocument &document,
                                  const FlexSliderProps &props);
  static std::string createProgressBar(FlexDocument &document,
                                       const FlexProgressBarProps &props);
  static std::string createDropdown(FlexDocument &document,
                                    const FlexDropdownProps &props);
  static std::string createRadio(FlexDocument &document,
                                    const FlexRadioProps &props);
  static std::string createSpinner(FlexDocument &document,
                                    const FlexSpinnerProps &props);
  static std::string createTooltip(FlexDocument &document,
                                    const FlexTooltipProps &props);
  static std::string createAlert(FlexDocument &document,
                                    const FlexAlertProps &props);
  static std::string createAccordion(FlexDocument &document,
                                    const FlexAccordionProps &props);
  static std::string createTabs(FlexDocument &document,
                                    const FlexTabsProps &props);
  
  // Layout structure creators
  static std::string createFlexContainer(FlexDocument &document,
                                         const FlexContainerProps &props);
  static std::string createFlexItem(FlexDocument &document,
                                    const FlexItemProps &props);
  static std::string createGrid(FlexDocument &document,
                                const FlexGridProps &props);
  static std::string createGridItem(FlexDocument &document,
                                    const FlexGridItemProps &props);
};

} // namespace flexui
