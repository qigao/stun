#include <flexUI/box_mutation_host.h>

#include <flexUI/box.h>
#include <flexUI/element.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace flexUI {

namespace {

using AttributeMap =
    std::unordered_map<Symbol, std::string, SymbolHash>;
using SymbolSet = std::unordered_set<Symbol, SymbolHash>;
using StringSet = std::unordered_set<std::string>;
using InputMutation =
    std::variant<SetBindingInputNumberMutation, SetBindingInputBoolMutation,
                 SetBindingInputStringMutation>;

static_assert(noexcept(std::declval<AttributeMap&>().swap(
    std::declval<AttributeMap&>())));
static_assert(noexcept(
    std::declval<SymbolSet&>().swap(std::declval<SymbolSet&>())));
static_assert(noexcept(
    std::declval<StringSet&>().swap(std::declval<StringSet&>())));
static_assert(noexcept(std::declval<std::string&>().swap(
    std::declval<std::string&>())));

struct PreparedSelectorState {
  SymbolSet classes;
  SymbolSet symbol_only_classes;
  StringSet class_names;
  StringSet utility_names;
  std::string class_attribute;
};

struct PreparedElementState {
  Element* element = nullptr;
  std::optional<PreparedSelectorState> selectors;
  std::optional<AttributeMap> attributes;
  std::optional<std::string> text;
};

StringSet split_tokens(std::string_view value) {
  StringSet tokens;
  std::size_t cursor = 0;
  while (cursor < value.size()) {
    while (cursor < value.size() &&
           std::isspace(static_cast<unsigned char>(value[cursor]))) {
      ++cursor;
    }
    const std::size_t start = cursor;
    while (cursor < value.size() &&
           !std::isspace(static_cast<unsigned char>(value[cursor]))) {
      ++cursor;
    }
    if (start != cursor) {
      tokens.emplace(value.substr(start, cursor - start));
    }
  }
  return tokens;
}

void rebuild_selector_state(PreparedSelectorState& state) {
  state.classes = state.symbol_only_classes;
  for (const auto& name : state.class_names) {
    state.classes.insert(Symbol(name));
  }

  std::vector<std::string> ordered(state.class_names.begin(),
                                   state.class_names.end());
  std::sort(ordered.begin(), ordered.end());
  state.class_attribute.clear();
  for (const auto& name : ordered) {
    if (!state.class_attribute.empty()) {
      state.class_attribute.push_back(' ');
    }
    state.class_attribute += name;
  }
}

bool is_active_application_target(const Box& box, const Element& element) {
  if (element.is_widget_owned() || !box.root()) {
    return false;
  }
  const Element* current = &element;
  while (current && current != box.root()) {
    current = current->parent_elem();
  }
  return current == box.root();
}

MutationPrepareResult prepare_error(MutationErrorCode code,
                                    std::size_t index,
                                    std::string message) {
  return {{}, {code, index, std::move(message)}};
}

} // namespace

class BoxPreparedMutation final : public IPreparedUiMutation {
public:
  BoxPreparedMutation(Box& box, std::vector<PreparedElementState> elements,
                      std::vector<InputMutation> inputs)
      : box_(box), elements_(std::move(elements)), inputs_(std::move(inputs)) {}

  void commit() noexcept override {
    if (committed_) {
      return;
    }
    committed_ = true;
    auto& data = box_.bindings().inputs();
    bool inputs_changed = false;
    for (auto& input : inputs_) {
      std::visit(
          [&data, &inputs_changed](auto& value) noexcept {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T,
                                         SetBindingInputNumberMutation>) {
              inputs_changed = data.commit_existing_number(value.name,
                                                            value.value) ||
                               inputs_changed;
            } else if constexpr (std::is_same_v<
                                     T, SetBindingInputBoolMutation>) {
              inputs_changed = data.commit_existing_bool(value.name,
                                                          value.value) ||
                               inputs_changed;
            } else {
              inputs_changed = data.commit_existing_string(value.name,
                                                            value.value) ||
                               inputs_changed;
            }
          },
          input);
    }
    if (inputs_changed) {
      box_.notify_dirty_style();
      box_.notify_dirty_layout();
      box_.notify_dirty_paint();
    }

