/*
    examples/example_whiteboard.cpp -- Interactive whiteboard with Fluent Design

    A collaborative whiteboard example demonstrating:
    - Freehand drawing
    - Shape tools (rectangle, circle, line)
    - Color picker
    - Eraser tool
    - Clear canvas
    - Fluent Design styling

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
*/

#include <iostream>
#include <nanogui/fluent_theme.h>
#include <nanogui/nanogui.h>
#include <nanogui/opengl.h>
#include <nanovg.h>
#include <vector>

// Backend-agnostic key codes
#if defined(NANOGUI_USE_SDL3)
  #include <SDL3/SDL.h>
  #define KEY_ESCAPE SDLK_ESCAPE
  #define KEY_PRESS 1
#else
  #include <GLFW/glfw3.h>
  #define KEY_ESCAPE GLFW_KEY_ESCAPE
  #define KEY_PRESS GLFW_PRESS
#endif

using namespace nanogui;

enum class Tool { Pen, Eraser, Line, Rectangle, Circle };

struct Point {
  float x, y;
  Point(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Stroke {
  std::vector<Point> points;
  Color color;
  float width;
  Tool tool;
};

class WhiteboardCanvas : public Canvas {
public:
  WhiteboardCanvas(Widget *parent) : Canvas(parent, 1, false, false, false) {
    set_draw_border(false);
    m_current_color = Color(0, 120, 215, 255); // Fluent blue
    m_current_width = 3.0f;
    m_current_tool = Tool::Pen;
  }

  virtual void draw(NVGcontext *ctx) override {
    Canvas::draw(ctx);

    // Draw all completed strokes
    for (const auto &stroke : m_strokes) {
      draw_stroke(ctx, stroke);
    }

    // Draw current stroke being drawn
    if (m_is_drawing && !m_current_stroke.points.empty()) {
      draw_stroke(ctx, m_current_stroke);
    }
  }

  virtual bool mouse_button_event(const Vector2i &p, int button, bool down,
                                  int modifiers) override {
    if (button == NANOGUI_MOUSE_BUTTON_LEFT) {
      Vector2i local_p = p - absolute_position();

      if (down) {
        request_focus();
        m_is_drawing = true;
        m_current_stroke.points.clear();
        m_current_stroke.color = m_current_color;
        m_current_stroke.width = m_current_width;
        m_current_stroke.tool = m_current_tool;
        m_current_stroke.points.push_back(Point(local_p.x(), local_p.y()));
        m_start_point = local_p;
        std::cout << "Started drawing at (" << local_p.x() << ", " << local_p.y() << ")"
                  << std::endl;
        return true;
      } else {
        if (m_is_drawing) {
          m_is_drawing = false;
          if (!m_current_stroke.points.empty()) {
            m_strokes.push_back(m_current_stroke);
            std::cout << "Finished stroke with " << m_current_stroke.points.size() << " points"
                      << std::endl;
          }
          m_current_stroke.points.clear();
          return true;
        }
      }
    }
    return Canvas::mouse_button_event(p, button, down, modifiers);
  }

  virtual bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                int modifiers) override {
    // button parameter is actually a bitmask, not the button number
    if (m_is_drawing) {
      Vector2i local_p = p - absolute_position();

      if (m_current_tool == Tool::Pen || m_current_tool == Tool::Eraser) {
        m_current_stroke.points.push_back(Point(local_p.x(), local_p.y()));
        std::cout << "Drag: Drawing point " << m_current_stroke.points.size() << " at ("
                  << local_p.x() << ", " << local_p.y() << ")" << std::endl;
      } else {
        // For shapes, keep only start and end point
        if (m_current_stroke.points.size() > 1) {
          m_current_stroke.points[1] = Point(local_p.x(), local_p.y());
        } else {
          m_current_stroke.points.push_back(Point(local_p.x(), local_p.y()));
        }
      }
      return true;
    }
    return false; // Don't call parent - we handle it
  }

  virtual bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                                  int modifiers) override {
    // Handle motion events for drawing while mouse button is held
    if (m_is_drawing) {
      Vector2i local_p = p - absolute_position();

      if (m_current_tool == Tool::Pen || m_current_tool == Tool::Eraser) {
        m_current_stroke.points.push_back(Point(local_p.x(), local_p.y()));
        std::cout << "Motion: point " << m_current_stroke.points.size() << " at (" << local_p.x()
                  << ", " << local_p.y() << ")" << std::endl;
      } else {
        // For shapes, keep only start and end point
        if (m_current_stroke.points.size() > 1) {
          m_current_stroke.points[1] = Point(local_p.x(), local_p.y());
        } else {
          m_current_stroke.points.push_back(Point(local_p.x(), local_p.y()));
        }
      }
      return true;
    }
    return false; // Don't call parent
  }

