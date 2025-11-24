#include <whiteboard/ddf/event_layer.h>
#include <whiteboard/ddf/data_layer.h>
#include <whiteboard/ddf/shape_layer.h>
#include <whiteboard/ddf/expression_parser.h>
#include <iostream>

namespace whiteboard {
namespace ddf {

void EventLayer::register_event(const Event& event) {
    events_[event.id] = event;
}

void EventLayer::unregister_event(const std::string& id) {
    events_.erase(id);
}

Event* EventLayer::get_event(const std::string& id) {
    auto it = events_.find(id);
    return it != events_.end() ? &it->second : nullptr;
}

const Event* EventLayer::get_event(const std::string& id) const {
    auto it = events_.find(id);
    return it != events_.end() ? &it->second : nullptr;
}

std::vector<Event*> EventLayer::get_all_events() {
    std::vector<Event*> result;
    result.reserve(events_.size());
    for (auto& [id, event] : events_) {
        result.push_back(&event);
    }
    return result;
}

std::vector<const Event*> EventLayer::get_all_events() const {
    std::vector<const Event*> result;
    result.reserve(events_.size());
    for (const auto& [id, event] : events_) {
        result.push_back(&event);
    }
    return result;
}

std::vector<Event*> EventLayer::get_events_for_target(const std::string& target_id) {
    std::vector<Event*> result;
    for (auto& [id, event] : events_) {
        if (event.target_id == target_id) {
            result.push_back(&event);
        }
    }
    return result;
}

std::vector<const Event*> EventLayer::get_events_for_target(const std::string& target_id) const {
    std::vector<const Event*> result;
    for (const auto& [id, event] : events_) {
        if (event.target_id == target_id) {
            result.push_back(&event);
        }
    }
    return result;
}

std::vector<Event*> EventLayer::get_events_for_trigger(EventTrigger trigger) {
    std::vector<Event*> result;
    for (auto& [id, event] : events_) {
        if (event.trigger == trigger) {
            result.push_back(&event);
        }
    }
    return result;
}

std::vector<const Event*> EventLayer::get_events_for_trigger(EventTrigger trigger) const {
    std::vector<const Event*> result;
    for (const auto& [id, event] : events_) {
        if (event.trigger == trigger) {
            result.push_back(&event);
        }
    }
    return result;
}

void EventLayer::handle_event(const std::string& target_id, EventTrigger trigger) {
    auto events = get_events_for_target(target_id);
    for (auto* event : events) {
        if (event->trigger == trigger) {
            // Check condition if present
            if (event->condition.has_value() && !evaluate_condition(event->condition.value())) {
                continue;
            }
            
            // Execute actions
            for (const auto& action : event->actions) {
                execute_action(action);
            }
        }
    }
}

void EventLayer::clear() {
    events_.clear();
}

void EventLayer::set_data_layer(DataLayer* data_layer) {
    data_layer_ = data_layer;
}

void EventLayer::set_shape_layer(ShapeLayer* shape_layer) {
    shape_layer_ = shape_layer;
}

void EventLayer::set_expression_parser(ExpressionParser* expression_parser) {
    expression_parser_ = expression_parser;
}

void EventLayer::execute_action(const EventAction& action) {
    switch (action.type) {
        case ActionType::UpdateData: {
            // Update data node property
            if (data_layer_ && action.parameters.count("node_id") && 
                action.parameters.count("property") && action.parameters.count("value")) {
                
                auto node_id = action.parameters.at("node_id");
                auto property = action.parameters.at("property");
                auto value = action.parameters.at("value");
                
                auto* node = data_layer_->get_node(node_id);
                if (node) {
                    node->properties[property] = value;
                }
            }
            break;
        }
        
        case ActionType::UpdateStyle: {
            // Update shape style property
            if (shape_layer_ && action.parameters.count("shape_id") && 
                action.parameters.count("property") && action.parameters.count("value")) {
                
                auto shape_id = action.parameters.at("shape_id");
                auto property = action.parameters.at("property");
                auto value = action.parameters.at("value");
                
                auto* shape = shape_layer_->get_shape(shape_id);
                if (shape) {
                    shape->inline_style[property] = value;
                }
            }
            break;
        }
        
        case ActionType::UpdateGeometry: {
            // Update shape geometry property
            if (shape_layer_ && action.parameters.count("shape_id") && 
                action.parameters.count("property") && action.parameters.count("value")) {
                
                auto shape_id = action.parameters.at("shape_id");
                auto property = action.parameters.at("property");
                auto value_str = action.parameters.at("value");
                
                auto* shape = shape_layer_->get_shape(shape_id);
                if (shape) {
                    try {
                        float value = std::stof(value_str);
                        shape->geometry[property] = value;
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to parse geometry value: " << value_str << std::endl;
                    }
                }
            }
            break;
        }
        
        case ActionType::ShowTooltip: {
            // Show tooltip (application-specific, just log for now)
            if (action.parameters.count("content")) {
                std::cout << "Tooltip: " << action.parameters.at("content") << std::endl;
            }
            break;
        }
        
        case ActionType::Navigate: {
            // Navigate to URL or view (application-specific, just log for now)
            if (action.parameters.count("target")) {
                std::cout << "Navigate to: " << action.parameters.at("target") << std::endl;
            }
            break;
        }
        
        case ActionType::ExecuteScript: {
            // Execute script (not implemented for security reasons)
            std::cerr << "ExecuteScript action is not supported" << std::endl;
            break;
        }
        
        case ActionType::EmitCustomEvent: {
            // Emit custom event (application-specific, just log for now)
            if (action.parameters.count("event_name")) {
                std::cout << "Custom event: " << action.parameters.at("event_name") << std::endl;
            }
            break;
        }
    }
}

bool EventLayer::evaluate_condition(const std::string& condition) {
    if (!expression_parser_) {
        // No expression parser available, default to true
        return true;
    }
    
    // Tokenize the condition
    ExpressionTokenizer tokenizer;
    auto tokens = tokenizer.tokenize(condition);
    
    if (tokenizer.has_error()) {
        std::cerr << "Failed to tokenize condition: " << tokenizer.get_error() << std::endl;
        return false;
    }
    
    // Parse the tokens into an AST
    auto ast = expression_parser_->parse(tokens);
    
    if (expression_parser_->has_error() || !ast) {
        std::cerr << "Failed to parse condition: " << expression_parser_->get_error() << std::endl;
        return false;
    }
    
    // Evaluate the AST
    ExpressionEvaluator evaluator;
    EvaluationContext context;
    
    // TODO: Populate context with relevant data (shape, data node, etc.)
    // For now, use an empty context
    
    auto result = evaluator.evaluate(ast.get(), context);
    
    if (evaluator.has_error()) {
        std::cerr << "Failed to evaluate condition: " << evaluator.get_error() << std::endl;
        return false;
    }
    
    // Return the truthiness of the result
    return result.is_truthy();
}

std::string EventLayer::generate_id(const std::string& prefix) {
    return prefix + "_" + std::to_string(id_counter_++);
}

} // namespace ddf
} // namespace whiteboard
