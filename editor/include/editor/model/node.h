/*
 * Editor Node
 *
 * Base class for all editable objects in the editor.
 * Wraps flex::Node with editor-specific functionality.
 * Uses Transform2D for unified coordinate transforms.
 */

#pragma once

#include "../core/types.h"
#include "../core/observable.h"
#include "../core/transform.h"
#include <flex/flex.h>
#include <string>
#include <memory>
#include <vector>
#include <limits>

namespace editor {

// Forward declarations
class Layer;

struct PropertyDescriptor {
    std::string name;
    std::string label;
    std::string type; // "float", "int", "color", "string", "bool", "enum"
    
    // Constraints
    float min = -std::numeric_limits<float>::max();
    float max = std::numeric_limits<float>::max();
    float step = 1.0f;
    
    // Enum options
    std::vector<std::string> options;
    
    // Constructors for convenience
    static PropertyDescriptor Float(const std::string& name, const std::string& label, float min = -1e30f, float max = 1e30f, float step = 1.0f) {
        PropertyDescriptor pd;
        pd.name = name; pd.label = label; pd.type = "float"; pd.min = min; pd.max = max; pd.step = step;
        return pd;
    }
    
    static PropertyDescriptor Color(const std::string& name, const std::string& label) {
        PropertyDescriptor pd;
        pd.name = name; pd.label = label; pd.type = "color";
        return pd;
    }
    
    static PropertyDescriptor String(const std::string& name, const std::string& label) {
        PropertyDescriptor pd;
        pd.name = name; pd.label = label; pd.type = "string";
        return pd;
    }
    
    static PropertyDescriptor Bool(const std::string& name, const std::string& label) {
        PropertyDescriptor pd;
        pd.name = name; pd.label = label; pd.type = "bool";
        return pd;
    }

    static PropertyDescriptor Enum(const std::string& name, const std::string& label, const std::vector<std::string>& options) {
        PropertyDescriptor pd;
        pd.name = name; pd.label = label; pd.type = "enum"; pd.options = options;
        return pd;
    }
};

class EditorNode : public Observable, public std::enable_shared_from_this<EditorNode> {
public:
    using Ptr = std::shared_ptr<EditorNode>;

    explicit EditorNode(flex::Node* node);
    virtual ~EditorNode() = default;

    // Introspection
    virtual std::vector<PropertyDescriptor> getComponents() const; // For future ECS-like expansion
    virtual std::vector<PropertyDescriptor> getProperties() const; // List of editable properties
    
    // Generic Access
    virtual void setPropertyValue(const std::string& name, const std::string& value);
    virtual std::string getPropertyValue(const std::string& name) const;

    // Access underlying flex node
    flex::Node* flexNode() const { return flex_node_; }

    // Identity
    const std::string& id() const { return id_; }
    void setId(const std::string& id) { id_ = id; }

    const std::string& name() const { return name_; }
    void setName(const std::string& name);

    // Transform - unified Transform2D interface
    const Transform2D& transform() const { return transform_; }
    void setTransform(const Transform2D& t);

    // Convenience accessors
    float x() const { return transform_.tx; }
    float y() const { return transform_.ty; }
    float rotation() const;
    float scaleX() const;
    float scaleY() const;

    // Convenience setters
    void setPosition(float x, float y);
    void setRotation(float degrees);
    void setScale(float sx, float sy);

    // Transform operations
    Transform2D translated(float dx, float dy) const;
    Transform2D rotatedAround(float degrees, const Point& pivot) const;
    Transform2D scaledAround(float sx, float sy, const Point& pivot) const;
    void applyTransform(const Transform2D& t);

    // Opacity
    float opacity() const;
    void setOpacity(float o);

    // Visibility
    bool visible() const;
    void setVisible(bool v);

    // Effects
    bool hasShadow() const;
    flex::Shadow shadow() const;
    void setShadow(const flex::Shadow& s);
    void setShadow(float ox, float oy, float blur, const Color& c);
    void clearShadow();

    bool hasBlur() const;
    flex::BlurFilter blur() const;
    void setBlur(const flex::BlurFilter& b);
    void setBlur(float radius);
    void clearBlur();

    // Bounds
    Rect localBounds() const;
    Rect bounds() const;

