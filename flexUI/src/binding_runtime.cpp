#include <flexUI/binding_runtime.h>

#include <flexUI/element.h>
#include <flexUI/text_value_widget.h>
#include <flexUI/widget.h>

#include <flex/core/expr_mir.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace flexUI {

namespace {

enum class InputKind : std::uint8_t { Number, Bool, String };

struct InputRecord {
  InputKind kind = InputKind::Number;
  std::uint64_t version = 0;
};

std::string format_number(double value, const std::string& suffix) {
  if (!std::isfinite(value)) {
    throw std::runtime_error("UI binding produced a non-finite number");
  }
  std::ostringstream stream;
  stream << std::setprecision(15) << value;
  return stream.str() + suffix;
}

void validate_custom_property_name(const std::string& property) {
  if (property.size() < 3 || property[0] != '-' || property[1] != '-') {
    throw std::invalid_argument(
        "bound CSS custom property names must start with --");
  }
}

struct ClassTarget {
  std::string name;
  bool previous = false;
};

struct ClassListTarget {
  std::string previous;
};

struct UtilityTarget {
  std::string name;
  bool previous = false;
};

struct UtilityListTarget {
  std::string previous;
};

struct AttributeTarget {
  std::string name;
  std::optional<std::string> previous;
  std::string true_value;
  std::string false_value;
};

struct TextTarget {
  std::string previous;
};

struct ValueTarget {
  TextValueWidget* widget = nullptr;
  TextValueObserverId observer_id = 0;
  std::string previous;
};

struct CustomPropertyTarget {
  std::string name;
  std::optional<std::string> previous;
  std::string suffix;
};

using BindingTarget =
    std::variant<ClassTarget, ClassListTarget, UtilityTarget,
                 UtilityListTarget, AttributeTarget, TextTarget, ValueTarget,
                 CustomPropertyTarget>;

enum class BindingSource : std::uint8_t { StringInput, BoolExpression,
                                          NumberExpression };

std::string target_key(const Element& target, const BindingTarget& binding_target) {
  const auto address = reinterpret_cast<std::uintptr_t>(&target);
  return std::visit(
      [address](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        std::string prefix;
        std::string name;
        if constexpr (std::is_same_v<T, ClassTarget>) {
          prefix = "class:";
          name = value.name;
        } else if constexpr (std::is_same_v<T, ClassListTarget>) {
          prefix = "classes:";
        } else if constexpr (std::is_same_v<T, UtilityTarget>) {
          prefix = "utility:";
          name = value.name;
        } else if constexpr (std::is_same_v<T, UtilityListTarget>) {
          prefix = "utilities:";
        } else if constexpr (std::is_same_v<T, AttributeTarget>) {
          prefix = "attribute:";
          name = value.name;
        } else if constexpr (std::is_same_v<T, TextTarget>) {
          prefix = "text:";
        } else if constexpr (std::is_same_v<T, ValueTarget>) {
          prefix = "value:";
        } else {
          prefix = "custom-property:";
          name = value.name;
        }
        return std::to_string(address) + ":" + prefix + name;
      },
      binding_target);
}

}  // namespace

struct UiDataContext::Impl {
  explicit Impl(std::function<void()> callback)
      : invalidated(std::move(callback)) {}

  void validate_name(const std::string& name) const {
    if (name.empty()) {
      throw std::invalid_argument("UI input name must not be empty");
    }
  }

  void validate_kind(const std::string& name, InputKind kind) const {
    auto it = records.find(flex::Symbol(name));
    if (it != records.end() && it->second.kind != kind) {
      throw std::invalid_argument("UI input type cannot change: " + name);
    }
  }

  std::uint64_t commit(flex::Symbol name, InputKind kind) {
    ++global_revision;
    records[name] = {kind, global_revision};
    if (invalidated) {
      invalidated();
    }
    return global_revision;
  }

  std::unordered_map<flex::Symbol, InputRecord, flex::SymbolHash> records;
  std::unordered_map<flex::Symbol, double, flex::SymbolHash> numeric_values;
  std::unordered_map<flex::Symbol, std::string, flex::SymbolHash> string_values;
  std::function<void()> invalidated;
  std::function<bool(flex::Symbol)> input_in_use;
  std::uint64_t global_revision = 0;
};

