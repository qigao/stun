#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace flexUI {

using TextValueObserverId = std::uint64_t;

/**
 * Narrow adapter for widgets whose editable value is text.
 *
 * Programmatic set_text_value() does not emit edit notifications. Observers
 * receive only user-originated edits and must unregister before destruction.
 */
class TextValueWidget {
 public:
  using EditObserver = std::function<void(const std::string&)>;

  virtual ~TextValueWidget() = default;
  virtual const std::string& text_value() const = 0;
  virtual void set_text_value(const std::string& value) = 0;
  virtual TextValueObserverId add_edit_observer(EditObserver observer) = 0;
  virtual bool remove_edit_observer(TextValueObserverId id) = 0;
};

}  // namespace flexUI
