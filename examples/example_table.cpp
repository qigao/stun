// Example usage of Fluent Web Table components

#include <nanogui.h>
#include <nanogui/fluent_web.h>

using namespace nanogui;

void create_table_example(Widget *parent) {
  // Table with sortable columns
  auto *table = new FluentWebTableView(parent);

  table->add_column({"Name", 2.0f, 120, FluentWebTableView::TextAlign::Start, true});
  table->add_column({"Email", 3.0f, 180, FluentWebTableView::TextAlign::Start, true});
  table->add_column({"Status", 1.0f, 100, FluentWebTableView::TextAlign::Center, true});
  table->add_column({"Score", 1.0f, 80, FluentWebTableView::TextAlign::End, true});

  table->add_row({"Alice Johnson", "alice@example.com", "Active", "95"},
                 FluentWebTableView::RowState::Success);
  table->add_row({"Bob Smith", "bob@example.com", "Pending", "78"},
                 FluentWebTableView::RowState::Warning);
  table->add_row({"Carol White", "carol@example.com", "Error", "42"},
                 FluentWebTableView::RowState::Error);

  table->set_selection_mode(FluentWebTableView::SelectionMode::Multiple);
  table->set_selection_callback([](const std::vector<int> &selected) {
    printf("Selected rows: ");
    for (int idx : selected)
      printf("%d ", idx);
    printf("\n");
  });

  table->set_sort_callback([](int column, bool ascending) {
    printf("Sort column %d %s\n", column, ascending ? "ascending" : "descending");
  });
}

void create_datagrid_example(Widget *parent) {
  // DataGrid with zebra striping
  auto *grid = new FluentWebDataGrid(parent);

  grid->add_column({"Product", 2.0f});
  grid->add_column({"Price", 1.0f, 100, FluentWebTableView::TextAlign::End});
  grid->add_column({"Stock", 1.0f, 80, FluentWebTableView::TextAlign::Center});

  for (int i = 0; i < 20; ++i) {
    grid->add_row({"Product " + std::to_string(i + 1), "$" + std::to_string((i + 1) * 10),
                   std::to_string((i + 1) * 5)});
  }
}

void create_breadcrumb_example(Widget *parent) {
  auto *breadcrumb = new FluentWebBreadcrumb(parent);

  breadcrumb->add_item("Home", 0xF015);      // home icon
  breadcrumb->add_item("Documents", 0xF07B); // folder icon
  breadcrumb->add_item("Projects", 0xF07B);
  breadcrumb->add_item("Current File", 0, false); // not clickable

  breadcrumb->set_item_callback([](int index) { printf("Breadcrumb clicked: %d\n", index); });
}

void create_toolbar_example(Widget *parent) {
  auto *toolbar = new FluentWebToolbar(parent);

  toolbar->add_action("New", FA_PLUS, []() { printf("New clicked\n"); }, true, true);
  toolbar->add_action("Open", FA_FOLDER_OPEN, []() { printf("Open clicked\n"); });
  toolbar->add_action("Save", FA_SAVE, []() { printf("Save clicked\n"); });
  toolbar->add_separator();
  toolbar->add_action("Cut", FA_CUT, []() { printf("Cut clicked\n"); });
  toolbar->add_action("Copy", FA_COPY, []() { printf("Copy clicked\n"); });
  toolbar->add_action("Paste", FA_PASTE, []() { printf("Paste clicked\n"); });
}

void create_rating_example(Widget *parent) {
  auto *rating = new FluentWebRating(parent, 5);

  rating->set_rating(3);
  rating->set_editable(true);
  rating->set_callback([](int stars) { printf("Rating changed to: %d stars\n", stars); });
}

void create_persona_example(Widget *parent) {
  auto *persona = new FluentWebPersona(parent, "Jane Doe", "Software Engineer");

  persona->set_initials("JD");
  persona->set_size(FluentWebPersona::Size::Medium);
  persona->set_presence(FluentWebPersona::Presence::Available);
  persona->set_clickable(true);
  persona->set_callback([]() { printf("Persona clicked\n"); });
}

int main(int argc, char **argv) {
  nanogui::init();

  {
    auto *screen = new Screen({1400, 900}, "Fluent Web Components Example");
    screen->set_theme(new FluentWebTheme(screen->nvg_context()));

    // Single window with all components
    auto *window = new Window(screen, "Fluent Web Components Demo");
    window->set_position({15, 15});
    window->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 15, 15));

    create_toolbar_example(window);
    create_breadcrumb_example(window);
    create_table_example(window);
    create_rating_example(window);
    create_persona_example(window);

    screen->set_visible(true);
    screen->perform_layout();
    screen->draw_all();

#if defined(NANOGUI_USE_SDL3)
    while (screen->process_events()) {
      screen->draw_all();
    }
#else
    nanogui::run();
#endif
  }

  nanogui::shutdown();
  return 0;
}
