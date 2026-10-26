/* Value-only, bounded retained-message geometry measurements.
 *
 * This is an optional private FlexUI/TUI projection aid, NOT a Tango/C11
 * height API or a complete variable-height offscreen virtualization path.
 * UI Owner alone synchronizes the current native transcript and observes
 * Box layout. Never retain C11 text, UiHandles or Element pointers.
 */
#ifndef FLEX_TUI_TRANSCRIPT_HEIGHT_INDEX_H
#define FLEX_TUI_TRANSCRIPT_HEIGHT_INDEX_H

#include <tui_transcript.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace flex::tui_backend {

class TranscriptHeightIndex final {
public:
  enum class Status { Updated, Unchanged, Invalid, Capacity, QueryRejected };

  struct Entry {
    std::uint64_t message_id = 0u;
    std::uint64_t generation = 0u;
    std::uint64_t last_sequence = 0u;
    float height = 0.0f;
    bool measured = false;
  };

  // Virtual geometry is a bounded numeric *plan*, not a renderer. No UI
  // slots, native borrowed text, or speculative Element heights escape it.
  struct WindowExtent {
    double leading = 0.0;
    double visible = 0.0;
    double trailing = 0.0;
    double total = 0.0;
    bool leading_exact = true;
    bool visible_exact = true;
    bool trailing_exact = true;
  };

  // Fixed maximum is determined by the application-owned C11 retainer.
  // Entries are values; only retained message IDs can occupy a slot.
  TranscriptHeightIndex(std::size_t max_retained, float fallback_height)
      : maximum_(max_retained), fallback_height_(fallback_height) {
    // Validate BEFORE allocating: an attacker-controlled SIZE_MAX request
    // must fail with a contract error, not attempt unbounded scratch storage.
    if (!maximum_ || maximum_ > 256u ||
        !std::isfinite(fallback_height) || fallback_height <= 0.0f ||
        fallback_height > 10000000.0f)
      throw std::invalid_argument("invalid bounded transcript height index");
    scratch_.resize(maximum_);
    entries_.reserve(maximum_);
  }

  std::size_t size() const noexcept { return entries_.size(); }
  std::size_t capacity() const noexcept { return maximum_; }
  float width() const noexcept { return width_; }
  std::uint64_t layout_revision() const noexcept { return layout_revision_; }

  // Admission is transactional: rejected owner queries, invalid widths and
  // capacity mismatches cannot evict existing measurements. An exact message
  // revision, width AND caller-controlled layout revision preserve a sample.
  // Increment layout_revision when font/theme/style metrics change without
  // changing the viewport width (CSS, DPI, terminal cell metrics).
  Status sync(const tui_transcript_t* transcript, float layout_width,
              std::uint64_t layout_revision = 0u) {
    if (!transcript || !std::isfinite(layout_width) ||
        layout_width <= 0.0f)
      return Status::Invalid;
    const std::size_t count = tui_transcript_count(transcript);
    if (count > maximum_) return Status::Capacity;
    std::size_t written = 0u;
    if (tui_transcript_window(transcript, 0u, count, scratch_.data(),
                              scratch_.size(), &written) != TUI_TRANSCRIPT_OK ||
        written != count)
      return Status::QueryRejected;

    const bool same_layout = width_ == layout_width &&
        layout_revision_ == layout_revision;
    bool changed = !same_layout || count != entries_.size();
    std::vector<Entry> staged;
    staged.reserve(count);
    std::uint64_t previous_id = 0u;
    for (std::size_t i = 0u; i < count; ++i) {
      const tui_transcript_message_t& message = scratch_[i];
      if (!message.message_id || message.message_id <= previous_id ||
          message.stream.message_id != message.message_id)
        return Status::QueryRejected;
      previous_id = message.message_id;
      Entry current{};
      current.message_id = message.message_id;
      current.generation = message.stream.generation;
      current.last_sequence = message.stream.last_sequence;
      const auto found = std::find_if(
          entries_.begin(), entries_.end(), [&](const Entry& old) {
            return old.message_id == current.message_id;
          });
      if (same_layout && found != entries_.end() &&
          found->generation == current.generation &&
          found->last_sequence == current.last_sequence) {
        current.measured = found->measured;
        current.height = found->height;
      } else {
        changed = true;
      }
      if (i >= entries_.size() ||
          entries_[i].message_id != current.message_id ||
          entries_[i].generation != current.generation ||
          entries_[i].last_sequence != current.last_sequence ||
          entries_[i].measured != current.measured)
        changed = true;
      staged.push_back(current);
    }

    entries_.swap(staged);
    width_ = layout_width;
    layout_revision_ = layout_revision;
    return changed ? Status::Updated : Status::Unchanged;
  }

