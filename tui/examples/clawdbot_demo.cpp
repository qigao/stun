/*
 * ClawdBot TUI Demo - Claude Code Style
 * A modern AI Chat Interface running in the terminal.
 */

#include <tui.h>
#include <flex/backends/tui/init.h>
#include <flexUI/box.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#else
#include <unistd.h>
#endif

using namespace flexUI;

// Modern Claude-Code inspired Theme
// Dark, minimal, utilizing space efficiently with subtle orange/earth tones.
static const char* APP_CSS = R"(
.root {
    display: flex;
    flex-direction: row;
    background-color: #111111;
    color: #e6e6e6;
    width: 100%;
    height: 100%;
    font-family: sans-serif;
}

/* Sidebar: Project Context & Navigation */
.sidebar {
    width: 260px;
    min-width: 260px;
    background-color: #191919;
    border-right: 1px solid #333333;
    padding: 16px;
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
}

.brand-section {
    margin-bottom: 24px;
    border-bottom: 1px solid #333333;
    padding-bottom: 16px;
}

.logo {
    font-size: 18px;
    font-weight: bold;
    color: #d4a276; /* Warm Claude-like accent */
    margin-bottom: 4px;
}

.sub-logo {
    font-size: 12px;
    color: #666666;
}

.section-label {
    color: #888888;
    font-size: 12px;
    margin-top: 16px;
    margin-bottom: 8px;
    text-transform: uppercase;
    letter-spacing: 1px;
}

.nav-item {
    padding: 8px 12px;
    margin-bottom: 4px;
    color: #bbbbbb;
    border-radius: 4px;
    display: flex;
    flex-direction: row;
    justify-content: space-between;
}
.nav-item:hover {
    background-color: #262626;
    color: #ffffff;
}
.nav-item-active {
    background-color: #2a2a2a;
    color: #d4a276;
    border-left: 2px solid #d4a276;
}

.shortcut-hint {
    color: #444444;
    font-size: 10px;
}

/* Main Content */
.main {
    flex: 1;
    display: flex;
    flex-direction: column;
    background-color: #0f0f0f;
}

.header {
    height: 50px;
    border-bottom: 1px solid #333333;
    display: flex;
    flex-direction: row;
    align-items: center;
    padding: 0 24px;
    background-color: #141414;
    justify-content: space-between;
}

.header-title {
    color: #e6e6e6;
    font-weight: bold;
}

.status-badge {
    background-color: #262626;
    color: #a0a0a0;
    padding: 4px 8px;
    border-radius: 4px;
    font-size: 12px;
}

/* Chat Area */
.chat-area {
    flex: 1;
    padding: 24px;
    background-color: #0f0f0f;
}

.msg-row {
    display: flex;
    flex-direction: column;
    margin-bottom: 20px;
}
.user-row { align-items: flex-end; }
.ai-row { align-items: flex-start; }

.msg-header {
    margin-bottom: 4px;
    display: flex;
    flex-direction: row;
    align-items: center;
}
.user-row .msg-header { justify-content: flex-end; }

.sender-name {
    font-size: 11px;
    color: #555555;
    margin-right: 8px;
    font-weight: bold;
}

.msg-bubble {
    padding: 12px 16px;
    max-width: 600px;
    line-height: 1.4;
}
.user-bubble { 
    background-color: #2a2a2a; 
    color: #ffffff;
    border-radius: 12px 12px 2px 12px;
    border: 1px solid #444444;
}
.ai-bubble { 
    background-color: transparent; 
    border-left: 2px solid #d4a276;
    padding-left: 16px;
    color: #dddddd;
    border-radius: 0;
}

/* Input Area */
.input-area {
    min-height: 80px; 
    border-top: 1px solid #333333;
    padding: 16px 24px;
    display: flex;
    flex-direction: column;
    background-color: #141414;
}

.input-container {
    display: flex;
    flex-direction: row;
    align-items: center;
    background-color: #1a1a1a;
    border: 1px solid #333333;
    border-radius: 8px;
    padding: 4px 12px;
}
.input-container:focus-within {
    border-color: #d4a276;
}