UiDataContext::UiDataContext(std::function<void()> invalidated)
    : impl_(std::make_unique<Impl>(std::move(invalidated))) {}

UiDataContext::~UiDataContext() = default;

void UiDataContext::set_number(const std::string& name, double value) {
  impl_->validate_name(name);
  if (!std::isfinite(value)) {
    throw std::invalid_argument("UI numeric inputs must be finite");
  }
  impl_->validate_kind(name, InputKind::Number);
  const flex::Symbol symbol(name);
  auto it = impl_->numeric_values.find(symbol);
  if (it != impl_->numeric_values.end() && it->second == value) {
    return;
  }
  impl_->numeric_values[symbol] = value;
  impl_->commit(symbol, InputKind::Number);
}

void UiDataContext::set_bool(const std::string& name, bool value) {
  impl_->validate_name(name);
  impl_->validate_kind(name, InputKind::Bool);
  const flex::Symbol symbol(name);
  const double numeric = value ? 1.0 : 0.0;
  auto it = impl_->numeric_values.find(symbol);
  if (it != impl_->numeric_values.end() && it->second == numeric) {
    return;
  }
  impl_->numeric_values[symbol] = numeric;
  impl_->commit(symbol, InputKind::Bool);
}

void UiDataContext::set_string(const std::string& name, std::string value) {
  impl_->validate_name(name);
  impl_->validate_kind(name, InputKind::String);
  const flex::Symbol symbol(name);
  auto it = impl_->string_values.find(symbol);
  if (it != impl_->string_values.end() && it->second == value) {
    return;
  }
  impl_->string_values[symbol] = std::move(value);
  impl_->commit(symbol, InputKind::String);
}

bool UiDataContext::erase(const std::string& name) {
  const flex::Symbol symbol(name);
  if (impl_->input_in_use && impl_->input_in_use(symbol)) {
    throw std::invalid_argument("cannot erase UI input while it is bound: " +
                                name);
  }
  if (impl_->records.erase(symbol) == 0) {
    return false;
  }
  impl_->numeric_values.erase(symbol);
  impl_->string_values.erase(symbol);
  ++impl_->global_revision;
  if (impl_->invalidated) {
    impl_->invalidated();
  }
  return true;
}

bool UiDataContext::contains(const std::string& name) const {
  return impl_->records.count(flex::Symbol(name)) > 0;
}

std::uint64_t UiDataContext::revision() const {
  return impl_->global_revision;
}

double UiDataContext::number(const std::string& name) const {
  const flex::Symbol symbol(name);
  auto record = impl_->records.find(symbol);
  if (record == impl_->records.end()) {
    throw std::invalid_argument("UI input does not exist: " + name);
  }
  if (record->second.kind != InputKind::Number) {
    throw std::invalid_argument("UI input is not a number: " + name);
  }
  return impl_->numeric_values.at(symbol);
}

bool UiDataContext::boolean(const std::string& name) const {
  const flex::Symbol symbol(name);
  auto record = impl_->records.find(symbol);
  if (record == impl_->records.end()) {
    throw std::invalid_argument("UI input does not exist: " + name);
  }
  if (record->second.kind != InputKind::Bool) {
    throw std::invalid_argument("UI input is not a bool: " + name);
  }
  return impl_->numeric_values.at(symbol) != 0.0;
}

const std::string& UiDataContext::string(const std::string& name) const {
  const flex::Symbol symbol(name);
  auto record = impl_->records.find(symbol);
  if (record == impl_->records.end()) {
    throw std::invalid_argument("UI input does not exist: " + name);
  }
  if (record->second.kind != InputKind::String) {
    throw std::invalid_argument("UI input is not a string: " + name);
  }
  return impl_->string_values.at(symbol);
}

struct UiBindingRuntime::Impl {
  struct Binding {
    UiBindingId id = 0;
    Element* element = nullptr;
    BindingTarget target;
    BindingSource source = BindingSource::StringInput;
    flex::Symbol string_input;
    std::vector<flex::Symbol> dependencies;
    std::vector<const double*> numeric_inputs;
    std::vector<double> numeric_slots;
    std::vector<std::uint64_t> observed_versions;
    std::shared_ptr<const flex::MirExpressionProgram> program;
    std::string owned_target_key;
    bool applied = false;
  };

