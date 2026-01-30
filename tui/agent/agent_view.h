#pragma once
#include <tui.h>
#include <flex/backends/tui/init.h> // For Renderer
#include <memory>
#include <string>

#include "agent_model.h"

// Forward decls
namespace flexUI {
    class Box;
    class Renderer;
    class VirtualizedListWidget;
    class InputWidget;
    struct Event;
    class Element;
}

class AgentView {
public:
    AgentView(tui::Terminal& term, flex::Renderer* renderer);
    ~AgentView();

    bool init();
    void set_model_reference(AgentModel* model);
    
    void update(float dt);
    void dispatch(flexUI::Event& evt);
    bool handle_key(tui::Key key);
    void update_viewport();
    
    std::string consume_input();
    void sync_messages();
    void focus_input();
    void show_model_selector();
    void hide_model_selector();
    void hide_command_popup();
    bool is_selector_open() const;
    bool is_cmd_popup_open() const;

private:
    tui::Terminal& term_;
    flex::Renderer* renderer_;
    std::unique_ptr<flexUI::Box> box_;
    AgentModel* model_ = nullptr;

    // Widget References
    flexUI::VirtualizedListWidget* chat_list_ = nullptr;
    flexUI::InputWidget* main_input_ = nullptr;
    flexUI::Element* input_wrapper_ = nullptr;
    flexUI::Element* selector_overlay_ = nullptr;
    flexUI::Element* cmd_popup_ = nullptr;
    flexUI::Element* cmd_container_ = nullptr;
    int model_selected_index_ = 0;
    int cmd_selected_index_ = 0;
    std::vector<flexUI::Element*> model_options_;
    std::vector<flexUI::Element*> cmd_options_;

    const std::vector<std::pair<std::string, std::string>> commands_ = {
        {"/help", "Show available commands"},
        {"/clear", "Clear chat history"},
        {"/quit", "Exit application"},
        {"/model", "Switch AI model"}
    };
    
    void build_ui();
    void create_model_selector(flexUI::Element* parent);
    void create_command_popup(flexUI::Element* parent);
    void handle_input_change(const std::string& text);
};
