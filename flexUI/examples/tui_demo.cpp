/*
 * flexUI TUI Demo - Complex UI in Terminal using tango backend
 */

#include <flexUI.h>
#include <backends/tui/init.h>
#include <tui.h>
#include <flexUI/widgets/input_widget.h>
#include "host_input_bridge.h"
#include "renderer_capability_label.h"

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

constexpr float CELL_W = 8.0f;
constexpr float CELL_H = 16.0f;

static flexUI::MouseButton tui_to_button(tui_key_t key) {
    switch (key) {
        case TUI_KEY_MOUSE_RIGHT: return flexUI::MouseButton::Right;
        case TUI_KEY_MOUSE_MIDDLE: return flexUI::MouseButton::Middle;
        default: return flexUI::MouseButton::Left;
    }
}

static flexUI::KeyCode tui_to_keycode(tui_key_t key) {
    switch (key) {
        case TUI_KEY_LEFT: return flexUI::KeyCode::Left;
        case TUI_KEY_RIGHT: return flexUI::KeyCode::Right;
        case TUI_KEY_UP: return flexUI::KeyCode::Up;
        case TUI_KEY_DOWN: return flexUI::KeyCode::Down;
        case TUI_KEY_HOME: return flexUI::KeyCode::Home;
        case TUI_KEY_END: return flexUI::KeyCode::End;
        case TUI_KEY_PAGE_UP: return flexUI::KeyCode::PageUp;
        case TUI_KEY_PAGE_DOWN: return flexUI::KeyCode::PageDown;
        case TUI_KEY_ENTER: return flexUI::KeyCode::Enter;
        case TUI_KEY_TAB: return flexUI::KeyCode::Tab;
        case TUI_KEY_ESCAPE: return flexUI::KeyCode::Escape;
        case TUI_KEY_BACKSPACE: return flexUI::KeyCode::Backspace;
        case TUI_KEY_DELETE: return flexUI::KeyCode::Delete;
        case TUI_KEY_INSERT: return flexUI::KeyCode::Insert;
        case TUI_KEY_SPACE: return flexUI::KeyCode::Space;
        default:
            if (key >= 'a' && key <= 'z') {
                return static_cast<flexUI::KeyCode>('A' + (key - 'a'));
            }
            if (key >= 'A' && key <= 'Z') {
                return static_cast<flexUI::KeyCode>(key);
            }
            if (key >= '0' && key <= '9') {
                return static_cast<flexUI::KeyCode>(key);
            }
            return flexUI::KeyCode::Unknown;
    }
}

static flexUI::KeyCode tui_event_to_keycode(const tui_event_t& event) {
    auto key = tui_to_keycode(event.key);
    if (key != flexUI::KeyCode::Unknown) {
        return key;
    }

    if ((event.mod & TUI_MOD_CTRL) && event.ch >= 'a' && event.ch <= 'z') {
        return static_cast<flexUI::KeyCode>('A' + (event.ch - 'a'));
    }
    if ((event.mod & TUI_MOD_CTRL) && event.ch >= 'A' && event.ch <= 'Z') {
        return static_cast<flexUI::KeyCode>(event.ch);
    }
    if (event.ch >= 'a' && event.ch <= 'z') {
        return static_cast<flexUI::KeyCode>('A' + (event.ch - 'a'));
    }
    if (event.ch >= 'A' && event.ch <= 'Z') {
        return static_cast<flexUI::KeyCode>(event.ch);
    }
    if (event.ch >= '0' && event.ch <= '9') {
        return static_cast<flexUI::KeyCode>(event.ch);
    }

    return flexUI::KeyCode::Unknown;
}

static int tui_to_mods(uint8_t mod) {
    int result = 0;
    if (mod & TUI_MOD_SHIFT) result |= static_cast<int>(flexUI::KeyMod::Shift);
    if (mod & TUI_MOD_CTRL) result |= static_cast<int>(flexUI::KeyMod::Control);
    if (mod & TUI_MOD_ALT) result |= static_cast<int>(flexUI::KeyMod::Alt);
    return result;
}

