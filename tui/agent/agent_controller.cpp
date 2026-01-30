#include "agent_controller.h"
#include <flexUI/box.h> // For Event definitions if needed? Controller talks to View which takes Event. View header forward declares Event.
// We need full Event definition to construct it.
#include <flexUI/event.h> 

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace flexUI;

AgentController::AgentController() {}

AgentController::~AgentController() {
    view_.reset();
    renderer_.reset();
    term_.cleanup();
}

bool AgentController::init() {
    if (!term_.init()) return false;
    
    renderer_ = flex::tui_backend::create_renderer(term_);
    if (!renderer_) return false;

    view_ = std::make_unique<AgentView>(term_, renderer_.get());
    if (!view_->init()) return false;
    
    // Connect Model to View
    view_->set_model_reference(&model_);
    view_->sync_messages();
    view_->focus_input();
    
    input_.enable_mouse();
    return true;
}

void AgentController::run() {
    running_ = true;
    while (running_) {
        process_input();
        view_->update(16.0f);
        term_.render(); 
        
        #ifdef _WIN32
        Sleep(16);
        #else
        usleep(16000);
        #endif
    }
}

void AgentController::process_input() {
    while (auto event = input_.poll()) {
        if (handle_global_keys(event)) continue;
        
        handle_ui_event(event);
        
        if (view_->handle_key(event->key)) continue;
        
        if (event->type == tui::Event::KeyPress && event->key == tui::Key::Enter) {
            handle_submit();
        }
    }
}

bool AgentController::handle_global_keys(const std::optional<tui::Event>& event) {
    if (event->is(tui::Key::Escape)) {
        if (view_->is_selector_open()) {
            view_->hide_model_selector();
            return true;
        }
        if (view_->is_cmd_popup_open()) {
            view_->hide_command_popup();
            return true;
        }
        running_ = false;
        return true;
    }
    if ((event->is('c') && tui::has_mod(event->mod, tui::Mod::Ctrl))) {
        running_ = false;
        return true;
    }
    if (event->type == tui::Event::Resize) {
        term_.query_size();
        view_->update_viewport();
        term_.refresh();
        return true;
    }
    return false;
}

void AgentController::handle_ui_event(const std::optional<tui::Event>& event) {
    Event fe;
    fe.x = event->x * flex::tui_backend::CHAR_WIDTH + flex::tui_backend::CHAR_WIDTH/2.0f;
    fe.y = event->y * flex::tui_backend::CHAR_HEIGHT + flex::tui_backend::CHAR_HEIGHT/2.0f;
    
    bool dispatch = false;
    
    if (event->type == tui::Event::MousePress) {
        fe.type = EventType::MouseDown;
        if (event->key == tui::Key::MouseLeft) fe.button = MouseButton::Left;
        if (event->key == tui::Key::MouseWheelUp) { fe.type = EventType::MouseWheel; fe.delta_y = 20.0f; }
        if (event->key == tui::Key::MouseWheelDown) { fe.type = EventType::MouseWheel; fe.delta_y = -20.0f; }
        dispatch = true;
    }
    else if (event->type == tui::Event::MouseRelease) {
        fe.type = EventType::MouseUp;
        dispatch = true;
    }
    else if (event->type == tui::Event::MouseMove) {
        fe.type = EventType::MouseMove;
        dispatch = true;
    }
    else if (event->type == tui::Event::KeyPress) {
        dispatch_key_event(*event);
        return; 
    }
    
    if (dispatch) view_->dispatch(fe);
}

void AgentController::dispatch_key_event(const tui::Event& event) {
    KeyCode key_code = KeyCode::Unknown;
    if (event.key == tui::Key::Enter) key_code = KeyCode::Enter;
    else if (event.key == tui::Key::Backspace) key_code = KeyCode::Backspace;
    else if (event.key == tui::Key::Delete) key_code = KeyCode::Delete;
    else if (event.key == tui::Key::Tab) key_code = KeyCode::Tab;
    else if (event.key == tui::Key::Up) key_code = KeyCode::Up;
    else if (event.key == tui::Key::Down) key_code = KeyCode::Down;
    else if (event.key == tui::Key::Left) key_code = KeyCode::Left;
    else if (event.key == tui::Key::Right) key_code = KeyCode::Right;
    else if (event.key == tui::Key::Home) key_code = KeyCode::Home;
    else if (event.key == tui::Key::End) key_code = KeyCode::End;

    // Key Down
    Event kd_evt = Event::key_down(key_code);
    view_->dispatch(kd_evt);
    
    // Text Input
    if (event.ch >= 32 && event.ch != 127) {
        std::string text_utf8;
        if (event.ch < 0x80) text_utf8 += static_cast<char>(event.ch);
        else if (event.ch < 0x800) {
            text_utf8 += static_cast<char>(0xC0 | (event.ch >> 6));
            text_utf8 += static_cast<char>(0x80 | (event.ch & 0x3F));
        }
        if (!text_utf8.empty()) {
            Event ti_evt = Event::text_input(text_utf8);
            view_->dispatch(ti_evt);
        }
    }
}

void AgentController::handle_submit() {
    view_->hide_command_popup();
    std::string input_text = view_->consume_input();
    if (input_text.empty()) return;

    if (input_text[0] == '/') {
        process_command(input_text);
    } else {
        model_.add_user_message(input_text);
        model_.generate_response(input_text);
    }
    view_->sync_messages();
}

void AgentController::process_command(const std::string& cmd) {
    if (cmd == "/quit" || cmd == "/exit") {
        running_ = false;
    } else if (cmd == "/clear") {
        model_.clear_history();
    } else if (cmd == "/help") {
        model_.add_system_message("Available commands:\n  /help  - Show help\n  /clear - Clear chat\n  /quit  - Exit\n  /model - Switch AI model");
    } else if (cmd == "/model" || cmd == "/modle") {
        view_->show_model_selector();
    } else {
        model_.add_system_message("Unknown command: " + cmd);
    }
}
