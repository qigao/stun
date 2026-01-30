#pragma once
#include <tui.h>
#include <flex/backends/tui/init.h> 
#include <memory>
#include <optional>
#include "agent_model.h"
#include "agent_view.h"

class AgentController {
public:
    AgentController();
    ~AgentController();
    
    bool init();
    void run();

private:
    tui::Terminal term_;
    tui::Input input_;
    std::unique_ptr<flex::Renderer> renderer_;
    
    AgentModel model_;
    std::unique_ptr<AgentView> view_;
    
    bool running_ = false;

    void process_input();
    bool handle_global_keys(const std::optional<tui::Event>& event);
    void handle_ui_event(const std::optional<tui::Event>& event);
    void dispatch_key_event(const tui::Event& event);
    void handle_submit();
    void process_command(const std::string& cmd);
};
