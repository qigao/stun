/*
 * Flex Engine - DSL Loader
 *
 * Loads and parses .flex files
 */

#pragma once

#include "flex/builder.h"
#include "flex/ast.h"
#include <string>
#include <memory>

namespace flex {

// ============================================================================
// DSL Loader - Loads .flex files and builds Flex objects
// ============================================================================

class DSLLoader {
public:
    DSLLoader() = default;
    ~DSLLoader() = default;

    // Load from file
    BuildResult load_from_file(const std::string& file_path);

    // Load from string
    BuildResult load_from_string(const std::string& dsl_content);

    // Get parsing errors
    const std::string& get_last_error() const { return last_error_; }

private:
    std::string last_error_;

    // Helper methods
    bool read_file(const std::string& path, std::string& content);
    ast::Document parse_ast(const std::string& content);
};

// ============================================================================
// Implementation
// ============================================================================

inline bool DSLLoader::read_file(const std::string& path, std::string& content) {
    // TODO: Implement file reading
    // This would use std::ifstream to read the file
    return false;
}

inline BuildResult DSLLoader::load_from_file(const std::string& file_path) {
    std::string content;
    if (!read_file(file_path, content)) {
        BuildResult result;
        last_error_ = "Failed to read file: " + file_path;
        return result;
    }
    return load_from_string(content);
}

inline BuildResult DSLLoader::load_from_string(const std::string& dsl_content) {
    // Parse DSL to AST
    ast::Document doc = parse_ast(dsl_content);

    // Build Flex objects from AST
    Builder builder;
    return builder.build(doc);
}

inline ast::Document DSLLoader::parse_ast(const std::string& content) {
    // TODO: Use flex_parser.y and flex_lexer.re
    // This would integrate with Lemon parser
    ast::Document doc;
    // ... parse implementation
    return doc;
}

} // namespace flex