  // Measured height comes from the final Box owner layout, after update().
  // It is not an estimated text-line count and never owns a live UI object.
  bool record_height(std::uint64_t message_id, float height) noexcept {
    if (!message_id || !std::isfinite(height) ||
        height < 0.0f || height > 10000000.0f)
      return false;
    for (Entry& entry : entries_) {
      if (entry.message_id != message_id) continue;
      entry.height = height;
      entry.measured = true;
      return true;
    }
    return false;
  }

  std::optional<Entry> lookup(std::uint64_t message_id) const noexcept {
    for (const Entry& entry : entries_)
      if (entry.message_id == message_id) return entry;
    return std::nullopt;
  }

  // Deterministic placeholders for not-yet-measured retained messages.
  // The returned flag explicitly distinguishes exact vs estimated geometry.
  std::optional<double> height_before(std::uint64_t message_id,
                                      bool* all_measured = nullptr) const noexcept {
    double sum = 0.0;
    bool exact = true;
    for (const Entry& entry : entries_) {
      if (entry.message_id == message_id) {
        if (all_measured) *all_measured = exact;
        return sum;
      }
      sum += entry.measured ? entry.height : fallback_height_;
      if (!entry.measured) exact = false;
    }
    return std::nullopt;  // Evicted or never admitted: do not invent an index.
  }

  double total_height_estimate(bool* all_measured = nullptr) const noexcept {
    double sum = 0.0;
    bool exact = true;
    for (const Entry& entry : entries_) {
      sum += entry.measured ? entry.height : fallback_height_;
      if (!entry.measured) exact = false;
    }
    if (all_measured) *all_measured = exact;
    return sum;
  }

  // A nonempty contiguous window uses only native retained IDs. Reject
  // gaps/ranges that extend past the retainer rather than manufacturing
  // virtual rows. Estimated offsets MUST NOT be called exact positions.
  std::optional<WindowExtent> window_extent(
      std::uint64_t first_message_id, std::size_t visible_count) const noexcept {
    if (!first_message_id || visible_count == 0u ||
        visible_count > entries_.size()) return std::nullopt;
    std::size_t first = 0u;
    while (first < entries_.size() &&
           entries_[first].message_id != first_message_id) ++first;
    if (first == entries_.size() ||
        visible_count > entries_.size() - first) return std::nullopt;

    WindowExtent extent{};
    for (std::size_t i = 0u; i < entries_.size(); ++i) {
      const Entry& e = entries_[i];
      const double height = e.measured ? e.height : fallback_height_;
      if (i < first) {
        extent.leading += height;
        if (!e.measured) extent.leading_exact = false;
      } else if (i - first < visible_count) {
        extent.visible += height;
        if (!e.measured) extent.visible_exact = false;
      } else {
        extent.trailing += height;
        if (!e.measured) extent.trailing_exact = false;
      }
    }
    extent.total = extent.leading + extent.visible + extent.trailing;
    return extent;
  }

private:
  std::size_t maximum_;
  float fallback_height_;
  float width_ = 0.0f;
  std::uint64_t layout_revision_ = 0u;
  std::vector<tui_transcript_message_t> scratch_;
  std::vector<Entry> entries_;
};

}  // namespace flex::tui_backend
#endif  // FLEX_TUI_TRANSCRIPT_HEIGHT_INDEX_H
