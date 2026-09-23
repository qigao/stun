/*
 * flexUI - Text Utilities
 *
 * UTF-8 scanning, grapheme boundaries, and Unicode emoji property facts are
 * owned by Salts::Unicode. FlexUI only applies the rendering/font policy.
 */

#include <flexUI/text_util.h>
#include <salts_unicode.h>

#include <cstring>
#include <stdexcept>
#include <utility>

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
// Emoji Detection and Grapheme Segmentation
// ============================================================================

namespace {

uint32_t unicode_properties(uint32_t cp) {
    uint32_t properties = 0u;
    const salts_unicode_status status =
        salts_unicode_scalar_properties(cp, &properties);
    if (status != SALTS_UNICODE_OK) {
        throw std::invalid_argument("Salts::Unicode rejected a scalar property query");
    }
    return properties;
}

bool grapheme_uses_emoji_presentation(vstr cluster) {
    size_t cursor = 0u;
    salts_unicode_scalar scalar{};
    bool has_emoji = false;
    bool has_default_emoji = false;
    bool has_extended_pictographic = false;
    bool has_modifier = false;
    bool has_emoji_selector = false;
    bool has_text_selector = false;
    bool has_keycap = false;

    while (cursor < cluster.len) {
        const salts_unicode_status status =
            salts_unicode_utf8_next(cluster, &cursor, &scalar);
        if (status != SALTS_UNICODE_OK) {
            if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
                throw std::invalid_argument("FlexUI text contains invalid UTF-8");
            }
            throw std::runtime_error("Salts::Unicode failed to scan a grapheme");
        }

        const uint32_t properties = scalar.properties;
        has_emoji |=
            (properties & SALTS_UNICODE_PROPERTY_EMOJI) != 0u;
        has_default_emoji |=
            (properties & SALTS_UNICODE_PROPERTY_EMOJI_PRESENTATION) != 0u;
        has_extended_pictographic |=
            (properties & SALTS_UNICODE_PROPERTY_EXTENDED_PICTOGRAPHIC) != 0u;
        has_modifier |=
            (properties & SALTS_UNICODE_PROPERTY_EMOJI_MODIFIER) != 0u;
        has_text_selector |= scalar.value == 0xFE0Eu;
        has_emoji_selector |= scalar.value == 0xFE0Fu;
        has_keycap |= scalar.value == 0x20E3u;
    }

    if (has_text_selector) {
        return false;
    }
    return has_default_emoji || has_extended_pictographic || has_modifier ||
           (has_emoji && (has_emoji_selector || has_keycap));
}

void append_segment(std::vector<TextSegment>& segments,
                    TextSegmentType type,
                    const char* bytes,
                    size_t length) {
    if (length == 0u) return;
    if (!segments.empty() && segments.back().type == type) {
        segments.back().text.append(bytes, length);
        return;
    }

    TextSegment segment;
    segment.text.assign(bytes, length);
    segment.type = type;
    segment.width = 0.0f;
    segments.push_back(std::move(segment));
}

} // namespace

bool is_emoji(uint32_t cp) {
    return (unicode_properties(cp) & SALTS_UNICODE_PROPERTY_EMOJI) != 0u;
}

bool is_emoji_modifier(uint32_t cp) {
    const uint32_t properties = unicode_properties(cp);
    return (properties & (SALTS_UNICODE_PROPERTY_EMOJI_MODIFIER |
                          SALTS_UNICODE_PROPERTY_EMOJI_COMPONENT)) != 0u;
}

std::vector<TextSegment> segment_text(const std::string& text) {
    std::vector<TextSegment> segments;
    if (text.empty()) return segments;

    size_t cursor = 0u;
    vstr cluster{};
    const vstr input = vstr_from_buf(text.data(), text.size());

    while (cursor < text.size()) {
        const size_t start = cursor;
        const salts_unicode_status status =
            salts_unicode_grapheme_next(input, &cursor, &cluster);
        if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
            throw std::invalid_argument(
                "invalid UTF-8 at byte offset " + std::to_string(start));
        }
        if (status != SALTS_UNICODE_OK || cursor <= start) {
            throw std::runtime_error(
                "Salts::Unicode failed to segment FlexUI text");
        }

        const TextSegmentType type =
            grapheme_uses_emoji_presentation(cluster)
                ? TextSegmentType::Emoji
                : TextSegmentType::Regular;
        append_segment(segments, type, text.data() + start, cursor - start);
    }

    return segments;
}

bool has_emoji(const std::string& text) {
    bool found = false;
    for (const auto& segment : segment_text(text)) {
        if (segment.type == TextSegmentType::Emoji) {
            found = true;
        }
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
