/*
 * Codex TUI App
 *
 * A FlexUI + TUI implementation recreating the Codex CLI interface.
 * Features:
 * - CSS-driven layout
 * - Split chat interface
 * - Input composer
 */

#include <tui.h>
#include <flex/backends/tui/init.h>
#include <flexUI/box.h>
#include <flexUI/widgets/button_widget.h>
#include <flexUI/widgets/label_widget.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/virtualized_list_widget.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/widgets/divider_widget.h>
#include <flexUI/widgets/badge_widget.h>
#include <memory>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace flexUI;

// -----------------------------------------------------------------------------
// CSS Styling
// -----------------------------------------------------------------------------
static const char* CODEX_CSS = R"(
.root {
    background-color: #0d0d0d;
    color: #cccccc;
    width: 100%;
    height: 100%;
    display: flex;
    flex-direction: column;
}

/* --- TOP: Static Header --- */
.header {
    height: 24px; /* Fixed height header */
    background-color: #1a1a1a;
    border-bottom: 1px solid #333333;
    display: flex;
    flex-direction: row;
    align-items: center;
    justify-content: space-between;
    padding: 0 16px; 
}
.header-title {
    color: #e0e0e0;
    font-weight: bold;
}
.header-right {
    color: #666666;
    font-size: 12px;
}

/* --- MIDDLE: Scrollable Conversation Area --- */
.content {
    flex-grow: 1; /* Takes all remaining vertical space */
    display: flex;
    flex-direction: column;
    padding: 8px 16px;
    background-color: #0d0d0d;
}

/* Chat List */
.chat-list {
    flex-grow: 1;
    overflow-y: scroll; /* Enable standard scrolling */
    width: 100%;
    border: 1px solid #333333;
}

/* Message Styles */
.message-row {
    height: auto;
    padding: 10px 12px;
    margin-bottom: 12px;
    display: flex;
    flex-direction: column;
    border-radius: 6px;
}

.user-msg {
    background-color: #161616;
    border-left: 3px solid #007acc;
}

.ai-msg {
    background-color: #1e1e1e;
    border-left: 3px solid #f97316;
}

.msg-header {
    height: 20px;
    margin-bottom: 4px;
    display: flex;
    flex-direction: row;
}

.role-badge {
    font-weight: bold;
    margin-right: 8px;
    font-size: 12px;
}

.msg-content {
    color: #d4d4d4;
    margin-left: 0; 
    flex-grow: 1;
    width: 100%;
}

/* --- BOTTOM: Status + Input --- */
.footer {
    display: flex;
    flex-direction: column;
    height: 80px;
    background-color: #1a1a1a;
    border-top: 1px solid #333333;
    padding-bottom: 4px;
}

.status-bar {
    height: 20px;
    display: flex;
    flex-direction: row;
    align-items: center;
    padding: 0 16px;
    background-color: #252526;
    color: #858585;
    font-size: 11px;
}

.input-area {
    padding: 8px 16px;
    display: flex;
    flex-direction: column;
    background-color: #111111;
    width: 100%;
}

/* Input Styles */
.input-box {
    width: 100%;
    height: 32px;
    border: 1px solid #444444;
    background-color: #000000;
    color: #ffffff;
    padding: 0 8px; 
    
    --input-bg: #000000;
    --input-text: #ffffff;
    --input-placeholder: #555555;
    --input-cursor: #007acc;
    --input-selection-bg: #264f78;
    --input-border: #444444;
}

.input-hint {
    color: #555555;
    font-size: 10px;
    margin-top: 4px;
    align-self: flex-end;
}

