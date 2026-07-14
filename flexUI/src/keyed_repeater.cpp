#include <flexUI/keyed_repeater.h>

#include <flexUI/box.h>
#include <flexUI/element.h>

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace flexUI {

namespace {

void require_owned_children(const Element& container,
                            const std::vector<std::string>& order,
                            const std::unordered_map<std::string, Element*>& active) {
  if (container.child_count() != order.size() || active.size() != order.size()) {
    throw std::logic_error(
        "keyed repeater container children were modified externally");
  }
  for (std::size_t index = 0; index < order.size(); ++index) {
    const auto item = active.find(order[index]);
    if (item == active.end() || container.child_at(index) != item->second ||
        item->second->parent_elem() != &container) {
      throw std::logic_error(
          "keyed repeater container children were modified externally");
    }
  }
}

}  // namespace

struct UiKeyedRepeater::Impl {
  Impl(Box& owner, Element& target, std::size_t capacity)
      : box(&owner), container(&target), key_capacity(capacity) {}

  Box* box = nullptr;
  Element* container = nullptr;
  std::size_t key_capacity = 0;
  std::unordered_map<std::string, Element*> active;
  std::unordered_map<std::string, Element*> retained;
  std::unordered_map<Element*, std::string> keys_by_element;
  std::vector<std::string> order;
};

UiKeyedRepeater::UiKeyedRepeater(Box& box, Element& container,
                                 std::size_t key_capacity)
    : impl_(std::make_unique<Impl>(box, container, key_capacity)) {
  if (key_capacity == 0) {
    throw std::invalid_argument("keyed repeater capacity must be positive");
  }
  if (!box.owns_element(&container)) {
    throw std::invalid_argument(
        "keyed repeater container must be owned by its Box");
  }
  if (container.is_widget_owned()) {
    throw std::invalid_argument(
        "keyed repeater requires an application-owned container");
  }
  if (container.child_count() != 0) {
    throw std::invalid_argument(
        "keyed repeater requires an initially empty container");
  }
}

UiKeyedRepeater::~UiKeyedRepeater() noexcept {
  if (!impl_) {
    return;
  }
  for (const auto& key : impl_->order) {
    const auto active = impl_->active.find(key);
    if (active == impl_->active.end() ||
        active->second->parent_elem() != impl_->container) {
      continue;
    }
    try {
      impl_->box->deactivate_subtree(active->second);
    } catch (...) {
      // Destructors cannot report event-handler failures; detachment still must
      // leave the container structurally valid.
    }
    impl_->container->remove(active->second);
  }
}

Box& UiKeyedRepeater::box() { return *impl_->box; }

UiRepeaterResult UiKeyedRepeater::reconcile_erased(
    std::size_t count,
    const std::function<std::string(std::size_t)>& key_at,
    const std::function<Element*(std::size_t)>& create_at,
    const std::function<void(Element&, std::size_t)>& update_at) {
  require_owned_children(*impl_->container, impl_->order, impl_->active);

  std::vector<std::string> keys;
  keys.reserve(count);
  std::unordered_set<std::string> unique;
  unique.reserve(count);
  std::size_t new_key_count = 0;
  for (std::size_t index = 0; index < count; ++index) {
    std::string key = key_at(index);
    if (key.empty()) {
      throw std::invalid_argument("keyed repeater keys must not be empty");
    }
    if (!unique.insert(key).second) {
      throw std::invalid_argument("duplicate keyed repeater key: " + key);
    }
    if (impl_->active.count(key) == 0 && impl_->retained.count(key) == 0) {
      ++new_key_count;
    }
    keys.push_back(std::move(key));
  }

  if (impl_->active.size() + impl_->retained.size() + new_key_count >
      impl_->key_capacity) {
    throw std::length_error("keyed repeater key capacity exceeded");
  }

  UiRepeaterResult result;
  std::unordered_map<std::string, Element*> next_active;
  next_active.reserve(count);
  std::vector<Element*> next_children;
  next_children.reserve(count);
  std::unordered_set<Element*> selected_elements;
  selected_elements.reserve(count);
  std::vector<std::pair<std::string, Element*>> created_elements;
  created_elements.reserve(new_key_count);

  try {
    for (std::size_t index = 0; index < count; ++index) {
      const std::string& key = keys[index];
      Element* element = nullptr;
      auto active = impl_->active.find(key);
      if (active != impl_->active.end()) {
        element = active->second;
        ++result.reused;
        if (index >= impl_->order.size() || impl_->order[index] != key) {
          ++result.moved;
        }
      } else {
        auto retained = impl_->retained.find(key);
        if (retained != impl_->retained.end()) {
          element = retained->second;
          ++result.reused;
        } else {
          element = create_at(index);
          if (!impl_->box->owns_element(element) ||
              element == impl_->container || element->is_widget_owned() ||
              element->parent_elem() ||
              impl_->keys_by_element.count(element) != 0) {
            throw std::invalid_argument(
                "keyed repeater create callback must return a detached "
                "untracked application element owned by the repeater Box");
          }
          created_elements.emplace_back(key, element);
          impl_->keys_by_element.emplace(element, key);
          ++result.created;
        }
      }
      if (!selected_elements.insert(element).second) {
        throw std::invalid_argument(
            "keyed repeater keys must resolve to distinct elements");
      }
      update_at(*element, index);
      next_active.emplace(key, element);
      next_children.push_back(element);
    }
    require_owned_children(*impl_->container, impl_->order, impl_->active);
  } catch (...) {
    for (const auto& [key, element] : created_elements) {
      impl_->retained.emplace(key, element);
    }
    throw;
  }

  for (const auto& [key, element] : impl_->active) {
    if (next_active.count(key) == 0) {
      impl_->box->deactivate_subtree(element);
      impl_->retained.emplace(key, element);
      ++result.retired;
    }
  }
  for (const auto& [key, element] : next_active) {
    (void)element;
    impl_->retained.erase(key);
  }

  impl_->container->replace_children(next_children);
  impl_->active = std::move(next_active);
  impl_->order = std::move(keys);
  result.active = impl_->active.size();
  result.retained = impl_->retained.size();
  return result;
}

UiRepeaterResult UiKeyedRepeater::clear() {
  return reconcile_erased(
      0, [](std::size_t) { return std::string{}; },
      [](std::size_t) -> Element* { return nullptr; },
      [](Element&, std::size_t) {});
}

Element* UiKeyedRepeater::find(const std::string& key) const {
  auto it = impl_->active.find(key);
  return it != impl_->active.end() ? it->second : nullptr;
}

std::size_t UiKeyedRepeater::active_count() const {
  return impl_->active.size();
}

std::size_t UiKeyedRepeater::retained_count() const {
  return impl_->retained.size();
}

std::size_t UiKeyedRepeater::key_capacity() const {
  return impl_->key_capacity;
}

}  // namespace flexUI
