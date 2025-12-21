/*
 * tvgbox2 - Text Utilities
 *
 * Provides emoji detection and text segmentation for proper rendering
 * of mixed text (regular characters + emoji) with font fallback.
 */

#ifndef TVGBOX2_TEXT_UTIL_H
#define TVGBOX2_TEXT_UTIL_H

#include <string>
#include <vector>
#include <cstdint>

namespace tvgbox2 {

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
 * Decode a single UTF-8 code point from string
 * @param str The UTF-8 string
 * @param pos Current position (updated to next character)
 * @return The Unicode code point
 */
uint32_t utf8_decode(const std::string& str, size_t& pos);

/**
 * Get the byte length of a UTF-8 character starting at pos
 */
size_t utf8_char_length(const std::string& str, size_t pos);

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

} // namespace tvgbox2

#endif // TVGBOX2_TEXT_UTIL_H