/* Markdown Styles */
.h1 { font-size: 24px; font-weight: bold; color: #569cd6; margin-top: 12px; margin-bottom: 12px; border-bottom: 2px solid #3e3e42; }
.h2 { font-size: 20px; font-weight: bold; color: #4ec9b0; margin-top: 10px; margin-bottom: 8px; }
.h3 { font-size: 16px; font-weight: bold; color: #9cdcfe; margin-top: 8px; margin-bottom: 6px; }

.md-p { margin-bottom: 10px; color: #e0e0e0; line-height: 1.4; }
.md-strong { font-weight: bold; color: #fac863; }
.md-em { font-style: italic; color: #c594c5; }

.md-code-block {
    display: flex;
    flex-direction: column;
    background-color: #1e1e1e;
    border: 1px solid #444444;
    padding: 10px;
    margin-top: 4px;
    margin-bottom: 12px;
    font-family: "Consolas", "Courier New", monospace;
    color: #dcdcaa;
    white-space: pre-wrap;
    border-radius: 4px;
}
.md-code {
    background-color: #2d2d2d;
    padding: 2px 6px;
    border-radius: 3px;
    color: #ce9178;
    font-family: monospace;
}
.md-quote {
    border-left: 4px solid #6a9955;
    padding-left: 12px;
    color: #b5cea8;
    font-style: italic;
    margin-bottom: 12px;
    background-color: #1a2a1a;
}
.md-ul, .md-ol { margin-bottom: 10px; padding-left: 20px; }
.md-li { margin-bottom: 6px; }
)";

// -----------------------------------------------------------------------------
// Data Models
// -----------------------------------------------------------------------------
struct Message {
    std::string role; // "User" or "AI"
    std::string content;
};

// -----------------------------------------------------------------------------
// Application
// -----------------------------------------------------------------------------
class CodexTuiApp {
public:
    bool init() {
        if (!term_.init()) return false;
        
        renderer_ = flex::tui_backend::create_renderer(term_);
        if (!renderer_) return false;
        
        box_ = std::make_unique<Box>(renderer_.get());
        update_viewport();
        box_->load_css(CODEX_CSS);
        input_.enable_mouse();
        
        messages_ = {
            {"User", "Hello, I want to rebuild the Codex CLI layout."},
            {"AI", "I can help with that. The layout typically consists of a static header, a scrolling chat history in the middle, and a fixed input section at the bottom."},
            {"User", "Can you show me a code example?"},
            {"AI", "Here is a C++ snippet:\n\n```cpp\nint main() {\n    return 0;\n}\n```\n\nThis uses the new **MarkdownWidget**."},
            {"User", "Exactly. The left sidebar was unnecessary."},
            {"AI", "Agreed. I have updated the layout to use a 'Holy Grail' inspired vertical flex column:\n\n1. .header { height: fixed; }\n2. .content { flex-grow: 1; }\n3. .footer { status + input }"},
        };

        for (int i = 0; i < 20; ++i) {
            messages_.push_back({"User", "History message #" + std::to_string(i)});
            messages_.push_back({"AI", "This is a response to message #" + std::to_string(i)});
        }
        
        build_ui();
        return true;
    }

    void run() {
        running_ = true;
        while (running_) {
            process_input();
            box_->update_time(16.0f);
            box_->update();
            term_.render();
#ifdef _WIN32
            Sleep(16);
#else
            usleep(16000);
#endif
        }
    }
    
    ~CodexTuiApp() {
        box_.reset();
        renderer_.reset();
        term_.cleanup();
    }

private:
    tui::Terminal term_;
    tui::Input input_;
    std::unique_ptr<flex::Renderer> renderer_;
    std::unique_ptr<Box> box_;
    bool running_ = false;
    
    std::vector<Message> messages_;
    
    VirtualizedListWidget* chat_list_ = nullptr;
    InputWidget* main_input_ = nullptr;

    void update_viewport() {
        float vp_w = static_cast<float>(term_.width() * flex::tui_backend::CHAR_WIDTH);
        float vp_h = static_cast<float>(term_.height() * flex::tui_backend::CHAR_HEIGHT);
        box_->set_viewport(vp_w, vp_h);
    }

    void process_input() {
        while (auto event = input_.poll()) {
            if (event->is(tui::Key::Escape) || (event->is('c') && tui::has_mod(event->mod, tui::Mod::Ctrl))) {
                running_ = false;
                break;
            }
            
            if (event->type == tui::Event::Resize) {
                term_.query_size();
                update_viewport();
                term_.refresh();
                continue;
            }

            Event fe;
            fe.x = event->x * flex::tui_backend::CHAR_WIDTH + flex::tui_backend::CHAR_WIDTH/2.0f;
            fe.y = event->y * flex::tui_backend::CHAR_HEIGHT + flex::tui_backend::CHAR_HEIGHT/2.0f;
            
            if (event->type == tui::Event::MousePress) {
                fe.type = EventType::MouseDown;
                if (event->key == tui::Key::MouseLeft) fe.button = MouseButton::Left;
                if (event->key == tui::Key::MouseWheelUp) { fe.type = EventType::MouseWheel; fe.delta_y = 20.0f; }
                if (event->key == tui::Key::MouseWheelDown) { fe.type = EventType::MouseWheel; fe.delta_y = -20.0f; }
                box_->dispatch_event(fe);
            }
            else if (event->type == tui::Event::MouseRelease) {
                fe.type = EventType::MouseUp;
                box_->dispatch_event(fe);
            }
            else if (event->type == tui::Event::MouseMove) {
                fe.type = EventType::MouseMove;
                box_->dispatch_event(fe);
            }
            else if (event->type == tui::Event::KeyPress) {
                KeyCode key_code = KeyCode::Unknown;
                if (event->key == tui::Key::Enter) key_code = KeyCode::Enter;
                else if (event->key == tui::Key::Backspace) key_code = KeyCode::Backspace;
                else if (event->key == tui::Key::Delete) key_code = KeyCode::Delete;
                else if (event->key == tui::Key::Tab) key_code = KeyCode::Tab;
                else if (event->key == tui::Key::Escape) key_code = KeyCode::Escape;
                else if (event->key == tui::Key::Up) key_code = KeyCode::Up;
                else if (event->key == tui::Key::Down) key_code = KeyCode::Down;
                else if (event->key == tui::Key::Left) key_code = KeyCode::Left;
                else if (event->key == tui::Key::Right) key_code = KeyCode::Right;
                else if (event->key == tui::Key::Home) key_code = KeyCode::Home;
                else if (event->key == tui::Key::End) key_code = KeyCode::End;

                Event kd_evt = Event::key_down(key_code);
                box_->dispatch_event(kd_evt);
                
                if (event->ch >= 32 && event->ch != 127) {
                    std::string text_utf8;
                    if (event->ch < 0x80) text_utf8 += static_cast<char>(event->ch);
                    else if (event->ch < 0x800) {
                        text_utf8 += static_cast<char>(0xC0 | (event->ch >> 6));
                        text_utf8 += static_cast<char>(0x80 | (event->ch & 0x3F));
                    }
                    if (!text_utf8.empty()) {
                        Event ti_evt = Event::text_input(text_utf8);
                        box_->dispatch_event(ti_evt);
                    }
                }
                
                if (event->key == tui::Key::Enter) {
                     if (main_input_ && !main_input_->text().empty()) {
                         std::string text = main_input_->text();
                         main_input_->set_text("");

                         if (text.size() > 0 && text[0] == '/') {
                             process_command(text);
                         } else {
                             messages_.push_back({"User", text});
                             // Mock AI response
                             messages_.push_back({"AI", "I received your message: \"" + text + "\""});
                         }
                         
                         chat_list_->set_item_count(messages_.size());
                         chat_list_->refresh();
                         chat_list_->scroll_to(messages_.size() - 1);
                     }
                }
            }
        }
    }

    void process_command(const std::string& cmd) {
        if (cmd == "/quit" || cmd == "/exit") {
            running_ = false;
        } else if (cmd == "/clear") {
            messages_.clear();
            messages_.push_back({"System", "Chat history cleared."});
        } else if (cmd == "/help") {
            messages_.push_back({"System", "Available commands:\n  /help  - Show this help\n  /clear - Clear history\n  /quit  - Exit application"});
        } else {
            messages_.push_back({"System", "Unknown command: " + cmd});
        }
    }

    void build_ui() {
        auto* root = box_->create("div", "root");
        root->add_class("root");
        box_->set_root(root);
        
        auto* header = box_->create("div", "header");
        header->add_class("header");
        root->append(header);
        
        header->append(box_->create_widget<LabelWidget>("label", "", "CODEX CLI"))->add_class("header-title");
        header->append(box_->create_widget<LabelWidget>("label", "", "v1.0.0"))->add_class("header-right");
        
        auto* content = box_->create("div", "content");
        content->add_class("content");
        root->append(content);
        
        chat_list_ = new VirtualizedListWidget();
        auto* list_elem = box_->create_with_widget("div", chat_list_, "chat-list");
        list_elem->add_class("chat-list");
        content->append(list_elem);

        chat_list_->set_row_height(150);
        chat_list_->set_item_count(messages_.size());

        chat_list_->set_create_row_fn([this]() {
            auto* row = box_->create("div", "");
            row->add_class("message-row");
            
            auto* header = box_->create("div", "");
            header->add_class("msg-header");
            row->append(header);
            
            auto* badge = box_->create_widget<LabelWidget>("label", "", "ROLE");
            badge->add_class("role-badge"); 
            header->append(badge);
            
            // Use MarkdownWidget for content
            auto* content = box_->create_widget<MarkdownWidget>("div", "", "Content");
            content->add_class("msg-content");
            row->append(content);
            
            return row;
        });

        chat_list_->set_bind_row_fn([this](Element* row, int index) {
            if (index < 0 || index >= (int)messages_.size()) return;
            const auto& msg = messages_[index];
            
            // Set role-based classes
            row->remove_class("user-msg");
            row->remove_class("ai-msg");
            if (msg.role == "User") row->add_class("user-msg");
            else row->add_class("ai-msg");

            if (row->child_count() < 1) return;
            auto* header = row->child_at(0);
            if (header->child_count() < 1) return;
            
            auto* badge_elem = header->child_at(0);
            auto* badge_w = badge_elem->widget_as<LabelWidget>();
            if (badge_w) {
                badge_w->set_text(msg.role == "User" ? "USER >" : "AI >");
                if (msg.role == "User") {
                    badge_elem->computed_style->text_color = {0.3f, 0.7f, 1.0f, 1.0f};
                } else if (msg.role == "System") {
                    badge_elem->computed_style->text_color = {1.0f, 0.3f, 0.3f, 1.0f};
                } else {
                    badge_elem->computed_style->text_color = {1.0f, 0.6f, 0.2f, 1.0f};
                }
            }

            if (row->child_count() < 2) return;
            auto* content_elem = row->child_at(1);
            auto* md_w = content_elem->widget_as<MarkdownWidget>();
            if (md_w) {
                md_w->set_markdown(msg.content);
            }
        });
        
        auto* footer = box_->create("div", "footer");
        footer->add_class("footer");
        root->append(footer);
        
        auto* status_bar = box_->create("div", "status-bar");
        status_bar->add_class("status-bar");
        status_bar->append(box_->create_widget<LabelWidget>("label", "", "○ Context: 4096 tokens  |  Model: gpt-4o"));
        footer->append(status_bar);
        
        auto* input_area = box_->create("div", "input-area");
        input_area->add_class("input-area");
        footer->append(input_area);
        
        auto* input_wrapper = box_->create_widget<InputWidget>("input", "main-input", "Send a message...");
        input_wrapper->add_class("input-box");
        main_input_ = dynamic_cast<InputWidget*>(input_wrapper->widget);
        input_area->append(input_wrapper);

        input_area->append(box_->create_widget<LabelWidget>("label", "", "Return to send  |  Ctrl+C to quit"))->add_class("input-hint");

        box_->set_focus(input_wrapper);
    }
};

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    CodexTuiApp app;
    if (app.init()) app.run();
    return 0;
}
