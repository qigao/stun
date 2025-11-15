#include <whiteboard/ddf/connector_layer.h>
#include <whiteboard/ddf/shape_layer.h>
#include <nanovg.h>
#include <cmath>
#include <algorithm>
#include <queue>
#include <set>
#include <sstream>

namespace whiteboard {
namespace ddf {

void ConnectorLayer::add_connector(const Connector& connector) {
    connectors_[connector.id] = connector;
}

void ConnectorLayer::remove_connector(const std::string& id) {
    connectors_.erase(id);
}

void ConnectorLayer::update_connector(const std::string& id, const Connector& connector) {
    connectors_[id] = connector;
}

Connector* ConnectorLayer::get_connector(const std::string& id) {
    auto it = connectors_.find(id);
    return it != connectors_.end() ? &it->second : nullptr;
}

const Connector* ConnectorLayer::get_connector(const std::string& id) const {
    auto it = connectors_.find(id);
    return it != connectors_.end() ? &it->second : nullptr;
}

std::vector<Connector*> ConnectorLayer::get_all_connectors() {
    std::vector<Connector*> result;
    result.reserve(connectors_.size());
    for (auto& [id, conn] : connectors_) {
        result.push_back(&conn);
    }
    return result;
}

std::vector<const Connector*> ConnectorLayer::get_all_connectors() const {
    std::vector<const Connector*> result;
    result.reserve(connectors_.size());
    for (const auto& [id, conn] : connectors_) {
        result.push_back(&conn);
    }
    return result;
}

std::vector<Connector*> ConnectorLayer::get_connectors_for_shape(const std::string& shape_id) {
    std::vector<Connector*> result;
    for (auto& [id, conn] : connectors_) {
        if (conn.from.shape_id == shape_id || conn.to.shape_id == shape_id) {
            result.push_back(&conn);
        }
    }
    return result;
}

std::vector<const Connector*> ConnectorLayer::get_connectors_for_shape(const std::string& shape_id) const {
    std::vector<const Connector*> result;
    for (const auto& [id, conn] : connectors_) {
        if (conn.from.shape_id == shape_id || conn.to.shape_id == shape_id) {
            result.push_back(&conn);
        }
    }
    return result;
}

void ConnectorLayer::compute_path(const std::string& connector_id) {
    auto* conn = get_connector(connector_id);
    if (!conn) return;
    
    // Route based on algorithm
    switch (conn->routing.algorithm) {
        case RoutingAlgorithm::Straight:
            conn->path_points = route_straight(conn->from, conn->to);
            break;
        case RoutingAlgorithm::Orthogonal:
        case RoutingAlgorithm::CurvedOrthogonal:
            conn->path_points = route_orthogonal(conn->from, conn->to, conn->routing.avoid_shapes);
            break;
        case RoutingAlgorithm::Bezier:
            conn->path_points = route_bezier(conn->from, conn->to);
            break;
    }
}

void ConnectorLayer::recompute_all_paths() {
    for (auto& [id, conn] : connectors_) {
        compute_path(id);
    }
}

void ConnectorLayer::recompute_paths_for_shape(const std::string& shape_id) {
    auto connectors = get_connectors_for_shape(shape_id);
    for (auto* conn : connectors) {
        compute_path(conn->id);
    }
}

std::string ConnectorLayer::render_to_svg(const std::string& connector_id) const {
    const Connector* conn = get_connector(connector_id);
    if (!conn || conn->path_points.empty()) return "";
    
    std::ostringstream svg;
    
    // Build path data
    std::ostringstream path_data;
    
    if (conn->routing.algorithm == RoutingAlgorithm::Bezier && conn->path_points.size() >= 8) {
        // Bezier curve: M x0 y0 C x1 y1, x2 y2, x3 y3
        path_data << "M " << conn->path_points[0] << " " << conn->path_points[1];
        path_data << " C " << conn->path_points[2] << " " << conn->path_points[3];
        path_data << ", " << conn->path_points[4] << " " << conn->path_points[5];
        path_data << ", " << conn->path_points[6] << " " << conn->path_points[7];
    } else {
        // Polyline for straight and orthogonal
        path_data << "M " << conn->path_points[0] << " " << conn->path_points[1];
        for (size_t i = 2; i < conn->path_points.size(); i += 2) {
            path_data << " L " << conn->path_points[i] << " " << conn->path_points[i + 1];
        }
    }
    
    // Get style properties
    std::string stroke = conn->style.count("stroke") ? conn->style.at("stroke") : "#000000";
    std::string stroke_width = conn->style.count("stroke-width") ? conn->style.at("stroke-width") : "2";
    std::string fill = "none";
    
    // Create path element
    svg << "<path d=\"" << path_data.str() << "\" ";
    svg << "stroke=\"" << stroke << "\" ";
    svg << "stroke-width=\"" << stroke_width << "\" ";
    svg << "fill=\"" << fill << "\" ";
    
    // Add marker references for arrows
    if (conn->arrow_start != ArrowType::None) {
        svg << "marker-start=\"url(#arrow-start-" << connector_id << ")\" ";
    }
    if (conn->arrow_end != ArrowType::None) {
        svg << "marker-end=\"url(#arrow-end-" << connector_id << ")\" ";
    }
    
    svg << "/>\n";
    
    // Add label if present
    if (conn->label.has_value() && !conn->label->text.empty() && conn->path_points.size() >= 4) {
        // Calculate position along path
        size_t num_points = conn->path_points.size() / 2;
        float t = conn->label->position;
        size_t idx = static_cast<size_t>(t * (num_points - 1));
        idx = std::min(idx, num_points - 2);
        
        float x1 = conn->path_points[idx * 2];
        float y1 = conn->path_points[idx * 2 + 1];
        float x2 = conn->path_points[(idx + 1) * 2];
        float y2 = conn->path_points[(idx + 1) * 2 + 1];
        
        float local_t = t * (num_points - 1) - idx;
        float label_x = x1 + (x2 - x1) * local_t + conn->label->offset_x;
        float label_y = y1 + (y2 - y1) * local_t + conn->label->offset_y;
        
        svg << "<text x=\"" << label_x << "\" y=\"" << label_y << "\" ";
        svg << "text-anchor=\"middle\" dominant-baseline=\"middle\" ";
        svg << "font-size=\"12\">";
        svg << conn->label->text;
        svg << "</text>\n";
    }
    
    return svg.str();
}

void ConnectorLayer::clear() {
    connectors_.clear();
}

void ConnectorLayer::set_shape_layer(ShapeLayer* shape_layer) {
    shape_layer_ = shape_layer;
}

bool ConnectorLayer::get_connection_point_position(const ConnectorEndpoint& endpoint, float& x, float& y) {
    if (!shape_layer_) return false;
    
    const Shape* shape = shape_layer_->get_shape(endpoint.shape_id);
    if (!shape) return false;
    
    // Find the connection point
    const ConnectionPoint* cp = nullptr;
    for (const auto& point : shape->connection_points) {
        if (point.id == endpoint.connection_point_id) {
            cp = &point;
            break;
        }
    }
    
    if (!cp) {
        // If no connection point found, use center of shape
        x = shape->geometry.count("x") ? shape->geometry.at("x") : 0.0f;
        y = shape->geometry.count("y") ? shape->geometry.at("y") : 0.0f;
        
        float width = shape->geometry.count("width") ? shape->geometry.at("width") : 0.0f;
        float height = shape->geometry.count("height") ? shape->geometry.at("height") : 0.0f;
        
        x += width * 0.5f;
        y += height * 0.5f;
        return true;
    }
    
    // Calculate absolute position from ratio
    float shape_x = shape->geometry.count("x") ? shape->geometry.at("x") : 0.0f;
    float shape_y = shape->geometry.count("y") ? shape->geometry.at("y") : 0.0f;
    float width = shape->geometry.count("width") ? shape->geometry.at("width") : 0.0f;
    float height = shape->geometry.count("height") ? shape->geometry.at("height") : 0.0f;
    
    x = shape_x + width * cp->x_ratio;
    y = shape_y + height * cp->y_ratio;
    
    return true;
}

std::vector<float> ConnectorLayer::route_straight(const ConnectorEndpoint& from,
                                                   const ConnectorEndpoint& to) {
    std::vector<float> path;
    
    float from_x, from_y, to_x, to_y;
    if (!get_connection_point_position(from, from_x, from_y)) return path;
    if (!get_connection_point_position(to, to_x, to_y)) return path;
    
    // Simple straight line: from point to to point
    path.push_back(from_x);
    path.push_back(from_y);
    path.push_back(to_x);
    path.push_back(to_y);
    
    return path;
}

std::vector<float> ConnectorLayer::route_orthogonal(const ConnectorEndpoint& from,
                                                     const ConnectorEndpoint& to,
                                                     bool avoid_shapes) {
    std::vector<float> path;
    
    float from_x, from_y, to_x, to_y;
    if (!get_connection_point_position(from, from_x, from_y)) return path;
    if (!get_connection_point_position(to, to_x, to_y)) return path;
    
    if (!avoid_shapes) {
        // Simple orthogonal routing without obstacle avoidance
        // Use Manhattan routing with one or two bends
        
        float mid_x = (from_x + to_x) / 2.0f;
        float mid_y = (from_y + to_y) / 2.0f;
        
        // Determine routing direction based on connection point positions
        // For simplicity, use a two-segment path
        path.push_back(from_x);
        path.push_back(from_y);
        
        // Try horizontal first, then vertical
        if (std::abs(to_x - from_x) > std::abs(to_y - from_y)) {
            path.push_back(mid_x);
            path.push_back(from_y);
            path.push_back(mid_x);
            path.push_back(to_y);
        } else {
            path.push_back(from_x);
            path.push_back(mid_y);
            path.push_back(to_x);
            path.push_back(mid_y);
        }
        
        path.push_back(to_x);
        path.push_back(to_y);
        
        return path;
    }
    
    // A* pathfinding with obstacle avoidance
    // For now, implement a simplified version without full spatial indexing
    // This creates a grid-based pathfinding approach
    
    const float grid_size = 20.0f;  // Grid cell size
    
    // Convert positions to grid coordinates
    auto to_grid = [grid_size](float val) -> int {
        return static_cast<int>(std::floor(val / grid_size));
    };
    
    int start_x = to_grid(from_x);
    int start_y = to_grid(from_y);
    int goal_x = to_grid(to_x);
    int goal_y = to_grid(to_y);
    
    // A* node structure
    struct Node {
        int x, y;
        float g_cost;  // Cost from start
        float h_cost;  // Heuristic to goal
        float f_cost() const { return g_cost + h_cost; }
        int parent_x, parent_y;
        
        bool operator>(const Node& other) const {
            return f_cost() > other.f_cost();
        }
    };
    
    // Manhattan distance heuristic
    auto heuristic = [](int x1, int y1, int x2, int y2) -> float {
        return static_cast<float>(std::abs(x2 - x1) + std::abs(y2 - y1));
    };
    
    // Check if a grid cell is blocked by a shape
    auto is_blocked = [this, grid_size](int gx, int gy) -> bool {
        if (!shape_layer_) return false;
        
        float world_x = gx * grid_size + grid_size * 0.5f;
        float world_y = gy * grid_size + grid_size * 0.5f;
        
        // Check if this point intersects any shape
        for (const auto* shape : shape_layer_->get_all_shapes()) {
            if (shape->type == "group") continue;  // Skip groups
            
            float sx = shape->geometry.count("x") ? shape->geometry.at("x") : 0.0f;
            float sy = shape->geometry.count("y") ? shape->geometry.at("y") : 0.0f;
            float sw = shape->geometry.count("width") ? shape->geometry.at("width") : 0.0f;
            float sh = shape->geometry.count("height") ? shape->geometry.at("height") : 0.0f;
            
            // Simple AABB check
            if (world_x >= sx && world_x <= sx + sw &&
                world_y >= sy && world_y <= sy + sh) {
                return true;
            }
        }
        return false;
    };
    
    // A* algorithm
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open_set;
    std::set<std::pair<int, int>> closed_set;
    std::map<std::pair<int, int>, Node> node_map;
    
    Node start_node;
    start_node.x = start_x;
    start_node.y = start_y;
    start_node.g_cost = 0.0f;
    start_node.h_cost = heuristic(start_x, start_y, goal_x, goal_y);
    start_node.parent_x = start_x;
    start_node.parent_y = start_y;
    
    open_set.push(start_node);
    node_map[{start_x, start_y}] = start_node;
    
    const int max_iterations = 1000;  // Prevent infinite loops
    int iterations = 0;
    
    bool found = false;
    Node goal_node = {};  // Initialize to avoid warning
    
    while (!open_set.empty() && iterations < max_iterations) {
        iterations++;
        
        Node current = open_set.top();
        open_set.pop();
        
        if (current.x == goal_x && current.y == goal_y) {
            goal_node = current;
            found = true;
            break;
        }
        
        if (closed_set.count({current.x, current.y})) continue;
        closed_set.insert({current.x, current.y});
        
        // Explore neighbors (4-directional: up, down, left, right)
        const int dx[] = {0, 0, -1, 1};
        const int dy[] = {-1, 1, 0, 0};
        
        for (int i = 0; i < 4; i++) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];
            
            if (closed_set.count({nx, ny})) continue;
            if (is_blocked(nx, ny) && !(nx == goal_x && ny == goal_y)) continue;
            
            float new_g_cost = current.g_cost + 1.0f;
            
            auto it = node_map.find({nx, ny});
            if (it == node_map.end() || new_g_cost < it->second.g_cost) {
                Node neighbor;
                neighbor.x = nx;
                neighbor.y = ny;
                neighbor.g_cost = new_g_cost;
                neighbor.h_cost = heuristic(nx, ny, goal_x, goal_y);
                neighbor.parent_x = current.x;
                neighbor.parent_y = current.y;
                
                node_map[{nx, ny}] = neighbor;
                open_set.push(neighbor);
            }
        }
    }
    
