/*
 * Flex Engine - Binary Reader Implementation
 */

#include "flex/binary/reader.h"
#include "flex/binary/format.h"
#include "flex/core/group.h"
#include "flex/core/image.h"
#include "flex/core/instance.h"
#include "flex/core/shape.h"
#include "flex/core/svg.h"
#include "flex/core/text.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <zstd.h>

namespace flex {
namespace binary {

namespace {

struct LegacyRadialGradientPaint {
    PaintType type;
    uint8_t reserved[3];
    float cx, cy, radius;
    uint32_t stop_count;
};

constexpr uint32_t SVG_PAYLOAD_VERSION = 0x00010002;
constexpr uint32_t RADIAL_FOCAL_VERSION = 0x00010003;
constexpr uint32_t INSTANCE_PAYLOAD_VERSION = 0x00010004;

Color to_runtime_color(const ColorData& color) {
    return Color(color.r, color.g, color.b, color.a);
}

Easing easing_from_byte(uint8_t easing) {
    if (easing > static_cast<uint8_t>(EasingType::BounceOut)) {
        return Easing::linear();
    }
    return Easing{static_cast<EasingType>(easing)};
}

size_t paint_size(const uint8_t* base, size_t size, uint32_t offset, uint32_t version) {
    if (!base || offset == 0 || offset + sizeof(PaintType) > size) {
        return 0;
    }

    auto type = *reinterpret_cast<const PaintType*>(base + offset);
    switch (type) {
        case PaintType::Solid:
            return sizeof(SolidPaint);
        case PaintType::LinearGradient: {
            if (offset + sizeof(LinearGradientPaint) > size) {
                return 0;
            }
            const auto* gradient = reinterpret_cast<const LinearGradientPaint*>(base + offset);
            return sizeof(LinearGradientPaint) + gradient->stop_count * sizeof(ColorStop);
        }
        case PaintType::RadialGradient: {
            if (version >= RADIAL_FOCAL_VERSION) {
                if (offset + sizeof(RadialGradientPaint) > size) {
                    return 0;
                }
                const auto* gradient = reinterpret_cast<const RadialGradientPaint*>(base + offset);
                return sizeof(RadialGradientPaint) + gradient->stop_count * sizeof(ColorStop);
            }

            if (offset + sizeof(LegacyRadialGradientPaint) > size) {
                return 0;
            }
            const auto* gradient = reinterpret_cast<const LegacyRadialGradientPaint*>(base + offset);
            return sizeof(LegacyRadialGradientPaint) + gradient->stop_count * sizeof(ColorStop);
        }
        default:
            return 0;
    }
}

size_t shape_geometry_size(ShapeType type) {
    switch (type) {
        case ShapeType::Rect:
            return sizeof(RectGeometry);
        case ShapeType::Circle:
            return sizeof(CircleGeometry);
        case ShapeType::Ellipse:
            return sizeof(EllipseGeometry);
        case ShapeType::Polygon:
            return sizeof(PolygonGeometry);
        case ShapeType::Star:
            return sizeof(StarGeometry);
        case ShapeType::Path:
            return sizeof(PathGeometry);
        case ShapeType::Line:
            return sizeof(LineGeometry);
        case ShapeType::Ring:
            return sizeof(RingGeometry);
        default:
            return 0;
    }
}

bool apply_fill_paint(Shape* shape, const uint8_t* base, size_t size, uint32_t offset, uint32_t version) {
    if (!shape || !base || offset == 0 || offset + sizeof(PaintType) > size) {
        return false;
    }

    auto type = *reinterpret_cast<const PaintType*>(base + offset);
    switch (type) {
        case PaintType::Solid: {
            if (offset + sizeof(SolidPaint) > size) return false;
            const auto* paint = reinterpret_cast<const SolidPaint*>(base + offset);
            shape->set_fill(to_runtime_color(paint->color));
            return true;
        }
        case PaintType::LinearGradient: {
            if (offset + sizeof(LinearGradientPaint) > size) return false;
            const auto* paint = reinterpret_cast<const LinearGradientPaint*>(base + offset);
            if (offset + sizeof(LinearGradientPaint) + paint->stop_count * sizeof(binary::ColorStop) > size) {
                return false;
            }

            LinearGradient gradient(paint->x1, paint->y1, paint->x2, paint->y2);
            const auto* stops = reinterpret_cast<const binary::ColorStop*>(base + offset + sizeof(LinearGradientPaint));
            for (uint32_t i = 0; i < paint->stop_count; ++i) {
                gradient.add_stop(stops[i].offset, to_runtime_color(stops[i].color));
            }
            shape->set_fill(gradient);
            return true;
        }
        case PaintType::RadialGradient: {
            RadialGradient gradient;
            const binary::ColorStop* stops = nullptr;
            uint32_t stop_count = 0;

            if (version >= RADIAL_FOCAL_VERSION) {
                if (offset + sizeof(RadialGradientPaint) > size) return false;
                const auto* paint = reinterpret_cast<const RadialGradientPaint*>(base + offset);
                if (offset + sizeof(RadialGradientPaint) + paint->stop_count * sizeof(binary::ColorStop) > size) {
                    return false;
                }
                gradient = RadialGradient(paint->cx, paint->cy, paint->radius);
                gradient.fx = paint->fx;
                gradient.fy = paint->fy;
                stops = reinterpret_cast<const binary::ColorStop*>(base + offset + sizeof(RadialGradientPaint));
                stop_count = paint->stop_count;
            } else {
                if (offset + sizeof(LegacyRadialGradientPaint) > size) return false;
                const auto* paint = reinterpret_cast<const LegacyRadialGradientPaint*>(base + offset);
                if (offset + sizeof(LegacyRadialGradientPaint) + paint->stop_count * sizeof(binary::ColorStop) > size) {
                    return false;
                }
                gradient = RadialGradient(paint->cx, paint->cy, paint->radius);
                stops = reinterpret_cast<const binary::ColorStop*>(base + offset + sizeof(LegacyRadialGradientPaint));
                stop_count = paint->stop_count;
            }

            for (uint32_t i = 0; i < stop_count; ++i) {
                gradient.add_stop(stops[i].offset, to_runtime_color(stops[i].color));
            }
            shape->set_fill(gradient);
            return true;
        }
        default:
            return false;
    }
}

bool apply_stroke_paint(Shape* shape, float width, const uint8_t* base, size_t size, uint32_t offset, uint32_t version) {
    if (!shape || !base || offset == 0 || offset + sizeof(PaintType) > size) {
        return false;
    }

    auto type = *reinterpret_cast<const PaintType*>(base + offset);
    switch (type) {
        case PaintType::Solid: {
            if (offset + sizeof(SolidPaint) > size) return false;
            const auto* paint = reinterpret_cast<const SolidPaint*>(base + offset);
            shape->set_stroke(to_runtime_color(paint->color), width);
            return true;
        }
        case PaintType::LinearGradient: {
            if (offset + sizeof(LinearGradientPaint) > size) return false;
            const auto* paint = reinterpret_cast<const LinearGradientPaint*>(base + offset);
            if (offset + sizeof(LinearGradientPaint) + paint->stop_count * sizeof(binary::ColorStop) > size) {
                return false;
            }

            LinearGradient gradient(paint->x1, paint->y1, paint->x2, paint->y2);
            const auto* stops = reinterpret_cast<const binary::ColorStop*>(base + offset + sizeof(LinearGradientPaint));
            for (uint32_t i = 0; i < paint->stop_count; ++i) {
                gradient.add_stop(stops[i].offset, to_runtime_color(stops[i].color));
            }
            shape->set_stroke(gradient, width);
            return true;
        }
        case PaintType::RadialGradient: {
            RadialGradient gradient;
            const binary::ColorStop* stops = nullptr;
            uint32_t stop_count = 0;

            if (version >= RADIAL_FOCAL_VERSION) {
                if (offset + sizeof(RadialGradientPaint) > size) return false;
                const auto* paint = reinterpret_cast<const RadialGradientPaint*>(base + offset);
                if (offset + sizeof(RadialGradientPaint) + paint->stop_count * sizeof(binary::ColorStop) > size) {
                    return false;
                }
                gradient = RadialGradient(paint->cx, paint->cy, paint->radius);
                gradient.fx = paint->fx;
                gradient.fy = paint->fy;
                stops = reinterpret_cast<const binary::ColorStop*>(base + offset + sizeof(RadialGradientPaint));
                stop_count = paint->stop_count;
            } else {
                if (offset + sizeof(LegacyRadialGradientPaint) > size) return false;
                const auto* paint = reinterpret_cast<const LegacyRadialGradientPaint*>(base + offset);
                if (offset + sizeof(LegacyRadialGradientPaint) + paint->stop_count * sizeof(binary::ColorStop) > size) {
                    return false;
                }
                gradient = RadialGradient(paint->cx, paint->cy, paint->radius);
                stops = reinterpret_cast<const binary::ColorStop*>(base + offset + sizeof(LegacyRadialGradientPaint));
                stop_count = paint->stop_count;
            }

            for (uint32_t i = 0; i < stop_count; ++i) {
                gradient.add_stop(stops[i].offset, to_runtime_color(stops[i].color));
            }
            shape->set_stroke(gradient, width);
            return true;
        }
        default:
            return false;
    }
}

void apply_common_properties(Node* node, const NodeHeader& header, const std::string& id) {
    if (!node) return;

    if (!id.empty()) {
        node->set_id(id);
    }

    node->set_visible((header.flags & 1) != 0);
    node->set_position(header.transform[4], header.transform[5]);
    node->set_opacity(header.opacity);
    node->set_rotation(header.rotation);
    node->set_scale(header.scale_x, header.scale_y);
    node->set_layout_size(header.layout_width, header.layout_height);
    node->set_flex(header.flex_grow, header.flex_shrink, header.flex_basis);
}

} // namespace

BinaryReader::BinaryReader() {
}

BinaryReader::~BinaryReader() {
}

bool BinaryReader::load_file(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error_message_ = "Failed to open file";
        return false;
    }

    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    data_.resize(file_size);
    if (!file.read(reinterpret_cast<char*>(data_.data()), file_size)) {
        error_message_ = "Failed to read file";
        return false;
    }