  void set_color(const Color &color) { m_current_color = color; }

  void set_tool(Tool tool) {
    m_current_tool = tool;
    if (tool == Tool::Eraser) {
      m_current_width = 20.0f;
    } else {
      m_current_width = 3.0f;
    }
  }

  void clear() {
    m_strokes.clear();
    m_current_stroke.points.clear();
  }

private:
  std::vector<Stroke> m_strokes;
  Stroke m_current_stroke;
  bool m_is_drawing = false;
  Color m_current_color;
  float m_current_width;
  Tool m_current_tool;
  Vector2i m_start_point;

  void draw_stroke(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.empty())
      return;

    switch (stroke.tool) {
    case Tool::Pen:
    case Tool::Eraser:
      draw_freehand(ctx, stroke);
      break;
    case Tool::Line:
      draw_line(ctx, stroke);
      break;
    case Tool::Rectangle:
      draw_rectangle(ctx, stroke);
      break;
    case Tool::Circle:
      draw_circle(ctx, stroke);
      break;
    }
  }

  void draw_freehand(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.empty())
      return;

    // Draw even single points as dots
    if (stroke.points.size() == 1) {
      nvgBeginPath(ctx);
      nvgCircle(ctx, stroke.points[0].x, stroke.points[0].y, stroke.width / 2);
      if (stroke.tool == Tool::Eraser) {
        nvgFillColor(ctx, Color(255, 255, 255, 255));
      } else {
        nvgFillColor(ctx, stroke.color);
      }
      nvgFill(ctx);
      return;
    }

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, stroke.points[0].x, stroke.points[0].y);

    for (size_t i = 1; i < stroke.points.size(); i++) {
      nvgLineTo(ctx, stroke.points[i].x, stroke.points[i].y);
    }

    if (stroke.tool == Tool::Eraser) {
      nvgStrokeColor(ctx, Color(255, 255, 255, 255)); // White for eraser
    } else {
      nvgStrokeColor(ctx, stroke.color);
    }
    nvgStrokeWidth(ctx, stroke.width);
    nvgLineCap(ctx, NVG_ROUND);
    nvgLineJoin(ctx, NVG_ROUND);
    nvgStroke(ctx);
  }

  void draw_line(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, stroke.points[0].x, stroke.points[0].y);
    nvgLineTo(ctx, stroke.points[1].x, stroke.points[1].y);
    nvgStrokeColor(ctx, stroke.color);
    nvgStrokeWidth(ctx, stroke.width);
    nvgLineCap(ctx, NVG_ROUND);
    nvgStroke(ctx);
  }

  void draw_rectangle(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    float x1 = stroke.points[0].x;
    float y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x;
    float y2 = stroke.points[1].y;

    nvgBeginPath(ctx);
    nvgRect(ctx, std::min(x1, x2), std::min(y1, y2), std::abs(x2 - x1), std::abs(y2 - y1));
    nvgStrokeColor(ctx, stroke.color);
    nvgStrokeWidth(ctx, stroke.width);
    nvgStroke(ctx);
  }

  void draw_circle(NVGcontext *ctx, const Stroke &stroke) {
    if (stroke.points.size() < 2)
      return;

    float x1 = stroke.points[0].x;
    float y1 = stroke.points[0].y;
    float x2 = stroke.points[1].x;
    float y2 = stroke.points[1].y;

    float dx = x2 - x1;
    float dy = y2 - y1;
    float radius = std::sqrt(dx * dx + dy * dy);

    nvgBeginPath(ctx);
    nvgCircle(ctx, x1, y1, radius);
    nvgStrokeColor(ctx, stroke.color);
    nvgStrokeWidth(ctx, stroke.width);
    nvgStroke(ctx);
  }
};

class WhiteboardApp : public Screen {
public:
  WhiteboardApp() : Screen(Vector2i(1200, 800), "Whiteboard - Fluent Design") {
    // Apply Fluent theme
    auto *theme = new FluentTheme(nvg_context(), FluentTheme::Palette::Light);
    set_theme(theme);

    // Set screen background to white
    set_background(Color(255, 255, 255, 255));

    // Create whiteboard canvas FIRST (so it's behind toolbar)
    m_canvas = new WhiteboardCanvas(this);
    m_canvas->set_position(Vector2i(0, 60));
    m_canvas->set_fixed_size(Vector2i(width(), height() - 60));
    m_canvas->set_background_color(Color(255, 255, 255, 255)); // Pure white background

    // Create toolbar AFTER (so it's on top)
    create_toolbar();

    perform_layout();

    std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Whiteboard - Fluent Design          ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝" << std::endl;
    std::cout << "\nControls:" << std::endl;
    std::cout << "  • Draw with mouse" << std::endl;
    std::cout << "  • Select tools from toolbar" << std::endl;
    std::cout << "  • Choose colors" << std::endl;
    std::cout << "  • ESC to exit\n" << std::endl;
  }

