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
// Emoji Detection
// ============================================================================

bool is_emoji(uint32_t cp) {
    int extended_pictographic = 0;
    if (salts_unicode_is_extended_pictographic(cp, &extended_pictographic) !=
        SALTS_UNICODE_OK) {
        return false;
    }
    if (extended_pictographic != 0) {
        return true;
    }

    salts_unicode_grapheme_break gcb = SALTS_UNICODE_GRAPHEME_OTHER;
    return salts_unicode_grapheme_break_class(cp, &gcb) == SALTS_UNICODE_OK &&
           gcb == SALTS_UNICODE_GRAPHEME_REGIONAL_INDICATOR;
}

namespace {

[[noreturn]] void throw_unicode_segment_error(salts_unicode_status status,
                                              const char* operation) {
    if (status == SALTS_UNICODE_ERR_INVALID_UTF8) {
        throw std::invalid_argument("FlexUI text contains invalid UTF-8");
    }
    throw std::runtime_error(operation);
}

bool cluster_uses_emoji_font(vstr cluster) {
    size_t cursor = 0u;
    salts_unicode_scalar scalar{};

    while (cursor < cluster.len) {
        const salts_unicode_status status =
            salts_unicode_utf8_next(cluster, &cursor, &scalar);
        if (status != SALTS_UNICODE_OK) {
            throw_unicode_segment_error(
                status, "Salts::Unicode failed to inspect grapheme cluster");
        }

        if (is_emoji(scalar.value) ||
            scalar.value == 0xFE0Fu || /* emoji variation selector */
            scalar.value == 0x20E3u) { /* combining enclosing keycap */
            return true;
        }
    }
    return false;
}

} // namespace

// ============================================================================
// Text Segmentation
// ============================================================================

std::vector<TextSegment> segment_text(const std::string& text) {
    std::vector<TextSegment> segments;
    if (text.empty()) return segments;

    const vstr input = vstr_from_buf(text.data(), text.size());
    size_t cursor = 0u;
    vstr cluster{};
    TextSegment regular;
    regular.type = TextSegmentType::Regular;
    regular.width = 0.0f;

    while (cursor < input.len) {
        const salts_unicode_status status =
            salts_unicode_grapheme_next(input, &cursor, &cluster);
        if (status != SALTS_UNICODE_OK) {
            throw_unicode_segment_error(
                status, "Salts::Unicode failed to segment grapheme clusters");
        }

        if (cluster_uses_emoji_font(cluster)) {
            if (!regular.text.empty()) {
                segments.push_back(regular);
                regular.text.clear();
            }
            TextSegment emoji;
            emoji.text.assign(cluster.data, cluster.len);
            emoji.type = TextSegmentType::Emoji;
            emoji.width = 0.0f;
            segments.push_back(std::move(emoji));
        } else {
            regular.text.append(cluster.data, cluster.len);
        }
    }

    if (!regular.text.empty()) {
        segments.push_back(std::move(regular));
    }
    return segments;
}

bool has_emoji(const std::string& text) {
    if (text.empty()) return false;

    const vstr input = vstr_from_buf(text.data(), text.size());
    size_t cursor = 0u;
    vstr cluster{};
    bool found = false;

    while (cursor < input.len) {
        const salts_unicode_status status =
            salts_unicode_grapheme_next(input, &cursor, &cluster);
        if (status != SALTS_UNICODE_OK) {
            throw_unicode_segment_error(
                status, "Salts::Unicode failed to segment grapheme clusters");
        }
        if (cluster_uses_emoji_font(cluster)) {
            found = true;
        }
    }
    return found;
}

// ============================================================================
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