    ptr_ = data_.data();
    size_ = data_.size();

    if (!parse_header()) return false;

    // Verify checksum and section bounds against the on-disk payload first.
    // For compressed files the checksum is computed over the compressed bytes.
    if (!verify_integrity()) return false;

    bool was_compressed = (header_.flags & FLAG_COMPRESSED) != 0;
    if (!was_compressed && !validate_section_bounds()) return false;

    // Check if compressed
    if (was_compressed) {
        // Extract compressed payload (everything after header)
        std::vector<uint8_t> compressed_payload(
            ptr_ + sizeof(FileHeader),
            ptr_ + size_
        );

        // Decompress
        std::vector<uint8_t> decompressed_payload = decompress_data(compressed_payload);
        if (decompressed_payload.empty()) {
            valid_ = false;
            return false;
        }

        // Replace data_ with [header + decompressed payload]
        data_.clear();
        data_.resize(sizeof(FileHeader) + decompressed_payload.size());
        std::memcpy(data_.data(), &header_, sizeof(FileHeader));
        std::memcpy(data_.data() + sizeof(FileHeader), decompressed_payload.data(), decompressed_payload.size());

        // Update pointers to decompressed data
        ptr_ = data_.data();
        size_ = data_.size();

        // Clear compression flag since we've decompressed
        header_.flags &= ~FLAG_COMPRESSED;
        std::memcpy(data_.data(), &header_, sizeof(FileHeader));
    }

