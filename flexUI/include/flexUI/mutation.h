#pragma once

#include "flexUI/ui_handle.h"

#include <cstddef>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace flexUI {

struct SetTextMutation {
  UiHandle target;
  std::string text;
};

struct SetAttributeMutation {
  UiHandle target;
  std::string name;
  std::string value;
};

struct RemoveAttributeMutation {
  UiHandle target;
  std::string name;
};

struct SetClassesMutation {
  UiHandle target;
  std::string classes;
};

struct SetUtilitiesMutation {
  UiHandle target;
  std::string utilities;
};

struct SetBindingInputNumberMutation {
  std::string name;
  double value = 0.0;
};

struct SetBindingInputBoolMutation {
  std::string name;
  bool value = false;
};

struct SetBindingInputStringMutation {
  std::string name;
  std::string value;
};

using UiMutation =
    std::variant<SetTextMutation, SetAttributeMutation,
                 RemoveAttributeMutation, SetClassesMutation,
                 SetUtilitiesMutation, SetBindingInputNumberMutation,
                 SetBindingInputBoolMutation, SetBindingInputStringMutation>;

struct MutationLimits {
  static constexpr std::size_t kDefaultMaxMutations = 256;
  static constexpr std::size_t kDefaultMaxStringBytes = 16 * 1024;
  static constexpr std::size_t kDefaultMaxTotalStringBytes = 64 * 1024;

  std::size_t max_mutations = kDefaultMaxMutations;
  std::size_t max_string_bytes = kDefaultMaxStringBytes;
  std::size_t max_total_string_bytes = kDefaultMaxTotalStringBytes;
};

enum class MutationErrorCode {
  None,
  BatchLimitExceeded,
  StringLimitExceeded,
  TotalStringLimitExceeded,
  InvalidTarget,
  InvalidName,
  InvalidNumber,
  InvalidType,
  InvalidValue,
  HostPrepareFailed,
  InternalInvariant,
};

struct MutationError {
  MutationErrorCode code = MutationErrorCode::None;
  std::size_t mutation_index = 0;
  std::string message;

  explicit operator bool() const noexcept {
    return code != MutationErrorCode::None;
  }
};

struct MutationResult {
  MutationError error;

  explicit operator bool() const noexcept {
    return !static_cast<bool>(error);
  }
};

/// Owning, bounded command batch produced by one script callback.
class UiMutationBatch final {
public:
  explicit UiMutationBatch(MutationLimits limits = {});

  /// Appends one owning mutation or rejects it without changing the batch.
  /// @param mutation Command whose strings are moved into the batch on success.
  /// @return BatchLimitExceeded, StringLimitExceeded, or
  ///         TotalStringLimitExceeded without changing size on failure.
  MutationResult append(UiMutation mutation);

  std::size_t size() const noexcept;
  bool empty() const noexcept;
  std::size_t string_bytes() const noexcept;
  const MutationLimits &limits() const noexcept;
  /// Borrowed until the next append or this batch's destruction.
  const std::vector<UiMutation> &mutations() const noexcept;

private:
  MutationLimits limits_;
  std::vector<UiMutation> mutations_;
  std::size_t string_bytes_ = 0;
};

/// Prepared host-owned staging. commit() cannot fail or allocate.
class IPreparedUiMutation {
public:
  virtual ~IPreparedUiMutation() = default;
  virtual void commit() noexcept = 0;
};

struct MutationPrepareResult {
  std::unique_ptr<IPreparedUiMutation> prepared;
  MutationError error;

  explicit operator bool() const noexcept {
    return prepared != nullptr && !static_cast<bool>(error);
  }
};

/// Box-facing adapter. prepare() must not modify observable UI state and the
/// returned staging object must not retain a view into the input batch.
class IUiMutationHost {
public:
  virtual ~IUiMutationHost() = default;
  /// Resolves targets and creates fully reserved, independently owned staging.
  /// @param batch Borrowed only for this call; observable UI state must not
  ///        change and the returned transaction must not retain this view.
  /// @return Prepared staging or one host-specific, indexed error.
  virtual MutationPrepareResult prepare(const UiMutationBatch &batch) = 0;
};

/// Normalizes an untrusted batch, prepares host staging, then commits once.
class UiMutationEngine final {
public:
  explicit UiMutationEngine(IUiMutationHost &host,
                            MutationLimits limits = {});

  /// Validates against engine limits, prepares once, and commits once.
  /// @param batch Immutable callback output; ownership remains with the caller.
  /// @return Success for an empty or committed batch. Any failure occurs before
  ///         commit and leaves host observable state unchanged.
  MutationResult apply(const UiMutationBatch &batch);

private:
  IUiMutationHost &host_;
  MutationLimits limits_;
};

} // namespace flexUI
