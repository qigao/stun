/**
 * \file document_state.h
 * \brief Snapshot of document state for undo/redo functionality.
 */

#pragma once

#include "whiteboard/types.h"
#include <vector>

namespace whiteboard {

/**
 * \struct DocumentState
 * \brief Complete snapshot of the whiteboard document state.
 *
 * This structure captures all mutable state needed to implement undo/redo.
 * It includes strokes, selection, and guides. Tool state and view state
 * are typically not included in undo/redo.
 */
struct DocumentState {
  /// All strokes in the document
  std::vector<Stroke> strokes;

  /// Indices of selected strokes
  std::vector<int> selected_indices;

  /// Guide lines for alignment
  std::vector<Guide> guides;

  /**
   * \brief Default constructor creates an empty state.
   */
  DocumentState() = default;

  /**
   * \brief Constructor from current document data.
   */
  DocumentState(const std::vector<Stroke> &s, const std::vector<int> &sel,
                const std::vector<Guide> &g)
      : strokes(s), selected_indices(sel), guides(g) {}

  /**
   * \brief Copy constructor.
   */
  DocumentState(const DocumentState &other) = default;

  /**
   * \brief Copy assignment operator.
   */
  DocumentState &operator=(const DocumentState &other) = default;

  /**
   * \brief Move constructor.
   */
  DocumentState(DocumentState &&other) noexcept = default;

  /**
   * \brief Move assignment operator.
   */
  DocumentState &operator=(DocumentState &&other) noexcept = default;
};

} // namespace whiteboard
