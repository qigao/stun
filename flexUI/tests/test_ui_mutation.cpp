#include <flexUI/mutation.h>

#include <tinytest.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <utility>

namespace {

class FakePreparedMutation final : public flexUI::IPreparedUiMutation {
public:
  FakePreparedMutation(int &value, int next_value, int &commits)
      : value_(value), next_value_(next_value), commits_(commits) {}

  void commit() noexcept override {
    value_ = next_value_;
    ++commits_;
  }

private:
  int &value_;
  int next_value_;
  int &commits_;
};

class FakeMutationHost final : public flexUI::IUiMutationHost {
public:
  flexUI::MutationPrepareResult
  prepare(const flexUI::UiMutationBatch &batch) override {
    ++prepare_calls;
    if (fail_prepare) {
      return {{}, {flexUI::MutationErrorCode::HostPrepareFailed, 0,
                   "injected prepare failure"}};
    }
    return {std::make_unique<FakePreparedMutation>(
                value, static_cast<int>(batch.size()), commits),
            {}};
  }

  int value = 7;
  int commits = 0;
  int prepare_calls = 0;
  bool fail_prepare = false;
};

flexUI::SetTextMutation set_text(std::string text) {
  return {{"title", 1}, std::move(text)};
}

} // namespace

spec("FlexUI UI mutation transactions") {
  it("treats an empty batch as a no-op without host preparation") {
    flexUI::UiMutationBatch batch;
    FakeMutationHost host;
    flexUI::UiMutationEngine engine(host);

    check(engine.apply(batch));
    check_equal(host.prepare_calls, 0);
    check_equal(host.commits, 0);
    check_equal(host.value, 7);
  }

  it("accepts exactly the configured batch capacity") {
    flexUI::MutationLimits limits;
    limits.max_mutations = 2;
    flexUI::UiMutationBatch batch(limits);

    check(batch.append(set_text("first")));
    check(batch.append(set_text("second")));
    check_equal(batch.size(), std::size_t{2});

    const auto overflow = batch.append(set_text("third"));
    check_false(static_cast<bool>(overflow));
    check(overflow.error.code ==
          flexUI::MutationErrorCode::BatchLimitExceeded);
    check_equal(batch.size(), std::size_t{2});
  }

  it("rejects one string and aggregate string bytes over their limits") {
    flexUI::MutationLimits limits;
    limits.max_string_bytes = 5;
    limits.max_total_string_bytes = 18;
    flexUI::UiMutationBatch batch(limits);

    check(batch.append(set_text("four")));
    const auto single_overflow = batch.append(set_text("123456"));
    check_false(static_cast<bool>(single_overflow));
    check(single_overflow.error.code ==
          flexUI::MutationErrorCode::StringLimitExceeded);

    check(batch.append(set_text("123")));
    const auto total_overflow = batch.append(set_text("1"));
    check_false(static_cast<bool>(total_overflow));
    check(total_overflow.error.code ==
          flexUI::MutationErrorCode::TotalStringLimitExceeded);
    check_equal(batch.size(), std::size_t{2});
  }

  it("normalizes invalid targets and numbers before host preparation") {
    flexUI::UiMutationBatch invalid_target;
    check(invalid_target.append(
        flexUI::SetTextMutation{{"title", 0}, "invalid"}));
    FakeMutationHost host;
    flexUI::UiMutationEngine engine(host);

    const auto target_result = engine.apply(invalid_target);
    check_false(static_cast<bool>(target_result));
    check(target_result.error.code ==
          flexUI::MutationErrorCode::InvalidTarget);
    check_equal(host.prepare_calls, 0);

    flexUI::UiMutationBatch invalid_number;
    check(invalid_number.append(flexUI::SetBindingInputNumberMutation{
        "progress", std::nan("")}));
    const auto number_result = engine.apply(invalid_number);
    check_false(static_cast<bool>(number_result));
    check(number_result.error.code ==
          flexUI::MutationErrorCode::InvalidNumber);
    check_equal(host.prepare_calls, 0);
  }

  it("revalidates an adapter batch against stricter engine limits") {
    flexUI::UiMutationBatch batch;
    check(batch.append(set_text("first")));
    check(batch.append(set_text("second")));
    FakeMutationHost host;
    flexUI::MutationLimits host_limits;
    host_limits.max_mutations = 1;
    flexUI::UiMutationEngine engine(host, host_limits);

    const auto applied = engine.apply(batch);
    check_false(static_cast<bool>(applied));
    check(applied.error.code ==
          flexUI::MutationErrorCode::BatchLimitExceeded);
    check_equal(host.prepare_calls, 0);
    check_equal(host.commits, 0);
  }

  it("commits prepared state exactly once") {
    flexUI::UiMutationBatch batch;
    check(batch.append(set_text("ready")));
    FakeMutationHost host;
    flexUI::UiMutationEngine engine(host);

    check(engine.apply(batch));
    check_equal(host.prepare_calls, 1);
    check_equal(host.commits, 1);
    check_equal(host.value, 1);
  }

  it("keeps host state unchanged when preparation fails") {
    flexUI::UiMutationBatch batch;
    check(batch.append(set_text("rejected")));
    FakeMutationHost host;
    host.fail_prepare = true;
    flexUI::UiMutationEngine engine(host);

    const auto applied = engine.apply(batch);
    check_false(static_cast<bool>(applied));
    check(applied.error.code ==
          flexUI::MutationErrorCode::HostPrepareFailed);
    check_equal(host.prepare_calls, 1);
    check_equal(host.commits, 0);
    check_equal(host.value, 7);
  }
}
