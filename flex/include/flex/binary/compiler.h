/*
 * Flex Engine - Binary Compiler
 *
 * Compiles .flex source files to .flexb binary format.
 *
 * Usage:
 *   BinaryCompiler compiler;
 *   compiler.set_compress(true);  // Optional: enable compression
 *   compiler.compile_to_file("app.flex", "app.flexb");
 *
 * Current Features:
 * - ✅ Parse .flex source (via Definition::load)
 * - ✅ Serialize to .flexb binary
 * - ✅ String deduplication
 * - ✅ CRC32 integrity verification
 * - ✅ Compilation statistics
 * - ✅ zstd compression (use set_compress(true))
 * - ❌ Optimization passes - NOT YET IMPLEMENTED
 * - ❌ Encryption - NOT YET IMPLEMENTED
 * - ❌ Asset embedding - NOT YET IMPLEMENTED
 */

#pragma once

#include "flex/binary/writer.h"
#include "flex/compiler.h"
#include <string>
#include <vector>
#include <memory>

namespace flex {
namespace binary {

// ============================================================================
// BinaryCompiler - Complete .flex → .flexb pipeline
// ============================================================================

class BinaryCompiler {
public:
    BinaryCompiler();
    ~BinaryCompiler();

    // Configuration
    void set_compress(bool compress) { compress_ = compress; }

    // Compile from source
    std::vector<uint8_t> compile_source(const char* source);

    // Compile from file
    std::vector<uint8_t> compile_file(const char* path);

    // Compile and write to file
    bool compile_to_file(const char* input_path, const char* output_path);

    // Check for errors
    bool has_error() const { return !error_message_.empty(); }
    const char* error_message() const { return error_message_.c_str(); }

    // Statistics
    struct Stats {
        size_t source_size = 0;
        size_t binary_size = 0;
        size_t node_count = 0;
        size_t string_count = 0;
        size_t timeline_count = 0;
    };

    const Stats& stats() const { return stats_; }

private:
    // Configuration
    bool compress_ = false;

    // State
    std::string error_message_;
    Stats stats_;

    // Write binary data
    std::vector<uint8_t> write_binary(
        Scene::RawPtr scene,
        const std::vector<Timeline::SharedPtr>& timelines);
};

} // namespace binary
} // namespace flex
