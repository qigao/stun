/*
 * flexUI - Text Utilities
 *
 * Provides emoji detection and text segmentation for proper rendering
 * of mixed text (regular characters + emoji) with font fallback.
 */

#ifndef FLEXUI_TEXT_UTIL_H
#define FLEXUI_TEXT_UTIL_H

#include <string>
#include <vector>
#include <cstdint>

namespace flexUI {

// ============================================================================
// Text Segment - A run of text with the same font requirement
// ============================================================================

enum class TextSegmentType {
    Regular,  // Normal text (use primary font)
    Emoji,    // Emoji (use emoji font)
    Symbol    // Symbols that may need special font
};

struct TextSegment {
    std::string text;
    TextSegmentType type;
    float width;  // Calculated width (set during rendering)
};

// ============================================================================
// Unicode Utilities
// ============================================================================

/**
 * One strictly decoded Unicode scalar and its original UTF-8 byte range.
 */
struct Utf8Scalar {
    uint32_t value = 0;
    size_t byte_offset = 0;
    size_t byte_length = 0;
};

/**
 * Decode exactly one Unicode scalar with Salts::Unicode.
 *
 * The caller must provide a cursor in [0, text.size()). Success advances the
 * cursor and returns the scalar plus its original byte range. Invalid UTF-8
 * throws std::invalid_argument and an invalid/end cursor throws
 * std::out_of_range. The caller cursor is unchanged on every failure.
 */
Utf8Scalar utf8_next_scalar(const std::string& text, size_t& cursor);

/**
 * Count Unicode scalars after strictly validating the complete UTF-8 string.
 */
size_t utf8_scalar_count(const std::string& text);

/**
 * Check if a code point is an emoji
 */
bool is_emoji(uint32_t codepoint);

/**
 * Check if a code point is an emoji modifier or ZWJ
 */
bool is_emoji_modifier(uint32_t codepoint);

// ============================================================================
// Text Segmentation
// ============================================================================

/**
 * Segment text into runs of regular text and emoji
 *
 * Example:
 *   "Hello 👋 World 🌍" -> [
 *     {text: "Hello ", type: Regular},
 *     {text: "👋", type: Emoji},
 *     {text: " World ", type: Regular},
 *     {text: "🌍", type: Emoji}
 *   ]
 */
std::vector<TextSegment> segment_text(const std::string& text);

/**
 * Check if text contains any emoji
 */
bool has_emoji(const std::string& text);

// ============================================================================
// Font Names
// ============================================================================

/**
 * Get the recommended emoji font name for the current platform
 */
const char* get_emoji_font_name();

/**
 * Get the recommended symbol font name for the current platform
 */
const char* get_symbol_font_name();

} // namespace flexUI

#endif // FLEXUI_TEXT_UTIL_H
