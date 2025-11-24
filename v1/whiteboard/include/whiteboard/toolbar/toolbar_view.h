/**
 * \file toolbar_view.h
 * \brief View component of Toolbar MVC - displays tool buttons and properties.
 */

#pragma once

#include "whiteboard/model/document_observer.h"
#include "whiteboard/model/whiteboard_document.h"
#include <nanogui/widget.h>

namespace whiteboard {

// Forward declaration
class ToolbarController;

/**
 * \class ToolbarView
 * \brief The View in Toolbar MVC - displays tools and properties.
 *
 * ToolbarView is responsible for:
 * - Displaying tool buttons
 * - Displaying property controls (color, width, fill)
 * - Updating UI when model changes
 * - Delegating user actions to controller
 */
class ToolbarView : public nanogui::Widget, public IDocumentObserver {
public:
  /**
   * \brief Constructor.
   * \param parent Parent widget
   * \param document The shared document model
   */
  ToolbarView(nanogui::Widget *parent, WhiteboardDocument *document);

  /**
   * \brief Destructor - unregisters from document.
   */
  ~ToolbarView() override;

  // === IDocumentObserver Implementation ===

  /**
   * \brief Called when tool changes - updates button states.
   */
  void on_tool_changed() override;

  /**
   * \brief Called when properties change - updates controls.
   */
  void on_properties_changed() override;

  /**
   * \brief Set the controller for this view.
   */
  void set_controller(ToolbarController *controller) { m_controller = controller; }

private:
  WhiteboardDocument *m_document;
  ToolbarController *m_controller;

  // UI will be created by the existing ToolbarPanelModule
  // This is a thin wrapper that observes the model
};

} // namespace whiteboard
