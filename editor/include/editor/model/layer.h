/*
 * Layer
 *
 * A layer contains multiple nodes and manages their z-order.
 * Layers can be locked, hidden, and have opacity.
 */

#pragma once

#include "../core/types.h"
#include "../core/observable.h"
#include "node.h"
#include <vector>
#include <string>
#include <memory>

namespace editor {

class Layer : public Observable {
public:
    using Ptr = std::shared_ptr<Layer>;

    explicit Layer(const std::string& name = "Layer");
    static Ptr create(const std::string& name = "Layer");

    // Properties
    const std::string& name() const { return name_; }
    void setName(const std::string& name);

    bool visible() const { return visible_; }
    void setVisible(bool v);

    bool locked() const { return locked_; }
    void setLocked(bool v);

    float opacity() const { return opacity_; }
    void setOpacity(float o);

    // Access underlying flex group
    flex::Group* flexGroup() const { return group_.get(); }
    flex::Group::Ptr flexGroupPtr() const { return group_; }

    // Node management
    const std::vector<EditorNode::Ptr>& nodes() const { return nodes_; }
    void addNode(EditorNode::Ptr node);
    void removeNode(EditorNode::Ptr node);
    void moveNode(EditorNode::Ptr node, size_t newIndex);

    // Find node at point
    EditorNode::Ptr nodeAt(const Point& p) const;

private:
    std::string name_;
    bool visible_ = true;
    bool locked_ = false;
    float opacity_ = 1.0f;

    flex::Group::Ptr group_;
    std::vector<EditorNode::Ptr> nodes_;
};

} // namespace editor
