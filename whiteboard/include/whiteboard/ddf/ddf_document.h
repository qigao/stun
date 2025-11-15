#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>

// Forward declarations
struct NVGcontext;

namespace whiteboard {
class SVGShapeLibrary;

namespace ddf {

// Forward declarations of layer classes
class DataLayer;
class ShapeLayer;
class ComponentLayer;
class ConnectorLayer;
class EventLayer;
class StyleLayer;
class RenderingPipeline;

/**
 * @brief Main DDF document manager that coordinates all layers
 * 
 * The DDFDocument is the central manager for the Diagram Definition Format.
 * It provides access to all layers and handles serialization, rendering, and validation.
 */
class DDFDocument {
public:
    DDFDocument();
    ~DDFDocument();

    // Layer access
    DataLayer& data_layer();
    const DataLayer& data_layer() const;
    
    ShapeLayer& shape_layer();
    const ShapeLayer& shape_layer() const;
    
    ComponentLayer& component_layer();
    const ComponentLayer& component_layer() const;
    
    ConnectorLayer& connector_layer();
    const ConnectorLayer& connector_layer() const;
    
    EventLayer& event_layer();
    const EventLayer& event_layer() const;
    
    StyleLayer& style_layer();
    const StyleLayer& style_layer() const;

    // Serialization
    bool load_from_json(const std::string& json_string);
    std::string save_to_json() const;
    
    bool load_from_file(const std::string& filepath);
    bool save_to_file(const std::string& filepath) const;

    // SVG export/import
    std::string export_to_svg() const;
    bool export_to_svg_file(const std::string& filepath) const;
    bool import_from_svg(const std::string& svg_string);

    // Data-driven generation
    void generate_from_data(const std::string& layout_algorithm);

    // Rendering
    void render(NVGcontext* ctx, float viewport_x, float viewport_y, float viewport_width, float viewport_height);
    void set_svg_shape_library(SVGShapeLibrary* library);

    // Validation
    bool validate() const;
    std::vector<std::string> get_validation_errors() const;

    // Metadata
    const std::string& version() const { return version_; }
    void set_version(const std::string& version) { version_ = version; }
    
    const std::map<std::string, std::string>& metadata() const { return metadata_; }
    void set_metadata(const std::string& key, const std::string& value) { metadata_[key] = value; }

private:
    std::unique_ptr<DataLayer> data_layer_;
    std::unique_ptr<ShapeLayer> shape_layer_;
    std::unique_ptr<ComponentLayer> component_layer_;
    std::unique_ptr<ConnectorLayer> connector_layer_;
    std::unique_ptr<EventLayer> event_layer_;
    std::unique_ptr<StyleLayer> style_layer_;
    std::unique_ptr<RenderingPipeline> rendering_pipeline_;
    SVGShapeLibrary* svg_shape_library_ = nullptr;

    std::string version_;
    std::map<std::string, std::string> metadata_;
};

} // namespace ddf
} // namespace whiteboard