.prompt-icon {
    color: #d4a276;
    font-weight: bold;
    margin-right: 12px;
    font-size: 16px;
}

.input-box {
    flex: 1;
    height: 48px; 
    background-color: transparent;
    color: #ffffff;
    border: none;
    padding: 12px 0;
}

.action-bar {
    display: flex;
    flex-direction: row;
    justify-content: flex-end;
    margin-top: 8px;
}

.send-btn {
    --bg: #d4a276;
    --text: #111111;
    padding: 4px 16px;
    height: 32px;
    border-radius: 4px;
    font-weight: bold;
}
.send-btn:hover {
    --bg: #e5b387;
}
)";

struct Message {
    std::string sender;
    std::string text;
    bool is_user;
};

class ClawdBotDemo {
public:
    ClawdBotDemo() {}
    ~ClawdBotDemo() {
        #ifdef _WIN32
        timeEndPeriod(1);
        #endif
    }

    bool init() {
        debug_log_.open("clawdbot_debug.log", std::ios::trunc);
        debug_log_ << "ClawdBot init start" << std::endl;

        #ifdef _WIN32
        timeBeginPeriod(1);
        #endif

        if (!term_.init()) return false;
        
        // Setup renderer
        renderer_ = flex::tui_backend::create_renderer(term_);
        if (!renderer_) return false;
        
        box_ = std::make_unique<Box>(renderer_.get());
        update_viewport();
        
        box_->load_css(APP_CSS);
        input_.enable_mouse();

        // Initial messages to match the persona
        messages_.push_back({"Clawd", "Hello. I'm Clawd, your coding assistant. How can I help you regarding your C++ project today?", false});
        
        build_ui();
        return true;
    }