  explicit Impl(std::function<void()> invalidated)
      : invalidated_callback(std::move(invalidated)),
        data(invalidated_callback) {
    data.impl_->input_in_use = [this](flex::Symbol input) {
      return std::any_of(bindings.begin(), bindings.end(),
                         [input](const Binding& binding) {
                           return std::find(binding.dependencies.begin(),
                                            binding.dependencies.end(),
                                            input) != binding.dependencies.end();
                         });
    };
  }

  ~Impl() {
    for (auto& binding : bindings) {
      if (auto* value = std::get_if<ValueTarget>(&binding.target)) {
        if (value->observer_id != 0) {
          value->widget->remove_edit_observer(value->observer_id);
        }
      }
    }
  }

  const InputRecord& require_input(const std::string& name, InputKind kind) const {
    const flex::Symbol symbol(name);
    auto it = data.impl_->records.find(symbol);
    if (it == data.impl_->records.end()) {
      throw std::invalid_argument("UI binding references a missing input: " +
                                  name);
    }
    if (it->second.kind != kind) {
      throw std::invalid_argument(
          "UI binding input has an incompatible type: " + name);
    }
    return it->second;
  }

  void attach_expression(
      const std::vector<std::string>& names,
      std::shared_ptr<const flex::MirExpressionProgram> program,
      Binding& binding) const {
    if (!program || program->names() != names) {
      throw std::invalid_argument(
          "compiled MIR UI binding inputs do not match its dependencies");
    }
    binding.dependencies.reserve(names.size());
    binding.numeric_inputs.reserve(names.size());
    binding.numeric_slots.resize(names.size());
    for (const auto& name : names) {
      const flex::Symbol symbol(name);
      auto it = data.impl_->records.find(symbol);
      if (it == data.impl_->records.end() || it->second.kind == InputKind::String) {
        throw std::invalid_argument(
            "MIR UI expression requires declared numeric or bool inputs: " + name);
      }
      binding.dependencies.push_back(symbol);
      binding.numeric_inputs.push_back(&data.impl_->numeric_values.at(symbol));
    }
    binding.program = std::move(program);
  }

  void compile_expression(const std::string& expression, Binding& binding) {
    const auto names = flex::MirExpressionProgram::collect_variables(expression);
    ++expression_compile_count;
    auto program = flex::MirExpressionProgram::compile(expression, names);
    if (!program) {
      throw std::invalid_argument("invalid MIR UI binding expression: " + expression);
    }
    attach_expression(
        names,
        std::shared_ptr<const flex::MirExpressionProgram>(std::move(program)),
        binding);
  }

  UiBindingHandle add_binding(Binding binding) {
    binding.owned_target_key = target_key(*binding.element, binding.target);
    const bool new_owns_class_list =
        std::holds_alternative<ClassListTarget>(binding.target) ||
        std::holds_alternative<UtilityListTarget>(binding.target);
    const bool new_owns_one_class =
        std::holds_alternative<ClassTarget>(binding.target) ||
        std::holds_alternative<UtilityTarget>(binding.target);
    const auto class_token = [](const BindingTarget& target)
        -> const std::string* {
      if (const auto* value = std::get_if<ClassTarget>(&target)) {
        return &value->name;
      }
      if (const auto* value = std::get_if<UtilityTarget>(&target)) {
        return &value->name;
      }
      return nullptr;
    };
    const bool class_conflict =
        (new_owns_class_list || new_owns_one_class) &&
        std::any_of(bindings.begin(), bindings.end(), [&](const Binding& existing) {
          if (existing.element != binding.element) {
            return false;
          }
          const bool existing_owns_class_list =
              std::holds_alternative<ClassListTarget>(existing.target) ||
              std::holds_alternative<UtilityListTarget>(existing.target);
          const bool existing_owns_one_class =
              std::holds_alternative<ClassTarget>(existing.target) ||
              std::holds_alternative<UtilityTarget>(existing.target);
          return (new_owns_class_list &&
                  (existing_owns_class_list || existing_owns_one_class)) ||
                 (new_owns_one_class && existing_owns_class_list) ||
                 (new_owns_one_class && existing_owns_one_class &&
                  *class_token(binding.target) ==
                      *class_token(existing.target));
        });
    if (class_conflict || owned_targets.count(binding.owned_target_key) != 0) {
      throw std::invalid_argument("UI binding target is already owned");
    }
    binding.id = next_id;
    const UiBindingHandle handle{binding.id,
                                 binding.program && binding.program->uses_jit()};
    bindings.push_back(std::move(binding));
    try {
      if (!owned_targets.insert(bindings.back().owned_target_key).second) {
        bindings.pop_back();
        throw std::invalid_argument("UI binding target is already owned");
      }
    } catch (...) {
      if (bindings.size() > 0 && bindings.back().id == handle.id) {
        bindings.pop_back();
      }
      throw;
    }
    ++next_id;
    if (!transaction_active && invalidated_callback) {
      invalidated_callback();
    }
    return handle;
  }