    if (was_compressed && !validate_section_bounds()) return false;

    return parse_string_table();
}

bool BinaryReader::load_memory(const void* data, size_t size) {
    ptr_ = static_cast<const uint8_t*>(data);
    size_ = size;

    if (!parse_header()) return false;

    // Verify checksum and section bounds against the on-disk payload first.
    // For compressed files the checksum is computed over the compressed bytes.
    if (!verify_integrity()) return false;

    bool was_compressed = (header_.flags & FLAG_COMPRESSED) != 0;
    if (!was_compressed && !validate_section_bounds()) return false;

    // Check if compressed
    if (was_compressed) {
        // Extract compressed payload (everything after header)
        std::vector<uint8_t> compressed_payload(
            ptr_ + sizeof(FileHeader),
            ptr_ + size
        );

        // Decompress
        std::vector<uint8_t> decompressed_payload = decompress_data(compressed_payload);
        if (decompressed_payload.empty()) {
            valid_ = false;
            return false;
        }

        // Replace data_ with [header + decompressed payload]
        data_.clear();
        data_.resize(sizeof(FileHeader) + decompressed_payload.size());
        std::memcpy(data_.data(), &header_, sizeof(FileHeader));
        std::memcpy(data_.data() + sizeof(FileHeader), decompressed_payload.data(), decompressed_payload.size());

        // Update pointers to decompressed data
        ptr_ = data_.data();
        size_ = data_.size();

        // Clear compression flag since we've decompressed
        header_.flags &= ~FLAG_COMPRESSED;
        std::memcpy(data_.data(), &header_, sizeof(FileHeader));
    }

    if (was_compressed && !validate_section_bounds()) return false;

    return parse_string_table();
}

bool BinaryReader::load_file_encrypted(const char* path, const char* password) {
    // Encryption not yet implemented
    error_message_ = "Encryption not yet supported";
    return false;
}

bool BinaryReader::load_memory_encrypted(const void* data, size_t size, const char* password) {
    // Encryption not yet implemented
    error_message_ = "Encryption not yet supported";
    return false;
}

bool BinaryReader::parse_header() {
    if (!ptr_ || size_ < sizeof(FileHeader)) {
        error_message_ = "File too small";
        valid_ = false;
        return false;
    }

    std::memcpy(&header_, ptr_, sizeof(FileHeader));

    // Check magic number
    if (header_.magic != MAGIC_NUMBER) {
        error_message_ = "Invalid magic number";
        valid_ = false;
        return false;
    }

    // Check version compatibility
    uint32_t major = (header_.version >> 16) & 0xFFFF;
    uint32_t expected_major = (FORMAT_VERSION >> 16) & 0xFFFF;
    if (major != expected_major) {
        error_message_ = "Incompatible version";
        valid_ = false;
        return false;
    }

    if ((header_.flags & FLAG_ENCRYPTED) != 0) {
        error_message_ = "Encrypted binaries are not supported";
        valid_ = false;
        return false;
    }

    return true;
}

bool BinaryReader::verify_integrity() {
    // Verify checksum WITHOUT copying entire file
    uint32_t stored_checksum = header_.checksum;

    // Create header copy with checksum = 0
    FileHeader temp_header = header_;
    temp_header.checksum = 0;

    // Calculate CRC32 of header (with checksum = 0)
    uint32_t crc = calculate_crc32(&temp_header, sizeof(FileHeader));

    // Continue with rest of file
    if (size_ > sizeof(FileHeader)) {
        crc = calculate_crc32_continue(ptr_ + sizeof(FileHeader),
                                        size_ - sizeof(FileHeader), crc);
    }

    if (crc != stored_checksum) {
        error_message_ = "Checksum verification failed";
        valid_ = false;
        return false;
    }

    valid_ = true;
    return true;
}

bool BinaryReader::validate_section_bounds() {
    // Validate string table bounds
    if (header_.string_table_offset > size_ ||
        header_.string_table_size > size_ ||
        header_.string_table_offset + header_.string_table_size > size_) {
        error_message_ = "Invalid string table bounds";
        valid_ = false;
        return false;
    }

    // Validate node data bounds
    if (header_.node_data_offset > size_ ||
        header_.node_data_size > size_ ||
        header_.node_data_offset + header_.node_data_size > size_) {
        error_message_ = "Invalid node data bounds";
        valid_ = false;
        return false;
    }

    if (header_.animation_data_offset > size_ ||
        header_.animation_data_size > size_ ||
        header_.animation_data_offset + header_.animation_data_size > size_) {
        error_message_ = "Invalid animation data bounds";
        valid_ = false;
        return false;
    }

    if (header_.asset_table_offset > size_ ||
        header_.asset_table_size > size_ ||
        header_.asset_table_offset + header_.asset_table_size > size_) {
        error_message_ = "Invalid asset table bounds";
        valid_ = false;
        return false;
    }

    if (header_.fsm_data_offset > size_ ||
        header_.fsm_data_size > size_ ||
        header_.fsm_data_offset + header_.fsm_data_size > size_) {
        error_message_ = "Invalid fsm data bounds";
        valid_ = false;
        return false;
    }

    valid_ = true;
    return true;
}

bool BinaryReader::parse_string_table() {
    strings_.clear();

    size_t offset = header_.string_table_offset;
    size_t end = offset + header_.string_table_size;

    while (offset < end) {
        if (offset + sizeof(uint32_t) > end || offset + sizeof(uint32_t) > size_) {
            error_message_ = "Invalid string table entry header";
            valid_ = false;
            strings_.clear();
            return false;
        }

        uint32_t length = *reinterpret_cast<const uint32_t*>(ptr_ + offset);
        offset += sizeof(uint32_t);

        if (offset + length > end || offset + length > size_) {
            error_message_ = "Invalid string table entry bounds";
            valid_ = false;
            strings_.clear();
            return false;
        }

        std::string str(reinterpret_cast<const char*>(ptr_ + offset), length);
        strings_.push_back(str);
        offset += length;
    }

    if (offset != end) {
        error_message_ = "Invalid string table alignment";
        valid_ = false;
        strings_.clear();
        return false;
    }

    valid_ = true;
    return true;
}

const std::string& BinaryReader::get_string(StringIndex index) const {
    static std::string empty_string;
    if (index == 0xFFFFFFFF || index >= strings_.size()) {
        return empty_string;
    }
    return strings_[index];
}

Scene* BinaryReader::create_scene() {
    if (!valid_) return nullptr;
    const uint8_t* node_end = ptr_ + header_.node_data_offset + header_.node_data_size;

    // Create scene with dimensions from header
    auto* scene = Scene::create(header_.canvas_width, header_.canvas_height, arena_);

    // Verify node data exists
    if (header_.node_data_size == 0) {
        return scene;
    }

    size_t offset = header_.node_data_offset;
    if (ptr_ + offset + sizeof(NodeHeader) > node_end) {
        return scene;
    }

    // Read root node (should be Scene type)
    const NodeHeader* node = reinterpret_cast<const NodeHeader*>(ptr_ + offset);
    if (node->type != NodeType::Scene) {
        return scene;
    }

    if (node->child_count == 0 || node->data_offset == 0) {
        return scene;
    }

    if (ptr_ + node->data_offset + sizeof(NodeHeader) > node_end) {
        return scene;
    }

    const uint8_t* child_ptr = ptr_ + node->data_offset;
    for (uint32_t i = 0; i < node->child_count; ++i) {
        size_t child_size = 0;
        Node* child = read_node(child_ptr, &child_size);
        if (!child || child_size == 0) {
            return scene;
        }
        scene->root()->add_child(child);
        child_ptr += child_size;
    }

    return scene;
}

std::vector<Timeline::SharedPtr> BinaryReader::create_timelines() {
    std::vector<Timeline::SharedPtr> timelines;
    if (!valid_ || header_.timeline_count == 0 || header_.animation_data_size == 0) {
        return timelines;
    }

    const uint8_t* timeline_ptr = ptr_ + header_.animation_data_offset;
    const uint8_t* end = ptr_ + header_.animation_data_offset + header_.animation_data_size;

    for (uint32_t i = 0; i < header_.timeline_count && timeline_ptr < end; ++i) {
        size_t timeline_size = 0;
        auto timeline = read_timeline(timeline_ptr, &timeline_size);
        if (!timeline || timeline_size == 0) {
            break;
        }
        timelines.push_back(timeline);
        timeline_ptr += timeline_size;
    }

    return timelines;
}

uint32_t BinaryReader::node_count() const {
    return valid_ ? header_.node_count : 0;
}

uint32_t BinaryReader::timeline_count() const {
    return valid_ ? header_.timeline_count : 0;
}

float BinaryReader::canvas_width() const {
    return valid_ ? header_.canvas_width : 0.0f;
}

float BinaryReader::canvas_height() const {
    return valid_ ? header_.canvas_height : 0.0f;
}

Node* BinaryReader::read_node(const uint8_t* node_data, size_t* bytes_consumed) {
    const uint8_t* node_end = ptr_ + header_.node_data_offset + header_.node_data_size;
    if (bytes_consumed) {
        *bytes_consumed = 0;
    }
    if (!node_data || node_data < ptr_ || node_data + sizeof(NodeHeader) > node_end) {
        return nullptr;
    }

    const auto* header = reinterpret_cast<const NodeHeader*>(node_data);
    const auto id = get_string(header->id);
    const uint8_t* furthest_end = node_data + sizeof(NodeHeader);
    Node* node = nullptr;

    switch (header->type) {
        case NodeType::Group: {
            auto* group = Group::create(arena_);
            apply_common_properties(group, *header, id);

            if (header->child_count > 0) {
                if (ptr_ + header->data_offset + sizeof(NodeHeader) > node_end) {
                    return nullptr;
                }
                const uint8_t* child_ptr = ptr_ + header->data_offset;
                for (uint32_t i = 0; i < header->child_count; ++i) {
                    size_t child_size = 0;
                    Node* child = read_node(child_ptr, &child_size);
                    if (!child || child_size == 0) {
                        return nullptr;
                    }
                    group->add_child(child);
                    child_ptr += child_size;
                }
                furthest_end = std::max(furthest_end, child_ptr);
            }

            node = group;
            break;
        }
        case NodeType::Shape: {
            if (ptr_ + header->data_offset + sizeof(ShapeData) > node_end) {
                return nullptr;
            }

            auto* shape = read_shape(ptr_ + header->data_offset);
            if (!shape) return nullptr;
            apply_common_properties(shape, *header, id);

            const auto* shape_data = reinterpret_cast<const ShapeData*>(ptr_ + header->data_offset);
            const size_t node_limit = static_cast<size_t>(node_end - ptr_);
            size_t payload_end = header->data_offset + sizeof(ShapeData);
            if (shape_data->has_fill) {
                const size_t fill_size = paint_size(ptr_, node_limit, shape_data->fill_paint_offset, header_.version);
                if (fill_size == 0) {
                    return nullptr;
                }
                payload_end = std::max(payload_end, static_cast<size_t>(shape_data->fill_paint_offset) + fill_size);
            }
            if (shape_data->has_stroke) {
                const size_t stroke_size = paint_size(ptr_, node_limit, shape_data->stroke_paint_offset, header_.version);
                if (stroke_size == 0) {
                    return nullptr;
                }
                payload_end = std::max(payload_end, static_cast<size_t>(shape_data->stroke_paint_offset) + stroke_size);
            }
            const size_t geometry_size = shape_geometry_size(shape_data->type);
            if (geometry_size == 0) {
                return nullptr;
            }
            payload_end = std::max(payload_end, static_cast<size_t>(shape_data->geometry_offset) + geometry_size);
            if (ptr_ + payload_end > node_end) {
                return nullptr;
            }
            furthest_end = std::max(furthest_end, ptr_ + payload_end);
            node = shape;
            break;
        }
        case NodeType::Text: {
            if (ptr_ + header->data_offset + sizeof(TextData) > node_end) {
                return nullptr;
            }
            auto* text = read_text(ptr_ + header->data_offset);
            if (!text) return nullptr;
            apply_common_properties(text, *header, id);
            furthest_end = std::max(furthest_end, ptr_ + header->data_offset + sizeof(TextData));
            node = text;
            break;
        }
        case NodeType::Image: {
            if (ptr_ + header->data_offset + sizeof(ImageData) > node_end) {
                return nullptr;
            }
            auto* image = read_image(ptr_ + header->data_offset);
            if (!image) return nullptr;
            apply_common_properties(image, *header, id);
            furthest_end = std::max(furthest_end, ptr_ + header->data_offset + sizeof(ImageData));
            node = image;
            break;
        }
        case NodeType::Svg: {
            auto* svg = Svg::create(arena_);
            size_t svg_payload_size = 0;

            if (header_.version >= SVG_PAYLOAD_VERSION) {
                if (ptr_ + header->data_offset + sizeof(SvgData) > node_end) {
                    return nullptr;
                }
                const auto* svg_data = reinterpret_cast<const SvgData*>(ptr_ + header->data_offset);
                const bool has_source = svg_data->has_source != 0;
                const bool has_data = svg_data->has_data != 0;
                if (has_source == has_data) {
                    return nullptr;
                }
                if (has_source && get_string(svg_data->source).empty()) {
                    return nullptr;
                }
                if (has_data && get_string(svg_data->data).empty()) {
                    return nullptr;
                }
                if (has_data) {
                    svg->set_data(get_string(svg_data->data));
                } else if (has_source) {
                    svg->set_src(get_string(svg_data->source));
                }
                svg->set_width(svg_data->width);
                svg->set_height(svg_data->height);
                svg_payload_size = sizeof(SvgData);
            } else {
                if (ptr_ + header->data_offset + sizeof(ImageData) > node_end) {
                    return nullptr;
                }
                const auto* image_data = reinterpret_cast<const ImageData*>(ptr_ + header->data_offset);
                svg->set_src(get_string(image_data->source));
                svg->set_width(image_data->width);
                svg->set_height(image_data->height);
                svg_payload_size = sizeof(ImageData);
            }
            apply_common_properties(svg, *header, id);
            furthest_end = std::max(furthest_end, ptr_ + header->data_offset + svg_payload_size);
            node = svg;
            break;
        }
        case NodeType::Instance: {
            if (header_.version < INSTANCE_PAYLOAD_VERSION) {
                return nullptr;
            }
            size_t instance_payload_size = 0;
            auto* instance = read_instance(ptr_ + header->data_offset, &instance_payload_size);
            if (!instance || instance_payload_size == 0) {
                return nullptr;
            }
            apply_common_properties(instance, *header, id);
            furthest_end = std::max(furthest_end, ptr_ + header->data_offset + instance_payload_size);
            node = instance;
            break;
        }
        default:
            return nullptr;
    }

    if (bytes_consumed) {
        *bytes_consumed = static_cast<size_t>(furthest_end - node_data);
    }
    return node;
}

Shape* BinaryReader::read_shape(const uint8_t* shape_data) {
    const uint8_t* node_end = ptr_ + header_.node_data_offset + header_.node_data_size;
    const size_t node_limit = static_cast<size_t>(node_end - ptr_);
    if (!shape_data || shape_data < ptr_ || shape_data + sizeof(ShapeData) > node_end) {
        return nullptr;
    }

    const auto* data = reinterpret_cast<const ShapeData*>(shape_data);
    auto* shape = Shape::create(arena_);

    if (data->has_fill) {
        if (!apply_fill_paint(shape, ptr_, node_limit, data->fill_paint_offset, header_.version)) {
            return nullptr;
        }
    }

    if (data->has_stroke) {
        if (!apply_stroke_paint(shape, data->stroke_width, ptr_, node_limit, data->stroke_paint_offset, header_.version)) {
            return nullptr;
        }
    }

    if (ptr_ + data->geometry_offset >= node_end) {
        return shape;
    }

    switch (data->type) {
        case ShapeType::Rect:
            if (ptr_ + data->geometry_offset + sizeof(RectGeometry) <= node_end) {
                const auto* geometry = reinterpret_cast<const RectGeometry*>(ptr_ + data->geometry_offset);
                shape->set_rect(geometry->width, geometry->height, geometry->corner_radius);
            }
            break;
        case ShapeType::Circle:
            if (ptr_ + data->geometry_offset + sizeof(CircleGeometry) <= node_end) {
                const auto* geometry = reinterpret_cast<const CircleGeometry*>(ptr_ + data->geometry_offset);
                shape->set_circle(geometry->radius);
            }
            break;
        case ShapeType::Ellipse:
            if (ptr_ + data->geometry_offset + sizeof(EllipseGeometry) <= node_end) {
                const auto* geometry = reinterpret_cast<const EllipseGeometry*>(ptr_ + data->geometry_offset);
                shape->set_ellipse(geometry->rx, geometry->ry);
            }
            break;
        case ShapeType::Polygon:
            if (ptr_ + data->geometry_offset + sizeof(PolygonGeometry) <= node_end) {
                const auto* geometry = reinterpret_cast<const PolygonGeometry*>(ptr_ + data->geometry_offset);
                shape->set_polygon(geometry->sides, geometry->radius);
            }
            break;
        case ShapeType::Star:
            if (ptr_ + data->geometry_offset + sizeof(StarGeometry) <= node_end) {
                const auto* geometry = reinterpret_cast<const StarGeometry*>(ptr_ + data->geometry_offset);
                shape->set_star(geometry->points, geometry->outer_radius, geometry->inner_radius);
            }
            break;
        case ShapeType::Path:
            if (ptr_ + data->geometry_offset + sizeof(PathGeometry) <= node_end) {
                const auto* geometry = reinterpret_cast<const PathGeometry*>(ptr_ + data->geometry_offset);
                shape->set_path(get_string(geometry->path_data));
            }
            break;
        case ShapeType::Line:
            if (ptr_ + data->geometry_offset + sizeof(LineGeometry) <= node_end) {
                const auto* geometry = reinterpret_cast<const LineGeometry*>(ptr_ + data->geometry_offset);
                shape->set_line(geometry->x2, geometry->y2);
            }
            break;
        case ShapeType::Ring:
            if (ptr_ + data->geometry_offset + sizeof(RingGeometry) <= node_end) {
                const auto* geometry = reinterpret_cast<const RingGeometry*>(ptr_ + data->geometry_offset);
                shape->set_ring(geometry->outer_radius, geometry->inner_radius);
            }
            break;
        default:
            break;
    }

    return shape;
}

Text* BinaryReader::read_text(const uint8_t* text_data) {
    const uint8_t* node_end = ptr_ + header_.node_data_offset + header_.node_data_size;
    if (!text_data || text_data < ptr_ || text_data + sizeof(TextData) > node_end) {
        return nullptr;
    }

    const auto* data = reinterpret_cast<const TextData*>(text_data);
    auto* text = Text::create(arena_);
    text->set_content(get_string(data->content));
    text->set_font_family(get_string(data->font_family));
    text->set_font_size(data->font_size);
    text->set_font_weight(data->bold ? FontWeight::Bold : FontWeight::Normal);
    text->set_font_style(data->italic ? FontStyle::Italic : FontStyle::Normal);
    text->set_color(to_runtime_color(data->color));
    return text;
}

Image* BinaryReader::read_image(const uint8_t* image_data) {
    const uint8_t* node_end = ptr_ + header_.node_data_offset + header_.node_data_size;
    if (!image_data || image_data < ptr_ || image_data + sizeof(ImageData) > node_end) {
        return nullptr;
    }

    const auto* data = reinterpret_cast<const ImageData*>(image_data);
    auto* image = Image::create(arena_);
    image->set_src(get_string(data->source));
    image->set_width(data->width);
    image->set_height(data->height);
    return image;
}

Group* BinaryReader::read_group(const uint8_t* group_data) {
    (void)group_data;
    return Group::create(arena_);
}

InstanceNode* BinaryReader::read_instance(const uint8_t* instance_data, size_t* bytes_consumed) {
    const uint8_t* node_end = ptr_ + header_.node_data_offset + header_.node_data_size;
    if (bytes_consumed) {
        *bytes_consumed = 0;
    }
    if (!instance_data || instance_data < ptr_ || instance_data + sizeof(InstanceData) > node_end) {
        return nullptr;
    }

    const auto* data = reinterpret_cast<const InstanceData*>(instance_data);
    const uint8_t* cursor = instance_data + sizeof(InstanceData);
    auto* instance = arena_.create<InstanceNode>();
    const auto& source = get_string(data->source);
    if (source.empty()) {
        return nullptr;
    }
    instance->set_source(source);

    for (uint32_t i = 0; i < data->float_input_count; ++i) {
        if (cursor + sizeof(InstanceFloatInput) > node_end) {
            return nullptr;
        }
        const auto* input = reinterpret_cast<const InstanceFloatInput*>(cursor);
        const auto& name = get_string(input->name);
        if (name.empty()) {
            return nullptr;
        }
        instance->set_input(name, input->value);
        cursor += sizeof(InstanceFloatInput);
    }

    for (uint32_t i = 0; i < data->string_input_count; ++i) {
        if (cursor + sizeof(InstanceStringInput) > node_end) {
            return nullptr;
        }
        const auto* input = reinterpret_cast<const InstanceStringInput*>(cursor);
        const auto& name = get_string(input->name);
        if (name.empty()) {
            return nullptr;
        }
        instance->set_input(name, get_string(input->value));
        cursor += sizeof(InstanceStringInput);
    }

    for (uint32_t i = 0; i < data->bool_input_count; ++i) {
        if (cursor + sizeof(InstanceBoolInput) > node_end) {
            return nullptr;
        }
        const auto* input = reinterpret_cast<const InstanceBoolInput*>(cursor);
        const auto& name = get_string(input->name);
        if (name.empty()) {
            return nullptr;
        }
        instance->set_input(name, input->value != 0);
        cursor += sizeof(InstanceBoolInput);
    }

    if (bytes_consumed) {
        *bytes_consumed = static_cast<size_t>(cursor - instance_data);
    }
    return instance;
}

Timeline::SharedPtr BinaryReader::read_timeline(const uint8_t* timeline_data, size_t* bytes_consumed) {
    const uint8_t* animation_end = ptr_ + header_.animation_data_offset + header_.animation_data_size;
    if (bytes_consumed) {
        *bytes_consumed = 0;
    }
    if (!timeline_data || timeline_data < ptr_ || timeline_data + sizeof(TimelineHeader) > animation_end) {
        return nullptr;
    }

    const auto* header = reinterpret_cast<const TimelineHeader*>(timeline_data);
    const auto& timeline_name = get_string(header->name);
    if (timeline_name.empty()) {
        return nullptr;
    }

    auto timeline = Timeline::create(timeline_name.c_str(), arena_);
    timeline->set_duration(header->duration);
    timeline->set_loop_mode(header->loop ? LoopMode::Loop : LoopMode::Once);

    const uint8_t* cursor = timeline_data + sizeof(TimelineHeader);
    for (uint32_t i = 0; i < header->track_count; ++i) {
        if (cursor + sizeof(TrackHeader) > animation_end) {
            return nullptr;
        }

        const auto* track_header = reinterpret_cast<const TrackHeader*>(cursor);
        cursor += sizeof(TrackHeader);

        std::string property = get_string(track_header->property);
        if (property.empty()) {
            return nullptr;
        }
        const auto& target_id = get_string(track_header->target_id);
        if (!target_id.empty()) {
            property = "#" + target_id + "/" + property;
        }

        auto track = timeline->add_track(property.c_str());
        for (uint32_t keyframe_index = 0; keyframe_index < track_header->keyframe_count; ++keyframe_index) {
            if (cursor + sizeof(Keyframe) > animation_end) {
                return nullptr;
            }

            const auto* keyframe = reinterpret_cast<const Keyframe*>(cursor);
            auto easing = easing_from_byte(keyframe->easing);
            switch (static_cast<AnimationValueType>(keyframe->value_type)) {
                case AnimationValueType::Float:
                    track->add_keyframe(keyframe->time, keyframe->float_value, easing);
                    break;
                case AnimationValueType::String:
                    track->add_keyframe(keyframe->time, get_string(keyframe->string_value).c_str(), easing);
                    break;
                case AnimationValueType::Color:
                    track->add_keyframe(keyframe->time, to_runtime_color(keyframe->color_value), easing);
                    break;
                default:
                    return nullptr;
            }
            cursor += sizeof(Keyframe);
        }
    }

    if (bytes_consumed) {
        *bytes_consumed = static_cast<size_t>(cursor - timeline_data);
    }
    return timeline;
}

std::vector<uint8_t> BinaryReader::decompress_data(const std::vector<uint8_t>& data) {
    if (data.empty()) return {};

    // Get decompressed size
    unsigned long long decompressed_size = ZSTD_getFrameContentSize(data.data(), data.size());

    if (decompressed_size == ZSTD_CONTENTSIZE_ERROR) {
        error_message_ = "Invalid zstd compressed data";
        return {};
    }

    if (decompressed_size == ZSTD_CONTENTSIZE_UNKNOWN) {
        error_message_ = "Unknown decompressed size";
        return {};
    }

    // Allocate buffer for decompressed data
    std::vector<uint8_t> decompressed(decompressed_size);

    // Decompress
    size_t result = ZSTD_decompress(
        decompressed.data(),
        decompressed_size,
        data.data(),
        data.size()
    );

    if (ZSTD_isError(result)) {
        error_message_ = "Decompression failed: ";
        error_message_ += ZSTD_getErrorName(result);
        return {};
    }

    return decompressed;
}

std::vector<uint8_t> BinaryReader::decrypt_data(const std::vector<uint8_t>& data, const char* password) {
    // AES decryption not yet implemented
    return data;
}

uint8_t BinaryReader::read_uint8(const uint8_t*& ptr) {
    uint8_t value = *ptr;
    ptr += sizeof(uint8_t);
    return value;
}

uint16_t BinaryReader::read_uint16(const uint8_t*& ptr) {
    uint16_t value = *reinterpret_cast<const uint16_t*>(ptr);
    ptr += sizeof(uint16_t);
    return value;
}

uint32_t BinaryReader::read_uint32(const uint8_t*& ptr) {
    uint32_t value = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += sizeof(uint32_t);
    return value;
}

float BinaryReader::read_float(const uint8_t*& ptr) {
    float value = *reinterpret_cast<const float*>(ptr);
    ptr += sizeof(float);
    return value;
}

void BinaryReader::read_bytes(const uint8_t*& ptr, void* dest, size_t size) {
    std::memcpy(dest, ptr, size);
    ptr += size;
}

} // namespace binary
} // namespace flex
