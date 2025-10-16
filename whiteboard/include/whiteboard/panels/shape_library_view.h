/**
 * \file shape_library_view.h
 * \brief View component of Shape Library Panel MVC.
 */

#pragma once

#include "whiteboard/model/document_observer.h"
#include "whiteboard/model/whiteboard_document.h"
#include "whiteboard/svg/svg_shape_library.h"
#include <nanogui/widget.h>
#include <nanogui/button.h>
#include <nanogui/vscrollpanel.h>
#include <vector>
#include <string>

namespace whiteboard {

class ShapeLibraryController;

/**
 * \class ShapeLibraryView
 * \brief The View in Shape Library Panel MVC - displays shape categories and shapes.
 *
 * ShapeLibraryView is responsible for:
 * - Displaying category buttons
 * - Displaying shape grid with thumbnails
 * - Showing shape preview on hover
 * - Delegating shape selection to controller
 */
class ShapeLibraryView : public nanogui::Widget, public IDocumentObserver {
public:
  /**
   * \brief Constructor.
   * \param parent Parent widget
   * \param document The shared document model
   * \param library The shape library
   */
  ShapeLibraryView(nanogui::Widget *parent, WhiteboardDocument *document, 
                   SVGShapeLibrary *library);

  /**
   * \brief Destructor - unregisters from document.
   */
  ~ShapeLibraryView() override;

  /**
   * \brief Set the controller for this view.
   */
  void set_controller(ShapeLibraryController *controller) { m_controller = controller; }

  /**
   * \brief Rebuild the shape grid for the current category.
   */
  void rebuild_shape_grid();

  /**
   * \brief Set the active category filter.
   * \param category Category name (empty for all)
   */
  void set_category(const std::string& category);

  /**
   * \brief Get the current category.
   */
  std::string get_category() const { return m_current_category; }

  // === IDocumentObserver Implementation ===
  void on_tool_changed() override;

private:
  WhiteboardDocument *m_document;
  ShapeLibraryController *m_controller;
  SVGShapeLibrary *m_library;

  std::string m_current_category;

  // UI Components
  nanogui::Widget *m_category_bar;
  nanogui::Widget *m_shape_grid_container;
  nanogui::VScrollPanel *m_scroll_panel;
  nanogui::Widget *m_shape_grid;

  std::vector<nanogui::Button*> m_category_buttons;
  std::vector<nanogui::Button*> m_shape_buttons;

  /**
   * \brief Build the category button bar.
   */
  void build_category_bar();

  /**
   * \brief Build the shape grid for current category.
   */
  void build_shape_grid();

  /**
   * \brief Handle category button click.
   */
  void on_category_clicked(const std::string& category);

  /**
   * \brief Handle shape button click.
   */
  void on_shape_clicked(const std::string& shape_id);
};

} // namespace whiteboard
