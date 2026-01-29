/*
 * TUI Demo - Basic counter application
 */

#include <tui.h>
#include <string>

struct Model {
    int count = 0;
    int x = 10, y = 5;
};

enum class Msg { Inc, Dec, Quit, Up, Down, Left, Right };

bool update(Msg msg, Model& m) {
    switch (msg) {
        case Msg::Quit: return false;
        case Msg::Inc: m.count++; break;
        case Msg::Dec: m.count--; break;
        case Msg::Up: if (m.y > 0) m.y--; break;
        case Msg::Down: m.y++; break;
        case Msg::Left: if (m.x > 0) m.x--; break;
        case Msg::Right: m.x++; break;
    }
    return true;
}

void view(const Model& m, tui::Buffer& buf) {
    buf.clear(tui::Color{20, 20, 30});
    
    // Title
    buf.text(2, 1, "TUI Demo", tui::Color::cyan(), tui::Color{20, 20, 30});
    buf.text(2, 2, "Press q to quit, +/- to count, arrows to move", 
             tui::Color::gray(), tui::Color{20, 20, 30});
    
    // Counter box
    buf.box_round(m.x, m.y, 20, 5, tui::Color::white(), tui::Color{20, 20, 30});
    buf.text(m.x + 2, m.y + 2, "Count: " + std::to_string(m.count),
             tui::Color::yellow(), tui::Color{20, 20, 30});
    
    // Position indicator
    std::string pos = "Pos: " + std::to_string(m.x) + "," + std::to_string(m.y);
    buf.text(2, buf.height() - 2, pos, tui::Color::gray(), tui::Color{20, 20, 30});
}

std::optional<Msg> event_map(const tui::Event& e) {
    if (e.is('q') || e.is('Q')) return Msg::Quit;
    if (e.is('+') || e.is('=')) return Msg::Inc;
    if (e.is('-') || e.is('_')) return Msg::Dec;
    if (e.is(tui::Key::Up)) return Msg::Up;
    if (e.is(tui::Key::Down)) return Msg::Down;
    if (e.is(tui::Key::Left)) return Msg::Left;
    if (e.is(tui::Key::Right)) return Msg::Right;
    return std::nullopt;
}

int main() {
    auto app = tui::App<Model, Msg>::create(Model{}, update, view, event_map);
    app.run();
    return 0;
}
