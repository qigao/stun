#include <flexUI/widget.h>
#include <flexUI/box.h>
#include <flexUI/element.h>

namespace flexUI {

void Widget::bind_host_element(Element* elem) {
  host_element_ = elem;
  build_semantic_tree();
  sync_host_semantics();
}

Element* Widget::host_element() {
  return host_element_;
}

const Element* Widget::host_element() const {
  return host_element_;
}

Element* Widget::create_part(const char* tag, const char* name) {
  if (!host_element_ || !host_element_->owner_box_ || !tag || !name || !*name) {
    return nullptr;
  }
  const Symbol key(name);
  if (const auto it = parts_.find(key); it != parts_.end()) {
    return it->second;
  }
  Element* elem = host_element_->owner_box_->create_widget_part(
      *host_element_, tag, name);
  parts_.emplace(key, elem);
  return elem;
}

Element* Widget::part(const char* name) {
  const auto it = parts_.find(Symbol(name));
  return it != parts_.end() ? it->second : nullptr;
}

const Element* Widget::part(const char* name) const {
  const auto it = parts_.find(Symbol(name));
  return it != parts_.end() ? it->second : nullptr;
}

void Widget::set_host_attribute(const char* name, const std::string& value) {
  if (!host_element_) {
    return;
  }
  host_element_->set_attribute(name, value);
}

void Widget::clear_host_attribute(const char* name) {
  if (!host_element_) {
    return;
  }
  host_element_->remove_attribute(name);
}

void Widget::set_host_presence_attribute(const char* name, bool enabled) {
  if (!host_element_) {
    return;
  }
  if (enabled) {
    host_element_->set_attribute(name);
  } else {
    host_element_->remove_attribute(name);
  }
}

void Widget::set_host_boolean_attribute(const char* name, bool enabled) {
  if (!host_element_) {
    return;
  }
  host_element_->set_attribute(name, enabled ? "true" : "false");
}

void Widget::set_host_state(const char* state, bool active) {
  if (!host_element_) {
    return;
  }
  host_element_->set_state(state, active);
}

void Widget::set_host_data_state(const char* active_value,
                                 const char* inactive_value, bool active) {
  set_host_attribute("data-state", active ? active_value : inactive_value);
}

} // namespace flexUI
