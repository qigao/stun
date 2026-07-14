#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace flexUI {

class Box;
class Element;

struct UiRepeaterResult {
  std::size_t created = 0;
  std::size_t reused = 0;
  std::size_t retired = 0;
  std::size_t moved = 0;
  std::size_t active = 0;
  std::size_t retained = 0;
};

/**
 * Reconciles a container's direct children by stable application keys.
 *
 * The repeater exclusively owns the container's direct-child order. Detached
 * keyed elements are retained by Box so a later matching key can reuse widget
 * and element state, including live data bindings. Transitions, animations,
 * focus, hover, active state, and pointer capture are cleared on retirement.
 * Total unique keys are bounded because Box does not yet expose general subtree
 * destruction.
 *
 * Reconciliation commits the child structure only after keys and callbacks
 * succeed. Mutations performed by callbacks on existing elements are not
 * rolled back when a later callback throws. Callbacks must not mutate the
 * managed container's direct children.
 */
class UiKeyedRepeater {
 public:
  static constexpr std::size_t kDefaultKeyCapacity = 1024;

  UiKeyedRepeater(Box& box, Element& container,
                  std::size_t key_capacity = kDefaultKeyCapacity);
  ~UiKeyedRepeater() noexcept;

  UiKeyedRepeater(const UiKeyedRepeater&) = delete;
  UiKeyedRepeater& operator=(const UiKeyedRepeater&) = delete;

  template <typename Item, typename KeyFn, typename CreateFn,
            typename UpdateFn>
  UiRepeaterResult reconcile(const std::vector<Item>& items, KeyFn&& key_fn,
                             CreateFn&& create_fn, UpdateFn&& update_fn) {
    return reconcile_erased(
        items.size(),
        [&](std::size_t index) {
          return std::string(std::invoke(key_fn, items[index]));
        },
        [&](std::size_t index) {
          return std::invoke(create_fn, box(), items[index]);
        },
        [&](Element& element, std::size_t index) {
          std::invoke(update_fn, element, items[index]);
        });
  }

  UiRepeaterResult clear();
  Element* find(const std::string& key) const;
  std::size_t active_count() const;
  std::size_t retained_count() const;
  std::size_t key_capacity() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;

  Box& box();
  UiRepeaterResult reconcile_erased(
      std::size_t count,
      const std::function<std::string(std::size_t)>& key_at,
      const std::function<Element*(std::size_t)>& create_at,
      const std::function<void(Element&, std::size_t)>& update_at);
};

}  // namespace flexUI
