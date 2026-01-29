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

    uint32_t full_crc = calculate_crc32(data, len);

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

TEST_CASE("BinaryWriter: Serialize empty scene", "[binary][writer]") {
    ArenaAllocator arena(4096);
    auto scene = Scene::create(800.0f, 600.0f, arena);

    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(scene);

    REQUIRE_FALSE(binary.empty());
    REQUIRE(binary.size() >= sizeof(FileHeader));

    const FileHeader* header = reinterpret_cast<const FileHeader*>(binary.data());
    REQUIRE(header->magic == MAGIC_NUMBER);
    REQUIRE(header->version == FORMAT_VERSION);
}

TEST_CASE("BinaryWriter: Scene dimensions stored correctly", "[binary][writer]") {
    ArenaAllocator arena(4096);
    float width = 1920.0f;
    float height = 1080.0f;
    auto scene = Scene::create(width, height, arena);

    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(scene);

    const FileHeader* header = reinterpret_cast<const FileHeader*>(binary.data());
    REQUIRE_THAT(header->canvas_width, WithinAbs(width, 0.001f));
    REQUIRE_THAT(header->canvas_height, WithinAbs(height, 0.001f));
}

TEST_CASE("BinaryWriter: Binary size tracking", "[binary][writer]") {
    ArenaAllocator arena(4096);
    auto scene = Scene::create(800.0f, 600.0f, arena);

    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(scene);

    REQUIRE(writer.binary_size() > 0);
    REQUIRE(writer.binary_size() == binary.size());
}

// ============================================================================
// READER TESTS
// ============================================================================

TEST_CASE("BinaryReader: Reject invalid magic number", "[binary][reader]") {
    std::vector<uint8_t> bad_data(sizeof(FileHeader), 0);
    FileHeader* header = reinterpret_cast<FileHeader*>(bad_data.data());
    header->magic = 0xDEADBEEF;

    BinaryReader reader;
    bool loaded = reader.load_memory(bad_data.data(), bad_data.size());

    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(reader.is_valid());
}

TEST_CASE("BinaryReader: Reject file too small", "[binary][reader]") {
    std::vector<uint8_t> tiny_data(10, 0);

    BinaryReader reader;
    bool loaded = reader.load_memory(tiny_data.data(), tiny_data.size());

    REQUIRE_FALSE(loaded);
    REQUIRE_FALSE(reader.is_valid());
}

