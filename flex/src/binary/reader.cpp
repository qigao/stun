/*
 * Flex Engine - Binary Reader Implementation
 */

#include "flex/binary/reader.h"
#include "flex/binary/format.h"
#include <fstream>
#include <cstring>
#include <zstd.h>

namespace flex {
namespace binary {

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

    // Verify integrity of compressed data BEFORE decompression
    bool was_compressed = (header_.flags & FLAG_COMPRESSED) != 0;
    if (!was_compressed) {
        if (!verify_integrity()) return false;
    }

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

    return parse_string_table();
}

bool BinaryReader::load_memory(const void* data, size_t size) {
    ptr_ = static_cast<const uint8_t*>(data);
    size_ = size;

    if (!parse_header()) return false;

    // Verify integrity of compressed data BEFORE decompression
    bool was_compressed = (header_.flags & FLAG_COMPRESSED) != 0;
    if (!was_compressed) {
        if (!verify_integrity()) return false;
    }

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

    valid_ = true;
    return true;
}

bool BinaryReader::parse_string_table() {
    strings_.clear();

    size_t offset = header_.string_table_offset;
    size_t end = offset + header_.string_table_size;

    while (offset < end && offset + sizeof(uint32_t) <= size_) {
        uint32_t length = *reinterpret_cast<const uint32_t*>(ptr_ + offset);
        offset += sizeof(uint32_t);

        if (offset + length > size_) break;

        std::string str(reinterpret_cast<const char*>(ptr_ + offset), length);
        strings_.push_back(str);
        offset += length;
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

Artboard::Ptr BinaryReader::create_artboard() {
    if (!valid_) return nullptr;

    // Create artboard with dimensions from header
    auto artboard = Artboard::create(header_.canvas_width, header_.canvas_height);

    // Verify node data exists
    if (header_.node_data_size == 0) {
        return artboard;
    }

    size_t offset = header_.node_data_offset;
    if (offset + sizeof(NodeHeader) > size_) {
        return artboard;
    }

    // Read root node (should be Artboard type)
    const NodeHeader* node = reinterpret_cast<const NodeHeader*>(ptr_ + offset);
    if (node->type != NodeType::Artboard) {
        return artboard;
    }

    // Note: Artboard doesn't have set_id() - ID stored in node header but not used
    // Child deserialization not yet implemented

    return artboard;
}

std::vector<Timeline::Ptr> BinaryReader::create_timelines() {
    // Timeline deserialization not yet implemented
    return {};
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

Node::Ptr BinaryReader::read_node(const uint8_t* node_data) {
    // Not yet implemented
    return nullptr;
}

Shape::Ptr BinaryReader::read_shape(const uint8_t* shape_data) {
    // Not yet implemented
    return nullptr;
}

Text::Ptr BinaryReader::read_text(const uint8_t* text_data) {
    // Not yet implemented
    return nullptr;
}

Image::Ptr BinaryReader::read_image(const uint8_t* image_data) {
    // Not yet implemented
    return nullptr;
}

Group::Ptr BinaryReader::read_group(const uint8_t* group_data) {
    // Not yet implemented
    return nullptr;
}

Timeline::Ptr BinaryReader::read_timeline(const uint8_t* timeline_data) {
    // Not yet implemented
    return nullptr;
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