    void run() {
        running_ = true;
        auto last_ticks = std::chrono::steady_clock::now();

        while (running_) {
            bool changed = false;
            while (auto event = input_.poll()) {
                changed = true;
                if (event->is(tui::Key::Escape)) { running_ = false; break; }
                
                // Handle Resize
                if (event->type == tui::Event::Resize) {
                    term_.query_size();
                    update_viewport();
                    term_.refresh();
                    continue;
                }
                
                // Convert TUI event to FlexUI event
                Event fe;
                fe.x = (float)event->x * flex::tui_backend::CHAR_WIDTH + (float)flex::tui_backend::CHAR_WIDTH / 2.0f;
                fe.y = (float)event->y * flex::tui_backend::CHAR_HEIGHT + (float)flex::tui_backend::CHAR_HEIGHT / 2.0f;
                
                if (event->type == tui::Event::MousePress) {
                    fe.type = EventType::MouseDown;
                    if (event->key == tui::Key::MouseLeft) fe.button = MouseButton::Left;
                    else if (event->key == tui::Key::MouseRight) fe.button = MouseButton::Right;
                    box_->dispatch_event(fe);
                } else if (event->type == tui::Event::MouseRelease) {
                    fe.type = EventType::MouseUp;
                    box_->dispatch_event(fe);
                } else if (event->type == tui::Event::MouseMove) {
                    fe.type = EventType::MouseMove;
                    box_->dispatch_event(fe);
                } else if (event->type == tui::Event::KeyPress) {
                    // Map keys
                    KeyCode k = KeyCode::Unknown;
                    if (event->key == tui::Key::Enter) k = KeyCode::Enter;
                    else if (event->key == tui::Key::Backspace) k = KeyCode::Backspace;
                    else if (event->key == tui::Key::Tab) k = KeyCode::Tab;
                    else if (event->key == tui::Key::Space) k = KeyCode::Space;
                    else if (event->key == tui::Key::Left) k = KeyCode::Left;
                    else if (event->key == tui::Key::Right) k = KeyCode::Right;
                    else if (event->key == tui::Key::Up) k = KeyCode::Up;
                    else if (event->key == tui::Key::Down) k = KeyCode::Down;
                    else if (event->key == tui::Key::Delete) k = KeyCode::Delete;

                    int mods = 0;
                    if (tui::has_mod(event->mod, tui::Mod::Shift)) mods |= (int)KeyMod::Shift;
                    if (tui::has_mod(event->mod, tui::Mod::Ctrl)) mods |= (int)KeyMod::Control;

                    // Send on Enter
                    if (k == KeyCode::Enter && active_input_ && active_input_ == box_->focused_element()) {
                         // Prevents newline insertion if handled
                         send_message();
                    } else if (k != KeyCode::Unknown) {
                        auto ke = Event::key_down(k, mods);
                        box_->dispatch_event(ke);
                    }

                    // Text Input
                    if (event->ch >= 32) {
                        std::string encoded;
                        if (event->ch < 0x80) encoded += (char)event->ch;
                        else {
                            if (event->ch < 0x800) {
                                encoded += (char)(0xC0 | (event->ch >> 6));
                                encoded += (char)(0x80 | (event->ch & 0x3F));
                            } else if (event->ch < 0x10000) {
                                encoded += (char)(0xE0 | (event->ch >> 12));
                                encoded += (char)(0x80 | ((event->ch >> 6) & 0x3F));
                                encoded += (char)(0x80 | (event->ch & 0x3F));
                            }
                        }
                        if (!encoded.empty()) {
                            auto te = Event::text_input(encoded);
                            box_->dispatch_event(te);
                        }
                    }
                }
            }
            
            auto now = std::chrono::steady_clock::now();
            float delta = std::chrono::duration<float, std::milli>(now - last_ticks).count();
            last_ticks = now;

            box_->update_time(delta);
            box_->update();
            
            if (changed || box_->is_dirty()) {
                term_.render();
            }
            
            #ifdef _WIN32
            Sleep(1); 
            #else
            usleep(1000);
            #endif
        }
    }

private:
    tui::Terminal term_;
    tui::Input input_;
    std::unique_ptr<flex::Renderer> renderer_;
    std::unique_ptr<Box> box_;
    bool running_ = false;
    std::vector<Message> messages_;
    Element* active_input_ = nullptr;
    Element* chat_container_ = nullptr;
    // Keep track of input area to scroll to bottom if needed
    std::ofstream debug_log_;

    void update_viewport() {
        box_->set_viewport((float)term_.width() * flex::tui_backend::CHAR_WIDTH, 
                          (float)term_.height() * flex::tui_backend::CHAR_HEIGHT);
    }
    
    void send_message() {
        if (!active_input_) return;
        auto* widget = active_input_->widget_as<InputWidget>();
        if (!widget) return;
        
        std::string txt = widget->text();
        if (txt.empty()) return;
        
        widget->set_text("");
        messages_.push_back({"User", txt, true});
        
        // Simulated AI response
        std::string response = "I'm analyzing your request regarding '" + txt + "'...";
        messages_.push_back({"Clawd", response, false});
        
        refresh_chat();
    }
    
    void refresh_chat() {
        if (!chat_container_) return;
        // Simple clear and rebuild for this demo
        while (chat_container_->child_count() > 0) {
            chat_container_->remove(chat_container_->child_at(0));
        }
        
        for (const auto& msg : messages_) {
            auto* row = box_->create("div", "");
            row->add_class("msg-row");
            row->add_class(msg.is_user ? "user-row" : "ai-row");
            
            // Header for name
            auto* header = box_->create("div", "");
            header->add_class("msg-header");
            
            auto* name = box_->create_widget<LabelWidget>("label", "", msg.sender);
            name->add_class("sender-name");
            header->append(name);
            row->append(header);
            
            // Bubble content
            auto* bubble = box_->create("div", "");
            bubble->add_class("msg-bubble");
            bubble->add_class(msg.is_user ? "user-bubble" : "ai-bubble");
            
            auto* text = box_->create_widget<LabelWidget>("label", "", msg.text);
            bubble->append(text);
            row->append(bubble);
            chat_container_->append(row);
        }
    }

