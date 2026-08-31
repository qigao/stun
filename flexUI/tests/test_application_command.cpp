#include <flexUI/application_command.h>

#include <tinytest.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakePreparedCommands final
    : public flexUI::IPreparedApplicationCommands {
public:
  FakePreparedCommands(std::size_t& reserved,
                       std::vector<flexUI::ApplicationCommand>& published,
                       std::vector<flexUI::ApplicationCommand> commands)
      : reserved_(reserved), published_(published),
        commands_(std::move(commands)) {}

  ~FakePreparedCommands() override {
    if (!published_once_) {
      reserved_ -= commands_.size();
    }
  }

  void publish() noexcept override {
    if (published_once_) {
      return;
    }
    published_once_ = true;
    reserved_ -= commands_.size();
    for (auto& command : commands_) {
      published_.push_back(std::move(command));
    }
  }

private:
  std::size_t& reserved_;
  std::vector<flexUI::ApplicationCommand>& published_;
  std::vector<flexUI::ApplicationCommand> commands_;
  bool published_once_ = false;
};

class FakeCommandQueue final : public flexUI::IApplicationCommandQueue {
public:
  flexUI::ApplicationCommandReserveResult
  reserve(const flexUI::ApplicationCommandBatch& batch) override {
    if (batch.size() > capacity - reserved - published.size()) {
      return {{}, {flexUI::ApplicationCommandErrorCode::QueueFull, 0,
                   "fake command queue is full"}};
    }
    std::vector<flexUI::ApplicationCommand> commands = batch.commands();
    const std::size_t command_count = commands.size();
    published.reserve(published.size() + commands.size());
    auto prepared = std::make_unique<FakePreparedCommands>(
        reserved, published, std::move(commands));
    reserved += command_count;
    return {std::move(prepared), {}};
  }

  std::size_t capacity = 2;
  std::size_t reserved = 0;
  std::vector<flexUI::ApplicationCommand> published;
};

flexUI::ApplicationCommand command(std::uint64_t request_id,
                                   std::string payload = {}) {
  return {request_id, "storage", "write", std::move(payload)};
}

} // namespace

spec("FlexUI application command") {
  it("bounds owning command batches without partial append") {
    flexUI::ApplicationCommandLimits limits;
    limits.max_commands = 2;
    limits.max_string_bytes = 8;
    limits.max_total_string_bytes = 20;
    flexUI::ApplicationCommandBatch batch(limits);

    check(batch.append(command(1, "data")));
    const auto bytes = batch.string_bytes();
    const auto oversized = batch.append(command(2, "123456789"));
    check_false(static_cast<bool>(oversized));
    check(oversized.error.code ==
          flexUI::ApplicationCommandErrorCode::StringLimitExceeded);
    check_equal(batch.size(), std::size_t{1});
    check_equal(batch.string_bytes(), bytes);
  }

  it("validates command identity before reserving queue capacity") {
    FakeCommandQueue queue;
    flexUI::ApplicationCommandEngine engine(queue);
    flexUI::ApplicationCommandBatch batch;
    check(batch.append({0, "storage", "write", "data"}));

    const auto prepared = engine.reserve(batch);
    check_false(static_cast<bool>(prepared));
    check(prepared.error.code ==
          flexUI::ApplicationCommandErrorCode::InvalidRequestId);
    check_equal(queue.reserved, std::size_t{0});
    check_empty(queue.published);
  }

  it("releases discarded reservations and publishes exactly once") {
    FakeCommandQueue queue;
    flexUI::ApplicationCommandEngine engine(queue);
    flexUI::ApplicationCommandBatch batch;
    check(batch.append(command(1, "data")));

    auto discarded = engine.reserve(batch);
    check(discarded);
    check_equal(queue.reserved, std::size_t{1});
    discarded.prepared.reset();
    check_equal(queue.reserved, std::size_t{0});
    check_empty(queue.published);

    auto published = engine.reserve(batch);
    check(published);
    published.prepared->publish();
    published.prepared->publish();
    check_equal(queue.reserved, std::size_t{0});
    check_equal(queue.published.size(), std::size_t{1});
    check_equal(queue.published.front().request_id, std::uint64_t{1});
  }

  it("returns queue capacity failures without publishing") {
    FakeCommandQueue queue;
    queue.capacity = 0;
    flexUI::ApplicationCommandEngine engine(queue);
    flexUI::ApplicationCommandBatch batch;
    check(batch.append(command(1)));

    const auto prepared = engine.reserve(batch);
    check_false(static_cast<bool>(prepared));
    check(prepared.error.code ==
          flexUI::ApplicationCommandErrorCode::QueueFull);
    check_equal(queue.reserved, std::size_t{0});
    check_empty(queue.published);
  }
}