static std::string utf8_from_codepoint(uint32_t codepoint) {
    std::string utf8;
    if (codepoint <= 0x7F) {
        utf8.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        utf8.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        utf8.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        utf8.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return utf8;
}

static std::string backend_status_text(const flex::RendererCapabilities& caps) {
    return std::string("● TUI ") + flexui_examples::renderer_capability_label(caps);
}

static bool is_text_input_event(const tui_event_t& event) {
    return event.type == TUI_EVENT_KEY &&
           event.ch >= 32 &&
           !(event.mod & TUI_MOD_CTRL) &&
           !(event.mod & TUI_MOD_ALT);
}

const char* CSS = R"(
#root {
    width: 100%;
    height: 100%;
    background-color: #1a1b26;
    padding: 16px;
    display: flex;
    flex-direction: column;
    gap: 16px;
}

#header {
    display: flex;
    flex-direction: row;
    justify-content: space-between;
    align-items: center;
    padding: 8px 16px;
    background-color: #24283b;
    border-radius: 4px;
}

#title {
    color: #7aa2f7;
    font-size: 18px;
    font-weight: bold;
}

#status {
    color: #9ece6a;
    font-size: 12px;
}

#main {
    display: flex;
    flex-direction: row;
    flex: 1;
    gap: 16px;
}

#sidebar {
    width: 200px;
    background-color: #24283b;
    padding: 16px;
    display: flex;
    flex-direction: column;
    gap: 8px;
    border-radius: 4px;
}

.menu-item {
    padding: 8px 12px;
    background-color: #1a1b26;
    color: #a9b1d6;
    border-radius: 4px;
}

.menu-item:hover {
    background-color: #414868;
}

.menu-item.active {
    background-color: #7aa2f7;
    color: #1a1b26;
}

#content {
    flex: 1;
    background-color: #24283b;
    padding: 16px;
    display: flex;
    flex-direction: column;
    gap: 16px;
    border-radius: 4px;
}

#content-title {
    color: #bb9af7;
    font-size: 16px;
    font-weight: bold;
    border-bottom: 1px solid #414868;
    padding-bottom: 8px;
}

#command-bar {
    display: flex;
    flex-direction: row;
    gap: 8px;
    align-items: center;
}

#command-input {
    width: 320px;
    height: 36px;
}

.card {
    background-color: #1a1b26;
    padding: 12px;
    border-radius: 4px;
    display: flex;
    flex-direction: column;
    gap: 8px;
}

.card-title {
    color: #7dcfff;
    font-size: 14px;
}

.card-value {
    color: #c0caf5;
    font-size: 24px;
    font-weight: bold;
}

.card-desc {
    color: #565f89;
    font-size: 11px;
}

#stats {
    display: flex;
    flex-direction: row;
    gap: 16px;
}

.stat-card {
    flex: 1;
}

#actions {
    display: flex;
    flex-direction: row;
    gap: 8px;
    margin-top: auto;
}

.btn {
    padding: 8px 16px;
    border-radius: 4px;
    color: #c0caf5;
}

.btn-primary {
    background-color: #7aa2f7;
    color: #1a1b26;
}

.btn-secondary {
    background-color: #414868;
}

.btn-danger {
    background-color: #f7768e;
    color: #1a1b26;
}

#footer {
    display: flex;
    flex-direction: row;
    justify-content: space-between;
    padding: 8px 16px;
    background-color: #24283b;
    border-radius: 4px;
}

.footer-text {
    color: #565f89;
    font-size: 11px;
}
)";

