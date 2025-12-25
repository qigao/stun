/*
 * Flex Engine - Binary Writer Implementation
 */

#include "flex/binary/writer.h"
#include "flex/binary/format.h"
#include <cstring>
#include <algorithm>
#include <zstd.h>

namespace flex {
namespace binary {

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

std::vector<uint8_t> BinaryWriter::write(Artboard* artboard) {
    if (!artboard) return {};

    // Reset state
    string_data_.clear();
    string_table_.clear();
    node_count_ = 0;

    std::vector<uint8_t> buffer;

    // Reserve header space
    size_t header_offset = buffer.size();
    buffer.resize(buffer.size() + sizeof(FileHeader));

    // Write artboard as root node
    size_t node_data_offset = buffer.size();
    {
        node_count_++;  // Count the artboard node

        NodeHeader node_header = {};
        node_header.type = NodeType::Artboard;
        node_header.id = add_string("");
        node_header.flags = 0;
        node_header.opacity = 1.0f;
        node_header.child_count = 0; // TODO: serialize children
        write_bytes(buffer, &node_header, sizeof(NodeHeader));
    }
    size_t node_data_size = buffer.size() - node_data_offset;

    // Write string table
    size_t string_table_offset = buffer.size();
    write_string_table(buffer);
    size_t string_table_size = buffer.size() - string_table_offset;

    // Apply compression if enabled (before writing header)
    size_t uncompressed_size = buffer.size() - sizeof(FileHeader);
    if (compress_) {
        // Extract payload (everything after header)
        std::vector<uint8_t> payload(buffer.begin() + sizeof(FileHeader), buffer.end());
        std::vector<uint8_t> compressed_payload = compress_data(payload);

        if (!compressed_payload.empty() && compressed_payload.size() < payload.size()) {
            // Compression succeeded - replace payload
            buffer.resize(sizeof(FileHeader));
            buffer.insert(buffer.end(), compressed_payload.begin(), compressed_payload.end());

            // Adjust offsets (all start from sizeof(FileHeader) now)
            node_data_offset = sizeof(FileHeader);
            node_data_size = compressed_payload.size();
            string_table_offset = 0;  // Embedded in compressed payload
            string_table_size = 0;
        }
    }

    // Fill file header
    FileHeader header = {};
    header.magic = MAGIC_NUMBER;
    header.version = FORMAT_VERSION;
    header.flags = 0;
    if (compress_ && buffer.size() < sizeof(FileHeader) + uncompressed_size) {
        header.flags |= FLAG_COMPRESSED;
    }
    header.header_size = sizeof(FileHeader);

    header.string_table_offset = static_cast<uint32_t>(string_table_offset);
    header.string_table_size = static_cast<uint32_t>(string_table_size);

    header.node_data_offset = static_cast<uint32_t>(node_data_offset);
    header.node_data_size = static_cast<uint32_t>(node_data_size);

    header.asset_table_offset = 0;
    header.asset_table_size = 0;
    header.animation_data_offset = 0;
    header.animation_data_size = 0;
    header.fsm_data_offset = 0;
    header.fsm_data_size = 0;

    header.node_count = node_count_;
    header.asset_count = 0;
    header.timeline_count = 0;
    header.fsm_count = 0;

    header.canvas_width = artboard->width();
    header.canvas_height = artboard->height();

    // Calculate checksum
    header.checksum = 0;
    std::memcpy(buffer.data() + header_offset, &header, sizeof(FileHeader));
    header.checksum = calculate_crc32(buffer.data(), buffer.size());
    std::memcpy(buffer.data() + header_offset, &header, sizeof(FileHeader));

    binary_size_ = buffer.size();

    return buffer;
}

std::vector<uint8_t> BinaryWriter::write(Artboard* artboard,
                                          const std::vector<Timeline::Ptr>& timelines) {
    // TODO: Implement timeline serialization
    return write(artboard);
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

    // Determine node type
    if (dynamic_cast<Group*>(node)) {
        header.type = NodeType::Group;
    } else if (dynamic_cast<Shape*>(node)) {
        header.type = NodeType::Shape;
    } else if (dynamic_cast<Text*>(node)) {
        header.type = NodeType::Text;
    } else if (dynamic_cast<Image*>(node)) {
        header.type = NodeType::Image;
    } else if (dynamic_cast<InstanceNode*>(node)) {
        header.type = NodeType::Instance;
    } else {
        return; // Unknown node type
    }

    header.id = add_string(node->id());
    header.flags = node->visible() ? 1 : 0;
    header.reserved = 0;

    // TODO: Transform, layout properties
    header.opacity = node->opacity();
    header.rotation = 0.0f;
    header.scale_x = 1.0f;
    header.scale_y = 1.0f;

    header.layout_width = 0.0f;
    header.layout_height = 0.0f;
    header.flex_grow = 0.0f;
    header.flex_shrink = 0.0f;
    header.flex_basis = 0.0f;

    header.child_count = 0;
    header.data_offset = 0;

    // Write header
    write_bytes(buffer, &header, sizeof(NodeHeader));

    // Write type-specific data
    if (auto* shape = dynamic_cast<Shape*>(node)) {
        write_shape(buffer, shape);
    } else if (auto* text = dynamic_cast<Text*>(node)) {
        write_text(buffer, text);
    } else if (auto* image = dynamic_cast<Image*>(node)) {
        write_image(buffer, image);
    } else if (auto* group = dynamic_cast<Group*>(node)) {
        write_group(buffer, group);
    }
}

void BinaryWriter::write_shape(std::vector<uint8_t>& buffer, Shape* shape) {
    // Shape serialization not yet implemented
}

void BinaryWriter::write_text(std::vector<uint8_t>& buffer, Text* text) {
    // Text serialization not yet implemented
}

void BinaryWriter::write_image(std::vector<uint8_t>& buffer, Image* image) {
    // Image serialization not yet implemented
}

void BinaryWriter::write_group(std::vector<uint8_t>& buffer, Group* group) {
    // Group serialization not yet implemented
}

void BinaryWriter::write_timelines(std::vector<uint8_t>& buffer, const std::vector<Timeline::Ptr>& timelines) {
    // Timeline serialization not yet implemented
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
