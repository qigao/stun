#include "flexui/widget_factory.h"
#include "flexui/widgets/button.h" // Include FlexButton
#include "flexui/widgets/checkbox.h" // Include FlexCheckbox
#include "flexui/widgets/toggle.h" // Include FlexToggle
#include <algorithm>
#include <unordered_set> // Include for std::unordered_set

namespace flexui {

namespace {
void ensureClass(std::unordered_set<std::string> &classes, const std::string &cls) {
  classes.insert(cls);
}

std::string buildStyleString(const std::unordered_map<std::string, std::string>& styles) {
    std::string style_str;
    for (const auto& [key, value] : styles) {
        style_str += key + ": " + value + "; ";
    }
    return style_str;
}

void applyStyles(FlexNodeDesc& desc, const std::unordered_map<std::string, std::string>& styles) {
    if (styles.empty()) return;
    std::string style_str = buildStyleString(styles);
    if (!style_str.empty()) {
        desc.attributes["style"] = style_str;
    }
}
} // namespace

FlexNode* FlexWidgetFactory::createNode(FlexDocument &document,
                                          const FlexNodeDesc &desc) {
  return &document.appendNode(desc, desc.parent_id);
}

FlexNode* FlexWidgetFactory::createLabel(FlexDocument &document,
                                           const FlexLabelProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "label";
  desc.classes = props.classes;
  ensureClass(desc.classes, "label");
  applyStyles(desc, props.styles);
  desc.text = props.text;
  return createNode(document, desc);
}

FlexButton* FlexWidgetFactory::createButton(FlexDocument &document,
                                            const FlexButtonProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = props.classes;
  ensureClass(desc.classes, "button");
  applyStyles(desc, props.styles);
  desc.text = props.text;
  
  // Use createNode template from Document or manual addNode
  return &document.createNode<FlexButton>(desc.parent_id, desc);
}

std::string
FlexWidgetFactory::createTextInput(FlexDocument &document,
                                   const FlexTextInputProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "div"; // Visually represented by a div
  desc.classes = props.classes;
  ensureClass(desc.classes, "text-input");
  applyStyles(desc, props.styles);
  desc.text = props.text;

  auto& node = document.appendNode(desc, desc.parent_id);

  if (!props.placeholder.empty()) {
    document.setAttribute(node.id(), "placeholder", props.placeholder);
  }

  return node.id();
}

FlexToggle* FlexWidgetFactory::createToggle(FlexDocument &document,
                                            const FlexToggleProps &props) {
  FlexNodeDesc track_desc;
  track_desc.id = props.id;
  track_desc.parent_id = props.parent_id;
  track_desc.tag = "div";
  track_desc.classes = props.track_classes;
  ensureClass(track_desc.classes, "toggle-track");
  applyStyles(track_desc, props.track_styles);

  FlexToggle* track_node = &document.createNode<FlexToggle>(track_desc.parent_id, track_desc);
  if (props.initial_on) {
    track_node->setOn(true);
  }

  FlexNodeDesc handle_desc;
  handle_desc.id = props.handle_id;
  handle_desc.parent_id = track_node->id();
  handle_desc.tag = "div";
  handle_desc.classes = props.handle_classes;
  ensureClass(handle_desc.classes, "toggle-handle");
  applyStyles(handle_desc, props.handle_styles);
  createNode(document, handle_desc);

  return track_node;
}

FlexCheckbox* FlexWidgetFactory::createCheckbox(FlexDocument &document,
                                              const FlexCheckboxProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = props.classes;
  ensureClass(desc.classes, "checkbox");
  applyStyles(desc, props.styles);
  
  FlexCheckbox* node = &document.createNode<FlexCheckbox>(desc.parent_id, desc);
  if (props.checked) {
      node->setChecked(true);
  }
  return node;
}

std::string FlexWidgetFactory::createSlider(FlexDocument &document,
                                            const FlexSliderProps &props) {
  FlexNodeDesc track_desc;
  track_desc.id = props.track_id;
  track_desc.parent_id = props.parent_id;
  track_desc.tag = "div";
  track_desc.classes = props.track_classes;
  ensureClass(track_desc.classes, "slider-track");

  // All layout (position, size) MUST be defined in CSS, not via props

  std::string track_id = createNode(document, track_desc)->id();

  FlexNodeDesc fill_desc;
  fill_desc.id = props.fill_id;
  fill_desc.parent_id = track_id;
  fill_desc.tag = "div";
  fill_desc.classes = props.fill_classes;
  ensureClass(fill_desc.classes, "slider-fill");
  // Width will be set by controller via a CSS variable or class
  createNode(document, fill_desc);

  FlexNodeDesc thumb_desc;
  thumb_desc.id = props.thumb_id;
  thumb_desc.parent_id = track_id;
  thumb_desc.tag = "div";
  thumb_desc.classes = props.thumb_classes;
  ensureClass(thumb_desc.classes, "slider-thumb");
  // Position will be set by controller
  createNode(document, thumb_desc);

  return track_id;
}

std::string
FlexWidgetFactory::createProgressBar(FlexDocument &document,
                                     const FlexProgressBarProps &props) {
  FlexNodeDesc track_desc;
  track_desc.id = props.track_id;
  track_desc.parent_id = props.parent_id;
  track_desc.tag = "div";
  track_desc.classes = props.track_classes;
  ensureClass(track_desc.classes, "progress-track");

  // All layout (position, size) MUST be defined in CSS, not via props

  std::string track_id = createNode(document, track_desc)->id();

  FlexNodeDesc fill_desc;
  fill_desc.id = props.fill_id;
  fill_desc.parent_id = track_id;
  fill_desc.tag = "div";
  fill_desc.classes = props.fill_classes;
  ensureClass(fill_desc.classes, "progress-fill");
  // Width will be set by controller
  createNode(document, fill_desc);

  return track_id;
}

std::string FlexWidgetFactory::createDropdown(FlexDocument &document,
                                              const FlexDropdownProps &props) {
  FlexNodeDesc container_desc;
  container_desc.id = props.id;
  container_desc.parent_id = props.parent_id;
  container_desc.tag = "div";
  container_desc.classes = props.container_classes;
  ensureClass(container_desc.classes, "dropdown-container");
  applyStyles(container_desc, props.container_styles);
  std::string container_id = createNode(document, container_desc)->id();

  FlexNodeDesc display_desc;
  display_desc.id = props.id + "-display";
  display_desc.parent_id = container_id;
  display_desc.tag = "div";
  display_desc.classes = {"dropdown-display"};
  display_desc.text = props.placeholder;
  createNode(document, display_desc);

  FlexNodeDesc menu_desc;
  menu_desc.id = props.id + "-menu";
  menu_desc.parent_id = container_id;
  menu_desc.tag = "div";
  menu_desc.classes = {"dropdown-menu"};
  std::string menu_id = createNode(document, menu_desc)->id();

  for(const auto& option : props.options) {
    FlexNodeDesc option_desc;
    option_desc.id = option.id;
    option_desc.parent_id = menu_id;
    option_desc.tag = "div";
    option_desc.classes = option.classes;
    ensureClass(option_desc.classes, "dropdown-option");
    option_desc.text = option.text;
    auto& node = document.appendNode(option_desc, menu_id);
    document.setAttribute(node.id(), "data-value", option.value);
  }

  return container_id;
}

std::string FlexWidgetFactory::createRadio(FlexDocument &document,
                                    const FlexRadioProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = props.classes;
  ensureClass(desc.classes, "radio");
  applyStyles(desc, props.styles);
  return createNode(document, desc)->id();
}

std::string FlexWidgetFactory::createSpinner(FlexDocument &document,
                                    const FlexSpinnerProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = props.classes;
  ensureClass(desc.classes, "spinner");
  applyStyles(desc, props.styles);
  // Size usually handled by CSS, but we could set width/height if size prop is used
  return createNode(document, desc)->id();
}

std::string FlexWidgetFactory::createTooltip(FlexDocument &document,
                                    const FlexTooltipProps &props) {
  FlexNodeDesc desc;
  desc.id = props.tooltip_id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = {"tooltip"};
  desc.text = props.text;
  return createNode(document, desc)->id();
}

std::string FlexWidgetFactory::createAlert(FlexDocument &document,
                                    const FlexAlertProps &props) {
  FlexNodeDesc desc;
  desc.id = props.alert_id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = {"alert", "alert-" + props.type};
  applyStyles(desc, props.styles);
  desc.text = props.message;
  return createNode(document, desc)->id();
}

std::string FlexWidgetFactory::createAccordion(FlexDocument &document,
                                    const FlexAccordionProps &props) {
  FlexNodeDesc container_desc;
  container_desc.id = props.container_id;
  container_desc.parent_id = props.parent_id;
  container_desc.tag = "div";
  container_desc.classes = {"accordion"};
  applyStyles(container_desc, props.styles);
  std::string container_id = createNode(document, container_desc)->id();

  for(const auto& item : props.items) {
      FlexNodeDesc header_desc;
      header_desc.id = item.header_id;
      header_desc.parent_id = container_id;
      header_desc.tag = "div";
      header_desc.classes = {"accordion-header"};
      header_desc.text = item.title;
      createNode(document, header_desc);

      FlexNodeDesc content_desc;
      content_desc.id = item.content_id;
      content_desc.parent_id = container_id;
      content_desc.tag = "div";
      content_desc.classes = {"accordion-content"};
      content_desc.text = item.content_text;
      createNode(document, content_desc);
  }
  return container_id;
}

std::string FlexWidgetFactory::createTabs(FlexDocument &document,
                                    const FlexTabsProps &props) {
  FlexNodeDesc container_desc;
  container_desc.id = props.container_id;
  container_desc.parent_id = props.parent_id;
  container_desc.tag = "div";
  container_desc.classes = {"tabs-container"};
  applyStyles(container_desc, props.container_styles);
  std::string container_id = createNode(document, container_desc)->id();
  
  // Tab headers container
  FlexNodeDesc headers_desc;
  headers_desc.id = props.container_id + "-headers";
  headers_desc.parent_id = container_id;
  headers_desc.tag = "div";
  headers_desc.classes = {"tabs-header"};
  std::string headers_id = createNode(document, headers_desc)->id();

  // Tab content container
  FlexNodeDesc content_desc;
  content_desc.id = props.container_id + "-content";
  content_desc.parent_id = container_id;
  content_desc.tag = "div";
  content_desc.classes = {"tabs-content"};
  std::string content_id = createNode(document, content_desc)->id();

  for(const auto& tab : props.tabs) {
      FlexNodeDesc tab_desc;
      tab_desc.id = tab.id;
      tab_desc.parent_id = headers_id;
      tab_desc.tag = "div";
      tab_desc.classes = tab.classes;
      ensureClass(tab_desc.classes, "tab-item");
      applyStyles(tab_desc, tab.styles);
      tab_desc.text = tab.title;
      createNode(document, tab_desc);

      FlexNodeDesc panel_desc;
      panel_desc.id = tab.panel_id;
      panel_desc.parent_id = content_id;
      panel_desc.tag = "div";
      panel_desc.classes = {"tab-panel"};
      createNode(document, panel_desc);
  }
  return container_id;
}

std::string
FlexWidgetFactory::createFlexContainer(FlexDocument &document,
                                       const FlexContainerProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = props.classes;
  ensureClass(desc.classes, "flex-container");
  applyStyles(desc, props.styles);
  return createNode(document, desc)->id();
}

std::string FlexWidgetFactory::createFlexItem(FlexDocument &document,
                                              const FlexItemProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = props.classes;
  ensureClass(desc.classes, "flex-item");
  applyStyles(desc, props.styles);
  desc.text = props.text;
  return createNode(document, desc)->id();
}

std::string FlexWidgetFactory::createGrid(FlexDocument &document,
                                          const FlexGridProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = props.classes;
  ensureClass(desc.classes, "grid-container");
  applyStyles(desc, props.styles);
  return createNode(document, desc)->id();
}

std::string FlexWidgetFactory::createGridItem(FlexDocument &document,
                                              const FlexGridItemProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.parent_id = props.parent_id;
  desc.tag = "div";
  desc.classes = props.classes;
  ensureClass(desc.classes, "grid-item");
  applyStyles(desc, props.styles);
  desc.text = props.text;
  return createNode(document, desc)->id();
}

} // namespace flexui
