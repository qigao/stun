/*
 * Flex Engine - Binary Writer
 *
 * Writes compiled Flex data to binary format.
 *
 * Current Features:
 * - ✅ Serialize scene to .flexb
 * - ✅ String table deduplication
 * - ✅ CRC32 integrity checksum
 * - ✅ Node counting and statistics
 * - ✅ zstd compression (use set_compress(true))
 * - ❌ Encryption - NOT YET IMPLEMENTED
 * - ❌ Timeline serialization - NOT YET IMPLEMENTED
 */

#pragma once

#include "flex/binary/format.h"
#include "flex/runtime.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

namespace flex {
namespace binary {

// ============================================================================
// BinaryWriter - Converts runtime objects to binary format
// ============================================================================

class BinaryWriter {
public:
    BinaryWriter();
    ~BinaryWriter();

    // Configuration
    void set_compress(bool compress) { compress_ = compress; }

    // Write scene to binary
    std::vector<uint8_t> write(Scene* scene);

    // Write with timelines
    std::vector<uint8_t> write(Scene* scene,
                                const std::vector<Timeline::SharedPtr>& timelines);

    // Get statistics
    size_t binary_size() const { return binary_size_; }
    size_t string_count() const { return string_table_.size(); }
    size_t node_count() const { return node_count_; }

private:
    // Configuration
    bool compress_ = false;

    // Statistics
    size_t binary_size_ = 0;
    size_t node_count_ = 0;

    // String deduplication
    std::unordered_map<std::string, StringIndex> string_table_;
    std::vector<std::string> string_data_;

    StringIndex add_string(const std::string& str);

    // Writing pipeline
    void write_header(std::vector<uint8_t>& buffer, const FileHeader& header);
    void write_string_table(std::vector<uint8_t>& buffer);
    void write_node(std::vector<uint8_t>& buffer, Node* node);
    void write_shape(std::vector<uint8_t>& buffer, Shape* shape);
    void write_text(std::vector<uint8_t>& buffer, Text* text);
    void write_image(std::vector<uint8_t>& buffer, Image* image);
    void write_group(std::vector<uint8_t>& buffer, Group* group);
    void write_instance(std::vector<uint8_t>& buffer, InstanceNode* instance);
    void write_timelines(std::vector<uint8_t>& buffer, const std::vector<Timeline::SharedPtr>& timelines);

    // Compression (zstd)
    std::vector<uint8_t> compress_data(const std::vector<uint8_t>& data);

    // Serialization helpers
    void write_uint8(std::vector<uint8_t>& buffer, uint8_t value);
    void write_uint16(std::vector<uint8_t>& buffer, uint16_t value);
    void write_uint32(std::vector<uint8_t>& buffer, uint32_t value);
    void write_float(std::vector<uint8_t>& buffer, float value);
    void write_bytes(std::vector<uint8_t>& buffer, const void* data, size_t size);
};

} // namespace binary
} // namespace flex
