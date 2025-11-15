#include "flexui/widget_factory.h"

#include <algorithm>
#include <cstdio>

namespace flexui {

namespace {
FlexNode &createNode(FlexDocument &document, FlexNodeDesc desc,
                     const std::string &parent_id) {
  if (desc.classes.empty()) {
    desc.classes.push_back("flex-node");
  }
  return document.appendNode(desc, parent_id);
}

void ensureClass(std::vector<std::string> &classes, const std::string &cls) {
  if (std::find(classes.begin(), classes.end(), cls) == classes.end()) {
    classes.push_back(cls);
  }
}

std::string px(float value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2fpx", value);
  return buffer;
}

} // namespace

std::string FlexWidgetFactory::createButton(FlexDocument &document,
                                            const FlexButtonProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.tag = "rect";
  desc.classes = props.classes;
  ensureClass(desc.classes, "button");
  desc.styles = props.styles;
  desc.text = props.text;

  FlexNode &node = createNode(document, std::move(desc), props.parent_id);
  return node.id();
}

std::string
FlexWidgetFactory::createTextInput(FlexDocument &document,
                                   const FlexTextInputProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.tag = "rect";
  desc.classes = props.classes;
  ensureClass(desc.classes, "text-input");
  desc.styles = props.styles;
  desc.text = props.text;

  FlexNode &node = createNode(document, std::move(desc), props.parent_id);
  if (!props.placeholder.empty()) {
    document.setAttribute(node.id(), "placeholder", props.placeholder);
  }

  return node.id();
}

std::string FlexWidgetFactory::createToggle(FlexDocument &document,
                                            const FlexToggleProps &props) {
  FlexNodeDesc track;
  track.id = props.id;
  track.tag = "rect";
  track.classes = props.track_classes;
  ensureClass(track.classes, "toggle");
  track.styles = props.track_styles;

  FlexNode &track_node =
      createNode(document, std::move(track), props.parent_id);

  FlexNodeDesc handle;
  handle.id =
      props.handle_id.empty() ? track_node.id() + "-handle" : props.handle_id;
  handle.tag = "circle";
  handle.classes = props.handle_classes;
  ensureClass(handle.classes, "toggle-handle");
  handle.styles = props.handle_styles;

  createNode(document, std::move(handle), track_node.id());
  document.setClass(track_node.id(), "toggle-on", props.initial_on);
  return track_node.id();
}

std::string FlexWidgetFactory::createCheckbox(FlexDocument &document,
                                              const FlexCheckboxProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.tag = "rect";
  desc.classes = props.classes;
  ensureClass(desc.classes, "checkbox");
  desc.styles = props.styles;

  FlexNode &node = createNode(document, std::move(desc), props.parent_id);
  document.setClass(node.id(), "checkbox-checked", props.checked);
  return node.id();
}

std::string FlexWidgetFactory::createSlider(FlexDocument &document,
                                            const FlexSliderProps &props) {
  FlexNodeDesc track;
  track.id = props.track_id;
  track.tag = "rect";
  track.classes = props.track_classes;
  ensureClass(track.classes, "slider-track");
  track.styles = props.track_styles;
  track.styles["left"] = px(props.left);
  track.styles["top"] = px(props.top);
  track.styles["width"] = px(props.width);
  track.styles["height"] = px(props.height);

  FlexNode &track_node =
      createNode(document, std::move(track), props.parent_id);

  FlexNodeDesc fill;
  fill.id = props.fill_id.empty() ? track_node.id() + "-fill" : props.fill_id;
  fill.tag = "rect";
  fill.classes = props.fill_classes;
  ensureClass(fill.classes, "slider-fill");
  fill.styles = props.fill_styles;
  fill.styles["left"] = px(props.left);
  fill.styles["top"] = px(props.top);
  fill.styles["width"] = px(props.value * props.width);
  fill.styles["height"] = px(props.height);
  createNode(document, std::move(fill), props.parent_id);

  FlexNodeDesc thumb;
  thumb.id = props.thumb_id.empty() ? track_node.id() + "-thumb" : props.thumb_id;
  thumb.tag = "circle";
  thumb.classes = props.thumb_classes;
  ensureClass(thumb.classes, "slider-thumb");
  thumb.styles = props.thumb_styles;
  const float thumb_left =
      props.left + props.value * props.width - props.thumb_size * 0.5f;
  const float thumb_top =
      props.top + props.height * 0.5f - props.thumb_size * 0.5f;
  thumb.styles["left"] = px(thumb_left);
  thumb.styles["top"] = px(thumb_top);
  thumb.styles["width"] = px(props.thumb_size);
  thumb.styles["height"] = px(props.thumb_size);
  createNode(document, std::move(thumb), props.parent_id);

  return track_node.id();
}

std::string FlexWidgetFactory::createRadio(FlexDocument &document,
                                           const FlexRadioProps &props) {
  FlexNodeDesc desc;
  desc.id = props.id;
  desc.tag = "circle";
  desc.classes = props.classes;
  ensureClass(desc.classes, "radio");
  desc.styles = props.styles;

  FlexNode &node = createNode(document, std::move(desc), props.parent_id);
  return node.id();
}

std::string FlexWidgetFactory::createDropdown(FlexDocument &document,
                                              const FlexDropdownProps &props) {
  FlexNodeDesc container;
  container.id = props.id;
  container.tag = "rect";
  container.classes = props.container_classes;
  ensureClass(container.classes, "dropdown");
  container.styles = props.container_styles;

  FlexNode &container_node =
      createNode(document, std::move(container), props.parent_id);

  FlexNodeDesc display;
  display.id = props.display_id.empty() ? container_node.id() + "-display"
                                        : props.display_id;
  display.tag = "rect";
  display.classes = {"dropdown-display"};
  display.text = props.placeholder;
  createNode(document, std::move(display), container_node.id());

  FlexNodeDesc button;
  button.id = props.button_id.empty() ? container_node.id() + "-button"
                                      : props.button_id;
  button.tag = "rect";
  button.classes = {"dropdown-button"};
  createNode(document, std::move(button), container_node.id());

  FlexNodeDesc menu;
  menu.id = props.menu_id.empty() ? container_node.id() + "-menu"
                                  : props.menu_id;
  menu.tag = "rect";
  menu.classes = {"dropdown-menu"};
  FlexNode &menu_node =
      createNode(document, std::move(menu), container_node.id());

  for (const auto &option : props.options) {
    FlexNodeDesc option_desc;
    option_desc.id = option.id;
    option_desc.tag = "label";
    option_desc.classes = option.classes;
    ensureClass(option_desc.classes, "dropdown-option");
    option_desc.styles = option.styles;
    option_desc.text = option.text;
    FlexNode &option_node =
        createNode(document, std::move(option_desc), menu_node.id());
    if (!option.value.empty()) {
      document.setAttribute(option_node.id(), "data-value", option.value);
    }
  }

  return container_node.id();
}

} // namespace flexui
