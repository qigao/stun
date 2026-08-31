#include <flexUI/application_command.h>

#include <exception>
#include <limits>
#include <string_view>
#include <utility>

namespace flexUI {
namespace {

ApplicationCommandResult fail(ApplicationCommandErrorCode code,
                              std::size_t index, std::string message) {
  return {{code, index, std::move(message)}};
}

ApplicationCommandReserveResult reserve_fail(
    ApplicationCommandErrorCode code, std::size_t index,
    std::string message) {
  return {{}, {code, index, std::move(message)}};
}

template <typename Visitor>
void visit_strings(const ApplicationCommand& command, Visitor&& visitor) {
  visitor(std::string_view(command.capability));
  visitor(std::string_view(command.operation));
  visitor(std::string_view(command.payload));
}

bool add_size(std::size_t& total, std::size_t value) noexcept {
  if (value > std::numeric_limits<std::size_t>::max() - total) {
    return false;
  }
  total += value;
  return true;
}

ApplicationCommandResult validate_batch(
    const ApplicationCommandBatch& batch,
    const ApplicationCommandLimits& limits) {
  if (batch.empty()) {
    return fail(ApplicationCommandErrorCode::EmptyBatch, 0,
                "application command batch must not be empty");
  }
  if (batch.size() > limits.max_commands) {
    return fail(ApplicationCommandErrorCode::BatchLimitExceeded,
                limits.max_commands,
                "application command batch exceeds configured command limit");
  }
  if (batch.string_bytes() > limits.max_total_string_bytes) {
    return fail(
        ApplicationCommandErrorCode::TotalStringLimitExceeded, 0,
        "application command batch exceeds configured string byte budget");
  }
  for (std::size_t index = 0; index < batch.commands().size(); ++index) {
    const auto& command = batch.commands()[index];
    bool oversized = false;
    visit_strings(command, [&](std::string_view value) {
      oversized = oversized || value.size() > limits.max_string_bytes;
    });
    if (oversized) {
      return fail(ApplicationCommandErrorCode::StringLimitExceeded, index,
                  "application command string exceeds configured byte limit");
    }
    if (command.request_id == 0) {
      return fail(ApplicationCommandErrorCode::InvalidRequestId, index,
                  "application command request id must not be zero");
    }
    if (command.capability.empty()) {
      return fail(ApplicationCommandErrorCode::InvalidCapability, index,
                  "application command capability must not be empty");
    }
    if (command.operation.empty()) {
      return fail(ApplicationCommandErrorCode::InvalidOperation, index,
                  "application command operation must not be empty");
    }
  }
  return {};
}

} // namespace

ApplicationCommandBatch::ApplicationCommandBatch(
    ApplicationCommandLimits limits)
    : limits_(limits) {}

ApplicationCommandResult
ApplicationCommandBatch::append(ApplicationCommand command) {
  const std::size_t index = commands_.size();
  if (index >= limits_.max_commands) {
    return fail(ApplicationCommandErrorCode::BatchLimitExceeded, index,
                "application command batch is full");
  }

  bool oversized = false;
  std::size_t bytes = 0;
  bool overflow = false;
  visit_strings(command, [&](std::string_view value) {
    oversized = oversized || value.size() > limits_.max_string_bytes;
    overflow = overflow || !add_size(bytes, value.size());
  });
  if (oversized) {
    return fail(ApplicationCommandErrorCode::StringLimitExceeded, index,
                "application command string exceeds configured byte limit");
  }
  if (overflow || string_bytes_ > limits_.max_total_string_bytes ||
      bytes > limits_.max_total_string_bytes - string_bytes_) {
    return fail(
        ApplicationCommandErrorCode::TotalStringLimitExceeded, index,
        "application command batch exceeds configured string byte budget");
  }

  commands_.push_back(std::move(command));
  string_bytes_ += bytes;
  return {};
}

std::size_t ApplicationCommandBatch::size() const noexcept {
  return commands_.size();
}

bool ApplicationCommandBatch::empty() const noexcept {
  return commands_.empty();
}

std::size_t ApplicationCommandBatch::string_bytes() const noexcept {
  return string_bytes_;
}

const ApplicationCommandLimits&
ApplicationCommandBatch::limits() const noexcept {
  return limits_;
}

const std::vector<ApplicationCommand>&
ApplicationCommandBatch::commands() const noexcept {
  return commands_;
}

ApplicationCommandEngine::ApplicationCommandEngine(
    IApplicationCommandQueue& queue, ApplicationCommandLimits limits)
    : queue_(queue), limits_(limits) {}

ApplicationCommandReserveResult ApplicationCommandEngine::reserve(
    const ApplicationCommandBatch& batch) {
  auto validated = validate_batch(batch, limits_);
  if (!validated) {
    return {{}, std::move(validated.error)};
  }

  ApplicationCommandReserveResult reserved;
  try {
    reserved = queue_.reserve(batch);
  } catch (const std::exception& error) {
    return reserve_fail(ApplicationCommandErrorCode::QueueReserveFailed, 0,
                        error.what());
  } catch (...) {
    return reserve_fail(
        ApplicationCommandErrorCode::QueueReserveFailed, 0,
        "application command queue raised an unknown exception");
  }
  if (reserved.error) {
    return reserved;
  }
  if (reserved.prepared == nullptr) {
    return reserve_fail(ApplicationCommandErrorCode::InternalInvariant, 0,
                        "application command queue returned no reservation");
  }
  return reserved;
}

} // namespace flexUI
