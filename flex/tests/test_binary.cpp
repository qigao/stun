/*
 * Flex Binary Format Tests
 * Tests binary serialization, CRC32, and round-trip consistency
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "flex/binary/format.h"
#include "flex/binary/writer.h"
#include "flex/binary/reader.h"
#include "flex/binary/compiler.h"
#include "flex/runtime.h"

using namespace flex;
using namespace flex::binary;
using Catch::Matchers::WithinAbs;

// ============================================================================
// CRC32 TESTS
// ============================================================================

TEST_CASE("CRC32: Basic calculation", "[binary][crc32]") {
    const char* data = "Hello, World!";
    uint32_t crc = calculate_crc32(data, strlen(data));

    REQUIRE(crc != 0);
    REQUIRE(crc != 0xFFFFFFFF);
}

TEST_CASE("CRC32: Same input produces same output", "[binary][crc32]") {
    const char* data = "Test data";
    uint32_t crc1 = calculate_crc32(data, strlen(data));
    uint32_t crc2 = calculate_crc32(data, strlen(data));

    REQUIRE(crc1 == crc2);
}

TEST_CASE("CRC32: Different input produces different output", "[binary][crc32]") {
    const char* data1 = "Test data 1";
    const char* data2 = "Test data 2";

    uint32_t crc1 = calculate_crc32(data1, strlen(data1));
    uint32_t crc2 = calculate_crc32(data2, strlen(data2));

    REQUIRE(crc1 != crc2);
}

TEST_CASE("CRC32: Incremental calculation matches full calculation", "[binary][crc32]") {
    const char* data = "This is a longer test string to verify incremental CRC32";
    size_t len = strlen(data);

    // Full calculation
    uint32_t full_crc = calculate_crc32(data, len);

    // Split into two parts
    size_t split = len / 2;
    uint32_t part1_crc = calculate_crc32(data, split);
    uint32_t incremental_crc = calculate_crc32_continue(data + split, len - split, part1_crc);

    REQUIRE(full_crc == incremental_crc);
}

TEST_CASE("CRC32: Checksum verification", "[binary][crc32]") {
    const char* data = "Verify me";
    uint32_t crc = calculate_crc32(data, strlen(data));

    REQUIRE(verify_checksum(data, strlen(data), crc));
    REQUIRE_FALSE(verify_checksum(data, strlen(data), crc + 1));
}

// ============================================================================
// WRITER TESTS
// ============================================================================

TEST_CASE("BinaryWriter: Serialize empty artboard", "[binary][writer]") {
    auto artboard = Artboard::create(800.0f, 600.0f);

    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(artboard.get());

    REQUIRE_FALSE(binary.empty());
    REQUIRE(binary.size() >= sizeof(FileHeader));

    // Check magic number
    const FileHeader* header = reinterpret_cast<const FileHeader*>(binary.data());
    REQUIRE(header->magic == MAGIC_NUMBER);
    REQUIRE(header->version == FORMAT_VERSION);
}

TEST_CASE("BinaryWriter: Artboard dimensions stored correctly", "[binary][writer]") {
    float width = 1920.0f;
    float height = 1080.0f;
    auto artboard = Artboard::create(width, height);

    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(artboard.get());

    const FileHeader* header = reinterpret_cast<const FileHeader*>(binary.data());
    REQUIRE_THAT(header->canvas_width, WithinAbs(width, 0.001f));
    REQUIRE_THAT(header->canvas_height, WithinAbs(height, 0.001f));
}

TEST_CASE("BinaryWriter: Binary size tracking", "[binary][writer]") {
    auto artboard = Artboard::create(800.0f, 600.0f);

    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(artboard.get());

    REQUIRE(writer.binary_size() > 0);      // Has data
    REQUIRE(writer.binary_size() == binary.size());  // Size matches
}

// ============================================================================
// READER TESTS
// ============================================================================

TEST_CASE("BinaryReader: Reject invalid magic number", "[binary][reader]") {
    std::vector<uint8_t> bad_data(sizeof(FileHeader), 0);
    FileHeader* header = reinterpret_cast<FileHeader*>(bad_data.data());
    header->magic = 0xDEADBEEF;  // Wrong magic

    BinaryReader reader;
    bool loaded = reader.load_memory(bad_data.data(), bad_data.size());

    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(reader.is_valid());
}

TEST_CASE("BinaryReader: Reject file too small", "[binary][reader]") {
    std::vector<uint8_t> tiny_data(10, 0);  // Too small for header

    BinaryReader reader;
    bool loaded = reader.load_memory(tiny_data.data(), tiny_data.size());

    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(reader.is_valid());
}

TEST_CASE("BinaryReader: Reject invalid checksum", "[binary][reader]") {
    // Create valid binary
    auto artboard = Artboard::create(800.0f, 600.0f);
    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(artboard.get());

    // Corrupt checksum
    FileHeader* header = reinterpret_cast<FileHeader*>(binary.data());
    header->checksum = 0xDEADBEEF;

    BinaryReader reader;
    bool loaded = reader.load_memory(binary.data(), binary.size());

    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(reader.is_valid());
}

// ============================================================================
// ROUND-TRIP TESTS
// ============================================================================

TEST_CASE("Round-trip: Empty artboard", "[binary][roundtrip]") {
    float width = 1024.0f;
    float height = 768.0f;

    // Write
    auto original = Artboard::create(width, height);
    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(original.get());

    REQUIRE_FALSE(binary.empty());

    // Read
    BinaryReader reader;
    REQUIRE(reader.load_memory(binary.data(), binary.size()));
    REQUIRE(reader.is_valid());

    auto loaded = reader.create_artboard();
    REQUIRE(loaded);

    // Verify dimensions
    REQUIRE_THAT(loaded->width(), WithinAbs(width, 0.001f));
    REQUIRE_THAT(loaded->height(), WithinAbs(height, 0.001f));
}

TEST_CASE("Round-trip: Multiple artboard sizes", "[binary][roundtrip]") {
    std::vector<std::pair<float, float>> sizes = {
        {100.0f, 100.0f},
        {800.0f, 600.0f},
        {1920.0f, 1080.0f},
        {3840.0f, 2160.0f},
    };

    for (auto [width, height] : sizes) {
        auto original = Artboard::create(width, height);

        BinaryWriter writer;
        std::vector<uint8_t> binary = writer.write(original.get());

        BinaryReader reader;
        REQUIRE(reader.load_memory(binary.data(), binary.size()));

        auto loaded = reader.create_artboard();
        REQUIRE(loaded);
        REQUIRE_THAT(loaded->width(), WithinAbs(width, 0.001f));
        REQUIRE_THAT(loaded->height(), WithinAbs(height, 0.001f));
    }
}

// ============================================================================
// COMPILER TESTS
// ============================================================================

TEST_CASE("BinaryCompiler: Compile simple scene source", "[binary][compiler]") {
    const char* source = R"(
scene TestScene {
    width: 800
    height: 600
}
)";

    BinaryCompiler compiler;
    std::vector<uint8_t> binary = compiler.compile_source(source);

    // Debug: print error if compilation failed
    if (compiler.has_error()) {
        INFO("Compilation error: " << compiler.error_message());
    }

    REQUIRE_FALSE(compiler.has_error());
    REQUIRE_FALSE(binary.empty());

    // Verify binary is valid
    BinaryReader reader;
    REQUIRE(reader.load_memory(binary.data(), binary.size()));

    auto artboard = reader.create_artboard();
    REQUIRE(artboard);
    REQUIRE_THAT(artboard->width(), WithinAbs(800.0f, 0.001f));
    REQUIRE_THAT(artboard->height(), WithinAbs(600.0f, 0.001f));
}

TEST_CASE("BinaryCompiler: Handle parse errors", "[binary][compiler]") {
    const char* bad_source = "this is not valid flex syntax {{{";

    BinaryCompiler compiler;
    std::vector<uint8_t> binary = compiler.compile_source(bad_source);

    REQUIRE(compiler.has_error());
    REQUIRE(binary.empty());
    REQUIRE(strlen(compiler.error_message()) > 0);
}

TEST_CASE("BinaryCompiler: Statistics collection", "[binary][compiler]") {
    const char* source = R"(
scene Test {
    width: 640
    height: 480
}
)";

    BinaryCompiler compiler;
    std::vector<uint8_t> binary = compiler.compile_source(source);

    // Debug: print error if compilation failed
    if (compiler.has_error()) {
        INFO("Compilation error: " << compiler.error_message());
    }

    REQUIRE_FALSE(compiler.has_error());

    const auto& stats = compiler.stats();
    REQUIRE(stats.source_size > 0);
    REQUIRE(stats.binary_size > 0);
    REQUIRE(stats.binary_size == binary.size());
    REQUIRE(stats.node_count > 0);
}

// ============================================================================
// BOUNDARY TESTS
// ============================================================================

TEST_CASE("BinaryReader: Reject out-of-bounds string table", "[binary][reader][boundary]") {
    // Create valid binary
    auto artboard = Artboard::create(800.0f, 600.0f);
    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(artboard.get());

    // Corrupt string table offset (point outside file)
    FileHeader* header = reinterpret_cast<FileHeader*>(binary.data());
    uint32_t stored_checksum = header->checksum;
    header->string_table_offset = 0xFFFFFFFF;

    // Recalculate checksum
    header->checksum = 0;
    header->checksum = calculate_crc32(binary.data(), binary.size());

    BinaryReader reader;
    bool loaded = reader.load_memory(binary.data(), binary.size());

    // Should detect invalid bounds
    REQUIRE_FALSE(loaded);
}

TEST_CASE("BinaryReader: Reject out-of-bounds node data", "[binary][reader][boundary]") {
    auto artboard = Artboard::create(800.0f, 600.0f);
    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(artboard.get());

    // Corrupt node data offset
    FileHeader* header = reinterpret_cast<FileHeader*>(binary.data());
    header->node_data_offset = 0xFFFFFFFF;

    // Recalculate checksum
    header->checksum = 0;
    header->checksum = calculate_crc32(binary.data(), binary.size());

    BinaryReader reader;
    bool loaded = reader.load_memory(binary.data(), binary.size());

    REQUIRE_FALSE(loaded);
}

// ============================================================================
// COMPRESSION TESTS
// ============================================================================

TEST_CASE("BinaryWriter: Compression reduces size", "[binary][compression]") {
    auto artboard = Artboard::create(1920.0f, 1080.0f);

    // Write without compression
    BinaryWriter writer_uncompressed;
    std::vector<uint8_t> binary_uncompressed = writer_uncompressed.write(artboard.get());

    // Write with compression
    BinaryWriter writer_compressed;
    writer_compressed.set_compress(true);
    std::vector<uint8_t> binary_compressed = writer_compressed.write(artboard.get());

    REQUIRE_FALSE(binary_uncompressed.empty());
    REQUIRE_FALSE(binary_compressed.empty());

    // Compressed should be smaller (or same size if data is incompressible)
    REQUIRE(binary_compressed.size() <= binary_uncompressed.size());

    // Check compression flag
    const FileHeader* header_uncompressed = reinterpret_cast<const FileHeader*>(binary_uncompressed.data());
    const FileHeader* header_compressed = reinterpret_cast<const FileHeader*>(binary_compressed.data());

    REQUIRE((header_uncompressed->flags & FLAG_COMPRESSED) == 0);
    // Compression flag should be set if size was reduced
    if (binary_compressed.size() < binary_uncompressed.size()) {
        REQUIRE((header_compressed->flags & FLAG_COMPRESSED) != 0);
    }
}

TEST_CASE("BinaryReader: Load compressed file", "[binary][compression]") {
    auto artboard = Artboard::create(800.0f, 600.0f);

    // Write compressed binary
    BinaryWriter writer;
    writer.set_compress(true);
    std::vector<uint8_t> binary = writer.write(artboard.get());

    REQUIRE_FALSE(binary.empty());

    // Load compressed binary
    BinaryReader reader;
    REQUIRE(reader.load_memory(binary.data(), binary.size()));

    // Verify artboard is created correctly
    auto loaded = reader.create_artboard();
    REQUIRE(loaded);
    REQUIRE_THAT(loaded->width(), WithinAbs(800.0f, 0.001f));
    REQUIRE_THAT(loaded->height(), WithinAbs(600.0f, 0.001f));
}

TEST_CASE("BinaryCompiler: Compile with compression", "[binary][compiler][compression]") {
    const char* source = R"(
scene TestScene {
    width: 1024
    height: 768
}
)";

    // Compile without compression
    BinaryCompiler compiler_uncompressed;
    std::vector<uint8_t> binary_uncompressed = compiler_uncompressed.compile_source(source);

    // Compile with compression
    BinaryCompiler compiler_compressed;
    compiler_compressed.set_compress(true);
    std::vector<uint8_t> binary_compressed = compiler_compressed.compile_source(source);

    REQUIRE_FALSE(compiler_uncompressed.has_error());
    REQUIRE_FALSE(compiler_compressed.has_error());
    REQUIRE_FALSE(binary_uncompressed.empty());
    REQUIRE_FALSE(binary_compressed.empty());

    // Compressed should be smaller or same size
    REQUIRE(binary_compressed.size() <= binary_uncompressed.size());

    // Both should load correctly
    BinaryReader reader_uncompressed;
    REQUIRE(reader_uncompressed.load_memory(binary_uncompressed.data(), binary_uncompressed.size()));

    BinaryReader reader_compressed;
    REQUIRE(reader_compressed.load_memory(binary_compressed.data(), binary_compressed.size()));

    // Both should produce identical artboards
    auto artboard_uncompressed = reader_uncompressed.create_artboard();
    auto artboard_compressed = reader_compressed.create_artboard();

    REQUIRE(artboard_uncompressed);
    REQUIRE(artboard_compressed);
    REQUIRE_THAT(artboard_uncompressed->width(), WithinAbs(artboard_compressed->width(), 0.001f));
    REQUIRE_THAT(artboard_uncompressed->height(), WithinAbs(artboard_compressed->height(), 0.001f));
}

TEST_CASE("BinaryReader: Round-trip compressed data", "[binary][compression][roundtrip]") {
    float width = 1280.0f;
    float height = 720.0f;
    auto artboard = Artboard::create(width, height);

    // Write compressed
    BinaryWriter writer;
    writer.set_compress(true);
    std::vector<uint8_t> binary = writer.write(artboard.get());

    REQUIRE_FALSE(binary.empty());

    // Load compressed
    BinaryReader reader;
    REQUIRE(reader.load_memory(binary.data(), binary.size()));

    // Verify data
    REQUIRE_THAT(reader.canvas_width(), WithinAbs(width, 0.001f));
    REQUIRE_THAT(reader.canvas_height(), WithinAbs(height, 0.001f));

    // Create artboard
    auto loaded = reader.create_artboard();
    REQUIRE(loaded);
    REQUIRE_THAT(loaded->width(), WithinAbs(width, 0.001f));
    REQUIRE_THAT(loaded->height(), WithinAbs(height, 0.001f));
}
