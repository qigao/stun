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

  // Fixed maximum is determined by the application-owned C11 retainer.
  // Entries are values; only retained message IDs can occupy a slot.
  TranscriptHeightIndex(std::size_t max_retained, float fallback_height)
      : maximum_(max_retained), fallback_height_(fallback_height),
        scratch_(max_retained) {
    if (!maximum_ || maximum_ > 256u ||
        !std::isfinite(fallback_height) || fallback_height <= 0.0f)
      throw std::invalid_argument("invalid bounded transcript height index");
    entries_.reserve(maximum_);
  }

  std::size_t size() const noexcept { return entries_.size(); }
  std::size_t capacity() const noexcept { return maximum_; }
  float width() const noexcept { return width_; }

  // Admission is transactional: a rejected owner query, invalid width, or
  // capacity mismatch cannot evict existing measurements. An exact revision
  // (generation + last_sequence) and width retain a prior measured height.
  // New/changed sources or a new layout width require remeasurement.
  Status sync(const tui_transcript_t* transcript, float layout_width) {
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

    const bool same_width = width_ == layout_width;
    bool changed = !same_width || count != entries_.size();
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
      if (same_width && found != entries_.end() &&
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

private:
  std::size_t maximum_;
  float fallback_height_;
  float width_ = 0.0f;
  std::vector<tui_transcript_message_t> scratch_;
  std::vector<Entry> entries_;
};

}  // namespace flex::tui_backend
#endif  // FLEX_TUI_TRANSCRIPT_HEIGHT_INDEX_H
