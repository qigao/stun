#include "agent_model.h"

AgentModel::AgentModel() {
    // Initialize with default/welcome content
    add_system_message("Agent CLI Initialized.");
    add_ai_message("Hello! How can I assist you with your code today?");
}

const std::vector<Message>& AgentModel::messages() const {
    return messages_;
}

void AgentModel::add_user_message(const std::string& text) {
    messages_.push_back({"User", text, false});
}

void AgentModel::add_ai_message(const std::string& text) {
    messages_.push_back({"AI", text, true});
}

void AgentModel::add_system_message(const std::string& text) {
    messages_.push_back({"System", text, false});
}

void AgentModel::clear_history() {
    messages_.clear();
    add_system_message("Chat history cleared.");
}

void AgentModel::generate_response(const std::string& user_input) {
    std::string response = "# SCROLL TEST\n\n";
    response += "You said: **" + user_input + "**\n\n";
    response += "Here is a very long code block to test internal scrolling and resizing:\n\n";
    response += "```cpp\n";
    for(int i = 0; i < 30; ++i) {
        response += "void function_" + std::to_string(i) + "() {\n";
        response += "    // This is line number " + std::to_string(i) + " in our scroll test.\n";
        response += "    printf(\"Scrolling is working! Row: " + std::to_string(i) + "\\n\");\n";
        response += "}\n\n";
    }
    response += "```\n\n";
    response += "And some more text below the code block to ensure the main chat list scrolls as well. ";
    for(int i = 0; i < 10; ++i) {
        response += "This is trailing paragraph line " + std::to_string(i) + ". ";
    }
    add_ai_message(response);
}
