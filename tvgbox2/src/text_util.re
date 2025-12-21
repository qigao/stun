// re2c --lang c
/*
 * tvgbox2 - Text Utilities (re2c lexer)
 *
 * re2c -8 -o text_util.cpp text_util.re
 */

#include <tvgbox2/text_util.h>
#include <cstring>

namespace tvgbox2 {

// ============================================================================
// UTF-8 Utilities
// ============================================================================

uint32_t utf8_decode(const std::string& str, size_t& pos) {
    if (pos >= str.size()) return 0;

    const unsigned char* s = reinterpret_cast<const unsigned char*>(str.data() + pos);
    uint32_t cp;

    if ((s[0] & 0x80) == 0) {
        cp = s[0];
        pos += 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        pos += 2;
    } else if ((s[0] & 0xF0) == 0xE0) {
        cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        pos += 3;
    } else if ((s[0] & 0xF8) == 0xF0) {
        cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        pos += 4;
    } else {
        pos += 1;
        cp = 0xFFFD; // Replacement character
    }

    return cp;
}

size_t utf8_char_length(const std::string& str, size_t pos) {
    if (pos >= str.size()) return 0;
    unsigned char c = str[pos];
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// ============================================================================
// Emoji Detection
// ============================================================================

bool is_emoji(uint32_t cp) {
    // Comprehensive emoji ranges
    // Reference: Unicode Emoji Data

    // Miscellaneous Symbols and Pictographs
    if (cp >= 0x1F300 && cp <= 0x1F5FF) return true;

    // Emoticons
    if (cp >= 0x1F600 && cp <= 0x1F64F) return true;

    // Transport and Map Symbols
    if (cp >= 0x1F680 && cp <= 0x1F6FF) return true;

    // Supplemental Symbols and Pictographs
    if (cp >= 0x1F900 && cp <= 0x1F9FF) return true;

    // Symbols and Pictographs Extended-A
    if (cp >= 0x1FA00 && cp <= 0x1FA6F) return true;

    // Symbols and Pictographs Extended-B
    if (cp >= 0x1FA70 && cp <= 0x1FAFF) return true;

    // Dingbats
    if (cp >= 0x2700 && cp <= 0x27BF) return true;

    // Miscellaneous Symbols
    if (cp >= 0x2600 && cp <= 0x26FF) return true;

    // Common emoji-style symbols
    if (cp == 0x2B50) return true;  // Star
    if (cp == 0x2B55) return true;  // Circle
    if (cp == 0x2764) return true;  // Heart
    if (cp == 0x2763) return true;  // Heart exclamation
    if (cp == 0x2665) return true;  // Heart suit
    if (cp == 0x2666) return true;  // Diamond suit
    if (cp == 0x2660) return true;  // Spade suit
    if (cp == 0x2663) return true;  // Club suit
    if (cp == 0x270A) return true;  // Raised fist
    if (cp == 0x270B) return true;  // Raised hand
    if (cp == 0x270C) return true;  // Victory hand
    if (cp == 0x270D) return true;  // Writing hand
    if (cp == 0x2728) return true;  // Sparkles
    if (cp == 0x2744) return true;  // Snowflake
    if (cp == 0x274C) return true;  // Cross mark
    if (cp == 0x274E) return true;  // Cross mark (negative)
    if (cp == 0x2753) return true;  // Question mark ornament
    if (cp == 0x2754) return true;  // White question mark
    if (cp == 0x2755) return true;  // White exclamation
    if (cp == 0x2757) return true;  // Exclamation mark
    if (cp == 0x203C) return true;  // Double exclamation
    if (cp == 0x2049) return true;  // Exclamation question

    // Regional Indicators (flags)
    if (cp >= 0x1F1E0 && cp <= 0x1F1FF) return true;

    // Keycap sequences base characters
    if (cp == 0x0023) return false;  // # (only emoji with variation selector)
    if (cp == 0x002A) return false;  // * (only emoji with variation selector)
    if (cp >= 0x0030 && cp <= 0x0039) return false;  // 0-9 (only emoji with variation selector)

    return false;
}

bool is_emoji_modifier(uint32_t cp) {
    // Emoji modifiers (skin tones)
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
// Text Segmentation (re2c generated)
// ============================================================================

/*!re2c
    re2c:define:YYCTYPE = "unsigned char";
    re2c:define:YYCURSOR = cursor;
    re2c:define:YYMARKER = marker;
    re2c:define:YYLIMIT = limit;
    re2c:yyfill:enable = 0;
    re2c:flags:utf-8 = 1;

    // Emoji patterns using Unicode ranges
    emoji_misc     = [\U0001F300-\U0001F5FF];
    emoji_emoticon = [\U0001F600-\U0001F64F];
    emoji_transport = [\U0001F680-\U0001F6FF];
    emoji_supp     = [\U0001F900-\U0001F9FF];
    emoji_ext_a    = [\U0001FA00-\U0001FA6F];
    emoji_ext_b    = [\U0001FA70-\U0001FAFF];
    emoji_dingbat  = [\x{2700}-\x{27BF}];
    emoji_symbol   = [\x{2600}-\x{26FF}];
    emoji_regional = [\U0001F1E0-\U0001F1FF];

    // Modifiers
    skin_tone      = [\U0001F3FB-\U0001F3FF];
    variation_sel  = [\x{FE0E}\x{FE0F}];
    zwj            = [\x{200D}];
    keycap         = [\x{20E3}];

    // Combined emoji (base + modifiers)
    emoji_base = emoji_misc | emoji_emoticon | emoji_transport | emoji_supp
               | emoji_ext_a | emoji_ext_b | emoji_dingbat | emoji_symbol | emoji_regional;
    emoji_mod  = skin_tone | variation_sel | zwj | keycap;
    emoji      = emoji_base emoji_mod*;

    // Regular text (anything else)
    regular = [^\U0001F300-\U0001FAFF\x{2600}-\x{27BF}\x{FE0E}\x{FE0F}\x{200D}\x{20E3}]+;
*/

std::vector<TextSegment> segment_text(const std::string& text) {
    std::vector<TextSegment> segments;

    if (text.empty()) return segments;

    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text.data());
    const unsigned char* limit = cursor + text.size();
    const unsigned char* marker;
    const unsigned char* start;

    TextSegment current;
    current.type = TextSegmentType::Regular;
    current.width = 0;

    while (cursor < limit) {
        start = cursor;

        /*!re2c
            emoji {
                // Flush any accumulated regular text
                if (!current.text.empty() && current.type == TextSegmentType::Regular) {
                    segments.push_back(current);
                    current.text.clear();
                }

                // Add emoji segment
                TextSegment emoji_seg;
                emoji_seg.text = std::string(reinterpret_cast<const char*>(start), cursor - start);
                emoji_seg.type = TextSegmentType::Emoji;
                emoji_seg.width = 0;
                segments.push_back(emoji_seg);
                continue;
            }

            regular {
                // Accumulate regular text
                if (current.type != TextSegmentType::Regular && !current.text.empty()) {
                    segments.push_back(current);
                    current.text.clear();
                }
                current.type = TextSegmentType::Regular;
                current.text += std::string(reinterpret_cast<const char*>(start), cursor - start);
                continue;
            }

            * {
                // Single character fallback
                current.text += std::string(reinterpret_cast<const char*>(start), cursor - start);
                continue;
            }
        */
    }

    // Flush remaining text
    if (!current.text.empty()) {
        segments.push_back(current);
    }

    return segments;
}

bool has_emoji(const std::string& text) {
    size_t pos = 0;
    while (pos < text.size()) {
        uint32_t cp = utf8_decode(text, pos);
        if (is_emoji(cp)) return true;
    }
    return false;
}

// ============================================================================
// Platform-specific Font Names
// ============================================================================

const char* get_emoji_font_name() {
#ifdef _WIN32
    return "Segoe UI Emoji";
#elif __APPLE__
    return "Apple Color Emoji";
#else
    return "Noto Color Emoji";
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

} // namespace tvgbox2
