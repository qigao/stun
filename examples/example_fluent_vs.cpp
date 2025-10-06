/*
    examples/example_fluent_vs.cpp -- Visual Studio style with AdvancedGridLayout

    This example demonstrates a proper Visual Studio-style IDE layout using
    NanoGUI's AdvancedGridLayout for professional panel management.

    Features:
    - Menu bar and toolbar
    - Resizable sidebar (Solution Explorer)
    - Main editor area with tabs
    - Resizable properties panel
    - Output panel at bottom
    - Status bar

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.
*/

#include <iostream>
#include <nanogui/fluent_theme.h>
#include <nanogui/nanogui.h>


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

class VSStyleApp : public Screen {
public:
  VSStyleApp() : Screen(Vector2i(1400, 900), "Visual Studio - Fluent Design") {
    // Apply Fluent theme
    auto *theme = new FluentTheme(nvg_context(), FluentTheme::Palette::Light);
    set_theme(theme);

    // Create main layout using AdvancedGridLayout
    // Grid: [Menu] [Toolbar] [Sidebar | Editor | Properties] [Output] [Status]
    //       Row 0: Menu bar (35px, fixed)
    //       Row 1: Toolbar (45px, fixed)
    //       Row 2: Main content (stretch)
    //       Row 3: Output panel (150px, fixed)
    //       Row 4: Status bar (30px, fixed)

    create_layout();

    perform_layout();

    std::cout << "\n╔════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Visual Studio Style - Advanced      ║" << std::endl;
    std::cout << "╚════════════════════════════════════════╝" << std::endl;
    std::cout << "\nLayout Features:" << std::endl;
    std::cout << "  • AdvancedGridLayout for proper sizing" << std::endl;
    std::cout << "  • Resizable panels with stretch factors" << std::endl;
    std::cout << "  • Professional IDE layout" << std::endl;
    std::cout << "  • Menu, Toolbar, Sidebar, Editor, Properties" << std::endl;
    std::cout << "  • Output panel and Status bar" << std::endl;
    std::cout << "\nPress ESC to exit\n" << std::endl;
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

  virtual bool resize_event(const Vector2i &size) override {
    // Update all windows to match new screen size
    int content_height = size.y() - 210;

    for (auto child : children()) {
      Window *window = dynamic_cast<Window *>(child);
      if (!window)
        continue;

      Vector2i pos = window->position();

      // Menu bar (y=0)
      if (pos.y() == 0) {
        window->set_fixed_width(size.x());
      }
      // Toolbar (y=35)
      else if (pos.y() == 35) {
        window->set_fixed_width(size.x());
      }
      // Sidebar (y=80, x=0)
      else if (pos.y() == 80 && pos.x() == 0) {
        window->set_fixed_height(content_height);
      }
      // Editor (y=80, x=250)
      else if (pos.y() == 80 && pos.x() == 250) {
        window->set_fixed_size(Vector2i(size.x() - 530, content_height));
      }
      // Properties (y=80, right side)
      else if (pos.y() == 80 && pos.x() > 250) {
        window->set_position(Vector2i(size.x() - 280, 80));
        window->set_fixed_height(content_height);
      }
      // Output panel
      else if (pos.y() == size.y() - 180 || (pos.y() > 80 && pos.y() < size.y() - 30 &&
                                             pos.x() == 0 && window->title() == "Output")) {
        window->set_position(Vector2i(0, size.y() - 180));
        window->set_fixed_width(size.x());
      }
      // Status bar
      else if (pos.y() >= size.y() - 30) {
        window->set_position(Vector2i(0, size.y() - 30));
        window->set_fixed_width(size.x());
      }
    }

    perform_layout();
    return Screen::resize_event(size);
  }

private:
  Label *m_status_label = nullptr;

  void create_layout() {
    // Use simple BoxLayout approach instead of AdvancedGridLayout
    // This is more reliable and easier to debug

    // Menu bar
    Window *menu_bar = new Window(this, "");
    menu_bar->set_position(Vector2i(0, 0));
    menu_bar->set_fixed_size(Vector2i(width(), 35));
    menu_bar->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 5));
    create_menu_bar(menu_bar);

