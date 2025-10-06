/*
    examples/example_node_editor.cpp -- Visual node editor with drag & drop

    This example demonstrates a node-based editor similar to ImGui node editor:
    - Draggable nodes using Window widgets
    - Visual node graph
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

// Connection structure
struct Connection {
  int from_node;
  int to_node;

  Connection(int from, int to) : from_node(from), to_node(to) {}
};

// Forward declaration
class NodeWindow;

// Canvas for drawing connections (declaration only)
class ConnectionCanvas : public Canvas {
public:
  std::vector<Connection> *connections = nullptr;
  std::vector<NodeWindow *> *nodes = nullptr;

  ConnectionCanvas(Widget *parent);
  virtual void draw(NVGcontext *ctx) override;
};

// Node Window widget - each node is a draggable window
class NodeWindow : public Window {
public:
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
  Color node_color;

  NodeWindow(Widget *parent, const std::string &title, const Color &color)
      : Window(parent, title), node_color(color) {
    set_layout(new GroupLayout(5));
  }

  virtual bool mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                int modifiers) override {
    if (button == NANOGUI_MOUSE_BUTTON_LEFT) {
      // Move the window
      set_position(position() + rel);
      std::cout << "Dragging " << title() << " to (" << position().x() << ", " << position().y()
                << ")" << std::endl;
      return true;
    }
    return Window::mouse_drag_event(p, rel, button, modifiers);
  }
};

// ConnectionCanvas implementation (after NodeWindow is fully defined)
ConnectionCanvas::ConnectionCanvas(Widget *parent) : Canvas(parent, 1, false, false, false) {
  set_draw_border(false);
}

void ConnectionCanvas::draw(NVGcontext *ctx) {
  Canvas::draw(ctx);

  if (!connections || !nodes)
    return;

  // Draw all connections
  for (const auto &conn : *connections) {
    if (conn.from_node >= (int)nodes->size() || conn.to_node >= (int)nodes->size())
      continue;

    NodeWindow *from = (*nodes)[conn.from_node];
    NodeWindow *to = (*nodes)[conn.to_node];

    // Calculate connection points (right side of from node, left side of to node)
    float x1 = from->position().x() + from->width();
    float y1 = from->position().y() + from->height() / 2;
    float x2 = to->position().x();
    float y2 = to->position().y() + to->height() / 2;

    // Draw bezier curve
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x1, y1);

    float dx = x2 - x1;
    nvgBezierTo(ctx, x1 + dx * 0.5f, y1, x2 - dx * 0.5f, y2, x2, y2);

    nvgStrokeColor(ctx, Color(150, 150, 150, 200));
    nvgStrokeWidth(ctx, 3.0f);
    nvgStroke(ctx);
  }
}

class NodeEditorScreen : public Screen {
public:
  NodeEditorScreen() : Screen(Vector2i(1200, 800), "Node Editor - Fluent Design") {
    // Apply Fluent theme with Light palette for brighter appearance
    auto *theme = new FluentTheme(nvg_context(), FluentTheme::Palette::Light);
    set_theme(theme);

    // Set a light gray background instead of black
    set_background(Color(240, 240, 245, 255));

    // Create toolbar
    create_toolbar();

    // Create connection canvas (behind nodes)
    m_canvas = new ConnectionCanvas(this);
    m_canvas->set_position(Vector2i(0, 50));
    m_canvas->set_fixed_size(Vector2i(width(), height() - 80));
    m_canvas->connections = &m_connections;
    m_canvas->nodes = &m_nodes;

    // Create sample nodes
    create_sample_nodes();

    // Create status bar
    create_status_bar();

    perform_layout();

    std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Node Editor - Drag & Drop           ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝" << std::endl;
    std::cout << "\nControls:" << std::endl;
    std::cout << "  • Drag nodes to move them" << std::endl;
    std::cout << "  • Click to select nodes" << std::endl;
    std::cout << "  • ESC to exit\n" << std::endl;
  }

  void add_node(const std::string &name, const Vector2i &pos, const Color &color) {
    NodeWindow *node = new NodeWindow(this, name, color);
    node->set_position(pos);
    node->set_fixed_size(Vector2i(150, 100));
    m_nodes.push_back(node);
    perform_layout();
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
  std::vector<NodeWindow *> m_nodes;
  std::vector<Connection> m_connections;
  ConnectionCanvas *m_canvas = nullptr;
  Label *m_status_label = nullptr;

  void create_sample_nodes() {
    // Input node - Bright Blue (Fluent accent color)
    NodeWindow *input = new NodeWindow(this, "Input", Color(0, 120, 215, 255));
    input->set_position(Vector2i(50, 100));
    input->set_fixed_size(Vector2i(150, 80));
    input->outputs = {"Value"};
    new Label(input, "Output: Value", "sans", 12);
    m_nodes.push_back(input);

    // Math node - Green
    NodeWindow *math = new NodeWindow(this, "Add", Color(16, 137, 62, 255));
    math->set_position(Vector2i(250, 100));
    math->set_fixed_size(Vector2i(150, 100));
    math->inputs = {"A", "B"};
    math->outputs = {"Result"};
    new Label(math, "Inputs: A, B", "sans", 12);
    new Label(math, "Output: Result", "sans", 12);
    m_nodes.push_back(math);

    // Output node - Orange
    NodeWindow *output = new NodeWindow(this, "Output", Color(247, 99, 12, 255));
    output->set_position(Vector2i(450, 100));
    output->set_fixed_size(Vector2i(150, 80));
    output->inputs = {"Value"};
    new Label(output, "Input: Value", "sans", 12);
    m_nodes.push_back(output);

    // Multiply node - Purple
    NodeWindow *multiply = new NodeWindow(this, "Multiply", Color(136, 23, 152, 255));
    multiply->set_position(Vector2i(250, 250));
    multiply->set_fixed_size(Vector2i(150, 100));
    multiply->inputs = {"A", "B"};
    multiply->outputs = {"Result"};
    new Label(multiply, "Inputs: A, B", "sans", 12);
    new Label(multiply, "Output: Result", "sans", 12);
    m_nodes.push_back(multiply);

    // Create connections
    m_connections.push_back(Connection(0, 1)); // Input -> Add
    m_connections.push_back(Connection(1, 2)); // Add -> Output
  }

  void create_toolbar() {
    Window *toolbar = new Window(this, "");
    toolbar->set_position(Vector2i(0, 0));
    toolbar->set_fixed_size(Vector2i(width(), 50));
    toolbar->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 5));

    new Label(toolbar, "Node Editor", "sans-bold", 16);

    new Label(toolbar, " | ");

    Button *add_btn = new Button(toolbar, "Add Node", FA_PLUS);
    add_btn->set_background_color(Color(0, 120, 212, 255));
    add_btn->set_text_color(Color(255, 255, 255, 255));
    add_btn->set_callback([this] {
      // Add random node
      int x = 100 + (rand() % 400);
      int y = 100 + (rand() % 300);
      Color color(100 + rand() % 155, 100 + rand() % 155, 100 + rand() % 155, 255);
      add_node("New Node", Vector2i(x, y), color);
      update_status("Added new node");
    });

    Button *clear_btn = new Button(toolbar, "Clear", FA_TRASH);
    clear_btn->set_callback([this] { update_status("Clear not implemented"); });

    new Label(toolbar, " | ");

    Button *light_theme = new Button(toolbar, "", FA_SUN);
    light_theme->set_tooltip("Light Theme");
    light_theme->set_callback([this] {
      set_theme(new FluentTheme(nvg_context(), FluentTheme::Palette::Light));
      update_status("Light theme");
    });

    Button *dark_theme = new Button(toolbar, "", FA_MOON);
    dark_theme->set_tooltip("Dark Theme");
    dark_theme->set_callback([this] {
      set_theme(new FluentTheme(nvg_context(), FluentTheme::Palette::Dark));
      update_status("Dark theme");
    });
  }

  void create_status_bar() {
    Window *status_bar = new Window(this, "");
    status_bar->set_position(Vector2i(0, height() - 30));
    status_bar->set_fixed_size(Vector2i(width(), 30));
    status_bar->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 5));

    m_status_label = new Label(status_bar, "Ready - Drag nodes to move them", "sans", 12);
  }

  void update_status(const std::string &msg) {
    if (m_status_label) {
      m_status_label->set_caption(msg);
      std::cout << "Status: " << msg << std::endl;
    }
  }
};

int main(int argc, char **argv) {
  try {
    nanogui::init();

    {
      ref<NodeEditorScreen> app = new NodeEditorScreen();
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
    std::cout << "\n✓ Node editor closed!" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "✗ Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
