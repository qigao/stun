/*
 * Editor Node Implementation
 */

#include <editor/model/node.h>
#include <cmath>
#include <algorithm>

namespace editor {

// ============================================================================
// EditorNode
// ============================================================================

namespace {
    // Helper to parse hex color #RRGGBB or #RRGGBBAA
    editor::Color hexToColor(const std::string& hex) {
        if (hex.empty() || hex[0] != '#') return {0.0f, 0.0f, 0.0f, 1.0f};
        
        std::string raw = hex.substr(1);
        if (raw.length() == 3) {
             std::string expanded;
             for (char c : raw) { expanded += c; expanded += c; }
             raw = expanded;
        }

        if (raw.length() == 6) raw += "FF"; 

        if (raw.length() != 8) return {0.0f, 0.0f, 0.0f, 1.0f};

        unsigned int r, g, b, a;
        if (sscanf(raw.c_str(), "%02x%02x%02x%02x", &r, &g, &b, &a) != 4) return {0.0f, 0.0f, 0.0f, 1.0f};

        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }

    std::string colorToHex(const editor::Color& c) {
        char buf[10];
        uint8_t r = static_cast<uint8_t>(c.r * 255.0f);
        uint8_t g = static_cast<uint8_t>(c.g * 255.0f);
        uint8_t b = static_cast<uint8_t>(c.b * 255.0f);
        uint8_t a = static_cast<uint8_t>(c.a * 255.0f);
        
        if (a == 255) sprintf(buf, "#%02x%02x%02x", r, g, b);
        else sprintf(buf, "#%02x%02x%02x%02x", r, g, b, a);
        return std::string(buf);
    }
}

EditorNode::EditorNode(flex::Node* node) : flex_node_(node) {}

void EditorNode::setName(const std::string& name) {
    name_ = name;
    notify(EventType::NodeModified, this);
}

void EditorNode::setTransform(const Transform2D& t) {
    transform_ = t;
    syncTransformToFlex();
    notify(EventType::NodeModified, this);
}

float EditorNode::rotation() const {
    return transform_.rotationDegrees();
}

float EditorNode::scaleX() const {
    return transform_.scaleX();
}

float EditorNode::scaleY() const {
    return transform_.scaleY();
}

void EditorNode::setPosition(float x, float y) {
    transform_.tx = x;
    transform_.ty = y;
    syncTransformToFlex();
    notify(EventType::NodeModified, this);
}

void EditorNode::setRotation(float degrees) {
    float sx = scaleX();
    float sy = scaleY();
    float rad = degrees * 3.14159265f / 180.0f;
    float c = std::cos(rad);
    float s = std::sin(rad);
    transform_.a = sx * c;
    transform_.b = sx * s;
    transform_.c = -sy * s;
    transform_.d = sy * c;
    syncTransformToFlex();
    notify(EventType::NodeModified, this);
}

void EditorNode::setScale(float sx, float sy) {
    float rad = transform_.rotationRadians();
    float c = std::cos(rad);
    float s = std::sin(rad);
    transform_.a = sx * c;
    transform_.b = sx * s;
    transform_.c = -sy * s;
    transform_.d = sy * c;
    syncTransformToFlex();
    notify(EventType::NodeModified, this);
}

Transform2D EditorNode::translated(float dx, float dy) const {
    return Transform2D::translation(dx, dy) * transform_;
}

Transform2D EditorNode::rotatedAround(float degrees, const Point& pivot) const {
    return Transform2D::rotationAround(degrees * 3.14159265f / 180.0f, pivot) * transform_;
}

Transform2D EditorNode::scaledAround(float sx, float sy, const Point& pivot) const {
    return Transform2D::scaleAround(sx, sy, pivot) * transform_;
}

void EditorNode::applyTransform(const Transform2D& t) {
    transform_ = t * transform_;
    syncTransformToFlex();
    notify(EventType::NodeModified, this);
}

float EditorNode::opacity() const {
    return flex_node_->opacity();
}

void EditorNode::setOpacity(float o) {
    flex_node_->set_opacity(o);
    notify(EventType::NodeModified, this);
}

bool EditorNode::visible() const {
    return flex_node_->visible();
}

void EditorNode::setVisible(bool v) {
    flex_node_->set_visible(v);
    notify(EventType::NodeModified, this);
}

bool EditorNode::hasShadow() const {
    return flex_node_->has_shadow();
}

flex::Shadow EditorNode::shadow() const {
    return flex_node_->shadow();
}

void EditorNode::setShadow(const flex::Shadow& s) {
    flex_node_->set_shadow(s);
    notify(EventType::NodeModified, this);
}

void EditorNode::setShadow(float ox, float oy, float blur, const Color& c) {
    flex_node_->set_shadow(ox, oy, blur, flex::Color{c.r, c.g, c.b, c.a});
    notify(EventType::NodeModified, this);
}

void EditorNode::clearShadow() {
    flex_node_->set_shadow(flex::Shadow{});
    notify(EventType::NodeModified, this);
}

bool EditorNode::hasBlur() const {
    return flex_node_->has_blur();
}

flex::BlurFilter EditorNode::blur() const {
    return flex_node_->blur();
}

void EditorNode::setBlur(const flex::BlurFilter& b) {
    flex_node_->set_blur(b);
    notify(EventType::NodeModified, this);
}

void EditorNode::setBlur(float radius) {
    flex_node_->set_blur(radius);
    notify(EventType::NodeModified, this);
}

void EditorNode::clearBlur() {
    flex_node_->set_blur(flex::BlurFilter{});
    notify(EventType::NodeModified, this);
}

Rect EditorNode::localBounds() const {
    auto b = flex_node_->bounds();
    // Return bounds in local space (relative to node position)
    // For rect: position is top-left, so local = {0, 0, w, h}
    // For circle: position is center, bounds.x = x - radius, so local = {-r, -r, 2r, 2r}
    return {b.x - flex_node_->x(), b.y - flex_node_->y(), b.width, b.height};
}

Rect EditorNode::bounds() const {
    return transform_.applyToRect(localBounds());
}

void EditorNode::setLocked(bool v) {
    locked_ = v;
    notify(EventType::NodeModified, this);
}

void EditorNode::setSelected(bool v) {
    selected_ = v;
    notify(EventType::SelectionChanged, this);
}

void EditorNode::syncTransformToFlex() {
    flex_node_->set_position(transform_.tx, transform_.ty);
    flex_node_->set_rotation(transform_.rotationDegrees());
    flex_node_->set_scale(transform_.scaleX(), transform_.scaleY());
}

void EditorNode::syncTransformFromFlex() {
    float x = flex_node_->x();
    float y = flex_node_->y();
    float rot = flex_node_->rotation();
    float sx = flex_node_->scale_x();
    float sy = flex_node_->scale_y();

    float rad = rot * 3.14159265f / 180.0f;
    float c = std::cos(rad);
    float s = std::sin(rad);

    transform_.a = sx * c;
    transform_.b = sx * s;
    transform_.c = -sy * s;
    transform_.d = sy * c;
    transform_.tx = x;
    transform_.ty = y;
}

// Introspection Implementation
std::vector<PropertyDescriptor> EditorNode::getComponents() const {
    return {}; // Future expansion
}

std::vector<PropertyDescriptor> EditorNode::getProperties() const {
    std::vector<PropertyDescriptor> props;
    props.push_back(PropertyDescriptor::String("name", "Name"));
    props.push_back(PropertyDescriptor::Float("x", "X"));
    props.push_back(PropertyDescriptor::Float("y", "Y"));
    props.push_back(PropertyDescriptor::Float("rotation", "Rotation"));
    props.push_back(PropertyDescriptor::Float("scaleX", "Scale X", 0.01f, 100.0f, 0.1f));
    props.push_back(PropertyDescriptor::Float("scaleY", "Scale Y", 0.01f, 100.0f, 0.1f));
    props.push_back(PropertyDescriptor::Float("opacity", "Opacity", 0.0f, 1.0f, 0.05f));
    props.push_back(PropertyDescriptor::Bool("visible", "Visible"));
    props.push_back(PropertyDescriptor::Bool("locked", "Locked"));
    
    // Shadow
    props.push_back(PropertyDescriptor::Bool("hasShadow", "Shadow"));
    if (hasShadow()) {
        props.push_back(PropertyDescriptor::Color("shadowColor", "S-Color"));
        props.push_back(PropertyDescriptor::Float("shadowX", "S-X"));
        props.push_back(PropertyDescriptor::Float("shadowY", "S-Y"));
        props.push_back(PropertyDescriptor::Float("shadowBlur", "S-Blur", 0.0f));
    }

    // Blur
    props.push_back(PropertyDescriptor::Bool("hasBlur", "Blur"));
    if (hasBlur()) {
        props.push_back(PropertyDescriptor::Float("blurRadius", "B-Radius", 0.0f, 100.0f));
    }

    // Layout
    props.push_back(PropertyDescriptor::Float("layoutWidth", "Width"));
    props.push_back(PropertyDescriptor::Float("layoutHeight", "Height"));
    props.push_back(PropertyDescriptor::Float("flexGrow", "Grow", 0.0f, 100.0f));
    props.push_back(PropertyDescriptor::Float("flexShrink", "Shrink", 0.0f, 100.0f));
    props.push_back(PropertyDescriptor::Float("flexBasis", "Basis"));
    
    std::vector<std::string> alignSelfOptions = {"Auto", "Start", "End", "Center", "Stretch"};
    props.push_back(PropertyDescriptor::Enum("alignSelf", "Align Self", alignSelfOptions));

    return props;
}

void EditorNode::setPropertyValue(const std::string& name, const std::string& value) {
    if (name == "name") setName(value);
    else if (name == "x") setPosition(std::stof(value), y());
    else if (name == "y") setPosition(x(), std::stof(value));
    else if (name == "rotation") setRotation(std::stof(value));
    else if (name == "scaleX") setScale(std::stof(value), scaleY());
    else if (name == "scaleY") setScale(scaleX(), std::stof(value));
    else if (name == "opacity") setOpacity(std::stof(value));
    else if (name == "visible") setVisible(value == "true");
    else if (name == "locked") setLocked(value == "true");
    
    // Shadow
    else if (name == "hasShadow") {
        if (value == "true" && !hasShadow()) {
            setShadow(0, 5, 10, {0,0,0,0.5f}); // Default shadow
        } else if (value == "false") {
            clearShadow();
        }
    }
    else if (hasShadow()) {
        auto s = shadow();
        if (name == "shadowColor") {
             auto c = hexToColor(value);
             setShadow(s.offset_x, s.offset_y, s.blur, c);
        } else if (name == "shadowX") {
             setShadow(std::stof(value), s.offset_y, s.blur, {s.color.r, s.color.g, s.color.b, s.color.a});
        } else if (name == "shadowY") {
             setShadow(s.offset_x, std::stof(value), s.blur, {s.color.r, s.color.g, s.color.b, s.color.a});
        } else if (name == "shadowBlur") {
             setShadow(s.offset_x, s.offset_y, std::stof(value), {s.color.r, s.color.g, s.color.b, s.color.a});
        }
    }

    // Blur
    else if (name == "hasBlur") {
        if (value == "true" && !hasBlur()) {
            setBlur(10.0f); // Default blur
        } else if (value == "false") {
            clearBlur();
        }
    }
    else if (hasBlur() && name == "blurRadius") {
        setBlur(std::stof(value));
    }
    
    // Layout
    else if (name == "layoutWidth") flex_node_->set_layout_width(std::stof(value));
    else if (name == "layoutHeight") flex_node_->set_layout_height(std::stof(value));
    else if (name == "flexGrow") flex_node_->set_flex_grow(std::stof(value));
    else if (name == "flexShrink") flex_node_->set_flex_shrink(std::stof(value));
    else if (name == "flexBasis") flex_node_->set_flex_basis(std::stof(value));
    else if (name == "alignSelf") {
        if (value == "Auto") flex_node_->set_align_self(flex::AlignSelf::Auto);
        else if (value == "Start") flex_node_->set_align_self(flex::AlignSelf::Start);
        else if (value == "End") flex_node_->set_align_self(flex::AlignSelf::End);
        else if (value == "Center") flex_node_->set_align_self(flex::AlignSelf::Center);
        else if (value == "Stretch") flex_node_->set_align_self(flex::AlignSelf::Stretch);
    }
}

std::string EditorNode::getPropertyValue(const std::string& name) const {
    if (name == "name") return name_;
    if (name == "x") return std::to_string(x());
    if (name == "y") return std::to_string(y());
    if (name == "rotation") return std::to_string(rotation());
    if (name == "scaleX") return std::to_string(scaleX());
    if (name == "scaleY") return std::to_string(scaleY());
    if (name == "opacity") return std::to_string(opacity());
    if (name == "visible") return visible() ? "true" : "false";
    if (name == "locked") return locked() ? "true" : "false";
    
    if (name == "hasShadow") return hasShadow() ? "true" : "false";
    if (hasShadow()) {
         auto s = shadow();
         if (name == "shadowColor") return colorToHex({s.color.r, s.color.g, s.color.b, s.color.a});
         if (name == "shadowX") return std::to_string(s.offset_x);
         if (name == "shadowY") return std::to_string(s.offset_y);
         if (name == "shadowBlur") return std::to_string(s.blur);
    }
    
    if (name == "hasBlur") return hasBlur() ? "true" : "false";
    if (hasBlur()) {
         if (name == "blurRadius") return std::to_string(blur().radius);
    }

    // Layout
    if (name == "layoutWidth") return std::to_string(flex_node_->layout_width());
    if (name == "layoutHeight") return std::to_string(flex_node_->layout_height());
    if (name == "flexGrow") return std::to_string(flex_node_->flex_grow());
    if (name == "flexShrink") return std::to_string(flex_node_->flex_shrink());
    if (name == "flexBasis") return std::to_string(flex_node_->flex_basis());
    if (name == "alignSelf") {
        switch (flex_node_->align_self()) {
            case flex::AlignSelf::Auto: return "Auto";
            case flex::AlignSelf::Start: return "Start";
            case flex::AlignSelf::End: return "End";
            case flex::AlignSelf::Center: return "Center";
            case flex::AlignSelf::Stretch: return "Stretch";
        }
    }

    return "";
}

// ============================================================================
// ShapeNode
// ============================================================================

ShapeNode::ShapeNode(flex::Shape* shape)
    : EditorNode(shape), shape_(shape) {
    syncTransformFromFlex();  // Initialize transform from flex node
}

ShapeNode::Ptr ShapeNode::create(flex::Shape::Ptr shape) {
    auto node = std::make_shared<ShapeNode>(shape.get());
    node->owned_shape_ = shape;
    return node;
}

flex::GeometryType ShapeNode::geometryType() const {
    return shape_->geometry_type();
}

bool ShapeNode::hasFill() const {
    return shape_->has_fill();
}

flex::Fill ShapeNode::fill() const {
    return shape_->fill();
}

void ShapeNode::setFill(const flex::Paint& paint) {
    switch (paint.type) {
        case flex::Paint::Type::Solid:
            shape_->set_fill(paint.color);
            break;
        case flex::Paint::Type::Linear:
            shape_->set_fill(paint.linear);
            break;
        case flex::Paint::Type::Radial:
            shape_->set_fill(paint.radial);
            break;
    }
    notify(EventType::NodeModified, this);
}

void ShapeNode::setFillColor(const Color& c) {
    shape_->set_fill(flex::Color{c.r, c.g, c.b, c.a});
    notify(EventType::NodeModified, this);
}

void ShapeNode::setFillLinearGradient(const flex::LinearGradient& gradient) {
    shape_->set_fill(gradient);
    notify(EventType::NodeModified, this);
}

void ShapeNode::setFillRadialGradient(const flex::RadialGradient& gradient) {
    shape_->set_fill(gradient);
    notify(EventType::NodeModified, this);
}

void ShapeNode::clearFill() {
    shape_->clear_fill();
    notify(EventType::NodeModified, this);
}

bool ShapeNode::hasStroke() const {
    return shape_->has_stroke();
}

flex::Stroke ShapeNode::stroke() const {
    return shape_->stroke();
}

void ShapeNode::setStroke(const flex::Paint& paint, float width) {
    switch (paint.type) {
        case flex::Paint::Type::Solid:
            shape_->set_stroke(paint.color, width);
            break;
        case flex::Paint::Type::Linear:
            shape_->set_stroke(paint.linear, width);
            break;
        case flex::Paint::Type::Radial:
            shape_->set_stroke(paint.radial, width);
            break;
    }
    notify(EventType::NodeModified, this);
}

void ShapeNode::setStrokeColor(const Color& c, float width) {
    shape_->set_stroke(flex::Color{c.r, c.g, c.b, c.a}, width);
    notify(EventType::NodeModified, this);
}

void ShapeNode::setStrokeLinearGradient(const flex::LinearGradient& gradient, float width) {
    shape_->set_stroke(gradient, width);
    notify(EventType::NodeModified, this);
}

void ShapeNode::setStrokeRadialGradient(const flex::RadialGradient& gradient, float width) {
    shape_->set_stroke(gradient, width);
    notify(EventType::NodeModified, this);
}

void ShapeNode::clearStroke() {
    shape_->clear_stroke();
    notify(EventType::NodeModified, this);
}



// ShapeNode Introspection
std::vector<PropertyDescriptor> ShapeNode::getProperties() const {
    auto props = EditorNode::getProperties(); // Base properties
    
    // Geometry specific
    auto type = geometryType();
    if (type == flex::GeometryType::Rect) {
        props.push_back(PropertyDescriptor::Float("width", "Width", 0.0f));
        props.push_back(PropertyDescriptor::Float("height", "Height", 0.0f));
        props.push_back(PropertyDescriptor::Float("cornerRadius", "Corner Radius", 0.0f));
    } else if (type == flex::GeometryType::Circle) {
        props.push_back(PropertyDescriptor::Float("radius", "Radius", 0.0f));
    }
    
    // Fill
    if (hasFill()) {
         props.push_back(PropertyDescriptor::Color("fillColor", "Fill Color"));
    }
    
    // Stroke
    if (hasStroke()) {
        props.push_back(PropertyDescriptor::Color("strokeColor", "Stroke Color"));
        props.push_back(PropertyDescriptor::Float("strokeWidth", "Stroke Width", 0.0f, 100.0f));
    }
    
    return props;
}

void ShapeNode::setPropertyValue(const std::string& name, const std::string& value) {
    // Check base properties first
    EditorNode::setPropertyValue(name, value);
    
    try {
        if (name == "fillColor") {
             setFillColor(hexToColor(value));
        }
        else if (name == "strokeColor") {
             setStrokeColor(hexToColor(value));
        }
        else if (name == "width" && geometryType() == flex::GeometryType::Rect) {
             auto r = shape_->rect();
             shape_->set_rect(std::stof(value), r.height, r.corner_radius);
        }
        else if (name == "height" && geometryType() == flex::GeometryType::Rect) {
             auto r = shape_->rect();
             shape_->set_rect(r.width, std::stof(value), r.corner_radius);
        }
        else if (name == "cornerRadius" && geometryType() == flex::GeometryType::Rect) {
             auto r = shape_->rect();
             shape_->set_rect(r.width, r.height, std::stof(value));
        }
        else if (name == "radius" && geometryType() == flex::GeometryType::Circle) {
             // Assuming circle setter API exists or needed
             // shape_->set_circle(std::stof(value)); 
        }
        else if (name == "strokeWidth") {
             auto s = stroke();
             if (s.type == flex::StrokeType::Solid) {
                  setStrokeColor({s.color.r, s.color.g, s.color.b, s.color.a}, std::stof(value));
             }
        }
    } catch (...) {}
}

std::string ShapeNode::getPropertyValue(const std::string& name) const {
    if (name == "width" && geometryType() == flex::GeometryType::Rect) return std::to_string(shape_->rect().width);
    if (name == "height" && geometryType() == flex::GeometryType::Rect) return std::to_string(shape_->rect().height);
    if (name == "cornerRadius" && geometryType() == flex::GeometryType::Rect) return std::to_string(shape_->rect().corner_radius);
    
    if (name == "fillColor" && hasFill()) {
         auto f = fill();
         if (f.type == flex::FillType::Solid) {
             return colorToHex({f.color.r, f.color.g, f.color.b, f.color.a});
         }
    }
    if (name == "strokeColor" && hasStroke()) {
         auto s = stroke();
         if (s.type == flex::StrokeType::Solid) {
             return colorToHex({s.color.r, s.color.g, s.color.b, s.color.a});
         }
    }
    if (name == "strokeWidth" && hasStroke()) {
         return std::to_string(stroke().width);
    }
    
    return EditorNode::getPropertyValue(name);
}

// ============================================================================
// GroupNode
// ============================================================================

GroupNode::GroupNode(flex::Group* group)
    : EditorNode(group), group_(group) {
    syncTransformFromFlex();  // Initialize transform from flex node
}

GroupNode::Ptr GroupNode::create(flex::Group::Ptr group) {
    auto node = std::make_shared<GroupNode>(group.get());
    node->owned_group_ = group;
    return node;
}

void GroupNode::addChild(EditorNode::Ptr child) {
    children_.push_back(child);
    if (auto* shapeNode = dynamic_cast<ShapeNode*>(child.get())) {
        group_->add_child(shapeNode->shapePtr());
    } else if (auto* groupChild = dynamic_cast<GroupNode*>(child.get())) {
        group_->add_child(groupChild->groupPtr());
    } else if (auto* textNode = dynamic_cast<TextNode*>(child.get())) {
        group_->add_child(textNode->textPtr());
    }
    notify(EventType::NodeAdded, child.get());
}

void GroupNode::removeChild(EditorNode::Ptr child) {
    children_.erase(
        std::remove(children_.begin(), children_.end(), child),
        children_.end());
    group_->remove_child(child->flexNode());
    notify(EventType::NodeRemoved, child.get());
}


std::vector<PropertyDescriptor> GroupNode::getProperties() const {
    auto props = EditorNode::getProperties();
    // Read-only property for child count
    // We don't have a specific ReadOnly flag in PropertyDescriptor yet, 
    // but typically "string" or "int" without a range works as input.
    // Ideally we want it disabled. But for now just show it.
    props.push_back(PropertyDescriptor::String("childCount", "Children")); 
    return props;
}

std::string GroupNode::getPropertyValue(const std::string& name) const {
    if (name == "childCount") return std::to_string(children_.size());
    return EditorNode::getPropertyValue(name);
}


// ============================================================================
// TextNode
// ============================================================================

TextNode::TextNode(flex::Text* text)
    : EditorNode(text), text_(text) {
    syncTransformFromFlex();
}

TextNode::Ptr TextNode::create(flex::Text::Ptr text) {
    auto node = std::make_shared<TextNode>(text.get());
    node->owned_text_ = text;
    return node;
}

const std::string& TextNode::content() const {
    return text_->content();
}

void TextNode::setContent(const std::string& content) {
    text_->set_content(content);
    notify(EventType::NodeModified, this);
}

const std::string& TextNode::fontFamily() const {
    return text_->font_family();
}

void TextNode::setFontFamily(const std::string& family) {
    text_->set_font_family(family);
    notify(EventType::NodeModified, this);
}

float TextNode::fontSize() const {
    return text_->font_size();
}

void TextNode::setFontSize(float size) {
    text_->set_font_size(size);
    notify(EventType::NodeModified, this);
}

bool TextNode::isBold() const {
    return text_->font_weight() == flex::FontWeight::Bold;
}

void TextNode::setBold(bool bold) {
    text_->set_font_weight(bold ? flex::FontWeight::Bold : flex::FontWeight::Normal);
    notify(EventType::NodeModified, this);
}

bool TextNode::isItalic() const {
    return text_->font_style() == flex::FontStyle::Italic;
}

void TextNode::setItalic(bool italic) {
    text_->set_font_style(italic ? flex::FontStyle::Italic : flex::FontStyle::Normal);
    notify(EventType::NodeModified, this);
}

Color TextNode::textColor() const {
    auto c = text_->color();
    return {c.r, c.g, c.b, c.a};
}

void TextNode::setTextColor(const Color& c) {
    text_->set_color(flex::Color{c.r, c.g, c.b, c.a});
    notify(EventType::NodeModified, this);
}

flex::TextAlign TextNode::textAlign() const {
    return text_->text_align();
}

void TextNode::setTextAlign(flex::TextAlign align) {
    text_->set_text_align(align);
    notify(EventType::NodeModified, this);
}

// TextNode Introspection
std::vector<PropertyDescriptor> TextNode::getProperties() const {
    auto props = EditorNode::getProperties();
    props.push_back(PropertyDescriptor::String("content", "Content"));
    props.push_back(PropertyDescriptor::Float("fontSize", "Font Size", 1.0f, 200.0f));
    props.push_back(PropertyDescriptor::Color("textColor", "Color"));
    return props;
}

void TextNode::setPropertyValue(const std::string& name, const std::string& value) {
    EditorNode::setPropertyValue(name, value);
    if (name == "content") setContent(value);
    else if (name == "fontSize") setFontSize(std::stof(value));
    else if (name == "textAlign") {
        if (value == "Left") setTextAlign(flex::TextAlign::Left);
        else if (value == "Center") setTextAlign(flex::TextAlign::Center);
        else if (value == "Right") setTextAlign(flex::TextAlign::Right);
    }
}

std::string TextNode::getPropertyValue(const std::string& name) const {
    if (name == "content") return content();
    if (name == "fontSize") return std::to_string(fontSize());
    if (name == "textAlign") {
        switch (textAlign()) {
            case flex::TextAlign::Left: return "Left";
            case flex::TextAlign::Center: return "Center";
            case flex::TextAlign::Right: return "Right";
        }
    }
    return EditorNode::getPropertyValue(name);
}

} // namespace editor
