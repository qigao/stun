/*
 * TUI Widgets Demo - Showcases all built-in widgets
 */

#include <tui.h>
#include <string>
#include <cmath>

using namespace tui;
using namespace tui::widgets;

struct Model {
    float progress = 0.0f;
    float slider = 0.5f;
    bool check1 = true;
    bool check2 = false;
    bool toggle1 = true;
    bool toggle2 = false;
    int spinner_frame = 0;
    int selected_button = 0;
};

enum class Msg { 
    Quit, Tick,
    ToggleCheck1, ToggleCheck2,
    ToggleSwitch1, ToggleSwitch2,
    SliderUp, SliderDown,
    NextButton, PrevButton
};

bool update(Msg msg, Model& m) {
    switch (msg) {
        case Msg::Quit: return false;
        case Msg::Tick:
            m.progress = std::fmod(m.progress + 0.01f, 1.0f);
            m.spinner_frame++;
            break;
        case Msg::ToggleCheck1: m.check1 = !m.check1; break;
        case Msg::ToggleCheck2: m.check2 = !m.check2; break;
        case Msg::ToggleSwitch1: m.toggle1 = !m.toggle1; break;
        case Msg::ToggleSwitch2: m.toggle2 = !m.toggle2; break;
        case Msg::SliderUp: m.slider = std::min(1.0f, m.slider + 0.1f); break;
        case Msg::SliderDown: m.slider = std::max(0.0f, m.slider - 0.1f); break;
        case Msg::NextButton: m.selected_button = (m.selected_button + 1) % 3; break;
        case Msg::PrevButton: m.selected_button = (m.selected_button + 2) % 3; break;
    }
    return true;
}

void view(const Model& m, Buffer& buf) {
    Theme theme = default_theme();
    buf.clear(theme.bg);
    
    int w = buf.width();
    int h = buf.height();
    
    // Title
    buf.text(2, 1, "TUI Widgets Demo", Color::cyan(), theme.bg);
    buf.text(2, 2, "q:quit  1/2:checkbox  3/4:toggle  </> slider  Tab:button", 
             Color::gray(), theme.bg);
    
    divider(buf, 0, 3, w, theme);
    
    // Left column
    int col1_x = 2;
    int y = 5;
    
    // Progress bars
    panel(buf, col1_x, y, 30, 6, "Progress", theme);
    progress_bar(buf, col1_x + 2, y + 2, 26, m.progress, theme);
    progress_bar(buf, col1_x + 2, y + 4, 26, 0.75f, "75%", theme);
    y += 7;
    
    // Checkboxes
    panel(buf, col1_x, y, 30, 5, "Checkboxes", theme);
    checkbox(buf, col1_x + 2, y + 2, m.check1, "Option 1 (press 1)", theme);
    checkbox(buf, col1_x + 2, y + 3, m.check2, "Option 2 (press 2)", theme);
    y += 6;
    
    // Buttons
    panel(buf, col1_x, y, 30, 4, "Buttons", theme);
    button(buf, col1_x + 2, y + 2, 8, "OK", m.selected_button == 0, false, theme);
    button(buf, col1_x + 11, y + 2, 8, "Cancel", m.selected_button == 1, false, theme);
    button(buf, col1_x + 20, y + 2, 8, "Help", m.selected_button == 2, false, theme);
    y += 5;
    
    // Right column
    int col2_x = 35;
    y = 5;
    
    // Slider
    panel(buf, col2_x, y, 30, 5, "Slider", theme);
    slider(buf, col2_x + 2, y + 2, 26, m.slider, theme);
    std::string slider_val = std::to_string(static_cast<int>(m.slider * 100)) + "%";
    buf.text(col2_x + 2, y + 3, slider_val, theme.fg, theme.bg);
    y += 6;
    
    // Toggles
    panel(buf, col2_x, y, 30, 5, "Toggles", theme);
    toggle(buf, col2_x + 2, y + 2, m.toggle1, theme);
    buf.text(col2_x + 8, y + 2, "WiFi (press 3)", theme.fg, theme.bg);
    toggle(buf, col2_x + 2, y + 3, m.toggle2, theme);
    buf.text(col2_x + 8, y + 3, "Bluetooth (press 4)", theme.fg, theme.bg);
    y += 6;
    
    // Spinner & Badges
    panel(buf, col2_x, y, 30, 4, "Spinner & Badges", theme);
    spinner(buf, col2_x + 2, y + 2, m.spinner_frame, theme);
    buf.text(col2_x + 4, y + 2, "Loading...", theme.fg, theme.bg);
    badge(buf, col2_x + 16, y + 2, "NEW", theme.primary, theme);
    badge(buf, col2_x + 22, y + 2, "OK", theme.success, theme);
    y += 5;
    
    // Radio buttons
    panel(buf, col2_x, y, 30, 5, "Radio Buttons", theme);
    radio(buf, col2_x + 2, y + 2, true, "Selected", theme);
    radio(buf, col2_x + 2, y + 3, false, "Not selected", theme);
    y += 6;
    
    // Box styles
    y = h - 8;
    buf.text(2, y, "Box Styles:", theme.fg, theme.bg);
    buf.box(2, y + 1, 10, 4, theme.border, theme.bg);
    buf.text(3, y + 2, "Normal", theme.fg, theme.bg);
    
    buf.box_double(14, y + 1, 10, 4, theme.border, theme.bg);
    buf.text(15, y + 2, "Double", theme.fg, theme.bg);
    
    buf.box_round(26, y + 1, 10, 4, theme.border, theme.bg);
    buf.text(27, y + 2, "Round", theme.fg, theme.bg);
    
    // Input field
    input(buf, 38, y + 2, 20, "Type here...", 12, true, theme);
    
    // Scrollbar
    scrollbar_v(buf, w - 2, 5, h - 10, m.progress, 0.3f, theme);
}

std::optional<Msg> event_map(const Event& e) {
    if (e.is('q') || e.is('Q')) return Msg::Quit;
    if (e.is('1')) return Msg::ToggleCheck1;
    if (e.is('2')) return Msg::ToggleCheck2;
    if (e.is('3')) return Msg::ToggleSwitch1;
    if (e.is('4')) return Msg::ToggleSwitch2;
    if (e.is('<') || e.is(',')) return Msg::SliderDown;
    if (e.is('>') || e.is('.')) return Msg::SliderUp;
    if (e.is(Key::Tab)) return Msg::NextButton;
    
    // Always tick for animation
    return Msg::Tick;
}

int main() {
    auto app = App<Model, Msg>::create(Model{}, update, view, event_map);
    app.run();
    return 0;
}
