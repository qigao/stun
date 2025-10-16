/**
 * \file test_whiteboard_document.cpp
 * \brief Unit tests for WhiteboardDocument model.vgzl
 */

#include "whiteboard/model/whiteboard_document.h"
#include <cassert>
#include <iostream>

using namespace whiteboard;

// Simple test framework (replace with Catch2 if available)
#define TEST(name) void test_##name()
#define ASSERT(condition)                                                                          \
  if (!(condition)) {                                                                              \
    std::cerr << "FAILED: " << #condition << " at line " << __LINE__ << std::endl;                 \
    exit(1);                                                                                       \
  }

// Mock observer for testing
class MockObserver : public IDocumentObserver {
public:
  int strokes_changed_count = 0;
  int selection_changed_count = 0;
  int tool_changed_count = 0;
  int properties_changed_count = 0;
  int view_changed_count = 0;

  void on_strokes_changed() override { strokes_changed_count++; }
  void on_selection_changed() override { selection_changed_count++; }
  void on_tool_changed() override { tool_changed_count++; }
  void on_properties_changed() override { properties_changed_count++; }
  void on_view_changed() override { view_changed_count++; }
};

TEST(add_stroke) {
  WhiteboardDocument doc;

  Stroke stroke;
  stroke.tool = Tool::Pen;
  stroke.points.push_back(Point(10, 20));

  doc.add_stroke(stroke);

  ASSERT(doc.get_strokes().size() == 1);
  ASSERT(doc.get_strokes()[0].tool == Tool::Pen);
  ASSERT(doc.get_strokes()[0].points.size() == 1);

  std::cout << "✓ test_add_stroke passed" << std::endl;
}

TEST(observer_notifications) {
  WhiteboardDocument doc;
  MockObserver observer;

  doc.add_observer(&observer);

  Stroke stroke;
  doc.add_stroke(stroke);

  ASSERT(observer.strokes_changed_count == 1);

  doc.remove_observer(&observer);

  std::cout << "✓ test_observer_notifications passed" << std::endl;
}

TEST(undo_redo) {
  WhiteboardDocument doc;

  Stroke stroke;
  doc.add_stroke(stroke);
  ASSERT(doc.get_strokes().size() == 1);
  ASSERT(doc.can_undo());
  ASSERT(!doc.can_redo());

  doc.undo();
  ASSERT(doc.get_strokes().size() == 0);
  ASSERT(!doc.can_undo());
  ASSERT(doc.can_redo());

  doc.redo();
  ASSERT(doc.get_strokes().size() == 1);
  ASSERT(doc.can_undo());
  ASSERT(!doc.can_redo());

  std::cout << "✓ test_undo_redo passed" << std::endl;
}

TEST(selection) {
  WhiteboardDocument doc;

  Stroke stroke1, stroke2;
  doc.add_stroke(stroke1);
  doc.add_stroke(stroke2);

  doc.set_selection({0});
  ASSERT(doc.get_selected_indices().size() == 1);
  ASSERT(doc.is_selected(0));
  ASSERT(!doc.is_selected(1));

  doc.set_selection({0, 1});
  ASSERT(doc.get_selected_indices().size() == 2);
  ASSERT(doc.is_selected(0));
  ASSERT(doc.is_selected(1));

  doc.clear_selection();
  ASSERT(doc.get_selected_indices().size() == 0);

  std::cout << "✓ test_selection passed" << std::endl;
}

TEST(tool_and_properties) {
  WhiteboardDocument doc;
  MockObserver observer;
  doc.add_observer(&observer);

  doc.set_current_tool(Tool::Rectangle);
  ASSERT(doc.get_current_tool() == Tool::Rectangle);
  ASSERT(observer.tool_changed_count == 1);

  doc.set_stroke_width(5.0f);
  ASSERT(doc.get_stroke_width() == 5.0f);
  ASSERT(observer.properties_changed_count == 1);

  doc.set_zoom(2.0f);
  ASSERT(doc.get_zoom() == 2.0f);
  ASSERT(observer.view_changed_count == 1);

  std::cout << "✓ test_tool_and_properties passed" << std::endl;
}

TEST(remove_strokes) {
  WhiteboardDocument doc;

  Stroke stroke1, stroke2, stroke3;
  doc.add_stroke(stroke1);
  doc.add_stroke(stroke2);
  doc.add_stroke(stroke3);

  ASSERT(doc.get_strokes().size() == 3);

  doc.remove_strokes({1}); // Remove middle stroke
  ASSERT(doc.get_strokes().size() == 2);

  std::cout << "✓ test_remove_strokes passed" << std::endl;
}

int main() {
  std::cout << "Running WhiteboardDocument tests..." << std::endl;

  test_add_stroke();
  test_observer_notifications();
  test_undo_redo();
  test_selection();
  test_tool_and_properties();
  test_remove_strokes();

  std::cout << "\n✅ All tests passed!" << std::endl;
  return 0;
}