    // Lock/unlock
    bool locked() const { return locked_; }
    void setLocked(bool v);

    // Selection state
    bool selected() const { return selected_; }
    void setSelected(bool v);

    // Parent layer
    Layer* layer() const { return layer_; }
    void setLayer(Layer* layer) { layer_ = layer; }

    // Sync transform with flex node
    void syncTransformToFlex();
    void syncTransformFromFlex();

protected:

    flex::Node* flex_node_ = nullptr;
    Transform2D transform_;
    std::string id_;
    std::string name_;
    bool locked_ = false;
    bool selected_ = false;
    Layer* layer_ = nullptr;
};

// Shape node - wraps flex::Shape
class ShapeNode : public EditorNode {
public:
    using Ptr = std::shared_ptr<ShapeNode>;

    explicit ShapeNode(flex::Shape* shape);
    static Ptr create(flex::Shape::Ptr shape);

    flex::Shape* shape() const { return shape_; }
    flex::Shape::Ptr shapePtr() const { return owned_shape_; }

    // Geometry type
    flex::GeometryType geometryType() const;
    
    // Introspection
    std::vector<PropertyDescriptor> getProperties() const override;
    void setPropertyValue(const std::string& name, const std::string& value) override;
    std::string getPropertyValue(const std::string& name) const override;

    // Fill - unified Paint API
    bool hasFill() const;
    flex::Fill fill() const;
    void setFill(const flex::Paint& paint);
    void setFillColor(const Color& c);
    void setFillLinearGradient(const flex::LinearGradient& gradient);
    void setFillRadialGradient(const flex::RadialGradient& gradient);
    void clearFill();

    // Stroke - unified Paint API
    bool hasStroke() const;
    flex::Stroke stroke() const;
    void setStroke(const flex::Paint& paint, float width = 1.0f);
    void setStrokeColor(const Color& c, float width = 1.0f);
    void setStrokeLinearGradient(const flex::LinearGradient& gradient, float width = 1.0f);
    void setStrokeRadialGradient(const flex::RadialGradient& gradient, float width = 1.0f);
    void clearStroke();

private:
    flex::Shape* shape_;
    flex::Shape::Ptr owned_shape_;
};

// Group node - wraps flex::Group
class GroupNode : public EditorNode {
public:
    using Ptr = std::shared_ptr<GroupNode>;

    explicit GroupNode(flex::Group* group);
    static Ptr create(flex::Group::Ptr group);

    flex::Group* group() const { return group_; }
    flex::Group::Ptr groupPtr() const { return owned_group_; }

    const std::vector<EditorNode::Ptr>& children() const { return children_; }

    void addChild(EditorNode::Ptr child);
    void removeChild(EditorNode::Ptr child);

    // Introspection
    std::vector<PropertyDescriptor> getProperties() const override;
    std::string getPropertyValue(const std::string& name) const override;

private:
    flex::Group* group_;
    flex::Group::Ptr owned_group_;
    std::vector<EditorNode::Ptr> children_;
};

// Text node - wraps flex::Text
class TextNode : public EditorNode {
public:
    using Ptr = std::shared_ptr<TextNode>;

    explicit TextNode(flex::Text* text);
    static Ptr create(flex::Text::Ptr text);

    flex::Text* text() const { return text_; }
    flex::Text::Ptr textPtr() const { return owned_text_; }

    // Introspection
    std::vector<PropertyDescriptor> getProperties() const override;
    void setPropertyValue(const std::string& name, const std::string& value) override;
    std::string getPropertyValue(const std::string& name) const override;

    // Content
    const std::string& content() const;
    void setContent(const std::string& content);

    // Font
    const std::string& fontFamily() const;
    void setFontFamily(const std::string& family);

    float fontSize() const;
    void setFontSize(float size);

    bool isBold() const;
    void setBold(bool bold);

    bool isItalic() const;
    void setItalic(bool italic);

    // Color
    Color textColor() const;
    void setTextColor(const Color& c);

    // Alignment
    flex::TextAlign textAlign() const;
    void setTextAlign(flex::TextAlign align);

private:
    flex::Text* text_;
    flex::Text::Ptr owned_text_;
};

} // namespace editor
