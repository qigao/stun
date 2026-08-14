/*
 * Flex Engine - Binary Writer Implementation
 */

#include "flex/binary/writer.h"
#include "flex/binary/format.h"
#include "flex/core/group.h"
#include "flex/core/image.h"
#include "flex/core/instance.h"
#include "flex/core/shape.h"
#include "flex/core/svg.h"
#include "flex/core/text.h"
#include <cstring>
#include <algorithm>
#include <zstd.h>

namespace flex {
namespace binary {

namespace {

void append_bytes(std::vector<uint8_t>& buffer, const void* data, size_t size) {
    const auto* ptr = static_cast<const uint8_t*>(data);
    buffer.insert(buffer.end(), ptr, ptr + size);
}

uint32_t write_linear_gradient(std::vector<uint8_t>& buffer, const LinearGradient& gradient) {
    const auto offset = static_cast<uint32_t>(buffer.size());
    LinearGradientPaint paint = {};
    paint.type = PaintType::LinearGradient;
    paint.x1 = gradient.x1;
    paint.y1 = gradient.y1;
    paint.x2 = gradient.x2;
    paint.y2 = gradient.y2;
    paint.stop_count = static_cast<uint32_t>(gradient.stops.size());
    append_bytes(buffer, &paint, sizeof(LinearGradientPaint));
    for (const auto& stop : gradient.stops) {
        binary::ColorStop binary_stop = {};
        binary_stop.offset = stop.offset;
        binary_stop.color = {stop.color.r, stop.color.g, stop.color.b, stop.color.a};
        append_bytes(buffer, &binary_stop, sizeof(binary::ColorStop));
    }
    return offset;
}

uint32_t write_radial_gradient(std::vector<uint8_t>& buffer, const RadialGradient& gradient) {
    const auto offset = static_cast<uint32_t>(buffer.size());
    RadialGradientPaint paint = {};
    paint.type = PaintType::RadialGradient;
    paint.cx = gradient.cx;
    paint.cy = gradient.cy;
    paint.radius = gradient.radius;
    paint.fx = gradient.fx;
    paint.fy = gradient.fy;
    paint.stop_count = static_cast<uint32_t>(gradient.stops.size());
    append_bytes(buffer, &paint, sizeof(RadialGradientPaint));
    for (const auto& stop : gradient.stops) {
        binary::ColorStop binary_stop = {};
        binary_stop.offset = stop.offset;
        binary_stop.color = {stop.color.r, stop.color.g, stop.color.b, stop.color.a};
        append_bytes(buffer, &binary_stop, sizeof(binary::ColorStop));
    }
    return offset;
}

} // namespace

BinaryWriter::BinaryWriter() = default;

BinaryWriter::~BinaryWriter() = default;

StringIndex BinaryWriter::add_string(const std::string& str) {
    if (str.empty()) return 0xFFFFFFFF;

    auto it = string_table_.find(str);
    if (it != string_table_.end()) {
        return it->second;
    }

    uint32_t index = static_cast<uint32_t>(string_data_.size());
    string_data_.push_back(str);
    string_table_[str] = index;
    return index;
}

std::vector<uint8_t> BinaryWriter::write(Scene* scene) {
    return write(scene, {});
}

std::vector<uint8_t> BinaryWriter::write(Scene* scene,
                                          const std::vector<Timeline::SharedPtr>& timelines) {
    if (!scene) return {};

    // The current .flexb track record has no expression field. Refuse the
    // write instead of silently replacing MIR interpolation with linear
    // interpolation after a round trip.
    for (const auto& timeline : timelines) {
        if (!timeline) continue;
        if (timeline->trigger_count() != 0) {
            return {};
        }
        for (const auto& track : timeline->tracks()) {
            if (!track) continue;
            if (track->has_numeric_expression() ||
                track->spatial_interpolation() != SpatialInterpolation::Linear) {
                return {};
            }
            for (const auto& keyframe : track->keyframes()) {
                if (std::holds_alternative<Vec2>(keyframe.value)) {
                    return {};
                }
            }
        }
    }

    string_data_.clear();
    string_table_.clear();
    node_count_ = 0;

    std::vector<uint8_t> buffer;
    size_t header_offset = buffer.size();
    buffer.resize(buffer.size() + sizeof(FileHeader));

    size_t node_data_offset = buffer.size();
    size_t root_offset = buffer.size();
    buffer.resize(buffer.size() + sizeof(NodeHeader));
    node_count_++;

    NodeHeader root_header = {};
    root_header.type = NodeType::Scene;
    root_header.id = add_string("");
    root_header.flags = 1;
    root_header.transform[0] = 1.0f;
    root_header.transform[3] = 1.0f;
    root_header.opacity = 1.0f;
    root_header.scale_x = 1.0f;
    root_header.scale_y = 1.0f;
    root_header.child_count = static_cast<uint32_t>(scene->root()->child_count());
    root_header.data_offset = root_header.child_count > 0
        ? static_cast<uint32_t>(buffer.size())
        : 0;

    for (auto* child : scene->root()->children()) {
        write_node(buffer, child);
    }

    std::memcpy(buffer.data() + root_offset, &root_header, sizeof(NodeHeader));
    size_t node_data_size = buffer.size() - node_data_offset;

    size_t animation_data_offset = buffer.size();
    write_timelines(buffer, timelines);
    size_t animation_data_size = buffer.size() - animation_data_offset;

    size_t string_table_offset = buffer.size();
    write_string_table(buffer);
    size_t string_table_size = buffer.size() - string_table_offset;

    std::vector<uint8_t> payload(buffer.begin() + sizeof(FileHeader), buffer.end());
    bool compressed = false;
    if (compress_) {
        std::vector<uint8_t> compressed_payload = compress_data(payload);
        if (!compressed_payload.empty() && compressed_payload.size() < payload.size()) {
            buffer.resize(sizeof(FileHeader));
            buffer.insert(buffer.end(), compressed_payload.begin(), compressed_payload.end());
            compressed = true;
        }
    }

    size_t timeline_count = 0;
    for (const auto& timeline : timelines) {
        if (timeline) {
            ++timeline_count;
        }
    }

    FileHeader header = {};
    header.magic = MAGIC_NUMBER;
    header.version = FORMAT_VERSION;
    header.flags = compressed ? FLAG_COMPRESSED : FLAG_NONE;
    header.header_size = sizeof(FileHeader);
    header.string_table_offset = static_cast<uint32_t>(string_table_offset);
    header.string_table_size = static_cast<uint32_t>(string_table_size);
    header.node_data_offset = static_cast<uint32_t>(node_data_offset);
    header.node_data_size = static_cast<uint32_t>(node_data_size);
    header.asset_table_offset = 0;
    header.asset_table_size = 0;
    header.animation_data_offset = animation_data_size > 0
        ? static_cast<uint32_t>(animation_data_offset)
        : 0;
    header.animation_data_size = static_cast<uint32_t>(animation_data_size);
    header.fsm_data_offset = 0;
    header.fsm_data_size = 0;
    header.node_count = static_cast<uint32_t>(node_count_);
    header.asset_count = 0;
    header.timeline_count = static_cast<uint32_t>(timeline_count);
    header.fsm_count = 0;
    header.canvas_width = scene->width();
    header.canvas_height = scene->height();

    header.checksum = 0;
    std::memcpy(buffer.data() + header_offset, &header, sizeof(FileHeader));
    header.checksum = calculate_crc32(buffer.data(), buffer.size());
    std::memcpy(buffer.data() + header_offset, &header, sizeof(FileHeader));

    binary_size_ = buffer.size();
    return buffer;
}

void BinaryWriter::write_string_table(std::vector<uint8_t>& buffer) {
    for (const auto& str : string_data_) {
        uint32_t length = static_cast<uint32_t>(str.size());
        write_uint32(buffer, length);
        write_bytes(buffer, str.data(), str.size());
    }
}

void BinaryWriter::write_node(std::vector<uint8_t>& buffer, Node* node) {
    if (!node) return;

    node_count_++;

    NodeHeader header = {};
    header.flags = node->visible() ? 1 : 0;
    header.id = add_string(node->id());
    header.transform[0] = 1.0f;
    header.transform[3] = 1.0f;
    header.transform[4] = node->x();
    header.transform[5] = node->y();
    header.opacity = node->opacity();
    header.rotation = node->rotation();
    header.scale_x = node->scale_x();
    header.scale_y = node->scale_y();
    header.layout_width = node->layout_width();
    header.layout_height = node->layout_height();
    header.flex_grow = node->flex_grow();
    header.flex_shrink = node->flex_shrink();
    header.flex_basis = node->flex_basis();

    size_t header_offset = buffer.size();
    buffer.resize(buffer.size() + sizeof(NodeHeader));

    // Determine node type
    if (dynamic_cast<Group*>(node)) {
        header.type = NodeType::Group;
        auto* group = static_cast<Group*>(node);
        header.child_count = static_cast<uint32_t>(group->child_count());
        header.data_offset = header.child_count > 0
            ? static_cast<uint32_t>(buffer.size())
            : 0;
        for (auto* child : group->children()) {
            write_node(buffer, child);
        }
    } else if (dynamic_cast<Shape*>(node)) {
        header.type = NodeType::Shape;
        header.data_offset = static_cast<uint32_t>(buffer.size());
        write_shape(buffer, static_cast<Shape*>(node));
    } else if (dynamic_cast<Text*>(node)) {
        header.type = NodeType::Text;
        header.data_offset = static_cast<uint32_t>(buffer.size());
        write_text(buffer, static_cast<Text*>(node));
    } else if (dynamic_cast<Image*>(node)) {
        header.type = NodeType::Image;
        header.data_offset = static_cast<uint32_t>(buffer.size());
        write_image(buffer, static_cast<Image*>(node));
    } else if (dynamic_cast<Svg*>(node)) {
        header.type = NodeType::Svg;
        header.data_offset = static_cast<uint32_t>(buffer.size());
        auto* svg = static_cast<Svg*>(node);
        SvgData svg_data = {};
        svg_data.source = add_string(svg->src());
        svg_data.data = add_string(svg->data());
        svg_data.width = svg->width();
        svg_data.height = svg->height();
        svg_data.has_source = svg->src().empty() ? 0 : 1;
        svg_data.has_data = svg->data().empty() ? 0 : 1;
        write_bytes(buffer, &svg_data, sizeof(SvgData));
    } else if (dynamic_cast<InstanceNode*>(node)) {
        header.type = NodeType::Instance;
        header.data_offset = static_cast<uint32_t>(buffer.size());
        write_instance(buffer, static_cast<InstanceNode*>(node));
    } else {
        return; // Unknown node type
    }
    std::memcpy(buffer.data() + header_offset, &header, sizeof(NodeHeader));
}

void BinaryWriter::write_shape(std::vector<uint8_t>& buffer, Shape* shape) {
    if (!shape) return;

    ShapeData shape_data = {};
    shape_data.stroke_width = shape->has_stroke() ? shape->stroke().width : 0.0f;

    size_t shape_offset = buffer.size();
    buffer.resize(buffer.size() + sizeof(ShapeData));

    if (shape->has_fill()) {
        auto fill = shape->fill();
        shape_data.has_fill = 1;
        switch (fill.type) {
            case FillType::Solid: {
                SolidPaint solid = {};
                solid.type = PaintType::Solid;
                solid.color = {fill.color.r, fill.color.g, fill.color.b, fill.color.a};
                shape_data.fill_paint_offset = static_cast<uint32_t>(buffer.size());
                write_bytes(buffer, &solid, sizeof(SolidPaint));
                break;
            }
            case FillType::LinearGradient:
                shape_data.fill_paint_offset = write_linear_gradient(buffer, fill.linear_gradient);
                break;
            case FillType::RadialGradient:
                shape_data.fill_paint_offset = write_radial_gradient(buffer, fill.radial_gradient);
                break;
        }
    }

    if (shape->has_stroke()) {
        auto stroke = shape->stroke();
        shape_data.has_stroke = 1;
        switch (stroke.type) {
            case StrokeType::Solid: {
                SolidPaint solid = {};
                solid.type = PaintType::Solid;
                solid.color = {stroke.color.r, stroke.color.g, stroke.color.b, stroke.color.a};
                shape_data.stroke_paint_offset = static_cast<uint32_t>(buffer.size());
                write_bytes(buffer, &solid, sizeof(SolidPaint));
                break;
            }
            case StrokeType::LinearGradient:
                shape_data.stroke_paint_offset = write_linear_gradient(buffer, stroke.linear_gradient);
                break;
            case StrokeType::RadialGradient:
                shape_data.stroke_paint_offset = write_radial_gradient(buffer, stroke.radial_gradient);
                break;
        }
    }

    shape_data.geometry_offset = static_cast<uint32_t>(buffer.size());
    switch (shape->geometry_type()) {
        case GeometryType::Rect: {
            shape_data.type = ShapeType::Rect;
            auto rect = shape->rect();
            RectGeometry geometry{rect.width, rect.height, rect.corner_radius};
            write_bytes(buffer, &geometry, sizeof(RectGeometry));
            break;
        }
        case GeometryType::Circle: {
            shape_data.type = ShapeType::Circle;
            CircleGeometry geometry{shape->circle().radius};
            write_bytes(buffer, &geometry, sizeof(CircleGeometry));
            break;
        }
        case GeometryType::Ellipse: {
            shape_data.type = ShapeType::Ellipse;
            auto ellipse = shape->ellipse();
            EllipseGeometry geometry{ellipse.rx, ellipse.ry};
            write_bytes(buffer, &geometry, sizeof(EllipseGeometry));
            break;
        }
        case GeometryType::Polygon: {
            shape_data.type = ShapeType::Polygon;
            auto polygon = shape->polygon();
            PolygonGeometry geometry{polygon.sides, polygon.radius};
            write_bytes(buffer, &geometry, sizeof(PolygonGeometry));
            break;
        }
        case GeometryType::Star: {
            shape_data.type = ShapeType::Star;
            auto star = shape->star();
            StarGeometry geometry{star.points, star.outer_radius, star.inner_radius};
            write_bytes(buffer, &geometry, sizeof(StarGeometry));
            break;
        }
        case GeometryType::Path: {
            shape_data.type = ShapeType::Path;
            PathGeometry geometry{add_string(shape->path().d)};
            write_bytes(buffer, &geometry, sizeof(PathGeometry));
            break;
        }
        case GeometryType::Line: {
            shape_data.type = ShapeType::Line;
            auto line = shape->line();
            LineGeometry geometry{line.x2, line.y2};
            write_bytes(buffer, &geometry, sizeof(LineGeometry));
            break;
        }
        case GeometryType::Ring: {
            shape_data.type = ShapeType::Ring;
            auto ring = shape->ring();
            RingGeometry geometry{ring.outer_radius, ring.inner_radius};
            write_bytes(buffer, &geometry, sizeof(RingGeometry));
            break;
        }
        default:
            shape_data.type = ShapeType::Rect;
            RectGeometry geometry{};
            write_bytes(buffer, &geometry, sizeof(RectGeometry));
            break;
    }

    std::memcpy(buffer.data() + shape_offset, &shape_data, sizeof(ShapeData));
}

void BinaryWriter::write_text(std::vector<uint8_t>& buffer, Text* text) {
    if (!text) return;
    TextData text_data = {};
    text_data.content = add_string(text->content());
    text_data.font_family = add_string(text->font_family());
    text_data.font_size = text->font_size();
    text_data.bold = text->font_weight() == FontWeight::Bold ? 1 : 0;
    text_data.italic = text->font_style() == FontStyle::Italic ? 1 : 0;
    auto color = text->color();
    text_data.color = {color.r, color.g, color.b, color.a};
    write_bytes(buffer, &text_data, sizeof(TextData));
}

void BinaryWriter::write_image(std::vector<uint8_t>& buffer, Image* image) {
    if (!image) return;
    ImageData image_data = {};
    image_data.source = add_string(image->src());
    image_data.width = image->width();
    image_data.height = image->height();
    image_data.embedded = 0;
    write_bytes(buffer, &image_data, sizeof(ImageData));
}

void BinaryWriter::write_group(std::vector<uint8_t>& buffer, Group* group) {
    // Group serialization not yet implemented
}

void BinaryWriter::write_instance(std::vector<uint8_t>& buffer, InstanceNode* instance) {
    if (!instance) return;

    InstanceData instance_data = {};
    instance_data.source = add_string(instance->source());
    instance_data.float_input_count = static_cast<uint32_t>(instance->float_inputs().size());
    instance_data.string_input_count = static_cast<uint32_t>(instance->string_inputs().size());
    instance_data.bool_input_count = static_cast<uint32_t>(instance->bool_inputs().size());
    write_bytes(buffer, &instance_data, sizeof(InstanceData));

    for (const auto& [name, value] : instance->float_inputs()) {
        InstanceFloatInput input = {};
        input.name = add_string(name);
        input.value = value;
        write_bytes(buffer, &input, sizeof(InstanceFloatInput));
    }

    for (const auto& [name, value] : instance->string_inputs()) {
        InstanceStringInput input = {};
        input.name = add_string(name);
        input.value = add_string(value);
        write_bytes(buffer, &input, sizeof(InstanceStringInput));
    }

    for (const auto& [name, value] : instance->bool_inputs()) {
        InstanceBoolInput input = {};
        input.name = add_string(name);
        input.value = value ? 1 : 0;
        write_bytes(buffer, &input, sizeof(InstanceBoolInput));
    }
}

void BinaryWriter::write_timelines(std::vector<uint8_t>& buffer, const std::vector<Timeline::SharedPtr>& timelines) {
    for (const auto& timeline : timelines) {
        if (!timeline) continue;

        uint32_t serializable_track_count = 0;
        for (const auto& track : timeline->tracks()) {
            if (!track) continue;

            uint32_t serializable_keyframe_count = 0;
            for (const auto& keyframe : track->keyframes()) {
                if (std::holds_alternative<float>(keyframe.value) ||
                    std::holds_alternative<std::string>(keyframe.value) ||
                    std::holds_alternative<Color>(keyframe.value)) {
                    ++serializable_keyframe_count;
                }
            }

            if (serializable_keyframe_count > 0) {
                ++serializable_track_count;
            }
        }

        TimelineHeader timeline_header = {};
        timeline_header.name = add_string(timeline->name());
        timeline_header.duration = timeline->duration();
        timeline_header.loop = timeline->loop_mode() == LoopMode::Loop ? 1 : 0;
        timeline_header.auto_play = 0;
        timeline_header.track_count = serializable_track_count;
        write_bytes(buffer, &timeline_header, sizeof(TimelineHeader));

        for (const auto& track : timeline->tracks()) {
            if (!track) continue;

            uint32_t serializable_keyframe_count = 0;
            for (const auto& keyframe : track->keyframes()) {
                if (std::holds_alternative<float>(keyframe.value) ||
                    std::holds_alternative<std::string>(keyframe.value) ||
                    std::holds_alternative<Color>(keyframe.value)) {
                    ++serializable_keyframe_count;
                }
            }

            if (serializable_keyframe_count == 0) {
                continue;
            }

            std::string property = track->property();
            std::string target_id;
            std::string property_name = property;
            if (!property.empty() && property[0] == '#') {
                size_t slash = property.find('/');
                if (slash != std::string::npos) {
                    target_id = property.substr(1, slash - 1);
                    property_name = property.substr(slash + 1);
                }
            }

            TrackHeader track_header = {};
            track_header.target_id = add_string(target_id);
            track_header.property = add_string(property_name);
            track_header.keyframe_count = serializable_keyframe_count;
            write_bytes(buffer, &track_header, sizeof(TrackHeader));

            for (const auto& keyframe : track->keyframes()) {
                Keyframe binary_keyframe = {};
                binary_keyframe.time = keyframe.time;
                binary_keyframe.easing = static_cast<uint8_t>(keyframe.easing.type);

                if (auto* float_value = std::get_if<float>(&keyframe.value)) {
                    binary_keyframe.value_type = static_cast<uint8_t>(AnimationValueType::Float);
                    binary_keyframe.float_value = *float_value;
                    write_bytes(buffer, &binary_keyframe, sizeof(Keyframe));
                } else if (auto* string_value = std::get_if<std::string>(&keyframe.value)) {
                    binary_keyframe.value_type = static_cast<uint8_t>(AnimationValueType::String);
                    binary_keyframe.string_value = add_string(*string_value);
                    write_bytes(buffer, &binary_keyframe, sizeof(Keyframe));
                } else if (auto* color_value = std::get_if<Color>(&keyframe.value)) {
                    binary_keyframe.value_type = static_cast<uint8_t>(AnimationValueType::Color);
                    binary_keyframe.color_value = {color_value->r, color_value->g, color_value->b, color_value->a};
                    write_bytes(buffer, &binary_keyframe, sizeof(Keyframe));
                }
            }
        }
    }
}

void BinaryWriter::write_header(std::vector<uint8_t>& buffer, const FileHeader& header) {
    write_bytes(buffer, &header, sizeof(FileHeader));
}

void BinaryWriter::write_uint8(std::vector<uint8_t>& buffer, uint8_t value) {
    buffer.push_back(value);
}

void BinaryWriter::write_uint16(std::vector<uint8_t>& buffer, uint16_t value) {
    write_bytes(buffer, &value, sizeof(uint16_t));
}

void BinaryWriter::write_uint32(std::vector<uint8_t>& buffer, uint32_t value) {
    write_bytes(buffer, &value, sizeof(uint32_t));
}

void BinaryWriter::write_float(std::vector<uint8_t>& buffer, float value) {
    write_bytes(buffer, &value, sizeof(float));
}

void BinaryWriter::write_bytes(std::vector<uint8_t>& buffer, const void* data, size_t size) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    buffer.insert(buffer.end(), ptr, ptr + size);
}

std::vector<uint8_t> BinaryWriter::compress_data(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};

    // Get maximum compressed size
    size_t max_compressed_size = ZSTD_compressBound(data.size());
    std::vector<uint8_t> compressed(max_compressed_size);

    // Compress with zstd (level 3 = fast, level 19 = max compression)
    // Level 3 is good balance: fast compression, decent ratio
    size_t compressed_size = ZSTD_compress(
        compressed.data(),
        max_compressed_size,
        data.data(),
        data.size(),
        3  // Compression level
    );

    // Check for errors
    if (ZSTD_isError(compressed_size)) {
        return {};  // Compression failed, return empty
    }

    // Resize to actual compressed size
    compressed.resize(compressed_size);
    return compressed;
}

} // namespace binary
} // namespace flex
