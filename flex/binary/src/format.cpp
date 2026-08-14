/*
 * Flex Engine - Binary Format Utilities
 */

#include "flex/binary/format.h"

namespace flex {
namespace binary {

// CRC32 lookup table (generated once)
static uint32_t crc32_table[256];
static bool crc32_table_initialized = false;

static void init_crc32_table() {
    if (crc32_table_initialized) return;

    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }

    crc32_table_initialized = true;
}

uint32_t calculate_crc32(const void* data, size_t size) {
    init_crc32_table();

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < size; ++i) {
        uint8_t index = (crc ^ bytes[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }

    return crc ^ 0xFFFFFFFF;
}

uint32_t calculate_crc32_continue(const void* data, size_t size, uint32_t previous_crc) {
    init_crc32_table();

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = previous_crc ^ 0xFFFFFFFF;

    for (size_t i = 0; i < size; ++i) {
        uint8_t index = (crc ^ bytes[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }

    return crc ^ 0xFFFFFFFF;
}

bool verify_checksum(const void* data, size_t size, uint32_t expected_crc) {
    uint32_t actual_crc = calculate_crc32(data, size);
    return actual_crc == expected_crc;
}

} // namespace binary
} // namespace flex
