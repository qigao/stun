/*
 * Layer Implementation
 */

#include <editor/model/layer.h>
#include <algorithm>

namespace editor {

Layer::Layer(const std::string& name) : name_(name) {
    group_ = flex::Group::create();
    group_->set_id(name);
}

Layer::Ptr Layer::create(const std::string& name) {
    return std::make_shared<Layer>(name);
}

void Layer::setName(const std::string& name) {
    name_ = name;
    group_->set_id(name);
    notify(EventType::LayerChanged, this);
}

void Layer::setVisible(bool v) {
    visible_ = v;
    group_->set_visible(v);
    notify(EventType::LayerChanged, this);
}

void Layer::setLocked(bool v) {
    locked_ = v;
    notify(EventType::LayerChanged, this);
}

void Layer::setOpacity(float o) {
    opacity_ = o;
    group_->set_opacity(o);
    notify(EventType::LayerChanged, this);
}

void Layer::addNode(EditorNode::Ptr node) {
    node->setLayer(this);
    nodes_.push_back(node);

    if (auto* shapeNode = dynamic_cast<ShapeNode*>(node.get())) {
        group_->add_child(shapeNode->shapePtr());
    } else if (auto* groupNode = dynamic_cast<GroupNode*>(node.get())) {
        group_->add_child(groupNode->groupPtr());
    } else if (auto* textNode = dynamic_cast<TextNode*>(node.get())) {
        group_->add_child(textNode->textPtr());
    }

    notify(EventType::NodeAdded, node.get());
}

void Layer::removeNode(EditorNode::Ptr node) {
    node->setLayer(nullptr);
    nodes_.erase(
        std::remove(nodes_.begin(), nodes_.end(), node),
        nodes_.end());
    group_->remove_child(node->flexNode());
    notify(EventType::NodeRemoved, node.get());
}

void Layer::moveNode(EditorNode::Ptr node, size_t newIndex) {
    auto it = std::find(nodes_.begin(), nodes_.end(), node);
    if (it == nodes_.end()) return;

    nodes_.erase(it);
    newIndex = std::min(newIndex, nodes_.size());
    nodes_.insert(nodes_.begin() + newIndex, node);

    // Rebuild flex group children order
    for (auto& n : nodes_) {
        group_->remove_child(n->flexNode());
    }
    for (auto& n : nodes_) {
        if (auto* shapeNode = dynamic_cast<ShapeNode*>(n.get())) {
            group_->add_child(shapeNode->shapePtr());
        } else if (auto* groupNode = dynamic_cast<GroupNode*>(n.get())) {
            group_->add_child(groupNode->groupPtr());
        } else if (auto* textNode = dynamic_cast<TextNode*>(n.get())) {
            group_->add_child(textNode->textPtr());
        }
    }

    notify(EventType::LayerChanged, this);
}

EditorNode::Ptr Layer::nodeAt(const Point& p) const {
    // Search in reverse order (top to bottom)
    for (auto it = nodes_.rbegin(); it != nodes_.rend(); ++it) {
        if (!(*it)->visible() || (*it)->locked()) continue;
        if ((*it)->bounds().contains(p)) {
            return *it;
        }
    }
    return nullptr;
}

} // namespace editor
