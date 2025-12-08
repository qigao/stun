/*
 * CSS Grid Track Parser - re2c-based parser for grid-template-columns/rows
 *
 * Uses the re2c-generated lexer (css_grid_lexer_gen.c) for tokenization,
 * then implements a recursive descent parser for the grammar:
 *
 *   track-list := track | track track-list
 *   track := size | minmax-func | fit-content-func | repeat-func
 *   size := NUMBER_PX | NUMBER_FR | NUMBER_PERCENT | auto | min-content | max-content
 *   minmax-func := 'minmax' '(' size ',' size ')'
 *   fit-content-func := 'fit-content' '(' NUMBER_PX ')'
 *   repeat-func := 'repeat' '(' (NUMBER | auto-fill | auto-fit) ',' track-list ')'
 */

#include "cssbox_types.h"
#include <vector>
#include <string>
#include <cstring>

// Include re2c-generated lexer
extern "C" {
#include "css_grid_lexer_gen.c"
}

namespace cssbox {
namespace convert {

// Helper to split space-separated names from a line name token
static std::vector<std::string> split_line_names(const char* start, int length) {
    std::vector<std::string> names;
    std::string current;

    for (int i = 0; i < length; ++i) {
        char c = start[i];
        if (c == ' ' || c == '\t') {
            if (!current.empty()) {
                names.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        names.push_back(current);
    }

    return names;
}

// Forward declaration (defined in header, but we need it here)
class GridTrackParser {
public:
    explicit GridTrackParser(const char* input) {
        CSSGridLexer_init(&lexer_, input);
        advance();
    }

    std::vector<GridTrack> parse() {
        std::vector<GridTrack> tracks;
        std::vector<std::string> pending_line_names;  // Names waiting for next track

        while (current_.type != GRID_END && current_.type != GRID_ERROR) {
            // Check for line names
            if (current_.type == GRID_LINE_NAME) {
                // Parse names and add to pending list
                auto names = split_line_names(current_.start, current_.length);
                for (auto& name : names) {
                    pending_line_names.push_back(std::move(name));
                }
                advance();
                continue;
            }

            auto track = parse_track();
            if (track.has_value()) {
                // Attach any pending line names to this track
                if (!pending_line_names.empty()) {
                    track->line_names = std::move(pending_line_names);
                    pending_line_names.clear();
                }
                tracks.push_back(*track);
            } else {
                // Skip unknown token
                advance();
            }
        }

        // Handle trailing line names (names after the last track)
        // Store them in a special track with type AUTO and empty value
        if (!pending_line_names.empty()) {
            GridTrack trailing;
            trailing.type = GridTrack::Type::AUTO;
            trailing.value = -1;  // Marker for trailing names only
            trailing.line_names = std::move(pending_line_names);
            tracks.push_back(trailing);
        }

        return tracks;
    }

private:
    CSSGridLexer lexer_;
    CSSGridToken current_;

    void advance() {
        current_ = CSSGridLexer_next_token(&lexer_);
    }

    bool match(CSSGridTokenType type) {
        if (current_.type == type) {
            advance();
            return true;
        }
        return false;
    }

    std::optional<GridTrack> parse_track() {
        switch (current_.type) {
            case GRID_NUMBER_PX: {
                float val = current_.value;
                advance();
                return GridTrack::px(val);
            }
            case GRID_NUMBER_FR: {
                float val = current_.value;
                advance();
                return GridTrack::fr(val);
            }
            case GRID_NUMBER_PERCENT: {
                // Treat percentage as fixed for now (will resolve at layout time)
                float val = current_.value;
                advance();
                return GridTrack::px(val);  // TODO: Add PERCENT type to GridTrack
            }
            case GRID_AUTO:
                advance();
                return GridTrack::auto_();
            case GRID_MIN_CONTENT:
                advance();
                return GridTrack::min_content();
            case GRID_MAX_CONTENT:
                advance();
                return GridTrack::max_content();
            case GRID_MINMAX:
                return parse_minmax();
            case GRID_FIT_CONTENT:
                return parse_fit_content();
            case GRID_REPEAT:
                return parse_repeat();
            default:
                return std::nullopt;
        }
    }

    GridSizeValue parse_size_value() {
        switch (current_.type) {
            case GRID_NUMBER_PX: {
                float val = current_.value;
                advance();
                return GridSizeValue::px(val);
            }
            case GRID_NUMBER_FR: {
                float val = current_.value;
                advance();
                return GridSizeValue::fr(val);
            }
            case GRID_NUMBER_PERCENT: {
                float val = current_.value;
                advance();
                return GridSizeValue::percent(val);
            }
            case GRID_AUTO:
                advance();
                return GridSizeValue::auto_();
            case GRID_MIN_CONTENT:
                advance();
                return GridSizeValue::min_content();
            case GRID_MAX_CONTENT:
                advance();
                return GridSizeValue::max_content();
            default:
                return GridSizeValue::auto_();
        }
    }

    std::optional<GridTrack> parse_minmax() {
        if (!match(GRID_MINMAX)) return std::nullopt;
        if (!match(GRID_LPAREN)) return std::nullopt;

        GridSizeValue min_size = parse_size_value();

        if (!match(GRID_COMMA)) return std::nullopt;

        GridSizeValue max_size = parse_size_value();

        if (!match(GRID_RPAREN)) return std::nullopt;

        return GridTrack::minmax(min_size, max_size);
    }

    std::optional<GridTrack> parse_fit_content() {
        if (!match(GRID_FIT_CONTENT)) return std::nullopt;
        if (!match(GRID_LPAREN)) return std::nullopt;

        float limit = 0;
        if (current_.type == GRID_NUMBER_PX) {
            limit = current_.value;
            advance();
        }

        if (!match(GRID_RPAREN)) return std::nullopt;

        return GridTrack::fit_content(limit);
    }

    std::optional<GridTrack> parse_repeat() {
        if (!match(GRID_REPEAT)) return std::nullopt;
        if (!match(GRID_LPAREN)) return std::nullopt;

        // Parse count (number, auto-fill, or auto-fit)
        int count = 1;
        bool is_auto_fill = false;
        bool is_auto_fit = false;

        if (current_.type == GRID_NUMBER) {
            count = static_cast<int>(current_.value);
            advance();
        } else if (current_.type == GRID_AUTO_FILL) {
            is_auto_fill = true;
            advance();
        } else if (current_.type == GRID_AUTO_FIT) {
            is_auto_fit = true;
            advance();
        }

        if (!match(GRID_COMMA)) return std::nullopt;

        // Parse track list inside repeat()
        std::vector<GridTrack> inner_tracks;
        int paren_depth = 1;  // We're inside repeat(

        while (current_.type != GRID_END && paren_depth > 0) {
            if (current_.type == GRID_RPAREN) {
                paren_depth--;
                if (paren_depth == 0) break;
            }
            if (current_.type == GRID_LPAREN) {
                paren_depth++;
            }

            auto track = parse_track();
            if (track.has_value()) {
                inner_tracks.push_back(*track);
            }
        }

        match(GRID_RPAREN);  // Consume closing paren

        // Return tracks - for auto-fill/auto-fit, just return inner tracks once
        // (actual expansion happens at layout time)
        // For integer count, expand here
        if (is_auto_fill || is_auto_fit) {
            // Return first track as placeholder (layout engine handles expansion)
            if (!inner_tracks.empty()) {
                return inner_tracks[0];
            }
            return std::nullopt;
        }

        // For integer count, we return std::nullopt here and let the caller handle
        // by calling parse_repeat_expanded() instead
        // Actually, we need to expand here. Let's use a different approach.
        return std::nullopt;  // Will be handled by expanded version
    }

public:
    // Parse and expand repeat() functions
    std::vector<GridTrack> parse_with_repeat_expansion() {
        std::vector<GridTrack> tracks;

        while (current_.type != GRID_END && current_.type != GRID_ERROR) {
            if (current_.type == GRID_REPEAT) {
                auto expanded = parse_repeat_expanded();
                tracks.insert(tracks.end(), expanded.begin(), expanded.end());
            } else {
                auto track = parse_track();
                if (track.has_value()) {
                    tracks.push_back(*track);
                } else {
                    advance();  // Skip unknown
                }
            }
        }

        return tracks;
    }

private:
    std::vector<GridTrack> parse_repeat_expanded() {
        std::vector<GridTrack> result;

        if (!match(GRID_REPEAT)) return result;
        if (!match(GRID_LPAREN)) return result;

        int count = 1;
        bool is_auto = false;

        if (current_.type == GRID_NUMBER) {
            count = static_cast<int>(current_.value);
            advance();
        } else if (current_.type == GRID_AUTO_FILL || current_.type == GRID_AUTO_FIT) {
            is_auto = true;
            count = 1;  // Return tracks once, layout handles expansion
            advance();
        }

        if (!match(GRID_COMMA)) return result;

        // Collect inner tracks until closing paren
        std::vector<GridTrack> inner_tracks;
        int paren_depth = 1;

        while (current_.type != GRID_END) {
            if (current_.type == GRID_RPAREN) {
                paren_depth--;
                if (paren_depth == 0) {
                    advance();  // Consume )
                    break;
                }
            }

            if (current_.type == GRID_MINMAX) {
                auto t = parse_minmax();
                if (t) inner_tracks.push_back(*t);
            } else if (current_.type == GRID_FIT_CONTENT) {
                auto t = parse_fit_content();
                if (t) inner_tracks.push_back(*t);
            } else {
                auto t = parse_track();
                if (t) inner_tracks.push_back(*t);
                else advance();
            }
        }

        // Expand
        if (is_auto) {
            // auto-fill/auto-fit: return inner tracks once
            result = inner_tracks;
        } else {
            // Integer count: expand N times
            for (int i = 0; i < count && i < 100; i++) {  // Safety limit
                for (const auto& t : inner_tracks) {
                    result.push_back(t);
                }
            }
        }

        return result;
    }
};

// Public API function
std::vector<GridTrack> parse_grid_track_list_re2c(const char* input) {
    if (!input || !*input) return {};
    GridTrackParser parser(input);
    return parser.parse_with_repeat_expansion();
}

} // namespace convert
} // namespace cssbox
