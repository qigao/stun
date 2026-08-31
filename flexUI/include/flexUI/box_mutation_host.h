#pragma once

#include "flexUI/mutation.h"

namespace flexUI {

class Box;

/// Transactional mutation adapter for one UI-thread-owned Box.
///
/// Element targets must be application-owned and reachable from Box::root().
/// Binding input mutations update only predeclared inputs of the same type;
/// scripts cannot create or change the input schema through this adapter.
class BoxMutationHost final : public IUiMutationHost {
public:
  /// @param box Non-owning reference; the Box must outlive this adapter and
  ///        every UiMutationEngine that uses it.
  explicit BoxMutationHost(Box& box) noexcept;

  /// Creates independently owned final states without changing the Box.
  /// @return Prepared no-fail commit, or an indexed target/value/schema error.
  MutationPrepareResult prepare(const UiMutationBatch& batch) override;

private:
  Box& box_;
};

} // namespace flexUI
