#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace flexUI {

/// Opaque request envelope routed to an application capability outside the
/// controller callback stack. payload owns serialized adapter data.
struct ApplicationCommand {
  std::uint64_t request_id = 0;
  std::string capability;
  std::string operation;
  std::string payload;
};

struct ApplicationCommandLimits {
  static constexpr std::size_t kDefaultMaxCommands = 64;
  static constexpr std::size_t kDefaultMaxStringBytes = 16 * 1024;
  static constexpr std::size_t kDefaultMaxTotalStringBytes = 64 * 1024;

  std::size_t max_commands = kDefaultMaxCommands;
  std::size_t max_string_bytes = kDefaultMaxStringBytes;
  std::size_t max_total_string_bytes = kDefaultMaxTotalStringBytes;
};

enum class ApplicationCommandErrorCode {
  None,
  EmptyBatch,
  BatchLimitExceeded,
  StringLimitExceeded,
  TotalStringLimitExceeded,
  InvalidRequestId,
  InvalidCapability,
  InvalidOperation,
  UnknownCapability,
  UnsupportedOperation,
  InvalidPayload,
  QueueFull,
  QueueReserveFailed,
  InternalInvariant,
};

struct ApplicationCommandError {
  ApplicationCommandErrorCode code = ApplicationCommandErrorCode::None;
  std::size_t command_index = 0;
  std::string message;

  explicit operator bool() const noexcept {
    return code != ApplicationCommandErrorCode::None;
  }
};

struct ApplicationCommandResult {
  ApplicationCommandError error;

  explicit operator bool() const noexcept {
    return !static_cast<bool>(error);
  }
};

/// Owning command batch produced by one successful script callback.
class ApplicationCommandBatch final {
public:
  explicit ApplicationCommandBatch(ApplicationCommandLimits limits = {});

  /// Appends one command or leaves the batch unchanged on a limit failure.
  ApplicationCommandResult append(ApplicationCommand command);

  std::size_t size() const noexcept;
  bool empty() const noexcept;
  std::size_t string_bytes() const noexcept;
  const ApplicationCommandLimits& limits() const noexcept;
  /// Borrowed until the next append or this batch's destruction.
  const std::vector<ApplicationCommand>& commands() const noexcept;

private:
  ApplicationCommandLimits limits_;
  std::vector<ApplicationCommand> commands_;
  std::size_t string_bytes_ = 0;
};

/// Queue-owned reservation. Destruction cancels unpublished slots.
class IPreparedApplicationCommands {
public:
  virtual ~IPreparedApplicationCommands() = default;
  /// Makes reserved commands visible to the consumer exactly once. This must
  /// not allocate, block, execute a command, or fail.
  virtual void publish() noexcept = 0;
};

struct ApplicationCommandReserveResult {
  std::unique_ptr<IPreparedApplicationCommands> prepared;
  ApplicationCommandError error;

  explicit operator bool() const noexcept {
    return prepared != nullptr && !static_cast<bool>(error);
  }
};

/// Bounded queue adapter. reserve() validates capability, operation-specific
/// payload schema, and capacity, then copies all command data into owned slots;
/// it must not make commands visible to a service consumer.
class IApplicationCommandQueue {
public:
  virtual ~IApplicationCommandQueue() = default;
  virtual ApplicationCommandReserveResult
  reserve(const ApplicationCommandBatch& batch) = 0;
};

/// Validates untrusted callback output before asking the queue for slots.
class ApplicationCommandEngine final {
public:
  explicit ApplicationCommandEngine(
      IApplicationCommandQueue& queue,
      ApplicationCommandLimits limits = {});

  /// Returns queue-owned unpublished slots. The caller publishes only after
  /// every same-callback UI mutation has committed successfully.
  ApplicationCommandReserveResult
  reserve(const ApplicationCommandBatch& batch);

private:
  IApplicationCommandQueue& queue_;
  ApplicationCommandLimits limits_;
};

} // namespace flexUI
