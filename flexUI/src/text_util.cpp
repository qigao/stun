/*
 * flexUI - Text Utilities
 *
 * UTF-8 scanning is owned by Salts::Unicode. Emoji grouping below remains
 * a rendering heuristic until shared Unicode boundary work in salts-utils#101.
 */

#include <flexUI/text_util.h>
#include <salts_unicode.h>

#include <cstring>
#include <stdexcept>

namespace flexUI {

// ============================================================================
// UTF-8 Utilities
// ============================================================================

Utf8Scalar utf8_next_scalar(const std::string& text, size_t& cursor) {
    if (cursor >= text.size()) {
        throw std::out_of_range("UTF-8 cursor is at or beyond the end of the input");
    }

    const size_t original_cursor = cursor;
    size_t next_cursor = cursor;
    salts_unicode_scalar raw{};
    const salts_unicode_status status = salts_unicode_utf8_next(
        vstr_from_buf(text.data(), text.size()), &next_cursor, &raw);

    if (status == SALTS_UNICODE_OK) {
        cursor = next_cursor;
        return Utf8Scalar{raw.value, raw.byte_offset, raw.byte_length};
    }
    if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
        throw std::invalid_argument(
            "invalid UTF-8 at byte offset " + std::to_string(original_cursor));
    }
    if (status == SALTS_UNICODE_END) {
        throw std::out_of_range("UTF-8 cursor is at the end of the input");
    }
    throw std::invalid_argument("Salts::Unicode rejected the UTF-8 scan arguments");
}

size_t utf8_scalar_count(const std::string& text) {
    size_t cursor = 0;
    size_t count = 0;
    while (cursor < text.size()) {
        (void)utf8_next_scalar(text, cursor);
        ++count;
    }
    return count;
}

// ============================================================================
// Emoji Detection
// ============================================================================

bool is_emoji(uint32_t cp) {
    // Miscellaneous Symbols and Pictographs (1F300-1F5FF)
    if (cp >= 0x1F300 && cp <= 0x1F5FF) return true;

    // Emoticons (1F600-1F64F)
    if (cp >= 0x1F600 && cp <= 0x1F64F) return true;

    // Transport and Map Symbols (1F680-1F6FF)
    if (cp >= 0x1F680 && cp <= 0x1F6FF) return true;

    // Supplemental Symbols and Pictographs (1F900-1F9FF)
    if (cp >= 0x1F900 && cp <= 0x1F9FF) return true;

    // Symbols and Pictographs Extended-A (1FA00-1FA6F)
    if (cp >= 0x1FA00 && cp <= 0x1FA6F) return true;

    // Symbols and Pictographs Extended-B (1FA70-1FAFF)
    if (cp >= 0x1FA70 && cp <= 0x1FAFF) return true;

    // Dingbats (2700-27BF)
    if (cp >= 0x2700 && cp <= 0x27BF) return true;

    // Miscellaneous Symbols (2600-26FF)
    if (cp >= 0x2600 && cp <= 0x26FF) return true;

    // Regional Indicators for flags (1F1E0-1F1FF)
    if (cp >= 0x1F1E0 && cp <= 0x1F1FF) return true;

    // Common standalone emoji
    switch (cp) {
        case 0x2B50:  // Star
        case 0x2B55:  // Circle
        case 0x2764:  // Red heart
        case 0x2763:  // Heart exclamation
        case 0x2665:  // Heart suit
        case 0x2666:  // Diamond suit
        case 0x2660:  // Spade suit
        case 0x2663:  // Club suit
        case 0x270A:  // Raised fist
        case 0x270B:  // Raised hand
        case 0x270C:  // Victory hand
        case 0x270D:  // Writing hand
        case 0x2728:  // Sparkles
        case 0x2744:  // Snowflake
        case 0x274C:  // Cross mark
        case 0x274E:  // Cross mark negative
        case 0x2753:  // Question ornament
        case 0x2754:  // White question
        case 0x2755:  // White exclamation
        case 0x2757:  // Exclamation mark
        case 0x203C:  // Double exclamation
        case 0x2049:  // Exclamation question
        case 0x00A9:  // Copyright
        case 0x00AE:  // Registered
        case 0x2122:  // Trademark
            return true;
    }

    return false;
}

