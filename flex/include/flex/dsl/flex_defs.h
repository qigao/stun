/*
 * Flex Engine - Internal Definitions
 *
 * Types shared between parser, lexer, and flex.cpp
 *
 * SIMPLIFIED: Parser directly builds Runtime objects (no AST layer)
 */

#pragma once
#include <string>
#include <vector>

// Forward declarations of Runtime types
namespace flex {
    class Scene;
    class Node;
    class Timeline;
    class Track;
    class Machine;
    class Layer;
    class State;
}

namespace flex {

/* Token value - must be POD for Lemon parser compatibility with MSVC */
struct FlexTokenValue {
    double number;
    char* string;
    bool boolean;
};

/* Parser state - Now builds Runtime objects directly */
struct FlexParserState {
    char* error_message = nullptr;
    int error_line = 0;
    int error_column = 0;

    // Current lexer position (updated before each Parse call)
    int current_line = 0;
    int current_column = 0;
    const char* current_token_text = nullptr;
    int current_token_len = 0;

    // Context stacks for building Runtime objects
    flex::Scene* current_scene = nullptr;
    flex::Node* current_node = nullptr;
    std::vector<flex::Node*> node_stack;

    flex::Timeline* current_timeline = nullptr;
    flex::Track* current_track = nullptr;

    flex::Machine* current_machine = nullptr;
    flex::Layer* current_layer = nullptr;
    flex::State* current_state = nullptr;

    // Paint context for Fill/Stroke property prefixing
    std::string paint_prefix;  // "fill." or "stroke." or ""
};

} // namespace flex
