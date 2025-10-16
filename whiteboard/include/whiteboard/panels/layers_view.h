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
  /**
   * \brief Rebuild the layer list UI.
   */
  void rebuild_layer_list();
  
  /**
   * \brief Get display name for a stroke/layer.
   */
  std::string get_layer_display_name(const Stroke &stroke) const;

  WhiteboardDocument *m_document;
  LayersController *m_controller;
  nanogui::Widget *m_layer_container;
};

} // namespace whiteboard