bool is_emoji_modifier(uint32_t cp) {
    // Skin tone modifiers (1F3FB-1F3FF)
    if (cp >= 0x1F3FB && cp <= 0x1F3FF) return true;

    // Variation selectors
    if (cp == 0xFE0E || cp == 0xFE0F) return true;

    // Zero-width joiner
    if (cp == 0x200D) return true;

    // Combining enclosing keycap
    if (cp == 0x20E3) return true;

    return false;
}

// ============================================================================
// Text Segmentation
// ============================================================================

std::vector<TextSegment> segment_text(const std::string& text) {
    std::vector<TextSegment> segments;

    if (text.empty()) return segments;

    TextSegment current;
    current.type = TextSegmentType::Regular;
    current.width = 0;

    size_t pos = 0;
    while (pos < text.size()) {
        size_t char_start = pos;
        uint32_t cp = utf8_next_scalar(text, pos).value;

        // Check if this is an emoji
        if (is_emoji(cp)) {
            // Flush accumulated regular text
            if (!current.text.empty() && current.type == TextSegmentType::Regular) {
                segments.push_back(current);
                current.text.clear();
            }

            // Start emoji segment
            std::string emoji_str = text.substr(char_start, pos - char_start);

            // Consume any following modifiers (skin tone, ZWJ sequences)
            while (pos < text.size()) {
                size_t next_start = pos;
                uint32_t next_cp = utf8_next_scalar(text, pos).value;

                if (is_emoji_modifier(next_cp)) {
                    // Include modifier in emoji
                    emoji_str += text.substr(next_start, pos - next_start);

                    // If ZWJ, also include the next emoji
                    if (next_cp == 0x200D && pos < text.size()) {
                        size_t emoji_start = pos;
                        uint32_t emoji_cp = utf8_next_scalar(text, pos).value;
                        if (is_emoji(emoji_cp) || is_emoji_modifier(emoji_cp)) {
                            emoji_str += text.substr(emoji_start, pos - emoji_start);
                        } else {
                            pos = emoji_start; // Back up
                            break;
                        }
                    }
                } else if (is_emoji(next_cp)) {
                    // Another emoji right after (might be flag sequence)
                    // Check if previous was regional indicator
                    if (cp >= 0x1F1E0 && cp <= 0x1F1FF &&
                        next_cp >= 0x1F1E0 && next_cp <= 0x1F1FF) {
                        // Flag sequence - include second regional indicator
                        emoji_str += text.substr(next_start, pos - next_start);
                        break;
                    } else {
                        // Not a sequence, back up
                        pos = next_start;
                        break;
                    }
                } else {
                    // Not a modifier or emoji, back up
                    pos = next_start;
                    break;
                }
            }

            // Add emoji segment
            TextSegment emoji_seg;
            emoji_seg.text = emoji_str;
            emoji_seg.type = TextSegmentType::Emoji;
            emoji_seg.width = 0;
            segments.push_back(emoji_seg);

        } else {
            // Regular character
            if (current.type != TextSegmentType::Regular && !current.text.empty()) {
                segments.push_back(current);
                current.text.clear();
            }
            current.type = TextSegmentType::Regular;
            current.text += text.substr(char_start, pos - char_start);
        }
    }

    // Flush remaining text
    if (!current.text.empty()) {
        segments.push_back(current);
    }

    return segments;
}

bool has_emoji(const std::string& text) {
    size_t pos = 0;
    bool found = false;
    while (pos < text.size()) {
        const uint32_t cp = utf8_next_scalar(text, pos).value;
        // A positive match must not hide malformed UTF-8 later in the input.
        if (is_emoji(cp)) found = true;
    }
    return found;
}

// ============================================================================
// Platform-specific Font Names
// ============================================================================

const char* get_emoji_font_name() {
    // Prefer symbol fonts so vector and terminal-oriented backends get stable glyphs.
#ifdef _WIN32
    return "Segoe UI Symbol";  // Monochrome, vector-based
#elif __APPLE__
    return "Apple Symbols";
#else
    return "Noto Sans Symbols";
#endif
}

const char* get_symbol_font_name() {
#ifdef _WIN32
    return "Segoe UI Symbol";
#elif __APPLE__
    return "Apple Symbols";
#else
    return "Noto Sans Symbols";
#endif
}

} // namespace flexUI