    void build_ui() {
        auto* root = box_->create("div", "");
        root->add_class("root");
        box_->set_root(root);
        
        // --- Sidebar ---
        auto* sidebar = box_->create("div", "");
        sidebar->add_class("sidebar");
        root->append(sidebar);
        
        auto* brand = box_->create("div", "");
        brand->add_class("brand-section");
        sidebar->append(brand);
        
        auto* logo = box_->create_widget<LabelWidget>("label", "", "ClawdBot");
        logo->add_class("logo");
        brand->append(logo);
        
        auto* sublogo = box_->create_widget<LabelWidget>("label", "", "v2.0 (Preview)");
        sublogo->add_class("sub-logo");
        brand->append(sublogo);
        
        // Context Section
        auto* sec1 = box_->create_widget<LabelWidget>("label", "", "Context");
        sec1->add_class("section-label");
        sidebar->append(sec1);
        
        add_nav_item(sidebar, "main.cpp", true);
        add_nav_item(sidebar, "utils.h", false);
        add_nav_item(sidebar, "CMakeLists.txt", false);
        
        // Commands Section
        auto* sec2 = box_->create_widget<LabelWidget>("label", "", "Tools");
        sec2->add_class("section-label");
        sidebar->append(sec2);
        
        add_nav_item(sidebar, "/bash", false, "Ctrl+B");
        add_nav_item(sidebar, "/architect", false);
        
        // --- Main Area ---
        auto* main = box_->create("div", "");
        main->add_class("main");
        root->append(main);
        
        // Header
        auto* header = box_->create("div", "");
        header->add_class("header");
        main->append(header);
        
        auto* title = box_->create_widget<LabelWidget>("label", "", "clawdbot_demo.cpp - Refactoring");
        title->add_class("header-title");
        header->append(title);
        
        auto* badge = box_->create_widget<LabelWidget>("label", "", "Connected");
        badge->add_class("status-badge");
        header->append(badge);
        
        // Chat
        chat_container_ = box_->create("div", "");
        chat_container_->add_class("chat-area");
        main->append(chat_container_);
        refresh_chat();
        
        // Input
        auto* input_area = box_->create("div", "");
        input_area->add_class("input-area");
        main->append(input_area);
        
        auto* input_container = box_->create("div", "");
        input_container->add_class("input-container");
        input_area->append(input_container);
        
        auto* prompt = box_->create_widget<LabelWidget>("label", "", ">");
        prompt->add_class("prompt-icon");
        input_container->append(prompt);
        
        active_input_ = box_->create_widget<InputWidget>("input", "msg-input", "Type a message or /command...");
        active_input_->add_class("input-box");
        active_input_->focusable = true;
        input_container->append(active_input_);
        
        auto* action_bar = box_->create("div", "");
        action_bar->add_class("action-bar");
        input_area->append(action_bar);
        
        auto* send_btn = box_->create_widget<ButtonWidget>("button", "send-btn", "Submit Request");
        send_btn->add_class("send-btn");
        send_btn->on_click([this](){ send_message(); });
        action_bar->append(send_btn);
        
        box_->set_focus(active_input_);
        
        debug_log_ << "UI built" << std::endl;
    }
    
    void add_nav_item(Element* parent, const std::string& text, bool active, const std::string& hint = "") {
        auto* item = box_->create("div", "");
        item->add_class("nav-item");
        if (active) item->add_class("nav-item-active");
        
        auto* lbl = box_->create_widget<LabelWidget>("label", "", text);
        item->append(lbl);
        
        if (!hint.empty()) {
            auto* h = box_->create_widget<LabelWidget>("label", "", hint);
            h->add_class("shortcut-hint");
            item->append(h);
        }
        
        parent->append(item);
    }
};

int main() {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif
    ClawdBotDemo app;
    if (app.init()) app.run();
    return 0;
}