    // Toolbar
    Window *toolbar = new Window(this, "");
    toolbar->set_position(Vector2i(0, 35));
    toolbar->set_fixed_size(Vector2i(width(), 45));
    toolbar->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 5));
    create_toolbar(toolbar);

    // Main content area
    int content_y = 80;
    int content_height = height() - 210; // Leave space for output and status

    // Sidebar
    Window *sidebar = new Window(this, "Solution Explorer");
    sidebar->set_position(Vector2i(0, content_y));
    sidebar->set_fixed_size(Vector2i(250, content_height));
    sidebar->set_layout(new GroupLayout(10));
    create_sidebar(sidebar);

    // Editor
    Window *editor = new Window(this, "main.cpp");
    editor->set_position(Vector2i(250, content_y));
    editor->set_fixed_size(Vector2i(width() - 530, content_height));
    editor->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 10, 10));
    create_editor(editor);

    // Properties
    Window *properties = new Window(this, "Properties");
    properties->set_position(Vector2i(width() - 280, content_y));
    properties->set_fixed_size(Vector2i(280, content_height));
    properties->set_layout(new GroupLayout(10));
    create_properties(properties);

    // Output panel
    Window *output = new Window(this, "Output");
    output->set_position(Vector2i(0, height() - 180));
    output->set_fixed_size(Vector2i(width(), 150));
    output->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));
    create_output_panel(output);

    // Status bar
    Window *status_bar = new Window(this, "");
    status_bar->set_position(Vector2i(0, height() - 30));
    status_bar->set_fixed_size(Vector2i(width(), 30));
    status_bar->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 10, 5));
    create_status_bar(status_bar);
  }

  void create_menu_bar(Widget *menu_bar) {
    // File menu
    PopupButton *file_menu = new PopupButton(menu_bar, "File");
    file_menu->set_chevron_icon(0);
    Popup *file_popup = file_menu->popup();
    file_popup->set_layout(new GroupLayout(5));
    new Button(file_popup, "New", FA_FILE);
    new Button(file_popup, "Open", FA_FOLDER_OPEN);
    new Button(file_popup, "Save", FA_SAVE);

    // Edit menu
    PopupButton *edit_menu = new PopupButton(menu_bar, "Edit");
    edit_menu->set_chevron_icon(0);
    Popup *edit_popup = edit_menu->popup();
    edit_popup->set_layout(new GroupLayout(5));
    new Button(edit_popup, "Undo", FA_UNDO);
    new Button(edit_popup, "Redo", FA_REDO);

    // View menu
    PopupButton *view_menu = new PopupButton(menu_bar, "View");
    view_menu->set_chevron_icon(0);
    Popup *view_popup = view_menu->popup();
    view_popup->set_layout(new GroupLayout(5));

    Button *light_theme = new Button(view_popup, "Light Theme");
    light_theme->set_callback([this] {
      set_theme(new FluentTheme(nvg_context(), FluentTheme::Palette::Light));
      update_status("Light theme");
    });

    Button *dark_theme = new Button(view_popup, "Dark Theme");
    dark_theme->set_callback([this] {
      set_theme(new FluentTheme(nvg_context(), FluentTheme::Palette::Dark));
      update_status("Dark theme");
    });
  }

  void create_toolbar(Widget *toolbar) {

    Button *new_btn = new Button(toolbar, "", FA_FILE);
    new_btn->set_tooltip("New");

    Button *open_btn = new Button(toolbar, "", FA_FOLDER_OPEN);
    open_btn->set_tooltip("Open");

    Button *save_btn = new Button(toolbar, "", FA_SAVE);
    save_btn->set_tooltip("Save");

    new Label(toolbar, " | ");

    Button *build_btn = new Button(toolbar, "", FA_HAMMER);
    build_btn->set_tooltip("Build");
    build_btn->set_background_color(Color(0, 120, 212, 255));
    build_btn->set_text_color(Color(255, 255, 255, 255));

    Button *run_btn = new Button(toolbar, "", FA_PLAY);
    run_btn->set_tooltip("Run");
    run_btn->set_background_color(Color(16, 124, 16, 255));
    run_btn->set_text_color(Color(255, 255, 255, 255));
  }

  void create_sidebar(Widget *sidebar) {

    new Label(sidebar, "Solution 'MyApp'", "sans-bold", 12);

    Button *project = new Button(sidebar, " MyApp", FA_FOLDER);
    project->set_flags(Button::RadioButton);

    Button *src = new Button(sidebar, "  Source Files", FA_FOLDER_OPEN);
    src->set_flags(Button::RadioButton);

    Button *main_cpp = new Button(sidebar, "    main.cpp", FA_FILE_CODE);
    main_cpp->set_flags(Button::RadioButton);

    Button *app_cpp = new Button(sidebar, "    app.cpp", FA_FILE_CODE);
    app_cpp->set_flags(Button::RadioButton);
  }

  void create_editor(Widget *editor) {

    // Tabs
    auto *tabs = new Widget(editor);
    tabs->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 5));

    Button *tab1 = new Button(tabs, "main.cpp");
    tab1->set_background_color(Color(0, 120, 212, 255));
    tab1->set_text_color(Color(255, 255, 255, 255));

    // Code editor
    TextBox *code = new TextBox(editor, "#include <iostream>\n\nint main() {\n    return 0;\n}");
    code->set_editable(true);
  }

  void create_properties(Widget *props) {

    new Label(props, "File Properties", "sans-bold", 14);

    new Label(props, "Name: main.cpp", "sans", 12);
    new Label(props, "Path: C:\\Projects\\", "sans", 12);
    new Label(props, "Size: 1.2 KB", "sans", 12);

    new Label(props, "Build Settings", "sans-bold", 14);

    CheckBox *optimize = new CheckBox(props, "Optimization");
    (void)optimize;

    ComboBox *config = new ComboBox(props, {"Debug", "Release"});
    (void)config;
  }

  void create_output_panel(Widget *output_panel) {

    new Label(output_panel, "Output", "sans-bold", 12);

    TextBox *output = new TextBox(output_panel, "Build started...\nBuild succeeded.");
    output->set_editable(false);
  }

  void create_status_bar(Widget *status_bar) {

    m_status_label = new Label(status_bar, "Ready", "sans", 12);
    new Label(status_bar, " | Ln 1, Col 1", "sans", 12);
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
      ref<VSStyleApp> app = new VSStyleApp();
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
    std::cout << "\n✓ VS-style demo completed!" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "✗ Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
