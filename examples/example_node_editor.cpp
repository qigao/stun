/*
    examples/example_node_editor.cpp -- Visual node editor with NEW Fluent Components

    This example demonstrates a node-based editor with NEW Fluent v2.2 components:
    - FluentMenuBar for File/Edit/View menus
    - FluentCommandBar for node operations
    - FluentPivot for node categories
    - FluentExpander for node properties
    - FluentNumberBox for node parameters
    - FluentInfoBar for status notifications
    - Draggable nodes using Window widgets
    - Visual node graph with bezier connections

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
*/

#include <iostream>
#include <nanogui.h>
#include <nanogui/fluent.h>
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
    m_canvas->set_position(Vector2i(0, 85));
    m_canvas->set_fixed_size(Vector2i(width(), height() - 85));
    m_canvas->connections = &m_connections;
    m_canvas->nodes = &m_nodes;

    // Create sample nodes
    create_sample_nodes();

    // Show welcome notification
    show_notification("Welcome to Node Editor! Drag nodes to move them.", 
                     FluentInfoBar::Severity::Informational);

    perform_layout();

    std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Node Editor - NEW Fluent v2.2       ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝" << std::endl;
    std::cout << "\nNew Components Showcased:" << std::endl;
    std::cout << "  ✨ FluentMenuBar - File/Edit/View menus" << std::endl;
    std::cout << "  ✨ FluentCommandBar - Node operations" << std::endl;
    std::cout << "  ✨ FluentPivot - Node categories" << std::endl;
    std::cout << "  ✨ FluentExpander - Node properties" << std::endl;
    std::cout << "  ✨ FluentNumberBox - Parameter controls" << std::endl;
    std::cout << "  ✨ FluentInfoBar - Status notifications" << std::endl;
    std::cout << "\nControls:" << std::endl;
    std::cout << "  • Drag nodes to move them" << std::endl;
    std::cout << "  • Use command bar for operations" << std::endl;
    std::cout << "  • Browse node categories with pivot" << std::endl;
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
  FluentInfoBar *m_info_bar = nullptr;
  
  void show_notification(const std::string &message, FluentInfoBar::Severity severity) {
    if (m_info_bar) {
      m_info_bar->set_visible(false);
      remove_child(m_info_bar);
    }
    
    m_info_bar = new FluentInfoBar(this, message, severity);
    m_info_bar->set_position(Vector2i(10, height() - 70));
    m_info_bar->set_fixed_width(350);
    m_info_bar->set_closable(true);
    m_info_bar->set_close_callback([this]() {
      if (m_info_bar) {
        m_info_bar->set_visible(false);
      }
    });
    
    perform_layout();
  }
  
  void create_node_palette() {
    // Create sidebar with node categories using FluentPivot!
    Window *sidebar = new Window(this, "Node Palette");
    sidebar->set_position(Vector2i(width() - 250, 85));
    sidebar->set_fixed_size(Vector2i(250, 400));
    sidebar->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));
    
    // Use NEW FluentPivot for node categories!
    auto *pivot = new FluentPivot(sidebar);
    pivot->add_item("Input", FA_SIGN_IN_ALT);
    pivot->add_item("Math", FA_CALCULATOR);
    pivot->add_item("Output", FA_SIGN_OUT_ALT);
    
    // Input nodes category
    auto *input_content = pivot->get_content(0);
    input_content->set_layout(new GroupLayout(5));
    
    auto *input_exp = new FluentExpander(input_content, "Input Nodes");
    input_exp->set_icon(FA_SIGN_IN_ALT);
    input_exp->set_expanded(true);
    
    Button *float_btn = new Button(input_exp->content(), "Float", FA_HASHTAG);
    float_btn->set_callback([this]() {
      add_node("Float", Vector2i(100, 150), Color(0, 120, 215, 255));
      show_notification("Float node added", FluentInfoBar::Severity::Success);
    });
    
    Button *vector_btn = new Button(input_exp->content(), "Vector", FA_ARROWS_ALT);
    vector_btn->set_callback([this]() {
      add_node("Vector", Vector2i(100, 250), Color(0, 120, 215, 255));
      show_notification("Vector node added", FluentInfoBar::Severity::Success);
    });
    
    // Math nodes category
    auto *math_content = pivot->get_content(1);
    math_content->set_layout(new GroupLayout(5));
    
    auto *math_exp = new FluentExpander(math_content, "Math Nodes");
    math_exp->set_icon(FA_CALCULATOR);
    math_exp->set_expanded(true);
    
    Button *add_btn = new Button(math_exp->content(), "Add", FA_PLUS);
    add_btn->set_callback([this]() {
      add_node("Add", Vector2i(300, 150), Color(16, 137, 62, 255));
      show_notification("Add node added", FluentInfoBar::Severity::Success);
    });
    
    Button *mult_btn = new Button(math_exp->content(), "Multiply", FA_TIMES);
    mult_btn->set_callback([this]() {
      add_node("Multiply", Vector2i(300, 250), Color(136, 23, 152, 255));
      show_notification("Multiply node added", FluentInfoBar::Severity::Success);
    });
    
    // Output nodes category
    auto *output_content = pivot->get_content(2);
    output_content->set_layout(new GroupLayout(5));
    
    auto *output_exp = new FluentExpander(output_content, "Output Nodes");
    output_exp->set_icon(FA_SIGN_OUT_ALT);
    output_exp->set_expanded(true);
    
    Button *display_btn = new Button(output_exp->content(), "Display", FA_DESKTOP);
    display_btn->set_callback([this]() {
      add_node("Display", Vector2i(500, 150), Color(247, 99, 12, 255));
      show_notification("Display node added", FluentInfoBar::Severity::Success);
    });
  }

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
    // Create menu bar window
    Window *menu_window = new Window(this, "");
    menu_window->set_position(Vector2i(0, 0));
    menu_window->set_fixed_size(Vector2i(width(), 35));
    menu_window->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 5, 5));
    
    // Use NEW FluentMenuBar!
    auto *menu_bar = new FluentMenuBar(menu_window);
    
    // File menu
    auto *file = menu_bar->add_menu("File");
    menu_bar->add_item(file, "New Graph", FA_FILE, [this]() {
      show_notification("New graph created", FluentInfoBar::Severity::Success);
    }, "Ctrl+N");
    menu_bar->add_item(file, "Open", FA_FOLDER_OPEN, [this]() {
      show_notification("Open not implemented", FluentInfoBar::Severity::Informational);
    }, "Ctrl+O");
    menu_bar->add_item(file, "Save", FA_SAVE, [this]() {
      show_notification("Graph saved", FluentInfoBar::Severity::Success);
    }, "Ctrl+S");
    menu_bar->add_separator(file);
    menu_bar->add_item(file, "Exit", FA_TIMES, [this]() {
      set_visible(false);
    }, "Alt+F4");
    
    // Edit menu
    auto *edit = menu_bar->add_menu("Edit");
    menu_bar->add_item(edit, "Undo", FA_UNDO, [this]() {
      show_notification("Undo", FluentInfoBar::Severity::Informational);
    }, "Ctrl+Z");
    menu_bar->add_item(edit, "Redo", FA_REDO, [this]() {
      show_notification("Redo", FluentInfoBar::Severity::Informational);
    }, "Ctrl+Y");
    menu_bar->add_separator(edit);
    menu_bar->add_item(edit, "Delete Node", FA_TRASH, [this]() {
      show_notification("Delete selected node", FluentInfoBar::Severity::Warning);
    }, "Delete");
    
    // View menu
    auto *view = menu_bar->add_menu("View");
    menu_bar->add_item(view, "Zoom In", FA_SEARCH_PLUS, [this]() {
      show_notification("Zoom in", FluentInfoBar::Severity::Informational);
    }, "Ctrl++");
    menu_bar->add_item(view, "Zoom Out", FA_SEARCH_MINUS, [this]() {
      show_notification("Zoom out", FluentInfoBar::Severity::Informational);
    }, "Ctrl+-");
    menu_bar->add_item(view, "Reset View", FA_HOME, [this]() {
      show_notification("View reset", FluentInfoBar::Severity::Informational);
    }, "Home");
    menu_bar->add_separator(view);
    menu_bar->add_item(view, "Light Theme", FA_SUN, [this]() {
      set_theme(new FluentTheme(nvg_context(), FluentTheme::Palette::Light));
      show_notification("Light theme applied", FluentInfoBar::Severity::Success);
    });
    menu_bar->add_item(view, "Dark Theme", FA_MOON, [this]() {
      set_theme(new FluentTheme(nvg_context(), FluentTheme::Palette::Dark));
      show_notification("Dark theme applied", FluentInfoBar::Severity::Success);
    });
    
    // Create command bar window
    Window *toolbar = new Window(this, "");
    toolbar->set_position(Vector2i(0, 35));
    toolbar->set_fixed_size(Vector2i(width(), 50));
    toolbar->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 5));

    new Label(toolbar, "Node Editor", "sans-bold", 16);
    
    // Use NEW FluentCommandBar for node operations!
    auto *cmd_bar = new FluentCommandBar(toolbar);
    
    // Node operations
    cmd_bar->add_command("Add Node", FA_PLUS, [this]() {
      int x = 100 + (rand() % 400);
      int y = 100 + (rand() % 300);
      Color color(100 + rand() % 155, 100 + rand() % 155, 100 + rand() % 155, 255);
      add_node("New Node", Vector2i(x, y), color);
      show_notification("Node added", FluentInfoBar::Severity::Success);
    });
    
    cmd_bar->add_command("Delete", FA_TRASH, [this]() {
      show_notification("Delete selected node", FluentInfoBar::Severity::Warning);
    });
    
    cmd_bar->add_separator();
    
    cmd_bar->add_command("Connect", FA_LINK, [this]() {
      show_notification("Connection mode", FluentInfoBar::Severity::Informational);
    });
    
    cmd_bar->add_command("Disconnect", FA_UNLINK, [this]() {
      show_notification("Disconnect mode", FluentInfoBar::Severity::Informational);
    });
    
    cmd_bar->add_separator();
    
    cmd_bar->add_command("Run", FA_PLAY, [this]() {
      show_notification("Graph executed", FluentInfoBar::Severity::Success);
    });
    
    // Create node palette sidebar
    create_node_palette();
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
