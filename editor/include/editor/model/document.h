/*
 * Document
 *
 * The root model object representing an editor document.
 * Contains layers, manages the scene graph, and provides serialization.
 */

#pragma once

#include "../core/types.h"
#include "../core/observable.h"
#include "layer.h"
#include <flex/flex.h>

namespace editor {

class Document : public Observable {
public:
    using Ptr = std::shared_ptr<Document>;

    Document();
    static Ptr create();

    // Document properties
    const std::string& name() const { return name_; }
    void setName(const std::string& name);

    const std::string& filePath() const { return file_path_; }
    void setFilePath(const std::string& path) { file_path_ = path; }

    float width() const { return width_; }
    float height() const { return height_; }
    void setSize(float w, float h);

    Color backgroundColor() const { return bg_color_; }
    void setBackgroundColor(const Color& c);

    bool modified() const { return modified_; }
    void setModified(bool m) { modified_ = m; }

    // Access underlying flex artboard
    flex::Artboard* artboard() const { return artboard_.get(); }

    // Layer management
    const std::vector<Layer::Ptr>& layers() const { return layers_; }
    std::vector<Layer::Ptr>& layers() { return layers_; }  // Non-const for serialization

    Layer::Ptr activeLayer() const { return active_layer_; }
    void setActiveLayer(Layer::Ptr layer);

    void addLayer(Layer::Ptr layer);
    void removeLayer(Layer::Ptr layer);
    void moveLayer(Layer::Ptr layer, size_t newIndex);

    // Find node at point across all layers
    EditorNode::Ptr nodeAt(const Point& p) const;

    // Get all nodes in document
    std::vector<EditorNode::Ptr> allNodes() const;

    // Rendering
    void render(flex::Renderer& renderer);

private:
    std::string name_ = "Untitled";
    std::string file_path_;
    float width_ = 1920;
    float height_ = 1080;
    Color bg_color_ = Color::rgb(255, 255, 255);
    bool modified_ = false;

    flex::Artboard::Ptr artboard_;
    std::vector<Layer::Ptr> layers_;
    Layer::Ptr active_layer_;
};

} // namespace editor
