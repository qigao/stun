#pragma once
#include <vector>
#include <string>

struct Message {
    std::string role; 
    std::string content;
    bool is_markdown = false;
};

class AgentModel {
public:
    AgentModel();

    const std::vector<Message>& messages() const;

    void add_user_message(const std::string& text);
    void add_ai_message(const std::string& text);
    void add_system_message(const std::string& text);
    
    void clear_history();

    // Mock AI response generation
    void generate_response(const std::string& user_input);

    void set_model_name(const std::string& name) { model_name_ = name; }
    const std::string& model_name() const { return model_name_; }

private:
    std::vector<Message> messages_;
    std::string model_name_ = "gpt-4o";
};
