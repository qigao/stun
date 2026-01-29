/*
 * Meta Editor - Connector System Implementation
 */

#include "meta_editor/connector.h"
#include "meta_editor/canvas.h"
#include <algorithm>

namespace meta_editor {

flex::Vec2 Connector::get_connection_point(flex::Node* node, ConnectionSide side) {
    if (!node) return flex::Vec2(0, 0);
    
    auto bounds = node->world_bounds();
    float cx = bounds.x + bounds.width / 2;
    float cy = bounds.y + bounds.height / 2;
    
    switch (side) {
        case ConnectionSide::Top:    return flex::Vec2(cx, bounds.y);
        case ConnectionSide::Bottom: return flex::Vec2(cx, bounds.y + bounds.height);
        case ConnectionSide::Left:   return flex::Vec2(bounds.x, cy);
        case ConnectionSide::Right:  return flex::Vec2(bounds.x + bounds.width, cy);
        case ConnectionSide::Center: return flex::Vec2(cx, cy);
    }
    return flex::Vec2(cx, cy);
}

void Connector::update_position() {
    if (!line) return;
    
    flex::Vec2 p1, p2;
    
    if (start.is_bound()) {
        p1 = get_connection_point(start.node, start.side);
    } else {
        // Free endpoint - use line's current start position
        p1 = flex::Vec2(line->x(), line->y());
    }
    
    if (end.is_bound()) {
        p2 = get_connection_point(end.node, end.side);
    } else {
        // Free endpoint - calculate from line geometry
        auto path = line->path();
        // For now, assume line ends at its stored endpoint
        // This is a simplification - real implementation would parse path
        p2 = p1 + flex::Vec2(100, 0);  // Fallback
    }
    
    // Update line position and geometry
    line->set_position(p1.x(), p1.y());
    
    float dx = p2.x() - p1.x();
    float dy = p2.y() - p1.y();
    
    // Rebuild path with arrow (simplified - assumes arrow at end)
    std::string path_data = "M 0 0 L " + std::to_string(dx) + " " + std::to_string(dy);
    
    // Add arrow head
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 0.001f) {
        float ux = dx / len;
        float uy = dy / len;
        float px = -uy;
        float py = ux;
        float arrow_size = 12.0f;
        float wing_back = arrow_size * 0.8f;
        float wing_width = arrow_size * 0.4f;
        
        float left_x = dx - ux * wing_back + px * wing_width;
        float left_y = dy - uy * wing_back + py * wing_width;
        float right_x = dx - ux * wing_back - px * wing_width;
        float right_y = dy - uy * wing_back - py * wing_width;
        
        char arrow[128];
        snprintf(arrow, sizeof(arrow), " M %.1f %.1f L %.1f %.1f M %.1f %.1f L %.1f %.1f",
                 dx, dy, left_x, left_y, dx, dy, right_x, right_y);
        path_data += arrow;
    }
    
    line->set_path(path_data);
}

// ConnectorManager

ConnectorManager::ConnectorManager(Canvas* canvas) : canvas_(canvas) {}

Connector* ConnectorManager::create_connector(flex::Shape* line) {
    auto conn = std::make_unique<Connector>();
    conn->line = line;
    Connector* ptr = conn.get();
    connectors_.push_back(std::move(conn));
    return ptr;
}

void ConnectorManager::bind_start(Connector* conn, flex::Node* node, ConnectionSide side) {
    if (!conn) return;
    
    // Remove old binding from index
    if (conn->start.is_bound()) {
        auto range = node_to_connectors_.equal_range(conn->start.node);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == conn) {
                node_to_connectors_.erase(it);
                break;
            }
        }
    }
    
    conn->start.node = node;
    conn->start.side = side;
    
    if (node) {
        node_to_connectors_.emplace(node, conn);
    }
    
    conn->update_position();
}

void ConnectorManager::bind_end(Connector* conn, flex::Node* node, ConnectionSide side) {
    if (!conn) return;
    
    // Remove old binding from index
    if (conn->end.is_bound()) {
        auto range = node_to_connectors_.equal_range(conn->end.node);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == conn) {
                node_to_connectors_.erase(it);
                break;
            }
        }
    }
    
    conn->end.node = node;
    conn->end.side = side;
    
    if (node) {
        node_to_connectors_.emplace(node, conn);
    }
    
    conn->update_position();
}

void ConnectorManager::unbind_start(Connector* conn) {
    bind_start(conn, nullptr, ConnectionSide::Center);
}

void ConnectorManager::unbind_end(Connector* conn) {
    bind_end(conn, nullptr, ConnectionSide::Center);
}

void ConnectorManager::remove_connector(Connector* conn) {
    if (!conn) return;
    
    // Remove from index
    if (conn->start.is_bound()) {
        auto range = node_to_connectors_.equal_range(conn->start.node);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == conn) {
                node_to_connectors_.erase(it);
                break;
            }
        }
    }
    if (conn->end.is_bound()) {
        auto range = node_to_connectors_.equal_range(conn->end.node);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == conn) {
                node_to_connectors_.erase(it);
                break;
            }
        }
    }
    
    // Remove from list
    connectors_.erase(
        std::remove_if(connectors_.begin(), connectors_.end(),
            [conn](const auto& c) { return c.get() == conn; }),
        connectors_.end()
    );
}

void ConnectorManager::remove_connector_for_line(flex::Shape* line) {
    if (auto* conn = find_by_line(line)) {
        remove_connector(conn);
    }
}

void ConnectorManager::update_connectors_for_node(flex::Node* node) {
    auto range = node_to_connectors_.equal_range(node);
    for (auto it = range.first; it != range.second; ++it) {
        it->second->update_position();
    }
}

void ConnectorManager::update_all() {
    for (auto& conn : connectors_) {
        conn->update_position();
    }
}

Connector* ConnectorManager::find_by_line(flex::Shape* line) {
    for (auto& conn : connectors_) {
        if (conn->line == line) return conn.get();
    }
    return nullptr;
}

std::vector<Connector*> ConnectorManager::get_connectors_for_node(flex::Node* node) {
    std::vector<Connector*> result;
    auto range = node_to_connectors_.equal_range(node);
    for (auto it = range.first; it != range.second; ++it) {
        result.push_back(it->second);
    }
    return result;
}

void ConnectorManager::rebuild_index() {
    node_to_connectors_.clear();
    for (auto& conn : connectors_) {
        if (conn->start.is_bound()) {
            node_to_connectors_.emplace(conn->start.node, conn.get());
        }
        if (conn->end.is_bound()) {
            node_to_connectors_.emplace(conn->end.node, conn.get());
        }
    }
}

} // namespace meta_editor
