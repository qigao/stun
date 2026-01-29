/*
 * ClawdBot TUI Demo - Polished Version
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

// Retro-Future AI Theme
static const char* APP_CSS = R"(
.root {
    display: flex;
    flex-direction: row;
    background-color: #0d1117;
    color: #c9d1d9;
    width: 100%;
    height: 100%;
}

.sidebar {
    width: 200px;
    min-width: 200px;
    background-color: #161b22;
    border-right: 1px solid #30363d;
    padding: 16px;
    display: flex;
    flex-direction: column;
    flex-shrink: 0;
}

.logo {
    font-size: 16px;
    font-weight: bold;
    color: #58a6ff;
    margin-bottom: 32px;
}

.nav-item {
    padding: 8px;
    margin-bottom: 8px;
    color: #8b949e;
}
.nav-item-active {
    color: #f0f6fc;
}

.new-chat-btn {
    --bg: #238636;
    --text: #ffffff;
    height: 32px;
    margin-bottom: 24px;
    border-radius: 4px;
}

.main {
    flex: 1;
    display: flex;
    flex-direction: column;
}

.header {
    height: 48px;
    border-bottom: 1px solid #30363d;
    display: flex;
    flex-direction: row;
    align-items: center;
    padding: 0 16px;
    background-color: #0d1117;
}

.chat-area {
    flex: 1;
    padding: 16px;
    background-color: #0d1117;
}

.msg-row {
    display: flex;
    flex-direction: column;
    margin-bottom: 16px;
}
.user-row { align-items: flex-end; }
.ai-row { align-items: flex-start; }

.msg-bubble {
    padding: 8px 12px;
    border-radius: 4px;
    max-width: 500px;
}
.user-bubble { background-color: #1f6feb; color: #ffffff; }
.ai-bubble { background-color: #21262d; border: 1px solid #30363d; }

.input-area {
    height: 112px; 
    border-top: 1px solid #30363d;
    padding: 16px;
    display: flex;
    flex-direction: row;
    align-items: center;
    background-color: #0d1117;
}

.input-box {
    --input-bg: #010409;
    --input-border: #30363d;
    --input-text: #c9d1d9;
    --input-placeholder: #8b949e;
    --input-cursor: #58a6ff;
    flex: 1;
    height: 80px; 
    padding: 8px 12px;
    background-color: #010409;
    margin-right: 16px;
    border: 1px solid #30363d;
}

.send-btn {
    --bg: #1f6feb;
    --text: #ffffff;
    width: 80px;
    height: 80px;
    border-radius: 4px;
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
        renderer_ = flex::tui_backend::create_renderer(term_);
        if (!renderer_) return false;
        
        box_ = std::make_unique<Box>(renderer_.get());
        update_viewport();
        
        box_->load_css(APP_CSS);
        input_.enable_mouse();

        messages_.push_back({"Clawd", "Hello! I am ClawdBot, your AI code assistant.", false});
        
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
                
                if (event->type == tui::Event::Resize) {
                    term_.query_size();
                    update_viewport();
                    term_.refresh();
                    continue;
                }
                
                Event fe;
                fe.x = (float)event->x * flex::tui_backend::CHAR_WIDTH + (float)flex::tui_backend::CHAR_WIDTH / 2.0f;
                fe.y = (float)event->y * flex::tui_backend::CHAR_HEIGHT + (float)flex::tui_backend::CHAR_HEIGHT / 2.0f;
                
                if (event->type == tui::Event::MousePress) {
                    fe.type = EventType::MouseDown;
                    if (event->key == tui::Key::MouseLeft) fe.button = MouseButton::Left;
                    else if (event->key == tui::Key::MouseRight) fe.button = MouseButton::Right;
                    box_->dispatch_event(fe);
                    debug_log_ << "MousePress at " << event->x << "," << event->y << " Focus: " << (box_->focused_element() ? "Yes" : "No") << std::endl;
                } else if (event->type == tui::Event::MouseRelease) {
                    fe.type = EventType::MouseUp;
                    box_->dispatch_event(fe);
                } else if (event->type == tui::Event::MouseMove) {
                    fe.type = EventType::MouseMove;
                    box_->dispatch_event(fe);
                } else if (event->type == tui::Event::KeyPress) {
                    KeyCode k = KeyCode::Unknown;
                    if (event->key == tui::Key::Enter) k = KeyCode::Enter;
                    else if (event->key == tui::Key::Backspace) k = KeyCode::Backspace;
                    else if (event->key == tui::Key::Tab) k = KeyCode::Tab;
                    else if (event->key == tui::Key::Space) k = KeyCode::Space;
                    else if (event->key == tui::Key::Left) k = KeyCode::Left;
                    else if (event->key == tui::Key::Right) k = KeyCode::Right;
                    else if (event->key == tui::Key::Up) k = KeyCode::Up;
                    else if (event->key == tui::Key::Down) k = KeyCode::Down;
                    else if (event->key == tui::Key::Home) k = KeyCode::Home;
                    else if (event->key == tui::Key::End) k = KeyCode::End;
                    else if (event->key == tui::Key::Delete) k = KeyCode::Delete;

                    int mods = 0;
                    if (tui::has_mod(event->mod, tui::Mod::Shift)) mods |= (int)KeyMod::Shift;
                    if (tui::has_mod(event->mod, tui::Mod::Ctrl)) mods |= (int)KeyMod::Control;

                    if (k == KeyCode::Enter && active_input_ && active_input_ == box_->focused_element()) {
                        send_message();
                    } else if (k != KeyCode::Unknown) {
                        auto ke = Event::key_down(k, mods);
                        box_->dispatch_event(ke);
                    }

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
        messages_.push_back({"Clawd", "I've received your request!", false});
        
        refresh_chat();
    }
    
    void refresh_chat() {
        if (!chat_container_) return;
        while (chat_container_->child_count() > 0) {
            chat_container_->remove(chat_container_->child_at(0));
        }
        
        for (const auto& msg : messages_) {
            auto* row = box_->create("div", "");
            row->add_class("msg-row");
            row->add_class(msg.is_user ? "user-row" : "ai-row");
            
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
        
        auto* sidebar = box_->create("div", "");
        sidebar->add_class("sidebar");
        root->append(sidebar);
        
        auto* logo = box_->create_widget<LabelWidget>("label", "logo", "ClawdBot \xF0\x9F\xA4\x96");
        logo->add_class("logo");
        sidebar->append(logo);
        
        auto* new_btn = box_->create_widget<ButtonWidget>("button", "new-btn", "+ New Chat");
        new_btn->add_class("new-chat-btn");
        new_btn->on_click([this](){ messages_.clear(); refresh_chat(); });
        sidebar->append(new_btn);
        
        auto* nav1 = box_->create_widget<LabelWidget>("label", "", "Recent Chat");
        nav1->add_class("nav-item");
        nav1->add_class("nav-item-active");
        sidebar->append(nav1);

        auto* nav2 = box_->create_widget<LabelWidget>("label", "", "Project Plan");
        nav2->add_class("nav-item");
        sidebar->append(nav2);
        
        auto* main = box_->create("div", "");
        main->add_class("main");
        root->append(main);
        
        auto* header = box_->create("div", "");
        header->add_class("header");
        main->append(header);
        header->append(box_->create_widget<LabelWidget>("label", "", "Chat Session"));
        
        chat_container_ = box_->create("div", "");
        chat_container_->add_class("chat-area");
        main->append(chat_container_);
        refresh_chat();
        
        auto* input_area = box_->create("div", "");
        input_area->add_class("input-area");
        main->append(input_area);
        
        active_input_ = box_->create_widget<InputWidget>("input", "msg-input", "Message...");
        active_input_->add_class("input-box");
        active_input_->focusable = true;
        input_area->append(active_input_);
        
        auto* send_btn = box_->create_widget<ButtonWidget>("button", "send-btn", "Send");
        send_btn->add_class("send-btn");
        send_btn->on_click([this](){ send_message(); });
        input_area->append(send_btn);
        
        box_->set_focus(active_input_);
        
        debug_log_ << "UI built" << std::endl;
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