    if (!found) {
        // Fallback to simple routing if pathfinding fails
        path.push_back(from_x);
        path.push_back(from_y);
        path.push_back(to_x);
        path.push_back(to_y);
        return path;
    }
    
    // Reconstruct path from goal to start
    std::vector<std::pair<int, int>> grid_path;
    int cx = goal_node.x;
    int cy = goal_node.y;
    
    while (cx != start_x || cy != start_y) {
        grid_path.push_back({cx, cy});
        auto it = node_map.find({cx, cy});
        if (it == node_map.end()) break;
        int px = it->second.parent_x;
        int py = it->second.parent_y;
        cx = px;
        cy = py;
    }
    grid_path.push_back({start_x, start_y});
    
    std::reverse(grid_path.begin(), grid_path.end());
    
    // Convert grid path to world coordinates
    path.push_back(from_x);
    path.push_back(from_y);
    
    for (size_t i = 1; i < grid_path.size() - 1; i++) {
        float wx = grid_path[i].first * grid_size + grid_size * 0.5f;
        float wy = grid_path[i].second * grid_size + grid_size * 0.5f;
        path.push_back(wx);
        path.push_back(wy);
    }
    
    path.push_back(to_x);
    path.push_back(to_y);
    
    return path;
}

std::vector<float> ConnectorLayer::route_bezier(const ConnectorEndpoint& from,
                                                 const ConnectorEndpoint& to) {
    std::vector<float> path;
    
    float from_x, from_y, to_x, to_y;
    if (!get_connection_point_position(from, from_x, from_y)) return path;
    if (!get_connection_point_position(to, to_x, to_y)) return path;
    
    // Determine connection directions based on connection point positions
    // For simplicity, assume connection points indicate direction
    
    // Get the shapes to determine connection point directions
    const Shape* from_shape = shape_layer_ ? shape_layer_->get_shape(from.shape_id) : nullptr;
    const Shape* to_shape = shape_layer_ ? shape_layer_->get_shape(to.shape_id) : nullptr;
    
    // Default control point offset
    float control_offset = 50.0f;
    
    // Determine direction vectors for control points
    float from_dx = 0.0f, from_dy = 0.0f;
    float to_dx = 0.0f, to_dy = 0.0f;
    
    if (from_shape) {
        // Find the connection point to determine direction
        for (const auto& cp : from_shape->connection_points) {
            if (cp.id == from.connection_point_id) {
                // Determine direction based on position ratio
                if (cp.y_ratio < 0.25f) {
                    from_dy = -1.0f;  // Top
                } else if (cp.y_ratio > 0.75f) {
                    from_dy = 1.0f;   // Bottom
                } else if (cp.x_ratio < 0.25f) {
                    from_dx = -1.0f;  // Left
                } else if (cp.x_ratio > 0.75f) {
                    from_dx = 1.0f;   // Right
                } else {
                    // Center or ambiguous - use direction to target
                    float dx = to_x - from_x;
                    float dy = to_y - from_y;
                    if (std::abs(dx) > std::abs(dy)) {
                        from_dx = dx > 0 ? 1.0f : -1.0f;
                    } else {
                        from_dy = dy > 0 ? 1.0f : -1.0f;
                    }
                }
                break;
            }
        }
    }
    
    if (to_shape) {
        // Find the connection point to determine direction
        for (const auto& cp : to_shape->connection_points) {
            if (cp.id == to.connection_point_id) {
                // Determine direction based on position ratio
                if (cp.y_ratio < 0.25f) {
                    to_dy = -1.0f;  // Top
                } else if (cp.y_ratio > 0.75f) {
                    to_dy = 1.0f;   // Bottom
                } else if (cp.x_ratio < 0.25f) {
                    to_dx = -1.0f;  // Left
                } else if (cp.x_ratio > 0.75f) {
                    to_dx = 1.0f;   // Right
                } else {
                    // Center or ambiguous - use direction from source
                    float dx = to_x - from_x;
                    float dy = to_y - from_y;
                    if (std::abs(dx) > std::abs(dy)) {
                        to_dx = dx > 0 ? -1.0f : 1.0f;
                    } else {
                        to_dy = dy > 0 ? -1.0f : 1.0f;
                    }
                }
                break;
            }
        }
    }
    
    // If no direction determined, use default based on relative positions
    if (from_dx == 0.0f && from_dy == 0.0f) {
        float dx = to_x - from_x;
        float dy = to_y - from_y;
        if (std::abs(dx) > std::abs(dy)) {
            from_dx = dx > 0 ? 1.0f : -1.0f;
        } else {
            from_dy = dy > 0 ? 1.0f : -1.0f;
        }
    }
    
    if (to_dx == 0.0f && to_dy == 0.0f) {
        float dx = to_x - from_x;
        float dy = to_y - from_y;
        if (std::abs(dx) > std::abs(dy)) {
            to_dx = dx > 0 ? -1.0f : 1.0f;
        } else {
            to_dy = dy > 0 ? -1.0f : 1.0f;
        }
    }
    
    // Calculate control points
    float cp1_x = from_x + from_dx * control_offset;
    float cp1_y = from_y + from_dy * control_offset;
    float cp2_x = to_x + to_dx * control_offset;
    float cp2_y = to_y + to_dy * control_offset;
    
    // Generate bezier curve points
    // For rendering, we'll store: start, cp1, cp2, end
    // The renderer will need to interpret these as bezier control points
    path.push_back(from_x);
    path.push_back(from_y);
    path.push_back(cp1_x);
    path.push_back(cp1_y);
    path.push_back(cp2_x);
    path.push_back(cp2_y);
    path.push_back(to_x);
    path.push_back(to_y);
    
    return path;
}

