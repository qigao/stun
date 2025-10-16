/**
 * \file layers_view.h
 * \brief View component of Layers Panel MVC - displays layer list.
 */

#pragma once

#include "whiteboard/model/document_observer.h"
#include "whiteboard/model/whiteboard_document.h"
#include <nanogui/widget.h>

namespace whiteboard {

// Forward declaration
class LayersController;

/**
 * \class LayersView
 * \brief The View in Layers Panel MVC - displays layers.
 *
 * LayersView is responsible for:
 * - Displaying layer list
 * - Showing visibility and lock state
 * - Highlighting selected layers
 * - Delegating user actions to controller
 */
class LayersView : public nanogui::Widget, public IDocumentObserver {
public:
  /**
   * \brief Constructor.
   * \param parent Parent widget
   * \param document The shared document model
   */
  LayersView(nanogui::Widget *parent, WhiteboardDocument *document);

  /**
   * \brief Destructor - unregisters from document.
   */
  ~LayersView() override;

  // === IDocumentObserver Implementation ===

  /**
   * \brief Called when strokes change - rebuilds layer list.
   */
  void on_strokes_changed() override;

  /**
   * \brief Called when selection changes - highlights selected layers.
   */
  void on_selection_changed() override;

  /**
   * \brief Set the controller for this view.
   */
  void set_controller(LayersController *controller) { m_controller = controller; }

private:
  WhiteboardDocument *m_document;
  LayersController *m_controller;

  // UI will be created by the existing LayersPanel
  // This is a thin wrapper that observes the model
};

} // namespace whiteboard