  bool dependencies_changed(const Binding& binding) const {
    if (!binding.applied ||
        binding.observed_versions.size() != binding.dependencies.size()) {
      return true;
    }
    for (size_t i = 0; i < binding.dependencies.size(); ++i) {
      auto it = data.impl_->records.find(binding.dependencies[i]);
      if (it == data.impl_->records.end()) {
        throw std::runtime_error("UI binding input was removed while in use");
      }
      if (binding.observed_versions[i] != it->second.version) {
        return true;
      }
    }
    return false;
  }

  void observe_dependencies(Binding& binding) const {
    binding.observed_versions.clear();
    binding.observed_versions.reserve(binding.dependencies.size());
    for (const auto dependency : binding.dependencies) {
      auto it = data.impl_->records.find(dependency);
      if (it == data.impl_->records.end()) {
        throw std::runtime_error("UI binding input was removed while in use");
      }
      binding.observed_versions.push_back(it->second.version);
    }
  }

  void restore(Binding& binding) {
    std::visit(
        [&binding](auto& target) {
          using T = std::decay_t<decltype(target)>;
          if constexpr (std::is_same_v<T, ClassTarget>) {
            binding.element->toggle_class(target.name, target.previous);
          } else if constexpr (std::is_same_v<T, ClassListTarget>) {
            binding.element->set_classes(target.previous);
          } else if constexpr (std::is_same_v<T, UtilityTarget>) {
            binding.element->toggle_utility(target.name, target.previous);
          } else if constexpr (std::is_same_v<T, UtilityListTarget>) {
            binding.element->set_utilities(target.previous);
          } else if constexpr (std::is_same_v<T, AttributeTarget>) {
            if (target.previous) {
              binding.element->set_attribute(target.name, *target.previous);
            } else {
              binding.element->remove_attribute(target.name);
            }
          } else if constexpr (std::is_same_v<T, TextTarget>) {
            binding.element->set_text(target.previous);
          } else if constexpr (std::is_same_v<T, ValueTarget>) {
            target.widget->remove_edit_observer(target.observer_id);
            target.observer_id = 0;
            target.widget->set_text_value(target.previous);
          } else {
            if (target.previous) {
              binding.element->set_custom_property(target.name, *target.previous);
            } else {
              binding.element->remove_custom_property(target.name);
            }
          }
        },
        binding.target);
  }

  std::function<void()> invalidated_callback;
  UiDataContext data;
  std::vector<Binding> bindings;
  std::unordered_set<std::string> owned_targets;
  UiBindingId next_id = 1;
  bool transaction_active = false;
  std::uint64_t update_count = 0;
  std::uint64_t evaluation_count = 0;
  std::uint64_t expression_compile_count = 0;
};

UiBindingRuntime::UiBindingRuntime(std::function<void()> invalidated)
    : impl_(std::make_unique<Impl>(std::move(invalidated))), targets_(*this) {}

UiBindingRuntime::~UiBindingRuntime() = default;

UiDataContext& UiBindingRuntime::inputs() { return impl_->data; }
const UiDataContext& UiBindingRuntime::inputs() const { return impl_->data; }

UiBindingTargets::UiBindingTargets(UiBindingRuntime& runtime)
    : runtime_(&runtime) {}

UiBindingTargets& UiBindingRuntime::targets() { return targets_; }
const UiBindingTargets& UiBindingRuntime::targets() const { return targets_; }

UiBindingRuntime::TransactionCheckpoint
UiBindingRuntime::begin_transaction() {
  if (impl_->transaction_active) {
    throw std::logic_error("nested UI binding transactions are not supported");
  }
  impl_->transaction_active = true;
  return TransactionCheckpoint{impl_->bindings.size(), impl_->next_id, true};
}

