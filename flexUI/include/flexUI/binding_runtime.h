#pragma once

#include <flex/core/types.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace flex {
class MirExpressionProgram;
}

namespace flexUI {

class Element;
class UiBindingRuntime;
class UiDocumentInstantiator;

using UiBindingId = std::uint64_t;

struct UiBindingHandle {
  UiBindingId id = 0;
  bool uses_jit = false;

  explicit operator bool() const { return id != 0; }
};

struct UiBindingStats {
  std::size_t binding_count = 0;
  std::uint64_t update_count = 0;
  std::uint64_t evaluation_count = 0;
};

/**
 * Typed input store for UI bindings.
 *
 * The context is single-threaded. Mutating an input invalidates only bindings
 * that depend on that input; unchanged values do not advance their version.
 */
class UiDataContext {
 public:
  void set_number(const std::string& name, double value);
  void set_bool(const std::string& name, bool value);
  void set_string(const std::string& name, std::string value);
  double number(const std::string& name) const;
  bool boolean(const std::string& name) const;
  const std::string& string(const std::string& name) const;
  bool erase(const std::string& name);
  bool contains(const std::string& name) const;
  std::uint64_t revision() const;

 private:
  friend class UiBindingRuntime;
  struct Impl;
  explicit UiDataContext(std::function<void()> invalidated);
  ~UiDataContext();

  UiDataContext(const UiDataContext&) = delete;
  UiDataContext& operator=(const UiDataContext&) = delete;

  std::unique_ptr<Impl> impl_;
};

/**
 * Projects typed C++ data into the flexUI rectangle tree.
 *
 * Numeric and boolean expressions are compiled once through flex MIR. String
 * bindings are direct typed references. A target is exclusively owned by one
 * binding until unbind() or clear() restores its previous value.
 */
class UiBindingTargets {
 public:
  UiBindingHandle bind_class(Element& target, std::string class_name,
                             std::string bool_expression);
  UiBindingHandle bind_classes(Element& target, std::string string_input);
  UiBindingHandle bind_utility(Element& target, std::string utility,
                               std::string bool_expression);
  UiBindingHandle bind_utilities(Element& target, std::string string_input);
  UiBindingHandle bind_attribute(Element& target, std::string attribute,
                                 std::string string_input);
  UiBindingHandle bind_attribute(Element& target, std::string attribute,
                                 std::string bool_expression,
                                 std::string true_value,
                                 std::string false_value);
  UiBindingHandle bind_text(Element& target, std::string string_input);
  UiBindingHandle bind_value(Element& target, std::string string_input);
  UiBindingHandle bind_custom_property(Element& target, std::string property,
                                       std::string string_input);
  UiBindingHandle bind_custom_property(Element& target, std::string property,
                                       std::string number_expression,
                                       std::string suffix);

  bool unbind(UiBindingId id);
  void clear();

 private:
  friend class UiBindingRuntime;
  explicit UiBindingTargets(UiBindingRuntime& runtime);

  UiBindingRuntime* runtime_ = nullptr;
};

class UiBindingRuntime {
 public:
  explicit UiBindingRuntime(std::function<void()> invalidated = {});
  ~UiBindingRuntime();

  UiBindingRuntime(const UiBindingRuntime&) = delete;
  UiBindingRuntime& operator=(const UiBindingRuntime&) = delete;

  UiDataContext& inputs();
  const UiDataContext& inputs() const;
  UiBindingTargets& targets();
  const UiBindingTargets& targets() const;

  bool update();
  UiBindingStats stats() const;
  std::uint64_t expression_compile_count() const;

 private:
  friend class UiBindingTargets;
  friend class UiDocumentInstantiator;

  struct TransactionCheckpoint {
    std::size_t binding_count = 0;
    UiBindingId next_id = 0;
    bool active = false;
  };

  TransactionCheckpoint begin_transaction();
  void commit_transaction(TransactionCheckpoint& checkpoint);
  void rollback_transaction(TransactionCheckpoint& checkpoint) noexcept;
  UiBindingHandle bind_compiled_class(
      Element& target, std::string class_name,
      const std::vector<std::string>& dependencies,
      std::shared_ptr<const flex::MirExpressionProgram> program);

  struct Impl;
  std::unique_ptr<Impl> impl_;
  UiBindingTargets targets_;
};

}  // namespace flexUI
