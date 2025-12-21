/*
 * Document Implementation
 */

#include <editor/model/document.h>
#include <algorithm>

namespace editor {

Document::Document() {
    // Create root artboard with default size
    artboard_ = flex::Artboard::create(1920, 1080);

    // Create default layer
    auto defaultLayer = Layer::create("Layer 1");
    addLayer(defaultLayer);
}

Document::Ptr Document::create() {
    return std::make_shared<Document>();
}

void Document::setName(const std::string& name) {
    name_ = name;
    notify(EventType::DocumentChanged, this);
}

void Document::setSize(float w, float h) {
    width_ = w;
    height_ = h;
    artboard_->set_size(w, h);
    notify(EventType::DocumentChanged, this);
}

void Document::setBackgroundColor(const Color& c) {
    bg_color_ = c;
    notify(EventType::DocumentChanged, this);
}

void Document::setActiveLayer(Layer::Ptr layer) {
    active_layer_ = layer;
    notify(EventType::LayerChanged, this);
}

void Document::addLayer(Layer::Ptr layer) {
    layers_.push_back(layer);
    artboard_->root()->add_child(layer->flexGroupPtr());

    if (!active_layer_) {
        active_layer_ = layer;
    }

    // Forward layer events
    layer->addObserver([this](EventType type, void* data) {
        notify(type, data);
        modified_ = true;
    });

    notify(EventType::LayerChanged, layer.get());
}

void Document::removeLayer(Layer::Ptr layer) {
    if (layers_.size() <= 1) return;  // Keep at least one layer

    layers_.erase(
        std::remove(layers_.begin(), layers_.end(), layer),
        layers_.end());
    artboard_->root()->remove_child(layer->flexGroup());

    if (active_layer_ == layer) {
        active_layer_ = layers_.empty() ? nullptr : layers_.front();
    }

    notify(EventType::LayerChanged, layer.get());
}

void Document::moveLayer(Layer::Ptr layer, size_t newIndex) {
    auto it = std::find(layers_.begin(), layers_.end(), layer);
    if (it == layers_.end()) return;

    layers_.erase(it);
    newIndex = std::min(newIndex, layers_.size());
    layers_.insert(layers_.begin() + newIndex, layer);

    // Rebuild artboard children order
    auto* root = artboard_->root();
    for (auto& l : layers_) {
        root->remove_child(l->flexGroup());
    }
    for (auto& l : layers_) {
        root->add_child(l->flexGroupPtr());
    }

    notify(EventType::LayerChanged, this);
}

EditorNode::Ptr Document::nodeAt(const Point& p) const {
    // Search layers from top to bottom
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        if (!(*it)->visible() || (*it)->locked()) continue;
        if (auto node = (*it)->nodeAt(p)) {
            return node;
        }
    }
    return nullptr;
}

std::vector<EditorNode::Ptr> Document::allNodes() const {
    std::vector<EditorNode::Ptr> result;
    for (auto& layer : layers_) {
        for (auto& node : layer->nodes()) {
            result.push_back(node);
        }
    }
    return result;
}

void Document::render(flex::Renderer& renderer) {
    artboard_->render(renderer);
}

} // namespace editor
