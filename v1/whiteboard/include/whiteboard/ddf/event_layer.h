#pragma once

#include <string>
#include <map>
#include <vector>
#include <optional>

namespace whiteboard {
namespace ddf {

/**
 * @brief Event trigger types
 */
enum class EventTrigger {
    Click,
    DoubleClick,
    RightClick,
    Hover,
    HoverEnd,
    DragStart,
    Drag,
    DragEnd,
    Select,
    Deselect
};

/**
 * @brief Action types for events
 */
enum class ActionType {
    UpdateData,
    UpdateStyle,
    UpdateGeometry,
    ShowTooltip,
    Navigate,
    ExecuteScript,
    EmitCustomEvent
};

/**
 * @brief Action to execute when event is triggered
 */
struct EventAction {
    ActionType type;
    std::map<std::string, std::string> parameters;
};

/**
 * @brief Event definition with trigger and actions
 */
struct Event {
    std::string id;
    std::string target_id;  // shape, component instance, or connector
    EventTrigger trigger;
    std::vector<EventAction> actions;
    std::optional<std::string> condition;  // Expression that must evaluate to true
};

/**
 * @brief Event layer for interactive behaviors
 * 
 * The event layer defines interactive behaviors declaratively.
 * Events can be triggered by user interactions and execute various actions.
 */
class EventLayer {
public:
    EventLayer() = default;
    ~EventLayer() = default;

    // Event operations
    void register_event(const Event& event);
    void unregister_event(const std::string& id);
    
    Event* get_event(const std::string& id);
    const Event* get_event(const std::string& id) const;
    
    std::vector<Event*> get_all_events();
    std::vector<const Event*> get_all_events() const;
    
    std::vector<Event*> get_events_for_target(const std::string& target_id);
    std::vector<const Event*> get_events_for_target(const std::string& target_id) const;
    
    std::vector<Event*> get_events_for_trigger(EventTrigger trigger);
    std::vector<const Event*> get_events_for_trigger(EventTrigger trigger) const;

    // Event handling
    void handle_event(const std::string& target_id, EventTrigger trigger);

    // Clear all events
    void clear();
    
    // Set layer dependencies for action execution
    void set_data_layer(class DataLayer* data_layer);
    void set_shape_layer(class ShapeLayer* shape_layer);
    void set_expression_parser(class ExpressionParser* expression_parser);

private:
    std::map<std::string, Event> events_;
    
    // Layer dependencies
    class DataLayer* data_layer_ = nullptr;
    class ShapeLayer* shape_layer_ = nullptr;
    class ExpressionParser* expression_parser_ = nullptr;
    
    // Helper methods
    void execute_action(const EventAction& action);
    bool evaluate_condition(const std::string& condition);
    
    // Helper to generate unique IDs
    std::string generate_id(const std::string& prefix);
    int id_counter_ = 0;
};

} // namespace ddf
} // namespace whiteboard
