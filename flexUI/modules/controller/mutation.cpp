#include "flexUI/mutation.h"

#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace flexUI {
namespace {

MutationResult fail(MutationErrorCode code, std::size_t index,
                    std::string message) {
  return {{code, index, std::move(message)}};
}

template <typename Visitor>
void visit_strings(const UiMutation &mutation, Visitor &&visitor) {
  std::visit(
      [&](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, SetTextMutation>) {
          visitor(value.target.id);
          visitor(value.text);
        } else if constexpr (std::is_same_v<T, SetAttributeMutation>) {
          visitor(value.target.id);
          visitor(value.name);
          visitor(value.value);
        } else if constexpr (std::is_same_v<T, RemoveAttributeMutation>) {
          visitor(value.target.id);
          visitor(value.name);
        } else if constexpr (std::is_same_v<T, SetClassesMutation>) {
          visitor(value.target.id);
          visitor(value.classes);
        } else if constexpr (std::is_same_v<T, SetUtilitiesMutation>) {
          visitor(value.target.id);
          visitor(value.utilities);
        } else if constexpr (
            std::is_same_v<T, SetBindingInputStringMutation>) {
          visitor(value.name);
          visitor(value.value);
        } else {
          visitor(value.name);
        }
      },
      mutation);
}

std::optional<std::size_t>
mutation_string_bytes(const UiMutation &mutation) noexcept {
  std::size_t total = 0;
  bool overflow = false;
  visit_strings(mutation, [&](std::string_view value) {
    if (value.size() > std::numeric_limits<std::size_t>::max() - total) {
      overflow = true;
      return;
    }
    total += value.size();
  });
  return overflow ? std::nullopt : std::optional<std::size_t>(total);
}

MutationResult validate_mutation(const UiMutation &mutation,
                                 std::size_t index,
                                 const MutationLimits &limits) {
  MutationResult result;
  visit_strings(mutation, [&](std::string_view value) {
    if (result && value.size() > limits.max_string_bytes) {
      result = fail(MutationErrorCode::StringLimitExceeded, index,
                    "mutation string exceeds configured byte limit");
    }
  });
  if (!result) {
    return result;
  }

  return std::visit(
      [&](const auto &value) -> MutationResult {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, SetBindingInputNumberMutation>) {
          if (value.name.empty()) {
            return fail(MutationErrorCode::InvalidName, index,
                        "binding input name must not be empty");
          }
          if (!std::isfinite(value.value)) {
            return fail(MutationErrorCode::InvalidNumber, index,
                        "binding input number must be finite");
          }
        } else if constexpr (
            std::is_same_v<T, SetBindingInputBoolMutation> ||
            std::is_same_v<T, SetBindingInputStringMutation>) {
          if (value.name.empty()) {
            return fail(MutationErrorCode::InvalidName, index,
                        "binding input name must not be empty");
          }
        } else {
          if (!value.target) {
            return fail(MutationErrorCode::InvalidTarget, index,
                        "UI mutation target must have an id and generation");
          }
          if constexpr (std::is_same_v<T, SetAttributeMutation> ||
                        std::is_same_v<T, RemoveAttributeMutation>) {
            if (value.name.empty()) {
              return fail(MutationErrorCode::InvalidName, index,
                          "attribute name must not be empty");
            }
          }
        }
        return {};
      },
      mutation);
}

MutationResult validate_batch(const UiMutationBatch &batch,
                              const MutationLimits &limits) {
  if (batch.size() > limits.max_mutations) {
    return fail(MutationErrorCode::BatchLimitExceeded, limits.max_mutations,
                "mutation batch exceeds configured command limit");
  }
  if (batch.string_bytes() > limits.max_total_string_bytes) {
    return fail(MutationErrorCode::TotalStringLimitExceeded, 0,
                "mutation batch exceeds configured string byte budget");
  }
  for (std::size_t index = 0; index < batch.mutations().size(); ++index) {
    auto result = validate_mutation(batch.mutations()[index], index, limits);
    if (!result) {
      return result;
    }
  }
  return {};
}

} // namespace

UiMutationBatch::UiMutationBatch(MutationLimits limits) : limits_(limits) {}

MutationResult UiMutationBatch::append(UiMutation mutation) {
  const std::size_t index = mutations_.size();
  if (index >= limits_.max_mutations) {
    return fail(MutationErrorCode::BatchLimitExceeded, index,
                "mutation batch is full");
  }

  bool oversized_string = false;
  visit_strings(mutation, [&](std::string_view value) {
    oversized_string =
        oversized_string || value.size() > limits_.max_string_bytes;
  });
  if (oversized_string) {
    return fail(MutationErrorCode::StringLimitExceeded, index,
                "mutation string exceeds configured byte limit");
  }

  const auto bytes = mutation_string_bytes(mutation);
  if (!bytes.has_value() || string_bytes_ > limits_.max_total_string_bytes ||
      *bytes > limits_.max_total_string_bytes - string_bytes_) {
    return fail(MutationErrorCode::TotalStringLimitExceeded, index,
                "mutation batch exceeds configured string byte budget");
  }

  mutations_.push_back(std::move(mutation));
  string_bytes_ += *bytes;
  return {};
}

std::size_t UiMutationBatch::size() const noexcept { return mutations_.size(); }

bool UiMutationBatch::empty() const noexcept { return mutations_.empty(); }

std::size_t UiMutationBatch::string_bytes() const noexcept {
  return string_bytes_;
}

const MutationLimits &UiMutationBatch::limits() const noexcept {
  return limits_;
}

const std::vector<UiMutation> &UiMutationBatch::mutations() const noexcept {
  return mutations_;
}

UiMutationEngine::UiMutationEngine(IUiMutationHost &host,
                                   MutationLimits limits)
    : host_(host), limits_(limits) {}

MutationResult UiMutationEngine::apply(const UiMutationBatch &batch) {
  auto normalized = validate_batch(batch, limits_);
  if (!normalized || batch.empty()) {
    return normalized;
  }

  MutationPrepareResult prepared;
  try {
    prepared = host_.prepare(batch);
  } catch (const std::exception &error) {
    return fail(MutationErrorCode::HostPrepareFailed, 0, error.what());
  } catch (...) {
    return fail(MutationErrorCode::HostPrepareFailed, 0,
                "mutation host preparation raised an unknown exception");
  }
  if (prepared.error) {
    return {std::move(prepared.error)};
  }
  if (prepared.prepared == nullptr) {
    return fail(MutationErrorCode::InternalInvariant, 0,
                "mutation host returned no prepared transaction");
  }

  prepared.prepared->commit();
  return {};
}

} // namespace flexUI
