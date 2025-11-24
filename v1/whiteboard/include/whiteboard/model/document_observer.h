/**
 * \file document_observer.h
 * \brief Observer interface for WhiteboardDocument changes.
 */

#pragma once

namespace whiteboard {

/**
 * \class IDocumentObserver
 * \brief Interface for components that observe WhiteboardDocument changes.
 *
 * Components (Views, Panels, Services) implement this interface to receive
 * notifications when the document state changes. Override only the notification
 * methods relevant to your component.
 */
class IDocumentObserver {
public:
  virtual ~IDocumentObserver() = default;

  /**
   * \brief Called when strokes are added, removed, or modified.
   *
   * This notification is sent when:
   * - A new stroke is added
   * - Strokes are deleted
   * - Stroke properties are modified (color, width, position, etc.)
   * - Strokes are reordered
   */
  virtual void on_strokes_changed() {}

  /**
   * \brief Called when the selection changes.
   *
   * This notification is sent when:
   * - Objects are selected or deselected
   * - The selection is cleared
   * - Selection is modified via shift-click or area selection
   */
  virtual void on_selection_changed() {}

  /**
   * \brief Called when the current tool changes.
   *
   * This notification is sent when:
   * - User selects a different tool (Pen, Rectangle, Select, etc.)
   * - Tool is changed programmatically
   */
  virtual void on_tool_changed() {}

  /**
   * \brief Called when tool properties change.
   *
   * This notification is sent when:
   * - Stroke color changes
   * - Stroke width changes
   * - Fill style or fill color changes
   * - Text properties change (font, size, alignment)
   */
  virtual void on_properties_changed() {}

  /**
   * \brief Called when view state changes.
   *
   * This notification is sent when:
   * - Zoom level changes
   * - Pan offset changes
   * - Grid visibility toggles
   * - Guide visibility toggles
   * - Snap settings change
   */
  virtual void on_view_changed() {}
};

} // namespace whiteboard