void UiBindingRuntime::commit_transaction(TransactionCheckpoint& checkpoint) {
  if (!checkpoint.active || !impl_->transaction_active) {
    throw std::logic_error("UI binding transaction is not active");
  }
  const bool changed = impl_->bindings.size() != checkpoint.binding_count;
  impl_->transaction_active = false;
  checkpoint.active = false;
  if (changed && impl_->invalidated_callback) {
    impl_->invalidated_callback();
  }
}

void UiBindingRuntime::rollback_transaction(
    TransactionCheckpoint& checkpoint) noexcept {
  if (!checkpoint.active || !impl_->transaction_active) {
    return;
  }
  while (impl_->bindings.size() > checkpoint.binding_count) {
    auto& binding = impl_->bindings.back();
    if (auto* value = std::get_if<ValueTarget>(&binding.target)) {
      if (value->observer_id != 0) {
        value->widget->remove_edit_observer(value->observer_id);
        value->observer_id = 0;
      }
    }
    impl_->owned_targets.erase(binding.owned_target_key);
    impl_->bindings.pop_back();
  }
  impl_->next_id = checkpoint.next_id;
  impl_->transaction_active = false;
  checkpoint.active = false;
}

UiBindingHandle UiBindingRuntime::bind_compiled_class(
    Element& target, std::string class_name,
    const std::vector<std::string>& dependencies,
    std::shared_ptr<const flex::MirExpressionProgram> program) {
  if (class_name.empty()) {
    throw std::invalid_argument("bound class name must not be empty");
  }
  if (!program || !program->uses_jit()) {
    throw std::invalid_argument(
        "shared MIR UI bindings require an immutable JIT artifact");
  }
  Impl::Binding binding;
  binding.element = &target;
  binding.target =
      ClassTarget{class_name, target.class_names().count(class_name) > 0};
  binding.source = BindingSource::BoolExpression;
  impl_->attach_expression(dependencies, std::move(program), binding);
  return impl_->add_binding(std::move(binding));
}

UiBindingHandle UiBindingTargets::bind_class(Element& target,
                                             std::string class_name,
                                             std::string bool_expression) {
  if (class_name.empty()) {
    throw std::invalid_argument("bound class name must not be empty");
  }
  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  binding.target =
      ClassTarget{class_name, target.class_names().count(class_name) > 0};
  binding.source = BindingSource::BoolExpression;
  runtime_->impl_->compile_expression(bool_expression, binding);
  return runtime_->impl_->add_binding(std::move(binding));
}

UiBindingHandle UiBindingTargets::bind_classes(Element& target,
                                               std::string string_input) {
  const flex::Symbol input_symbol(string_input);
  runtime_->impl_->require_input(string_input, InputKind::String);
  const std::string* previous = target.attribute("class");
  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  binding.target = ClassListTarget{previous ? *previous : std::string{}};
  binding.source = BindingSource::StringInput;
  binding.string_input = input_symbol;
  binding.dependencies = {input_symbol};
  return runtime_->impl_->add_binding(std::move(binding));
}

UiBindingHandle UiBindingTargets::bind_utility(Element& target,
                                               std::string utility,
                                               std::string bool_expression) {
  if (utility.empty()) {
    throw std::invalid_argument("bound utility name must not be empty");
  }
  const bool previous = target.utility_names().count(utility) > 0;
  if (!previous) {
    target.add_utility(utility);
    target.remove_utility(utility);
  }
  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  binding.target = UtilityTarget{std::move(utility), previous};
  binding.source = BindingSource::BoolExpression;
  runtime_->impl_->compile_expression(bool_expression, binding);
  return runtime_->impl_->add_binding(std::move(binding));
}

UiBindingHandle UiBindingTargets::bind_utilities(Element& target,
                                                 std::string string_input) {
  const flex::Symbol input_symbol(string_input);
  runtime_->impl_->require_input(string_input, InputKind::String);
  std::vector<std::string> previous_tokens(target.utility_names().begin(),
                                           target.utility_names().end());
  std::sort(previous_tokens.begin(), previous_tokens.end());
  std::string previous;
  for (const auto& token : previous_tokens) {
    if (!previous.empty()) {
      previous.push_back(' ');
    }
    previous += token;
  }
  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  binding.target = UtilityListTarget{std::move(previous)};
  binding.source = BindingSource::StringInput;
  binding.string_input = input_symbol;
  binding.dependencies = {input_symbol};
  return runtime_->impl_->add_binding(std::move(binding));
}