void ConnectorLayer::render(void* vctx, const std::string& connector_id) const {
    NVGcontext* ctx = static_cast<NVGcontext*>(vctx);
    
    auto it = connectors_.find(connector_id);
    if (it == connectors_.end()) return;
    const Connector* conn = &it->second;
    if (!conn || !ctx || conn->path_points.empty()) return;
    
    // Parse stroke color
    auto parse_color = [](const std::string& color_str) -> NVGcolor {
        if (color_str.empty() || color_str[0] != '#') {
            return nvgRGBA(0, 0, 0, 255);
        }
        
        unsigned int r = 0, g = 0, b = 0;
        if (color_str.length() >= 7) {
            sscanf(color_str.c_str(), "#%02x%02x%02x", &r, &g, &b);
        }
        return nvgRGBA(r, g, b, 255);
    };
    
    // Get style properties
    NVGcolor stroke_color = parse_color(conn->style.count("stroke") ? conn->style.at("stroke") : "#000000");
    float stroke_width = conn->style.count("stroke-width") ? std::stof(conn->style.at("stroke-width")) : 2.0f;
    
    // Begin path
    nvgBeginPath(ctx);
    
    if (conn->routing.algorithm == RoutingAlgorithm::Bezier && conn->path_points.size() >= 8) {
        // Bezier curve
        nvgMoveTo(ctx, conn->path_points[0], conn->path_points[1]);
        nvgBezierTo(ctx, 
                    conn->path_points[2], conn->path_points[3],
                    conn->path_points[4], conn->path_points[5],
                    conn->path_points[6], conn->path_points[7]);
    } else {
        // Polyline for straight and orthogonal
        nvgMoveTo(ctx, conn->path_points[0], conn->path_points[1]);
        for (size_t i = 2; i < conn->path_points.size(); i += 2) {
            nvgLineTo(ctx, conn->path_points[i], conn->path_points[i + 1]);
        }
    }
    
    // Stroke the path
    nvgStrokeColor(ctx, stroke_color);
    nvgStrokeWidth(ctx, stroke_width);
    nvgStroke(ctx);
    
    // Render arrows
    auto render_arrow = [ctx, stroke_color, stroke_width](float x, float y, float angle, ArrowType type) {
        if (type == ArrowType::None) return;
        
        nvgSave(ctx);
        nvgTranslate(ctx, x, y);
        nvgRotate(ctx, angle);
        
        nvgBeginPath(ctx);
        
        switch (type) {
            case ArrowType::Arrow: {
                float size = stroke_width * 3.0f;
                nvgMoveTo(ctx, 0, 0);
                nvgLineTo(ctx, -size, -size * 0.5f);
                nvgLineTo(ctx, -size, size * 0.5f);
                nvgClosePath(ctx);
                nvgFillColor(ctx, stroke_color);
                nvgFill(ctx);
                break;
            }
            case ArrowType::Circle: {
                float radius = stroke_width * 1.5f;
                nvgCircle(ctx, 0, 0, radius);
                nvgFillColor(ctx, stroke_color);
                nvgFill(ctx);
                break;
            }
            case ArrowType::Diamond: {
                float size = stroke_width * 2.0f;
                nvgMoveTo(ctx, 0, 0);
                nvgLineTo(ctx, -size, -size * 0.5f);
                nvgLineTo(ctx, -size * 2, 0);
                nvgLineTo(ctx, -size, size * 0.5f);
                nvgClosePath(ctx);
                nvgFillColor(ctx, stroke_color);
                nvgFill(ctx);
                break;
            }
            case ArrowType::Square: {
                float size = stroke_width * 1.5f;
                nvgRect(ctx, -size, -size * 0.5f, size, size);
                nvgFillColor(ctx, stroke_color);
                nvgFill(ctx);
                break;
            }
            default:
                break;
        }
        
        nvgRestore(ctx);
    };
    
    // Calculate arrow positions and angles
    if (conn->path_points.size() >= 4) {
        // End arrow
        if (conn->arrow_end != ArrowType::None) {
            size_t last_idx = conn->path_points.size() - 2;
            float end_x = conn->path_points[last_idx];
            float end_y = conn->path_points[last_idx + 1];
            float prev_x = conn->path_points[last_idx - 2];
            float prev_y = conn->path_points[last_idx - 1];
            
            float angle = std::atan2(end_y - prev_y, end_x - prev_x);
            render_arrow(end_x, end_y, angle, conn->arrow_end);
        }
        
        // Start arrow
        if (conn->arrow_start != ArrowType::None) {
            float start_x = conn->path_points[0];
            float start_y = conn->path_points[1];
            float next_x = conn->path_points[2];
            float next_y = conn->path_points[3];
            
            float angle = std::atan2(next_y - start_y, next_x - start_x) + 3.14159f;
            render_arrow(start_x, start_y, angle, conn->arrow_start);
        }
    }
    
    // Render label
    if (conn->label.has_value() && !conn->label->text.empty() && conn->path_points.size() >= 4) {
        size_t num_points = conn->path_points.size() / 2;
        float t = conn->label->position;
        size_t idx = static_cast<size_t>(t * (num_points - 1));
        idx = std::min(idx, num_points - 2);
        
        float x1 = conn->path_points[idx * 2];
        float y1 = conn->path_points[idx * 2 + 1];
        float x2 = conn->path_points[(idx + 1) * 2];
        float y2 = conn->path_points[(idx + 1) * 2 + 1];
        
        float local_t = t * (num_points - 1) - idx;
        float label_x = x1 + (x2 - x1) * local_t + conn->label->offset_x;
        float label_y = y1 + (y2 - y1) * local_t + conn->label->offset_y;
        
        nvgFontSize(ctx, 12.0f);
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, nvgRGBA(0, 0, 0, 255));
        nvgText(ctx, label_x, label_y, conn->label->text.c_str(), nullptr);
    }
}

void ConnectorLayer::render_all(void* ctx) const {
    for (auto it = connectors_.begin(); it != connectors_.end(); ++it) {
        render(ctx, it->first);
    }
}

std::string ConnectorLayer::generate_id(const std::string& prefix) {
    return prefix + "_" + std::to_string(id_counter_++);
}

} // namespace ddf
} // namespace whiteboard
