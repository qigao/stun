/*
 * Flex Engine - Binary Compiler Implementation
 */

#include "flex/binary/compiler.h"
#include "flex/binary/writer.h"
#include "flex.h"
#include <fstream>

namespace flex {
namespace binary {

BinaryCompiler::BinaryCompiler() = default;

BinaryCompiler::~BinaryCompiler() = default;

std::vector<uint8_t> BinaryCompiler::compile_source(const char* source) {
    if (!source || !source[0]) {
        error_message_ = "Empty source";
        return {};
    }

    // Parse source using Definition::load()
    auto definition = Definition::load(source);
    if (!definition) {
        error_message_ = "Failed to parse source";
        return {};
    }

    if (definition->has_error()) {
        error_message_ = definition->error_message();
        return {};
    }

    // Get scene from definition
    Scene::RawPtr scene = definition->scene();
    if (!scene) {
        error_message_ = "No scene in definition";
        return {};
    }

    const auto& timelines = definition->timelines();

    stats_.source_size = strlen(source);

    return write_binary(scene, timelines);
}

std::vector<uint8_t> BinaryCompiler::compile_file(const char* path) {
    if (!path || !path[0]) {
        error_message_ = "Empty path";
        return {};
    }

    // Load file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error_message_ = "Failed to open file: ";
        error_message_ += path;
        return {};
    }

    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string source;
    source.resize(size);
    if (!file.read(&source[0], size)) {
        error_message_ = "Failed to read file: ";
        error_message_ += path;
        return {};
    }

    return compile_source(source.c_str());
}

bool BinaryCompiler::compile_to_file(const char* input_path, const char* output_path) {
    std::vector<uint8_t> binary = compile_file(input_path);
    if (binary.empty()) {
        return false;
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        error_message_ = "Failed to open output file: ";
        error_message_ += output_path;
        return false;
    }

    out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
    if (!out) {
        error_message_ = "Failed to write output file: ";
        error_message_ += output_path;
        return false;
    }

    return true;
}

std::vector<uint8_t> BinaryCompiler::write_binary(
    Scene::RawPtr scene,
    const std::vector<Timeline::SharedPtr>& timelines) {
    if (!scene) {
        error_message_ = "No scene to serialize";
        return {};
    }

    BinaryWriter writer;
    writer.set_compress(compress_);
    std::vector<uint8_t> binary = writer.write(scene, timelines);
    if (binary.empty()) {
        error_message_ = "Failed to serialize scene";
        return {};
    }

    stats_.binary_size = writer.binary_size();
    stats_.node_count = writer.node_count();
    stats_.string_count = writer.string_count();
    stats_.timeline_count = timelines.size();

    return binary;
}

} // namespace binary
} // namespace flex
