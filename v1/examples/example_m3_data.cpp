/*
    M3 Data Display Example
    Demonstrates: DataTable, Autocomplete, DatePicker, TimePicker
*/

#include <iostream>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/m3.h>
#include <nanogui/screen.h>

using namespace nanogui;

int main(int argc, char **argv) {
  nanogui::init();

  {
    Screen *screen = new Screen(Vector2i(1200, 800), "M3 Data Display Example");
    auto *theme =
        new M3Theme(screen->nvg_context(), Color(0.4f, 0.2f, 0.8f, 1.0f));
    screen->set_theme(theme);

    // Top App Bar
    auto *appbar =
        new M3TopAppBar(screen, "Data Display", M3TopAppBar::Type::Small);
    appbar->set_leading_icon(0xf0c9);

    // Main content
    Widget *content = new Widget(screen);
    content->set_layout(
        new BoxLayout(Orientation::Vertical, Alignment::Fill, 20, 20));

    // Section 1: Autocomplete
    new Label(content, "Autocomplete Search", "sans-bold", 20);

    auto *autocomplete = new M3Autocomplete(content, "Search users...");
    autocomplete->set_filter_callback([](const std::string &query) {
      std::vector<M3Autocomplete::Suggestion> results;

      if (query.length() >= 2) {
        results.emplace_back("John Doe", "john@example.com", 0xf007);
        results.emplace_back("Jane Smith", "jane@example.com", 0xf007);
        results.emplace_back("Bob Johnson", "bob@example.com", 0xf007);
        results.emplace_back("Alice Brown", "alice@example.com", 0xf007);
      }

      return results;
    });

    autocomplete->set_select_callback(
        [](const M3Autocomplete::Suggestion &item) {
          std::cout << "Selected: " << item.text << " - " << item.secondary
                    << std::endl;
        });

    // Section 2: Data Table
    new Label(content, "User Data Table", "sans-bold", 20);

    auto *table = new M3DataTable(content);
    table->set_columns({{"Name", 200, true},
                        {"Email", 250, true},
                        {"Role", 150, true},
                        {"Status", 120, false}});

    table->set_data(
        {{"John Doe", "john@example.com", "Admin", "Active"},
         {"Jane Smith", "jane@example.com", "Editor", "Active"},
         {"Bob Johnson", "bob@example.com", "Viewer", "Active"},
         {"Alice Brown", "alice@example.com", "Editor", "Inactive"},
         {"Charlie Wilson", "charlie@example.com", "Viewer", "Active"}});

    table->set_row_callback(
        [](int row) { std::cout << "Selected row: " << row << std::endl; });

    table->set_sort_callback([](int column, bool ascending) {
      std::cout << "Sort column " << column << " "
                << (ascending ? "ascending" : "descending") << std::endl;
    });

    // Section 3: Date and Time Pickers
    Widget *datetime_section = new Widget(content);
    datetime_section->set_layout(
        new BoxLayout(Orientation::Horizontal, Alignment::Fill, 10, 10));

    new Label(datetime_section, "Date & Time Selection:", "sans-bold", 16);

    auto *date_btn = new M3Button(datetime_section, "Pick Date", 0,
                                  M3Button::Style::Outlined);
    auto *time_btn = new M3Button(datetime_section, "Pick Time", 0,
                                  M3Button::Style::Outlined);

    // Date Picker
    auto *date_picker = new M3DatePicker(screen);
    date_picker->set_callback([](int year, int month, int day) {
      std::cout << "Selected date: " << year << "-" << month << "-" << day
                << std::endl;
    });
    date_btn->set_callback([date_picker]() { date_picker->show(); });

    // Time Picker
    auto *time_picker = new M3TimePicker(screen, false);
    time_picker->set_callback([](int hour, int minute) {
      std::cout << "Selected time: " << hour << ":" << minute << std::endl;
    });
    time_btn->set_callback([time_picker]() { time_picker->show(); });

    // Extended FAB
    auto *fab = new M3ExtendedFAB(screen, "Add User", 0xf234);
    fab->set_callback(
        []() { std::cout << "Add new user clicked" << std::endl; });

    screen->perform_layout();
    screen->set_visible(true);
    screen->draw_all();

    nanogui::run();
  }

  nanogui::shutdown();
  return 0;
}