UiBindingHandle UiBindingTargets::bind_attribute(Element& target,
                                                 std::string attribute,
                                                 std::string string_input) {
  if (attribute.empty()) {
    throw std::invalid_argument("bound attribute name must not be empty");
  }
  if (attribute == "class") {
    throw std::invalid_argument("use bind_classes() for the class attribute");
  }
  const flex::Symbol input_symbol(string_input);
  runtime_->impl_->require_input(string_input, InputKind::String);
  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  const std::string* previous = target.attribute(attribute);
  binding.target = AttributeTarget{attribute,
                                   previous ? std::optional<std::string>(*previous)
                                            : std::nullopt,
                                   {}, {}};
  binding.source = BindingSource::StringInput;
  binding.string_input = input_symbol;
  binding.dependencies = {input_symbol};
  return runtime_->impl_->add_binding(std::move(binding));
}

UiBindingHandle UiBindingTargets::bind_attribute(
    Element& target, std::string attribute, std::string bool_expression,
    std::string true_value, std::string false_value) {
  if (attribute.empty()) {
    throw std::invalid_argument("bound attribute name must not be empty");
  }
  if (attribute == "class") {
    throw std::invalid_argument("use bind_classes() for the class attribute");
  }
  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  const std::string* previous = target.attribute(attribute);
  binding.target = AttributeTarget{attribute,
                                   previous ? std::optional<std::string>(*previous)
                                            : std::nullopt,
                                   std::move(true_value), std::move(false_value)};
  binding.source = BindingSource::BoolExpression;
  runtime_->impl_->compile_expression(bool_expression, binding);
  return runtime_->impl_->add_binding(std::move(binding));
}

UiBindingHandle UiBindingTargets::bind_text(Element& target,
                                            std::string string_input) {
  const flex::Symbol input_symbol(string_input);
  runtime_->impl_->require_input(string_input, InputKind::String);
  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  binding.target = TextTarget{target.text()};
  binding.source = BindingSource::StringInput;
  binding.string_input = input_symbol;
  binding.dependencies = {input_symbol};
  return runtime_->impl_->add_binding(std::move(binding));
}

UiBindingHandle UiBindingTargets::bind_value(Element& target,
                                             std::string string_input) {
  runtime_->impl_->require_input(string_input, InputKind::String);
  auto* value_widget =
      target.widget ? dynamic_cast<TextValueWidget*>(target.widget) : nullptr;
  if (!value_widget) {
    throw std::invalid_argument(
        "bind_value() requires an editable text value widget");
  }

  const flex::Symbol input_symbol(string_input);
  const TextValueObserverId observer_id = value_widget->add_edit_observer(
      [runtime = runtime_, input = std::move(string_input)](
          const std::string& value) { runtime->inputs().set_string(input, value); });

  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  binding.target =
      ValueTarget{value_widget, observer_id, value_widget->text_value()};
  binding.source = BindingSource::StringInput;
  binding.string_input = input_symbol;
  binding.dependencies = {input_symbol};
  try {
    return runtime_->impl_->add_binding(std::move(binding));
  } catch (...) {
    value_widget->remove_edit_observer(observer_id);
    throw;
  }
}

UiBindingHandle UiBindingTargets::bind_custom_property(
    Element& target, std::string property, std::string string_input) {
  validate_custom_property_name(property);
  const flex::Symbol input_symbol(string_input);
  runtime_->impl_->require_input(string_input, InputKind::String);
  const std::string* previous = target.custom_property(property);
  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  binding.target = CustomPropertyTarget{
      property, previous ? std::optional<std::string>(*previous) : std::nullopt, {}};
  binding.source = BindingSource::StringInput;
  binding.string_input = input_symbol;
  binding.dependencies = {input_symbol};
  return runtime_->impl_->add_binding(std::move(binding));
}