int main() {
    tui_terminal_t* term = tui_terminal_create();
    if (!tui_terminal_init(term)) return 1;
    tui_terminal_hide_cursor(term);
    tui_terminal_enable_mouse(term);

    auto renderer = flex::tui_backend::create_renderer(term);

    int cols = tui_terminal_width(term);
    int rows = tui_terminal_height(term);
    float frame_w = cols * CELL_W;
    float frame_h = rows * CELL_H;

    flexUI::Box box(renderer.get());
    box.set_viewport(frame_w, frame_h);
    box.load_css(CSS);
    const auto caps = box.renderer_capabilities();

    // Build UI tree
    auto* root = box.create("div", "root");
    box.set_root(root);

    // Header
    auto* header = box.create("div", "header");
    root->append(header);
    
    auto* title = box.create("div", "title");
    title->widget = new flexUI::LabelWidget("flexUI Terminal Dashboard");
    header->append(title);
    
    auto* status = box.create("div", "status");
    status->widget = new flexUI::LabelWidget(backend_status_text(caps));
    header->append(status);

    // Main area
    auto* main_area = box.create("div", "main");
    root->append(main_area);

    // Sidebar
    auto* sidebar = box.create("div", "sidebar");
    main_area->append(sidebar);

    // Content (create early so we can reference content_title in menu click handlers)
    auto* content = box.create("div", "content");
    main_area->append(content);

    auto* content_title = box.create("div", "content-title");
    content_title->widget = new flexUI::LabelWidget("Dashboard View");
    content->append(content_title);

    auto* command_bar = box.create("div", "command-bar");
    auto* command_label = box.create("div", "");
    command_label->widget = new flexUI::LabelWidget("Command");
    command_bar->append(command_label);

    auto* command_input = box.create_widget<flexUI::InputWidget>("input", "command-input");
    static_cast<flexUI::InputWidget*>(command_input->widget)->set_placeholder("Type here...");
    command_bar->append(command_input);
    content->append(command_bar);

    const char* menu_items[] = {"Dashboard", "Analytics", "Reports", "Settings"};
    flexUI::Element* menu_elements[4];
    int active_menu = 0;
    
    for (int i = 0; i < 4; i++) {
        auto* item = box.create("div", "");
        item->add_class("menu-item");
        if (i == 0) item->add_class("active");
        item->widget = new flexUI::LabelWidget(menu_items[i]);
        menu_elements[i] = item;
        
        // Click handler
        int idx = i;
        item->on_click([&menu_elements, &active_menu, content_title, idx, menu_items]() {
            // Update active state
            menu_elements[active_menu]->remove_class("active");
            menu_elements[idx]->add_class("active");
            active_menu = idx;
            // Update content title
            static_cast<flexUI::LabelWidget*>(content_title->widget)->set_text(
                std::string(menu_items[idx]) + " View");
        });
        
        sidebar->append(item);
    }

    // Stats cards
    auto* stats = box.create("div", "stats");
    content->append(stats);

    struct StatData { const char* title; const char* value; const char* desc; };
    StatData stat_data[] = {
        {"CPU Usage", "42%", "4 cores active"},
        {"Memory", "8.2 GB", "of 16 GB used"},
        {"Network", "1.2 MB/s", "upload/download"},
    };

    for (auto& sd : stat_data) {
        auto* card = box.create("div", "");
        card->add_class("card");
        card->add_class("stat-card");
        
        auto* card_title = box.create("div", "");
        card_title->add_class("card-title");
        card_title->widget = new flexUI::LabelWidget(sd.title);
        card->append(card_title);
        
        auto* card_value = box.create("div", "");
        card_value->add_class("card-value");
        card_value->widget = new flexUI::LabelWidget(sd.value);
        card->append(card_value);
        
        auto* card_desc = box.create("div", "");
        card_desc->add_class("card-desc");
        card_desc->widget = new flexUI::LabelWidget(sd.desc);
        card->append(card_desc);
        
        stats->append(card);
    }

    // Action buttons
    auto* actions = box.create("div", "actions");
    content->append(actions);

    auto* btn1 = box.create("div", "");
    btn1->add_class("btn");
    btn1->add_class("btn-primary");
    btn1->widget = new flexUI::LabelWidget("Refresh");
    btn1->on_click([&status]() {
        static_cast<flexUI::LabelWidget*>(status->widget)->set_text("● Refreshing...");
    });
    actions->append(btn1);

    auto* btn2 = box.create("div", "");
    btn2->add_class("btn");
    btn2->add_class("btn-secondary");
    btn2->widget = new flexUI::LabelWidget("Export");
    btn2->on_click([&status]() {
        static_cast<flexUI::LabelWidget*>(status->widget)->set_text("● Exporting...");
    });
    actions->append(btn2);

    auto* btn3 = box.create("div", "");
    btn3->add_class("btn");
    btn3->add_class("btn-danger");
    btn3->widget = new flexUI::LabelWidget("Reset");
    btn3->on_click([&status]() {
        static_cast<flexUI::LabelWidget*>(status->widget)->set_text("● Reset!");
    });
    actions->append(btn3);

    // Footer
    auto* footer = box.create("div", "footer");
    root->append(footer);

    auto* footer_left = box.create("div", "");
    footer_left->add_class("footer-text");
    footer_left->widget = new flexUI::LabelWidget("Press 'q' or ESC to quit");
    footer->append(footer_left);

    auto* footer_right = box.create("div", "");
    footer_right->add_class("footer-text");
    footer_right->widget = new flexUI::LabelWidget("flexUI + tango v1.0");
    footer->append(footer_right);

    bool running = true;
    while (running) {
        tui_event_t event;
        while (tui_terminal_poll(term, &event)) {
            bool text_focus = flexui_examples::box_wants_text_input(&box);

            if (event.type == TUI_EVENT_KEY) {
                if (event.key == TUI_KEY_ESCAPE) {
                    running = false;
                    continue;
                }

                if (!text_focus && event.ch == 'q') {
                    running = false;
                    continue;
                }

                auto key = tui_event_to_keycode(event);
                if (key != flexUI::KeyCode::Unknown) {
                    auto e = flexUI::Event::key_down(key, tui_to_mods(event.mod));
                    box.dispatch_event(e);
                }

                if (text_focus && is_text_input_event(event)) {
                    flexui_examples::dispatch_text_input_if_focused(
                        &box, utf8_from_codepoint(event.ch));
                }
            } else if (event.type == TUI_EVENT_RESIZE) {
                tui_terminal_query_size(term);
                cols = tui_terminal_width(term);
                rows = tui_terminal_height(term);
                frame_w = cols * CELL_W;
                frame_h = rows * CELL_H;
                box.set_viewport(frame_w, frame_h);
            } else if (event.type == TUI_EVENT_MOUSE_MOVE) {
                float mx = event.x * CELL_W + CELL_W / 2;
                float my = event.y * CELL_H + CELL_H / 2;
                auto e = flexUI::Event::mouse_move(mx, my);
                box.dispatch_event(e);
            } else if (event.type == TUI_EVENT_MOUSE_PRESS) {
                float mx = event.x * CELL_W + CELL_W / 2;
                float my = event.y * CELL_H + CELL_H / 2;

                if (event.key == TUI_KEY_MOUSE_WHEEL_UP) {
                    auto e = flexUI::Event::mouse_wheel(mx, my, 0.0f, 1.0f);
                    box.dispatch_event(e);
                } else if (event.key == TUI_KEY_MOUSE_WHEEL_DOWN) {
                    auto e = flexUI::Event::mouse_wheel(mx, my, 0.0f, -1.0f);
                    box.dispatch_event(e);
                } else {
                    auto e = flexUI::Event::mouse_down(mx, my, tui_to_button(event.key));
                    box.dispatch_event(e);
                }
            } else if (event.type == TUI_EVENT_MOUSE_RELEASE) {
                float mx = event.x * CELL_W + CELL_W / 2;
                float my = event.y * CELL_H + CELL_H / 2;
                auto e = flexUI::Event::mouse_up(mx, my, tui_to_button(event.key));
                box.dispatch_event(e);
            }
        }

        box.update_time(16.0f);
        box.invalidate();
        box.update();

        tui_terminal_render(term);
        sleep_ms(16);
    }

    tui_terminal_cleanup(term);
    tui_terminal_destroy(term);
    return 0;
}