TEST_CASE("BinaryReader: Reject invalid checksum", "[binary][reader]") {
    ArenaAllocator arena(4096);
    auto scene = Scene::create(800.0f, 600.0f, arena);
    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(scene);

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

TEST_CASE("Round-trip: Empty scene", "[binary][roundtrip]") {
    ArenaAllocator arena(4096);
    float width = 1024.0f;
    float height = 768.0f;

    auto original = Scene::create(width, height, arena);
    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(original);

    REQUIRE_FALSE(binary.empty());

    BinaryReader reader;
    REQUIRE(reader.load_memory(binary.data(), binary.size()));
    REQUIRE(reader.is_valid());

    auto loaded = reader.create_scene();
    REQUIRE(loaded);

    REQUIRE_THAT(loaded->width(), WithinAbs(width, 0.001f));
    REQUIRE_THAT(loaded->height(), WithinAbs(height, 0.001f));
}

TEST_CASE("Round-trip: Multiple scene sizes", "[binary][roundtrip]") {
    std::vector<std::pair<float, float>> sizes = {
        {100.0f, 100.0f},
        {800.0f, 600.0f},
        {1920.0f, 1080.0f},
        {3840.0f, 2160.0f},
    };

    for (auto [width, height] : sizes) {
        ArenaAllocator arena(4096);
        auto original = Scene::create(width, height, arena);

        BinaryWriter writer;
        std::vector<uint8_t> binary = writer.write(original);

        BinaryReader reader;
        REQUIRE(reader.load_memory(binary.data(), binary.size()));

        auto loaded = reader.create_scene();
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

    if (compiler.has_error()) {
        INFO("Compilation error: " << compiler.error_message());
    }

    REQUIRE_FALSE(compiler.has_error());
    REQUIRE_FALSE(binary.empty());

    BinaryReader reader;
    REQUIRE(reader.load_memory(binary.data(), binary.size()));

    auto scene = reader.create_scene();
    REQUIRE(scene);
    REQUIRE_THAT(scene->width(), WithinAbs(800.0f, 0.001f));
    REQUIRE_THAT(scene->height(), WithinAbs(600.0f, 0.001f));
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
    ArenaAllocator arena(4096);
    auto scene = Scene::create(800.0f, 600.0f, arena);
    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(scene);

    FileHeader* header = reinterpret_cast<FileHeader*>(binary.data());
    header->string_table_offset = 0xFFFFFFFF;

    header->checksum = 0;
    header->checksum = calculate_crc32(binary.data(), binary.size());

    BinaryReader reader;
    bool loaded = reader.load_memory(binary.data(), binary.size());

    REQUIRE_FALSE(loaded);
}

TEST_CASE("BinaryReader: Reject out-of-bounds node data", "[binary][reader][boundary]") {
    ArenaAllocator arena(4096);
    auto scene = Scene::create(800.0f, 600.0f, arena);
    BinaryWriter writer;
    std::vector<uint8_t> binary = writer.write(scene);

    FileHeader* header = reinterpret_cast<FileHeader*>(binary.data());
    header->node_data_offset = 0xFFFFFFFF;

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
    ArenaAllocator arena(4096);
    auto scene = Scene::create(1920.0f, 1080.0f, arena);

    BinaryWriter writer_uncompressed;
    std::vector<uint8_t> binary_uncompressed = writer_uncompressed.write(scene);

    BinaryWriter writer_compressed;
    writer_compressed.set_compress(true);
    std::vector<uint8_t> binary_compressed = writer_compressed.write(scene);

    REQUIRE_FALSE(binary_uncompressed.empty());
    REQUIRE_FALSE(binary_compressed.empty());

    REQUIRE(binary_compressed.size() <= binary_uncompressed.size());

    const FileHeader* header_uncompressed = reinterpret_cast<const FileHeader*>(binary_uncompressed.data());
    const FileHeader* header_compressed = reinterpret_cast<const FileHeader*>(binary_compressed.data());

    REQUIRE((header_uncompressed->flags & FLAG_COMPRESSED) == 0);
    if (binary_compressed.size() < binary_uncompressed.size()) {
        REQUIRE((header_compressed->flags & FLAG_COMPRESSED) != 0);
    }
}

TEST_CASE("BinaryReader: Load compressed file", "[binary][compression]") {
    ArenaAllocator arena(4096);
    auto scene = Scene::create(800.0f, 600.0f, arena);

    BinaryWriter writer;
    writer.set_compress(true);
    std::vector<uint8_t> binary = writer.write(scene);

    REQUIRE_FALSE(binary.empty());

    BinaryReader reader;
    REQUIRE(reader.load_memory(binary.data(), binary.size()));

    auto loaded = reader.create_scene();
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

    BinaryCompiler compiler_uncompressed;
    std::vector<uint8_t> binary_uncompressed = compiler_uncompressed.compile_source(source);

    BinaryCompiler compiler_compressed;
    compiler_compressed.set_compress(true);
    std::vector<uint8_t> binary_compressed = compiler_compressed.compile_source(source);

    REQUIRE_FALSE(compiler_uncompressed.has_error());
    REQUIRE_FALSE(compiler_compressed.has_error());
    REQUIRE_FALSE(binary_uncompressed.empty());
    REQUIRE_FALSE(binary_compressed.empty());

    REQUIRE(binary_compressed.size() <= binary_uncompressed.size());

    BinaryReader reader_uncompressed;
    REQUIRE(reader_uncompressed.load_memory(binary_uncompressed.data(), binary_uncompressed.size()));

    BinaryReader reader_compressed;
    REQUIRE(reader_compressed.load_memory(binary_compressed.data(), binary_compressed.size()));

    auto scene_uncompressed = reader_uncompressed.create_scene();
    auto scene_compressed = reader_compressed.create_scene();

    REQUIRE(scene_uncompressed);
    REQUIRE(scene_compressed);
    REQUIRE_THAT(scene_uncompressed->width(), WithinAbs(scene_compressed->width(), 0.001f));
    REQUIRE_THAT(scene_uncompressed->height(), WithinAbs(scene_compressed->height(), 0.001f));
}

TEST_CASE("BinaryReader: Round-trip compressed data", "[binary][compression][roundtrip]") {
    ArenaAllocator arena(4096);
    float width = 1280.0f;
    float height = 720.0f;
    auto scene = Scene::create(width, height, arena);

    BinaryWriter writer;
    writer.set_compress(true);
    std::vector<uint8_t> binary = writer.write(scene);

    REQUIRE_FALSE(binary.empty());

    BinaryReader reader;
    REQUIRE(reader.load_memory(binary.data(), binary.size()));

    REQUIRE_THAT(reader.canvas_width(), WithinAbs(width, 0.001f));
    REQUIRE_THAT(reader.canvas_height(), WithinAbs(height, 0.001f));

    auto loaded = reader.create_scene();
    REQUIRE(loaded);
    REQUIRE_THAT(loaded->width(), WithinAbs(width, 0.001f));
    REQUIRE_THAT(loaded->height(), WithinAbs(height, 0.001f));
}
