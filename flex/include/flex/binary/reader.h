/*
 * Flex Engine - Binary Reader
 *
 * Reads compiled Flex binary format and creates runtime objects.
 *
 * Current Status:
 * - ✅ Load .flexb files (load_file, load_memory)
 * - ✅ CRC32 integrity verification
 * - ✅ String table deduplication
 * - ✅ Create runtime objects (scene, timelines)
 * - ✅ Compression (zstd)
 * - ❌ Encryption (decrypt_data) - NOT YET IMPLEMENTED
 */

#pragma once

#include "flex/binary/format.h"
#include "flex/runtime.h"
#include "flex/core/allocator.h"
#include <vector>
#include <memory>

namespace flex {
namespace binary {

// ============================================================================
// BinaryReader - Deserializes binary format to runtime objects
// ============================================================================

class BinaryReader {
public:
    BinaryReader();
    ~BinaryReader();

    // Load from file
    bool load_file(const char* path);

    // Load from memory
    bool load_memory(const void* data, size_t size);

    // Load with password (for encrypted binaries) - NOT YET IMPLEMENTED
    // These methods exist for future compatibility but will currently return false
    bool load_file_encrypted(const char* path, const char* password);
    bool load_memory_encrypted(const void* data, size_t size, const char* password);

    // Check if loaded successfully
    bool is_valid() const { return valid_; }

    // Get error message
    const char* error_message() const { return error_message_.c_str(); }

    // Create runtime objects (uses internal arena)
    Scene* create_scene();
    std::vector<Timeline::SharedPtr> create_timelines();

    // Access arena
    ArenaAllocator& arena() { return arena_; }

    // Get metadata
    uint32_t node_count() const;
    uint32_t timeline_count() const;
    float canvas_width() const;
    float canvas_height() const;

private:
    bool valid_ = false;
    std::string error_message_;

    // Arena allocator for created objects
    ArenaAllocator arena_{64 * 1024};  // 64KB

    // Binary data
    std::vector<uint8_t> data_;
    const uint8_t* ptr_ = nullptr;
    size_t size_ = 0;

    // Parsed header
    FileHeader header_;

    // String table
    std::vector<std::string> strings_;

    // Parsing
    bool parse_header();
    bool parse_string_table();
    bool verify_integrity();
    bool validate_section_bounds();

    Node* read_node(const uint8_t* node_data, size_t* bytes_consumed = nullptr);
    Shape* read_shape(const uint8_t* shape_data);
    Text* read_text(const uint8_t* text_data);
    Image* read_image(const uint8_t* image_data);
    Group* read_group(const uint8_t* group_data);
    InstanceNode* read_instance(const uint8_t* instance_data, size_t* bytes_consumed = nullptr);

    Timeline::SharedPtr read_timeline(const uint8_t* timeline_data, size_t* bytes_consumed = nullptr);

    // Decompression
    std::vector<uint8_t> decompress_data(const std::vector<uint8_t>& data);

    // Decryption
    std::vector<uint8_t> decrypt_data(const std::vector<uint8_t>& data, const char* password);

    // Deserialization helpers
    uint8_t read_uint8(const uint8_t*& ptr);
    uint16_t read_uint16(const uint8_t*& ptr);
    uint32_t read_uint32(const uint8_t*& ptr);
    float read_float(const uint8_t*& ptr);
    void read_bytes(const uint8_t*& ptr, void* dest, size_t size);

    const std::string& get_string(StringIndex index) const;
};

} // namespace binary
} // namespace flex