  virtual bool keyboard_event(int key, int scancode, int action, int modifiers) override {
    if (Screen::keyboard_event(key, scancode, action, modifiers))
      return true;

    if (key == KEY_ESCAPE && action == KEY_PRESS) {
      set_visible(false);
      return true;
    }

    return false;
  }

private:
  WhiteboardCanvas *m_canvas = nullptr;

  void create_toolbar() {
    Window *toolbar = new Window(this, "");
    toolbar->set_position(Vector2i(0, 0));
    toolbar->set_fixed_size(Vector2i(width(), 60));
    toolbar->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 5));

    new Label(toolbar, "Whiteboard", "sans-bold", 16);
    new Label(toolbar, " | ");

    // Tool buttons
    new Label(toolbar, "Tools:", "sans-bold");

    Button *pen_btn = new Button(toolbar, "", FA_PEN);
    pen_btn->set_tooltip("Pen");
    pen_btn->set_background_color(Color(0, 120, 215, 255));
    pen_btn->set_text_color(Color(255, 255, 255, 255));
    pen_btn->set_callback([this] {
      m_canvas->set_tool(Tool::Pen);
      std::cout << "Tool: Pen" << std::endl;
    });

    Button *eraser_btn = new Button(toolbar, "", FA_ERASER);
    eraser_btn->set_tooltip("Eraser");
    eraser_btn->set_callback([this] {
      m_canvas->set_tool(Tool::Eraser);
      std::cout << "Tool: Eraser" << std::endl;
    });

    Button *line_btn = new Button(toolbar, "", FA_MINUS);
    line_btn->set_tooltip("Line");
    line_btn->set_callback([this] {
      m_canvas->set_tool(Tool::Line);
      std::cout << "Tool: Line" << std::endl;
    });

    Button *rect_btn = new Button(toolbar, "", FA_SQUARE);
    rect_btn->set_tooltip("Rectangle");
    rect_btn->set_callback([this] {
      m_canvas->set_tool(Tool::Rectangle);
      std::cout << "Tool: Rectangle" << std::endl;
    });

    Button *circle_btn = new Button(toolbar, "", FA_CIRCLE);
    circle_btn->set_tooltip("Circle");
    circle_btn->set_callback([this] {
      m_canvas->set_tool(Tool::Circle);
      std::cout << "Tool: Circle" << std::endl;
    });

    new Label(toolbar, " | ");

    // Color buttons
    new Label(toolbar, "Colors:", "sans-bold");

    create_color_button(toolbar, Color(0, 120, 215, 255));  // Blue
    create_color_button(toolbar, Color(16, 137, 62, 255));  // Green
    create_color_button(toolbar, Color(247, 99, 12, 255));  // Orange
    create_color_button(toolbar, Color(232, 17, 35, 255));  // Red
    create_color_button(toolbar, Color(136, 23, 152, 255)); // Purple
    create_color_button(toolbar, Color(0, 0, 0, 255));      // Black

    new Label(toolbar, " | ");

    // Clear button
    Button *clear_btn = new Button(toolbar, "Clear", FA_TRASH);
    clear_btn->set_callback([this] {
      m_canvas->clear();
      std::cout << "Canvas cleared" << std::endl;
    });
  }

  void create_color_button(Widget *parent, const Color &color) {
    Button *btn = new Button(parent, "");
    btn->set_fixed_size(Vector2i(30, 30));
    btn->set_background_color(color);
    btn->set_callback([this, color] {
      m_canvas->set_color(color);
      std::cout << "Color changed" << std::endl;
    });
  }
};

int main(int argc, char **argv) {
  try {
    nanogui::init();

    {
      ref<WhiteboardApp> app = new WhiteboardApp();
      app->set_visible(true);

#if defined(NANOGUI_USE_SDL3)
      while (app->process_events()) {
        app->draw_all();
      }
#else
      app->draw_all();
      nanogui::mainloop();
#endif
    }

    nanogui::shutdown();
    std::cout << "\n✓ Whiteboard closed!" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "✗ Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
