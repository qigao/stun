/*
 * Meta Editor - Connector System
 *
 * Connectors are lines that bind to shapes and follow them when moved.
 * Core data structure for whiteboard/diagram applications.
 */

#pragma once

#include <flex.h>
#include <vector>
#include <unordered_map>

namespace meta_editor {

// Connection point on a shape's edge
enum class ConnectionSide : uint8_t {
    Top,
    Right,
    Bottom,
    Left,
    Center
};

// A binding between a connector endpoint and a shape
struct ConnectionBinding {
    flex::Node* node = nullptr;     // Target shape (null = free endpoint)
    ConnectionSide side = ConnectionSide::Center;
    
    bool is_bound() const { return node != nullptr; }
};

// A connector links two points (optionally bound to shapes)
struct Connector {
    flex::Shape* line = nullptr;    // The visual line shape
    ConnectionBinding start;
    ConnectionBinding end;
    
    // Recalculate line position based on bound shapes
    void update_position();
    
    // Get world position of a connection point on a shape
    static flex::Vec2 get_connection_point(flex::Node* node, ConnectionSide side);
};

// Manages all connectors and updates them when shapes move
class ConnectorManager {
public:
    explicit ConnectorManager(class Canvas* canvas);
    
    // Create a new connector
    Connector* create_connector(flex::Shape* line);
    
    // Bind connector endpoints to shapes
    void bind_start(Connector* conn, flex::Node* node, ConnectionSide side);
    void bind_end(Connector* conn, flex::Node* node, ConnectionSide side);
    
    // Unbind endpoints
    void unbind_start(Connector* conn);
    void unbind_end(Connector* conn);
    
    // Remove connector
    void remove_connector(Connector* conn);
    void remove_connector_for_line(flex::Shape* line);
    
    // Update all connectors bound to a specific node
    void update_connectors_for_node(flex::Node* node);
    
    // Update all connectors
    void update_all();
    
    // Find connector by line shape
    Connector* find_by_line(flex::Shape* line);
    
    // Get all connectors bound to a node
    std::vector<Connector*> get_connectors_for_node(flex::Node* node);

private:
    Canvas* canvas_;
    std::vector<std::unique_ptr<Connector>> connectors_;
    
    // Index: node -> connectors that reference it
    std::unordered_multimap<flex::Node*, Connector*> node_to_connectors_;
    
    void rebuild_index();
};

} // namespace meta_editor
