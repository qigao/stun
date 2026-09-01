#include <flexUI/application_completion.h>

#include <tinytest.hpp>

#include <atomic>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

flexUI::ApplicationCompletion completion(
    std::uint64_t id, std::uint64_t generation,
    flexUI::ApplicationCompletionStatus status = flexUI::ApplicationCompletionStatus::Succeeded,
    std::string payload = {}) {
  flexUI::ApplicationCompletion value;
  value.token = {id, generation};
  value.status = status;
  value.payload = std::move(payload);
  if (status == flexUI::ApplicationCompletionStatus::Failed) {
    value.error_code = "service-failed";
    value.error_message = "service operation failed";
  }
  return value;
}

} // namespace

spec("FlexUI application completion mailbox") {
  it("rejects invalid generation and non-power-of-two capacity") {
    auto invalid_generation = flexUI::ApplicationCompletionMailbox::create(0);
    check_false(static_cast<bool>(invalid_generation));
    check(invalid_generation.error.code ==
          flexUI::ApplicationCompletionErrorCode::InvalidGeneration);

    flexUI::ApplicationCompletionLimits limits;
    limits.capacity = 3;
    auto invalid_capacity = flexUI::ApplicationCompletionMailbox::create(1, limits);
    check_false(static_cast<bool>(invalid_capacity));
    check(invalid_capacity.error.code == flexUI::ApplicationCompletionErrorCode::InvalidCapacity);

    limits.capacity = std::uint64_t{1} << 63;
    auto overflowing_capacity = flexUI::ApplicationCompletionMailbox::create(1, limits);
    check_false(static_cast<bool>(overflowing_capacity));
    check(overflowing_capacity.error.code ==
          flexUI::ApplicationCompletionErrorCode::InvalidCapacity);
  }

  it("validates token status and owning string budgets before publication") {
    flexUI::ApplicationCompletionLimits limits;
    limits.capacity = 2;
    limits.max_payload_bytes = 4;
    limits.max_error_code_bytes = 8;
    limits.max_error_message_bytes = 8;
    limits.max_total_string_bytes = 12;
    auto created = flexUI::ApplicationCompletionMailbox::create(7, limits);
    check(created);

    auto invalid_token = created.mailbox->try_post(completion(0, 7));
    check_false(static_cast<bool>(invalid_token));
    check(invalid_token.error.code == flexUI::ApplicationCompletionErrorCode::InvalidToken);

    auto invalid_success = completion(1, 7);
    invalid_success.error_code = "failure";
    auto invalid_shape = created.mailbox->try_post(std::move(invalid_success));
    check_false(static_cast<bool>(invalid_shape));
    check(invalid_shape.error.code == flexUI::ApplicationCompletionErrorCode::InvalidCompletion);

    auto oversized = created.mailbox->try_post(
        completion(2, 7, flexUI::ApplicationCompletionStatus::Succeeded, "12345"));
    check_false(static_cast<bool>(oversized));
    check(oversized.error.code == flexUI::ApplicationCompletionErrorCode::StringLimitExceeded);
    check_equal(created.mailbox->statistics().current_depth, std::uint64_t{0});
  }

  it("delivers FIFO records and rejects the entry beyond fixed capacity") {
    flexUI::ApplicationCompletionLimits limits;
    limits.capacity = 2;
    auto created = flexUI::ApplicationCompletionMailbox::create(4, limits);
    check(created);

    check(created.mailbox->try_post(
        completion(1, 4, flexUI::ApplicationCompletionStatus::Succeeded, "one")));
    check(created.mailbox->try_post(
        completion(2, 4, flexUI::ApplicationCompletionStatus::Succeeded, "two")));
    const auto rejected_source =
        completion(3, 4, flexUI::ApplicationCompletionStatus::Succeeded, "kept");
    auto full = created.mailbox->try_post(rejected_source);
    check_false(static_cast<bool>(full));
    check(full.error.code == flexUI::ApplicationCompletionErrorCode::QueueFull);
    check_equal(rejected_source.token.id, std::uint64_t{3});
    check_equal(rejected_source.payload, std::string("kept"));

    auto first = created.mailbox->try_receive();
    check(first.status == flexUI::ApplicationCompletionReceiveStatus::Ready);
    check(first.completion.has_value());
    check_equal(first.completion->token.id, std::uint64_t{1});
    check_equal(first.completion->payload, std::string("one"));

    auto second = created.mailbox->try_receive();
    check(second.status == flexUI::ApplicationCompletionReceiveStatus::Ready);
    check(second.completion.has_value());
    check_equal(second.completion->token.id, std::uint64_t{2});

    auto empty = created.mailbox->try_receive();
    check(empty.status == flexUI::ApplicationCompletionReceiveStatus::Empty);
    check_false(empty.completion.has_value());

    const auto stats = created.mailbox->statistics();
    check_equal(stats.published, std::uint64_t{2});
    check_equal(stats.consumed, std::uint64_t{2});
    check_equal(stats.queue_full, std::uint64_t{1});
    check_equal(stats.peak_depth, std::uint64_t{2});
    check_equal(stats.current_depth, std::uint64_t{0});
  }

  it("restricts consumption and lifecycle control to the owner thread") {
    auto created = flexUI::ApplicationCompletionMailbox::create(1);
    check(created);
    check(created.mailbox->try_post(completion(1, 1)));

    flexUI::ApplicationCompletionReceiveResult received;
    flexUI::ApplicationCompletionControlResult closed;
    std::thread foreign([&] {
      received = created.mailbox->try_receive();
      closed = created.mailbox->close();
    });
    foreign.join();

    check(received.error.code == flexUI::ApplicationCompletionErrorCode::WrongThread);
    check(closed.error.code == flexUI::ApplicationCompletionErrorCode::WrongThread);
    check(created.mailbox->state() == flexUI::ApplicationCompletionMailboxState::Accepting);
    check(created.mailbox->try_receive().completion.has_value());
  }

  it("cancels queued records and rejects old tokens across generation change") {
    auto created = flexUI::ApplicationCompletionMailbox::create(5);
    check(created);
    check(created.mailbox->try_post(completion(1, 5)));
    check(created.mailbox->try_post(completion(2, 5)));

    auto advanced = created.mailbox->advance_generation(6);
    check(advanced);
    check_equal(advanced.cancelled, std::uint64_t{2});
    check_equal(created.mailbox->generation(), std::uint64_t{6});
    check(created.mailbox->state() == flexUI::ApplicationCompletionMailboxState::Accepting);

    auto stale = created.mailbox->try_post(completion(3, 5));
    check_false(static_cast<bool>(stale));
    check(stale.error.code == flexUI::ApplicationCompletionErrorCode::StaleGeneration);
    check(created.mailbox->try_post(completion(4, 6)));
    auto current = created.mailbox->try_receive();
    check(current.completion.has_value());
    check_equal(current.completion->token.id, std::uint64_t{4});

    auto non_monotonic = created.mailbox->advance_generation(6);
    check_false(static_cast<bool>(non_monotonic));
    check(non_monotonic.error.code == flexUI::ApplicationCompletionErrorCode::InvalidGeneration);
  }

  it("closes idempotently after cancelling published records") {
    auto created = flexUI::ApplicationCompletionMailbox::create(2);
    check(created);
    check(created.mailbox->try_post(completion(1, 2)));

    auto closed = created.mailbox->close();
    check(closed);
    check_equal(closed.cancelled, std::uint64_t{1});
    check(created.mailbox->state() == flexUI::ApplicationCompletionMailboxState::Closed);

    auto late = created.mailbox->try_post(completion(2, 2));
    check_false(static_cast<bool>(late));
    check(late.error.code == flexUI::ApplicationCompletionErrorCode::Closed);
    auto polled = created.mailbox->try_receive();
    check(polled.status == flexUI::ApplicationCompletionReceiveStatus::Closed);

    auto repeated = created.mailbox->close();
    check(repeated);
    check_equal(repeated.cancelled, std::uint64_t{0});
  }

  it("quiesces concurrent non-blocking producers before close returns") {
    flexUI::ApplicationCompletionLimits limits;
    limits.capacity = 64;
    auto created = flexUI::ApplicationCompletionMailbox::create(9, limits);
    check(created);

    constexpr std::size_t kProducerCount = 4;
    std::atomic<std::size_t> ready{0};
    std::atomic<bool> start{false};
    std::atomic<std::uint64_t> attempts{0};
    std::atomic<std::uint64_t> unexpected{0};
    std::vector<std::thread> producers;
    producers.reserve(kProducerCount);
    for (std::size_t producer = 0; producer < kProducerCount; ++producer) {
      producers.emplace_back([&, producer] {
        ready.fetch_add(1, std::memory_order_release);
        while (!start.load(std::memory_order_acquire)) {
          std::this_thread::yield();
        }
        std::uint64_t sequence = 1;
        for (;;) {
          const auto id = static_cast<std::uint64_t>(producer + 1) * 1'000'000 + sequence++;
          auto posted = created.mailbox->try_post(completion(id, 9));
          attempts.fetch_add(1, std::memory_order_relaxed);
          if (posted) {
            continue;
          }
          if (posted.error.code == flexUI::ApplicationCompletionErrorCode::QueueFull) {
            std::this_thread::yield();
            continue;
          }
          if (posted.error.code == flexUI::ApplicationCompletionErrorCode::Closed) {
            break;
          }
          unexpected.fetch_add(1, std::memory_order_relaxed);
          break;
        }
      });
    }

    while (ready.load(std::memory_order_acquire) != kProducerCount) {
      std::this_thread::yield();
    }
    start.store(true, std::memory_order_release);
    while (attempts.load(std::memory_order_relaxed) < 256) {
      std::this_thread::yield();
    }

    auto closed = created.mailbox->close();
    check(closed);
    for (auto &producer : producers) {
      producer.join();
    }

    check_equal(unexpected.load(std::memory_order_relaxed), std::uint64_t{0});
    const auto stats = created.mailbox->statistics();
    check_equal(stats.current_depth, std::uint64_t{0});
    check_equal(stats.published, stats.cancelled);
    check_true(stats.rejected_closed >= kProducerCount);
  }
}