UiBindingHandle UiBindingTargets::bind_custom_property(
    Element& target, std::string property, std::string number_expression,
    std::string suffix) {
  validate_custom_property_name(property);
  const std::string* previous = target.custom_property(property);
  UiBindingRuntime::Impl::Binding binding;
  binding.element = &target;
  binding.target = CustomPropertyTarget{
      property, previous ? std::optional<std::string>(*previous) : std::nullopt,
      std::move(suffix)};
  binding.source = BindingSource::NumberExpression;
  runtime_->impl_->compile_expression(number_expression, binding);
  return runtime_->impl_->add_binding(std::move(binding));
}

bool UiBindingTargets::unbind(UiBindingId id) {
  auto it = std::find_if(runtime_->impl_->bindings.begin(),
                         runtime_->impl_->bindings.end(),
                         [id](const auto& binding) { return binding.id == id; });
  if (it == runtime_->impl_->bindings.end()) {
    return false;
  }
  runtime_->impl_->restore(*it);
  runtime_->impl_->owned_targets.erase(it->owned_target_key);
  runtime_->impl_->bindings.erase(it);
  if (runtime_->impl_->invalidated_callback) {
    runtime_->impl_->invalidated_callback();
  }
  return true;
}

void UiBindingTargets::clear() {
  for (auto it = runtime_->impl_->bindings.rbegin();
       it != runtime_->impl_->bindings.rend(); ++it) {
    runtime_->impl_->restore(*it);
  }
  runtime_->impl_->bindings.clear();
  runtime_->impl_->owned_targets.clear();
  if (runtime_->impl_->invalidated_callback) {
    runtime_->impl_->invalidated_callback();
  }
}

bool UiBindingRuntime::update() {
  ++impl_->update_count;
  bool changed = false;
  for (auto& binding : impl_->bindings) {
    if (!impl_->dependencies_changed(binding)) {
      continue;
    }

    if (binding.source == BindingSource::StringInput) {
      auto input = impl_->data.impl_->string_values.find(binding.string_input);
      if (input == impl_->data.impl_->string_values.end()) {
        throw std::runtime_error("UI string binding input was removed while in use");
      }
      std::visit(
          [&binding, &input](auto& target) {
            using T = std::decay_t<decltype(target)>;
            if constexpr (std::is_same_v<T, AttributeTarget>) {
              binding.element->set_attribute(target.name, input->second);
            } else if constexpr (std::is_same_v<T, ClassListTarget>) {
              binding.element->set_classes(input->second);
            } else if constexpr (std::is_same_v<T, UtilityListTarget>) {
              binding.element->set_utilities(input->second);
            } else if constexpr (std::is_same_v<T, TextTarget>) {
              binding.element->set_text(input->second);
            } else if constexpr (std::is_same_v<T, ValueTarget>) {
              target.widget->set_text_value(input->second);
            } else if constexpr (std::is_same_v<T, CustomPropertyTarget>) {
              binding.element->set_custom_property(target.name, input->second);
            }
          },
          binding.target);
    } else {
      for (size_t i = 0; i < binding.numeric_inputs.size(); ++i) {
        binding.numeric_slots[i] = *binding.numeric_inputs[i];
      }
      const double result =
          binding.program->evaluate_slots(binding.numeric_slots);
      if (!std::isfinite(result)) {
        throw std::runtime_error("MIR UI binding produced a non-finite number");
      }
      std::visit(
          [&binding, result](auto& target) {
            using T = std::decay_t<decltype(target)>;
            if constexpr (std::is_same_v<T, ClassTarget>) {
              binding.element->toggle_class(target.name, result != 0.0);
            } else if constexpr (std::is_same_v<T, UtilityTarget>) {
              binding.element->toggle_utility(target.name, result != 0.0);
            } else if constexpr (std::is_same_v<T, AttributeTarget>) {
              binding.element->set_attribute(
                  target.name, result != 0.0 ? target.true_value : target.false_value);
            } else if constexpr (std::is_same_v<T, CustomPropertyTarget>) {
              binding.element->set_custom_property(
                  target.name, format_number(result, target.suffix));
            }
          },
          binding.target);
    }
    ++impl_->evaluation_count;
    impl_->observe_dependencies(binding);
    binding.applied = true;
    changed = true;
  }
  return changed;
}

UiBindingStats UiBindingRuntime::stats() const {
  return {impl_->bindings.size(), impl_->update_count,
          impl_->evaluation_count};
}

std::uint64_t UiBindingRuntime::expression_compile_count() const {
  return impl_->expression_compile_count;
}

}  // namespace flexUI