    for (auto& state : elements_) {
      Element& element = *state.element;
      if (state.selectors) {
        const bool utilities_changed =
            element.utility_names_ != state.selectors->utility_names;
        element.classes_.swap(state.selectors->classes);
        element.symbol_only_classes_.swap(
            state.selectors->symbol_only_classes);
        element.class_names_.swap(state.selectors->class_names);
        element.utility_names_.swap(state.selectors->utility_names);
        element.class_attribute_.swap(state.selectors->class_attribute);
        element.mark_style_dirty();
        if (auto* parent = element.parent_elem()) {
          parent->mark_style_dirty();
        }
        if (utilities_changed) {
          box_.notify_utility_tree_changed();
        }
      }
      if (state.attributes) {
        element.attributes_.swap(*state.attributes);
        element.mark_style_dirty();
        if (auto* parent = element.parent_elem()) {
          parent->mark_style_dirty();
        }
      }
      if (state.text) {
        element.text_content_.swap(*state.text);
        element.mark_layout_dirty();
      }
    }
  }

private:
  Box& box_;
  std::vector<PreparedElementState> elements_;
  std::vector<InputMutation> inputs_;
  bool committed_ = false;
};

BoxMutationHost::BoxMutationHost(Box& box) noexcept : box_(box) {}

MutationPrepareResult BoxMutationHost::prepare(
    const UiMutationBatch& batch) {
  std::vector<PreparedElementState> elements;
  elements.reserve(batch.size());
  std::unordered_map<Element*, std::size_t> element_index;
  element_index.reserve(batch.size());
  std::vector<InputMutation> inputs;
  inputs.reserve(batch.size());

  const auto state_for = [this, &elements, &element_index](
                             const UiHandle& handle,
                             std::size_t mutation_index)
      -> std::variant<PreparedElementState*, MutationPrepareResult> {
    Element* element = box_.resolve_handle(handle);
    if (!element || !is_active_application_target(box_, *element)) {
      return prepare_error(MutationErrorCode::InvalidTarget, mutation_index,
                           "mutation target is stale, detached, or not "
                           "application-owned");
    }
    const auto found = element_index.find(element);
    if (found != element_index.end()) {
      return &elements[found->second];
    }
    const std::size_t index = elements.size();
    elements.push_back(PreparedElementState{element});
    element_index.emplace(element, index);
    return &elements.back();
  };

  std::size_t input_mutation_count = 0;
  for (std::size_t index = 0; index < batch.mutations().size(); ++index) {
    const auto& mutation = batch.mutations()[index];
    auto error = std::visit(
        [this, index, &state_for, &inputs,
         &input_mutation_count](const auto& value)
            -> std::optional<MutationPrepareResult> {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, SetBindingInputNumberMutation> ||
                        std::is_same_v<T, SetBindingInputBoolMutation> ||
                        std::is_same_v<T, SetBindingInputStringMutation>) {
            auto& data = box_.bindings().inputs();
            if (!data.contains(value.name)) {
              return prepare_error(MutationErrorCode::InvalidName, index,
                                   "binding input is not declared: " +
                                       value.name);
            }
            try {
              if constexpr (std::is_same_v<
                                T, SetBindingInputNumberMutation>) {
                (void)data.number(value.name);
              } else if constexpr (std::is_same_v<
                                       T, SetBindingInputBoolMutation>) {
                (void)data.boolean(value.name);
              } else {
                (void)data.string(value.name);
              }
            } catch (const std::invalid_argument&) {
              return prepare_error(MutationErrorCode::InvalidType, index,
                                   "binding input type does not match: " +
                                       value.name);
            }
            inputs.emplace_back(value);
            ++input_mutation_count;
            return std::nullopt;
          } else {
            auto resolved = state_for(value.target, index);
            if (auto* failure = std::get_if<MutationPrepareResult>(&resolved)) {
              return std::move(*failure);
            }
            PreparedElementState& state =
                **std::get_if<PreparedElementState*>(&resolved);
            if constexpr (std::is_same_v<T, SetTextMutation>) {
              if (!state.text) {
                if (state.element->text_content_ != value.text) {
                  state.text = value.text;
                }
              } else if (*state.text != value.text) {
                *state.text = value.text;
              }
            } else if constexpr (std::is_same_v<T,
                                                SetAttributeMutation>) {
              if (value.name == "class") {
                return prepare_error(
                    MutationErrorCode::InvalidName, index,
                    "class must be changed with SetClassesMutation");
              }
              const Symbol name(value.name);
              if (!state.attributes) {
                const auto found = state.element->attributes_.find(name);
                if (found != state.element->attributes_.end() &&
                    found->second == value.value) {
                  return std::nullopt;
                }
                state.attributes = state.element->attributes_;
              }
              auto found = state.attributes->find(name);
              if (found == state.attributes->end() ||
                  found->second != value.value) {
                (*state.attributes)[name] = value.value;
              }
            } else if constexpr (std::is_same_v<T,
                                                RemoveAttributeMutation>) {
              if (value.name == "class") {
                return prepare_error(
                    MutationErrorCode::InvalidName, index,
                    "class must be changed with SetClassesMutation");
              }
              const Symbol name(value.name);
              if (!state.attributes) {
                if (state.element->attributes_.count(name) == 0) {
                  return std::nullopt;
                }
                state.attributes = state.element->attributes_;
              }
              state.attributes->erase(name);
            } else if constexpr (std::is_same_v<T, SetClassesMutation>) {
              auto next = split_tokens(value.classes);
              const auto& current = state.selectors
                                        ? state.selectors->class_names
                                        : state.element->class_names_;
              if (next == current) {
                return std::nullopt;
              }
              if (!state.selectors) {
                state.selectors = PreparedSelectorState{
                    state.element->classes_,
                    state.element->symbol_only_classes_,
                    state.element->class_names_,
                    state.element->utility_names_,
                    state.element->class_attribute_};
              }
              auto& selectors = *state.selectors;
              selectors.class_names = std::move(next);
              for (auto it = selectors.utility_names.begin();
                   it != selectors.utility_names.end();) {
                if (selectors.class_names.count(*it) == 0) {
                  it = selectors.utility_names.erase(it);
                } else {
                  ++it;
                }
              }
              rebuild_selector_state(selectors);
            } else if constexpr (std::is_same_v<T,
                                                SetUtilitiesMutation>) {
              auto next = split_tokens(value.utilities);
              for (const auto& utility : next) {
                if (!box_.is_known_utility(utility)) {
                  return prepare_error(MutationErrorCode::InvalidValue, index,
                                       "unknown utility token: " + utility);
                }
              }
              const auto& current = state.selectors
                                        ? state.selectors->utility_names
                                        : state.element->utility_names_;
              if (next == current) {
                return std::nullopt;
              }
              if (!state.selectors) {
                state.selectors = PreparedSelectorState{
                    state.element->classes_,
                    state.element->symbol_only_classes_,
                    state.element->class_names_,
                    state.element->utility_names_,
                    state.element->class_attribute_};
              }
              auto& selectors = *state.selectors;
              for (const auto& utility : selectors.utility_names) {
                selectors.class_names.erase(utility);
              }
              for (const auto& utility : next) {
                selectors.class_names.insert(utility);
              }
              selectors.utility_names = std::move(next);
              rebuild_selector_state(selectors);
            }
            return std::nullopt;
          }
        },
        mutation);
    if (error) {
      return std::move(*error);
    }
  }

  const auto revision = box_.bindings().inputs().revision();
  if (input_mutation_count >
      std::numeric_limits<std::uint64_t>::max() - revision) {
    return prepare_error(MutationErrorCode::InvalidValue, 0,
                         "binding input revision would overflow");
  }

  return {std::make_unique<BoxPreparedMutation>(
              box_, std::move(elements), std::move(inputs)),
          {}};
}

} // namespace flexUI
