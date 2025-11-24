#pragma once

#include <string>
#include <map>
#include <vector>
#include <optional>

// Forward declaration
struct NVGcontext;

namespace whiteboard {
namespace ddf {

/**
 * @brief Routing algorithms for connectors
 */
enum class RoutingAlgorithm {
    Straight,
    Orthogonal,
    Bezier,
    CurvedOrthogonal
};

/**
 * @brief Arrow types for connector endpoints
 */
enum class ArrowType {
    None,
    Arrow,
    Diamond,
    Circle,
    Square
};

/**
 * @brief Endpoint of a connector
 */
struct ConnectorEndpoint {
    std::string shape_id;
    std::string connection_point_id;
};

/**
 * @brief Routing configuration for connectors
 */
struct ConnectorRouting {
    RoutingAlgorithm algorithm = RoutingAlgorithm::Straight;
    bool avoid_shapes = false;
    float padding = 10.0f;
    float corner_radius = 5.0f;  // For orthogonal routing
};

/**
 * @brief Label configuration for connectors
 */
struct ConnectorLabel {
    std::string text;
    float position = 0.5f;  // 0-1 along the path
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

/**
 * @brief Smart connector between shapes
 */
struct Connector {
    std::string id;
    ConnectorEndpoint from;
    ConnectorEndpoint to;
    ConnectorRouting routing;
    std::map<std::string, std::string> style;
    ArrowType arrow_start = ArrowType::None;
    ArrowType arrow_end = ArrowType::Arrow;
    std::optional<ConnectorLabel> label;
    
    // Computed path (cached) - stored as x, y pairs
    std::vector<float> path_points;
};

/**
 * @brief Connector layer for smart routing between shapes
 * 
 * The connector layer provides smart routing between shapes with automatic path finding.
 * It supports multiple routing algorithms and automatic re-routing when shapes move.
 */
class ConnectorLayer {
public:
    ConnectorLayer() = default;
    ~ConnectorLayer() = default;

    // Connector operations
    void add_connector(const Connector& connector);
    void remove_connector(const std::string& id);
    void update_connector(const std::string& id, const Connector& connector);
    
    Connector* get_connector(const std::string& id);
    const Connector* get_connector(const std::string& id) const;
    
    std::vector<Connector*> get_all_connectors();
    std::vector<const Connector*> get_all_connectors() const;
    
    std::vector<Connector*> get_connectors_for_shape(const std::string& shape_id);
    std::vector<const Connector*> get_connectors_for_shape(const std::string& shape_id) const;

    // Routing
    void compute_path(const std::string& connector_id);
    void recompute_all_paths();
    void recompute_paths_for_shape(const std::string& shape_id);

    // Rendering
    std::string render_to_svg(const std::string& connector_id) const;
    void render(void* ctx, const std::string& connector_id) const;
    void render_all(void* ctx) const;

    // Clear all connectors
    void clear();
    
    // Set shape layer for connection point resolution
    void set_shape_layer(class ShapeLayer* shape_layer);

private:
    std::map<std::string, Connector> connectors_;
    class ShapeLayer* shape_layer_ = nullptr;
    
    // Routing algorithms - return x, y pairs
    std::vector<float> route_straight(const ConnectorEndpoint& from,
                                       const ConnectorEndpoint& to);
    std::vector<float> route_orthogonal(const ConnectorEndpoint& from,
                                         const ConnectorEndpoint& to,
                                         bool avoid_shapes);
    std::vector<float> route_bezier(const ConnectorEndpoint& from,
                                     const ConnectorEndpoint& to);
    
    // Helper methods
    bool get_connection_point_position(const ConnectorEndpoint& endpoint, float& x, float& y);
    std::string generate_id(const std::string& prefix);
    int id_counter_ = 0;
};

} // namespace ddf
} // namespace whiteboard
