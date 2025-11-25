/*
 * NanoVG CSS - Grid Layout (Phase 4 Sprint 5)
 *
 * Basic CSS Grid implementation with fixed-size tracks.
 * Based on W3C CSS Grid Layout Module Level 1
 */

#include "nanovg_css_internal.h"
#include <fmtlog.h>
#include <algorithm>
#include <sstream>
#include <cmath>

// Forward declarations (Sprint 18 & 19)
float apply_dimension_constraints(float value, float min_value, float max_value);
void calculate_content_dimensions(
    float total_width,
    float total_height,
    const float padding[4],
    const float border[4],
    const std::string& box_sizing,
    float& content_width,
    float& content_height);

// ============================================================================
// Grid Track Structure
// ============================================================================

struct GridTrack {
    enum Type {
        FIXED,       // px values
        FRACTIONAL,  // fr values
        AUTO,        // auto sizing
        MINMAX       // minmax() function (Sprint 15)
    };

    Type type;
    float value;           // Either px or fr value
    float computed_size;   // Final size after calculation

    // Sprint 15: minmax constraints
    struct MinMaxConstraints {
        Type min_type;     // Type of minimum (FIXED, AUTO, FRACTIONAL)
        float min_value;   // Minimum value
        Type max_type;     // Type of maximum (FIXED, AUTO, FRACTIONAL)
        float max_value;   // Maximum value

        MinMaxConstraints() : min_type(FIXED), min_value(0),
                             max_type(FIXED), max_value(0) {}
    } minmax;

    GridTrack() : type(FIXED), value(100.0f), computed_size(100.0f) {}
    GridTrack(Type t, float v) : type(t), value(v), computed_size(v) {}
};

// ============================================================================
// Grid Item Placement
// ============================================================================

struct GridItemPlacement {
    int row_start;      // 1-based row start
    int row_end;        // 1-based row end
    int column_start;   // 1-based column start
    int column_end;     // 1-based column end

    float x, y;         // Computed position
    float width, height; // Computed size

    NVGCSSElement* element;

    GridItemPlacement() : row_start(-1), row_end(-1),
                          column_start(-1), column_end(-1),
                          x(0), y(0), width(0), height(0),
                          element(nullptr) {}

    // Sprint 12: Helper methods for spanning
    int column_span() const {
        if (column_end > 0 && column_start > 0) {
            return column_end - column_start;
        }
        return (element && element->explicit_style.grid_column_span > 0)
            ? element->explicit_style.grid_column_span : 1;
    }

    int row_span() const {
        if (row_end > 0 && row_start > 0) {
            return row_end - row_start;
        }
        return (element && element->explicit_style.grid_row_span > 0)
            ? element->explicit_style.grid_row_span : 1;
    }
};

// ============================================================================
// Grid Container
// ============================================================================

struct GridContainer {
    std::vector<GridTrack> rows;
    std::vector<GridTrack> columns;

    float row_gap;
    float column_gap;

    float container_x;
    float container_y;
    float container_width;
    float container_height;

    std::vector<GridItemPlacement> items;

    // Alignment properties (Sprint 8)
    std::string justify_items;   // start | end | center | stretch
    std::string align_items;     // start | end | center | stretch
    std::string justify_content; // start | end | center | space-between | space-around | space-evenly
    std::string align_content;   // start | end | center | space-between | space-around | space-evenly

    // Named Grid Lines (Sprint 26)
    // Maps line name → list of line indices (1-based)
    std::map<std::string, std::vector<int>> column_line_names;
    std::map<std::string, std::vector<int>> row_line_names;
    // Grid Auto Flow (Sprint 32)
    std::string grid_auto_flow;  // "row" | "column" | "dense" | "row dense" | "column dense"


    GridContainer() : row_gap(0), column_gap(0),
                      container_x(0), container_y(0),
                      container_width(0), container_height(0),
                      justify_items("stretch"), align_items("stretch"),
                      justify_content("start"), align_content("start"), grid_auto_flow("row") {}
};

// ============================================================================
// Grid Template Areas (Sprint 13)
// ============================================================================

/**
 * @brief Represents a named grid area with its boundaries
 */
struct GridArea {
    std::string name;        // Area name (e.g., "header")
    int row_start;           // 1-based row start
    int row_end;             // 1-based row end (exclusive)
    int column_start;        // 1-based column start
    int column_end;          // 1-based column end (exclusive)

    GridArea() : row_start(-1), row_end(-1),
                 column_start(-1), column_end(-1) {}

    GridArea(const std::string& n) : name(n), row_start(-1), row_end(-1),
                                     column_start(-1), column_end(-1) {}
};

/**
 * @brief Parsed grid template areas
 */
struct GridTemplateAreas {
    std::vector<std::vector<std::string>> grid;  // 2D grid of area names ("." = empty)
    std::map<std::string, GridArea> areas;       // Name -> area mapping
    int num_rows;
    int num_cols;

    GridTemplateAreas() : num_rows(0), num_cols(0) {}

    bool is_valid() const {
        return num_rows > 0 && num_cols > 0 && !grid.empty();
    }
};

// ============================================================================
// Named Grid Lines (Sprint 26)
// ============================================================================

/**
 * @brief Result of parsing a track list with named lines
 */
struct TrackListWithNames {
    std::vector<GridTrack> tracks;
    std::map<std::string, std::vector<int>> line_names;  // name → line indices (1-based)
};

/**
 * @brief Resolve a line name to a line number
 *
 * @param line_names Map of line names to line numbers
 * @param name The line name to resolve
 * @param occurrence Which occurrence to use (1 = first, 2 = second, etc.)
 * @return Line number (1-based), or -1 if not found
 */
int resolve_line_name(
    const std::map<std::string, std::vector<int>>& line_names,
    const std::string& name,
    int occurrence = 1
) {
    auto it = line_names.find(name);
    if (it == line_names.end()) {
        return -1;  // Name not found
    }

    // Return nth occurrence (1-based index)
    if (occurrence > 0 && occurrence <= (int)it->second.size()) {
        return it->second[occurrence - 1];
    }

    return -1;
}

/**
 * @brief Parse grid position value that might be a name or number
 *
 * @param value The CSS value (e.g., "start", "3", "span 2")
 * @param line_names Map of line names for resolution
 * @return Line number (1-based), 0 for auto, or negative for span
 */
int parse_grid_position_value(
    const std::string& value,
    const std::map<std::string, std::vector<int>>& line_names
) {
    std::string trimmed = nvgcss_utils::trim(value);

    if (trimmed.empty() || trimmed == "auto") {
        return 0;  // Auto placement
    }

    // Check for "span" keyword
    if (trimmed.find("span") == 0) {
        // Extract span count (e.g., "span 2" → -2)
        std::string span_str = trimmed.substr(4);
        span_str = nvgcss_utils::trim(span_str);
        if (!span_str.empty()) {
            try {
                int span_count = std::stoi(span_str);
                return -span_count;  // Negative indicates span
            } catch (const std::exception&) {
                // Invalid span value, treat as auto
                return 0;
            }
        }
        return -1;  // "span" alone means span 1
    }

    // Check if it's a pure number (including negative numbers)
    bool is_number = !trimmed.empty();
    for (size_t i = 0; i < trimmed.length(); i++) {
        char c = trimmed[i];
        // Allow negative sign at start
        if (i == 0 && c == '-') continue;
        if (!std::isdigit(c)) {
            is_number = false;
            break;
        }
    }

    if (is_number) {
        try {
            return std::stoi(trimmed);
        } catch (const std::exception&) {
            // Parse error, treat as auto
            return 0;
        }
    }

    // It's a name - resolve it
    int line_num = resolve_line_name(line_names, trimmed);
    if (line_num > 0) {
        return line_num;
    }

    // Name not found - treat as auto
    return 0;
}

// ============================================================================
// Parsing Functions
// ============================================================================

// ============================================================================
// Grid Template Areas Parsing (Sprint 13)
// ============================================================================

/**
 * @brief Split template areas string into rows
 */
std::vector<std::string> split_area_rows(const std::string& value) {
    std::vector<std::string> rows;
    if (value.empty()) return rows;

    std::string current_row;
    bool in_quotes = false;

    for (size_t i = 0; i < value.length(); i++) {
        char c = value[i];

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (!in_quotes && (c == '\n' || c == '\r')) {
            // End of row
            if (!current_row.empty()) {
                rows.push_back(current_row);
                current_row.clear();
            }
        } else if (in_quotes) {
            current_row += c;
        }
    }

    // Add last row if not empty
    if (!current_row.empty()) {
        rows.push_back(current_row);
    }

    return rows;
}

/**
 * @brief Split a row string into column area names
 */
std::vector<std::string> split_area_columns(const std::string& row) {
    std::vector<std::string> columns;
    std::istringstream ss(row);
    std::string name;

    while (ss >> name) {
        columns.push_back(name);
    }

    return columns;
}

/**
 * @brief Extract named areas from the grid
 */
void extract_named_areas(GridTemplateAreas& template_areas) {
    for (int r = 0; r < template_areas.num_rows; r++) {
        for (int c = 0; c < template_areas.num_cols; c++) {
            const std::string& name = template_areas.grid[r][c];

            // Skip empty cells
            if (name.empty() || name == ".") continue;

            if (template_areas.areas.find(name) == template_areas.areas.end()) {
                // First occurrence - create new area
                GridArea area(name);
                area.row_start = r + 1;
                area.column_start = c + 1;
                area.row_end = r + 2;      // Will be updated
                area.column_end = c + 2;   // Will be updated
                template_areas.areas[name] = area;
            } else {
                // Update existing area's end bounds
                template_areas.areas[name].row_end = std::max(
                    template_areas.areas[name].row_end, r + 2);
                template_areas.areas[name].column_end = std::max(
                    template_areas.areas[name].column_end, c + 2);
            }
        }
    }
}

/**
 * @brief Validate that all areas are rectangular
 */
bool validate_areas_rectangular(const GridTemplateAreas& template_areas) {
    for (const auto& [name, area] : template_areas.areas) {
        // Check that all cells in the rectangle contain this name
        for (int r = area.row_start - 1; r < area.row_end - 1; r++) {
            for (int c = area.column_start - 1; c < area.column_end - 1; c++) {
                if (r >= template_areas.num_rows || c >= template_areas.num_cols) {
                    return false;  // Out of bounds
                }
                if (template_areas.grid[r][c] != name) {
                    return false;  // Non-rectangular area
                }
            }
        }
    }
    return true;
}

/**
 * @brief Parse grid-template-areas CSS property
 */
GridTemplateAreas parse_grid_template_areas(const std::string& value) {
    GridTemplateAreas result;

    if (value.empty()) return result;

    // Split by rows
    std::vector<std::string> rows = split_area_rows(value);

    if (rows.empty()) return result;

    // Parse each row
    for (const auto& row_str : rows) {
        std::vector<std::string> columns = split_area_columns(row_str);
        if (!columns.empty()) {
            result.grid.push_back(columns);
        }
    }

    if (result.grid.empty()) return result;

    result.num_rows = result.grid.size();
    result.num_cols = result.grid[0].size();

    // Validate: all rows have same column count
    for (const auto& row : result.grid) {
        if ((int)row.size() != result.num_cols) {
            // Error: inconsistent columns
            return GridTemplateAreas();  // Return empty
        }
    }

    // Extract named areas
    extract_named_areas(result);

    // Validate: all areas are rectangular
    if (!validate_areas_rectangular(result)) {
        return GridTemplateAreas();  // Return empty
    }

    return result;
}

// ============================================================================
// Track Parsing - Sprint 15: minmax() Support, Sprint 16: repeat() Support
// ============================================================================

/**
 * @brief Repeat pattern for repeat() function
 *
 * Sprint 16: Stores repeat count and track pattern
 * Sprint 17: Added auto-fill and auto-fit support
 */
struct RepeatPattern {
    enum CountType {
        FIXED,      // Integer count (e.g., repeat(3, 1fr))
        AUTO_FILL,  // auto-fill keyword (creates max tracks, keeps empty ones)
        AUTO_FIT    // auto-fit keyword (creates max tracks, collapses empty ones)
    };

    CountType count_type;             // Type of repeat count
    int count;                        // Repeat count (for FIXED, or calculated for AUTO_*)
    std::vector<std::string> tracks;  // Track pattern to repeat

    RepeatPattern() : count_type(FIXED), count(0) {}
};

// Forward declarations
bool parse_minmax_track(const std::string& token, GridTrack& track);

/**
 * @brief Split a track list string into individual track tokens
 *
 * Sprint 16: Handles nested functions like minmax() and repeat() properly
 * Example: "100px minmax(200px, 1fr) auto" -> ["100px", "minmax(200px, 1fr)", "auto"]
 */
std::vector<std::string> split_track_list(const std::string& track_list) {
    std::vector<std::string> tokens;
    std::string current_token;
    int paren_depth = 0;

    for (size_t i = 0; i < track_list.length(); i++) {
        char c = track_list[i];

        if (c == '(') {
            paren_depth++;
            current_token += c;
        } else if (c == ')') {
            paren_depth--;
            current_token += c;
        } else if ((c == ' ' || c == '\t') && paren_depth == 0) {
            // Space outside of parentheses - end of token
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
        } else {
            current_token += c;
        }
    }

    // Add last token
    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }

    return tokens;
}

/**
 * @brief Parse a single track token (px, fr, auto, calc(), min(), max(), clamp(), or minmax)
 *
 * Sprint 16: Extracted from parse_track_list() for reuse in repeat()
 * Sprint 24: Added calc() support
 * Sprint 25: Added min(), max(), clamp() support
 */
GridTrack parse_single_track(const std::string& token, float container_size = 0.0f) {
    GridTrack track;

    // Try parsing as minmax first (Sprint 15)
    if (parse_minmax_track(token, track)) {
        return track;
    }
    // Sprint 24: Check for calc() function
    else if (token.find("calc(") == 0 && token.back() == ')') {
        // calc() expression - extract and evaluate
        std::string calc_expr = token.substr(5, token.length() - 6);
        // Evaluate with container_size as context for percentages
        float px_value = nvgcss_utils::parse_calc_expression(calc_expr, container_size);
        track.type = GridTrack::FIXED;
        track.value = px_value;
        return track;
    }
    // Sprint 25: Check for min() function
    else if (token.find("min(") == 0 && token.back() == ')') {
        std::string args = token.substr(4, token.length() - 5);
        float px_value = nvgcss_utils::parse_min_function(args, container_size, 16.0f);
        track.type = GridTrack::FIXED;
        track.value = px_value;
        return track;
    }
    // Sprint 25: Check for max() function
    else if (token.find("max(") == 0 && token.back() == ')') {
        std::string args = token.substr(4, token.length() - 5);
        float px_value = nvgcss_utils::parse_max_function(args, container_size, 16.0f);
        track.type = GridTrack::FIXED;
        track.value = px_value;
        return track;
    }
    // Sprint 25: Check for clamp() function
    else if (token.find("clamp(") == 0 && token.back() == ')') {
        std::string args = token.substr(6, token.length() - 7);
        float px_value = nvgcss_utils::parse_clamp_function(args, container_size, 16.0f);
        track.type = GridTrack::FIXED;
        track.value = px_value;
        return track;
    }
    else if (token.find("fr") != std::string::npos) {
        // Fractional unit: "1fr", "2.5fr"
        float fr_value = std::stof(token.substr(0, token.find("fr")));
        track.type = GridTrack::FRACTIONAL;
        track.value = fr_value;
        return track;
    }
    else if (token.find("px") != std::string::npos) {
        // Fixed pixel unit: "100px"
        float px_value = std::stof(token.substr(0, token.find("px")));
        track.type = GridTrack::FIXED;
        track.value = px_value;
        return track;
    }
    else if (token == "auto") {
        // Auto sizing
        track.type = GridTrack::AUTO;
        track.value = 0;
        return track;
    }
    else {
        // Plain number, treat as px
        try {
            float value = std::stof(token);
            track.type = GridTrack::FIXED;
            track.value = value;
        } catch (const std::exception&) {
            // Invalid number, use default
            track.type = GridTrack::FIXED;
            track.value = 100.0f;
        }
        return track;
    }
}

/**
 * @brief Parse a repeat() function
 *
 * Sprint 16: Parses repeat(count, track-list) syntax
 * Sprint 17: Added support for auto-fill and auto-fit
 * Examples:
 *   "repeat(3, 1fr)"
 *   "repeat(auto-fill, minmax(200px, 1fr))"
 *   "repeat(auto-fit, minmax(250px, 1fr))"
 */
bool parse_repeat_function(const std::string& token, RepeatPattern& pattern) {
    // Check if token starts with "repeat("
    if (token.find("repeat(") != 0) {
        return false;
    }

    // Extract content between parentheses
    size_t start = token.find('(');
    size_t end = token.rfind(')');
    if (start == std::string::npos || end == std::string::npos) {
        return false;
    }

    std::string content = token.substr(start + 1, end - start - 1);

    // Find first comma (separates count from track-list)
    size_t comma = content.find(',');
    if (comma == std::string::npos) {
        return false;
    }

    // Parse count (integer or auto-fill/auto-fit)
    std::string count_str = content.substr(0, comma);
    count_str.erase(0, count_str.find_first_not_of(" \t"));
    count_str.erase(count_str.find_last_not_of(" \t") + 1);

    // Sprint 17: Check for auto-fill and auto-fit keywords
    if (count_str == "auto-fill") {
        pattern.count_type = RepeatPattern::AUTO_FILL;
        pattern.count = -1;  // Will be calculated during track sizing
    } else if (count_str == "auto-fit") {
        pattern.count_type = RepeatPattern::AUTO_FIT;
        pattern.count = -1;  // Will be calculated during track sizing
    } else {
        // Integer count (existing Sprint 16 logic)
        try {
            pattern.count_type = RepeatPattern::FIXED;
            pattern.count = std::stoi(count_str);
        } catch (const std::exception&) {
            // Invalid repeat count
            return false;
        }

        if (pattern.count <= 0) {
            return false;  // Invalid count
        }
    }

    // Extract track-list (everything after first comma)
    std::string track_list_str = content.substr(comma + 1);
    track_list_str.erase(0, track_list_str.find_first_not_of(" \t"));
    track_list_str.erase(track_list_str.find_last_not_of(" \t") + 1);

    // Split track-list into individual tracks
    // This handles nested functions like minmax() properly
    pattern.tracks = split_track_list(track_list_str);

    return true;
}

/**
 * @brief Parse a single minmax track value
 *
 * Parses: minmax(min, max) where min/max can be px, fr, or auto
 * Examples: "minmax(100px, 300px)", "minmax(auto, 1fr)", "minmax(100px, auto)"
 */
bool parse_minmax_track(const std::string& token, GridTrack& track) {
    // Check if token starts with "minmax("
    if (token.find("minmax(") != 0) {
        return false;
    }

    // Extract content between parentheses
    size_t start = token.find('(');
    size_t end = token.rfind(')');
    if (start == std::string::npos || end == std::string::npos) {
        return false;
    }

    std::string content = token.substr(start + 1, end - start - 1);

    // Split by comma
    size_t comma = content.find(',');
    if (comma == std::string::npos) {
        return false;
    }

    std::string min_str = content.substr(0, comma);
    std::string max_str = content.substr(comma + 1);

    // Trim whitespace
    min_str.erase(0, min_str.find_first_not_of(" \t"));
    min_str.erase(min_str.find_last_not_of(" \t") + 1);
    max_str.erase(0, max_str.find_first_not_of(" \t"));
    max_str.erase(max_str.find_last_not_of(" \t") + 1);

    // Parse min value
    if (min_str == "auto") {
        track.minmax.min_type = GridTrack::AUTO;
        track.minmax.min_value = 0;
    } else if (min_str.find("fr") != std::string::npos) {
        track.minmax.min_type = GridTrack::FRACTIONAL;
        track.minmax.min_value = std::stof(min_str.substr(0, min_str.find("fr")));
    } else if (min_str.find("px") != std::string::npos) {
        track.minmax.min_type = GridTrack::FIXED;
        track.minmax.min_value = std::stof(min_str.substr(0, min_str.find("px")));
    } else {
        // Plain number, treat as px
        try {
            track.minmax.min_type = GridTrack::FIXED;
            track.minmax.min_value = std::stof(min_str);
        } catch (const std::exception&) {
            // Invalid minmax minimum value
            return false;
        }
    }

    // Parse max value
    if (max_str == "auto") {
        track.minmax.max_type = GridTrack::AUTO;
        track.minmax.max_value = 0;
    } else if (max_str.find("fr") != std::string::npos) {
        track.minmax.max_type = GridTrack::FRACTIONAL;
        track.minmax.max_value = std::stof(max_str.substr(0, max_str.find("fr")));
    } else if (max_str.find("px") != std::string::npos) {
        track.minmax.max_type = GridTrack::FIXED;
        track.minmax.max_value = std::stof(max_str.substr(0, max_str.find("px")));
    } else {
        // Plain number, treat as px
        try {
            track.minmax.max_type = GridTrack::FIXED;
            track.minmax.max_value = std::stof(max_str);
        } catch (const std::exception&) {
            // Invalid minmax maximum value
            return false;
        }
    }

    // Validate: max should be >= min (for fixed values)
    if (track.minmax.min_type == GridTrack::FIXED &&
        track.minmax.max_type == GridTrack::FIXED &&
        track.minmax.max_value < track.minmax.min_value) {
        // If max < min, treat as fixed at max value
        track.type = GridTrack::FIXED;
        track.value = track.minmax.max_value;
        return true;
    }

    track.type = GridTrack::MINMAX;
    return true;
}

/**
 * @brief Calculate track count for auto-fill/auto-fit
 *
 * Sprint 17: Determines how many repetitions of the track pattern can fit
 * in the available container space.
 *
 * Algorithm:
 * 1. Calculate minimum size of one pattern repetition
 * 2. Account for gaps between tracks
 * 3. Divide available space by (pattern_min + gap)
 * 4. Return maximum number of repetitions that fit
 *
 * @param pattern The repeat pattern with track definitions
 * @param container_size Total container size
 * @param gap Gap between tracks
 * @return Number of pattern repetitions that fit (at least 1)
 */
int calculate_auto_track_count(
    const RepeatPattern& pattern,
    float container_size,
    float gap)
{
    // Calculate minimum size of one pattern repetition
    float pattern_min_size = 0;

    for (const auto& track_str : pattern.tracks) {
        GridTrack track = parse_single_track(track_str, container_size);

        if (track.type == GridTrack::FIXED) {
            // Fixed track: use its value
            pattern_min_size += track.value;
        } else if (track.type == GridTrack::MINMAX) {
            // minmax track: use minimum value
            if (track.minmax.min_type == GridTrack::FIXED) {
                pattern_min_size += track.minmax.min_value;
            }
            // AUTO min is treated as 0 for this calculation
        }
        // FR and AUTO tracks have 0 minimum for this calculation
    }

    // Add gaps within the pattern (between tracks in one repetition)
    if (pattern.tracks.size() > 1) {
        pattern_min_size += gap * (pattern.tracks.size() - 1);
    }

    if (pattern_min_size <= 0) {
        return 1;  // At least one repetition
    }

    // Calculate how many pattern repetitions fit
    // Each repetition needs pattern_min_size
    // Plus gap between repetitions (but not after the last one)
    int count = 0;
    float used_space = 0;

    while (used_space + pattern_min_size <= container_size) {
        count++;
        used_space += pattern_min_size;

        // Add gap before next repetition (if there's space for another)
        if (used_space + gap + pattern_min_size <= container_size) {
            used_space += gap;
        }
    }

    return std::max(1, count);  // At least one repetition
}


/**
 * @brief Parse track list with support for px, fr, auto, minmax(), and repeat()
 *
 * Sprint 16: Now handles repeat() function for concise track definitions
 * Sprint 17: Added auto-fill and auto-fit support in repeat()
 * Examples:
 *   "100px 200px" -> [100px, 200px]
 *   "repeat(3, 1fr)" -> [1fr, 1fr, 1fr]
 *   "100px repeat(2, 1fr) 200px" -> [100px, 1fr, 1fr, 200px]
 *   "repeat(auto-fill, minmax(200px, 1fr))" -> calculated based on container_size
 *
 * @param value The track list string to parse
 * @param container_size Container width (for columns) or height (for rows)
 * @param gap Gap between tracks
 * @return Vector of parsed and expanded GridTrack objects
 */
std::vector<GridTrack> parse_track_list(
    const std::string& value,
    float container_size,
    float gap)
{
    std::vector<GridTrack> tracks;
    if (value.empty()) return tracks;

    // Sprint 16: Split into tokens handling nested functions
    std::vector<std::string> tokens = split_track_list(value);

    for (const auto& token : tokens) {
        // Try parsing as repeat() first (Sprint 16)
        RepeatPattern pattern;
        if (parse_repeat_function(token, pattern)) {
            // Sprint 17: Calculate count for auto-fill/auto-fit
            if (pattern.count_type == RepeatPattern::AUTO_FILL ||
                pattern.count_type == RepeatPattern::AUTO_FIT) {
                // Calculate how many repetitions fit in the container
                pattern.count = calculate_auto_track_count(pattern, container_size, gap);
            }

            // Expand repeat: repeat(N, tracks) -> tracks repeated N times
            for (int i = 0; i < pattern.count; i++) {
                for (const auto& track_token : pattern.tracks) {
                    GridTrack track = parse_single_track(track_token, container_size);
                    tracks.push_back(track);
                }
            }
        } else {
            // Parse as regular track (px, fr, auto, calc(), or minmax)
            GridTrack track = parse_single_track(token, container_size);
            tracks.push_back(track);
        }
    }

    return tracks;
}

/**
 * @brief Parse track list with named grid lines (Sprint 26)
 *
 * Parses grid-template-columns/rows with named lines like:
 * "[start] 100px [middle] 200px [end]"
 *
 * @param value CSS track list string
 * @param container_size Container size for percentage/calc resolution
 * @param gap Gap size for auto-fill/auto-fit calculations
 * @return Tracks and line names
 */
TrackListWithNames parse_track_list_with_names(
    const std::string& value,
    float container_size,
    float gap)
{
    TrackListWithNames result;
    if (value.empty()) return result;

    // Parse character by character to extract line names and tracks
    size_t i = 0;
    int current_line = 1;  // Line numbers are 1-based

    while (i < value.length()) {
        // Skip whitespace
        while (i < value.length() && std::isspace(value[i])) i++;
        if (i >= value.length()) break;

        // Check for line names: [name1 name2]
        if (value[i] == '[') {
            size_t end = value.find(']', i);
            if (end != std::string::npos) {
                // Extract names between brackets
                std::string names_str = value.substr(i + 1, end - i - 1);

                // Split by spaces to get multiple names
                std::vector<std::string> names;
                std::string current_name;
                for (char c : names_str) {
                    if (std::isspace(c)) {
                        if (!current_name.empty()) {
                            names.push_back(nvgcss_utils::trim(current_name));
                            current_name.clear();
                        }
                    } else {
                        current_name += c;
                    }
                }
                if (!current_name.empty()) {
                    names.push_back(nvgcss_utils::trim(current_name));
                }

                // Add all names to the current line
                for (const auto& name : names) {
                    if (!name.empty()) {
                        result.line_names[name].push_back(current_line);
                    }
                }

                i = end + 1;
                continue;
            }
        }

        // Parse track size token
        size_t track_start = i;

        // Handle nested parentheses for calc(), min(), max(), etc.
        int paren_depth = 0;
        while (i < value.length()) {
            if (value[i] == '(') {
                paren_depth++;
                i++;
            } else if (value[i] == ')') {
                paren_depth--;
                i++;
            } else if (value[i] == '[') {
                // Hit next line name, stop here
                break;
            } else if (paren_depth == 0 && std::isspace(value[i])) {
                // Space outside parentheses = end of token
                break;
            } else {
                i++;
            }
        }

        std::string track_token = value.substr(track_start, i - track_start);
        track_token = nvgcss_utils::trim(track_token);

        if (!track_token.empty()) {
            // Try parsing as repeat() first
            RepeatPattern pattern;
            if (parse_repeat_function(track_token, pattern)) {
                // Calculate count for auto-fill/auto-fit
                if (pattern.count_type == RepeatPattern::AUTO_FILL ||
                    pattern.count_type == RepeatPattern::AUTO_FIT) {
                    pattern.count = calculate_auto_track_count(pattern, container_size, gap);
                }

                // Expand repeat
                for (int rep = 0; rep < pattern.count; rep++) {
                    for (const auto& track_str : pattern.tracks) {
                        GridTrack track = parse_single_track(track_str, container_size);
                        result.tracks.push_back(track);
                        current_line++;  // Each track creates a new line after it
                    }
                }
            } else {
                // Parse as regular track
                GridTrack track = parse_single_track(track_token, container_size);
                result.tracks.push_back(track);
                current_line++;  // Each track creates a new line after it
            }
        }
    }

    return result;
}

// ============================================================================
// Content Measurement for Auto Track Sizing (Sprint 14)
// ============================================================================

/**
 * @brief Measure the content width of an element
 *
 * For auto track sizing, we need to measure how much space an element
 * needs based on its content.
 *
 * Sprint 14: Uses explicit_style (input) not computed (output)
 */
float measure_content_width(NVGCSSElement* element, NVGCSSRenderer* renderer) {
    if (!element) return 0;

    float content_width = 0;

    // If element has explicit width, use that
    if (element->explicit_style.width > 0) {
        content_width = element->explicit_style.width;
    }
    // For text elements with NanoVG context
    else if (renderer && renderer->vg && !element->text_content.empty()) {
        float bounds[4];
        nvgTextBounds(renderer->vg, 0, 0, element->text_content.c_str(), nullptr, bounds);
        content_width = bounds[2] - bounds[0];
    }
    // For containers, measure children
    // IMPORTANT: Copy children locally before recursing, because nvgcssGetChildren
    // uses a static cache that gets overwritten by recursive calls
    int child_count = 0;
    NVGCSSElement** children_ptr = nvgcssGetChildren(renderer, element, &child_count);
    std::vector<NVGCSSElement*> children_copy(children_ptr, children_ptr + child_count);
    if (!children_copy.empty()) {
        // For flex/grid containers, sum or max children depending on direction
        // For now, use max width of children
        for (NVGCSSElement* child : children_copy) {
            float child_width = measure_content_width(child, renderer);
            content_width = std::max(content_width, child_width);
        }
    }

    // Add padding and border from explicit_style
    // padding[4] = top, right, bottom, left
    // border_width[4] = top, right, bottom, left
    content_width += element->explicit_style.padding[3] + element->explicit_style.padding[1];  // left + right
    content_width += element->explicit_style.border_width[3] + element->explicit_style.border_width[1];    // left + right

    return content_width;
}

/**
 * @brief Measure the content height of an element
 *
 * Sprint 14: Uses explicit_style (input) not computed (output)
 */
float measure_content_height(NVGCSSElement* element, NVGCSSRenderer* renderer) {
    if (!element) return 0;

    float content_height = 0;

    // If element has explicit height, use that
    if (element->explicit_style.height > 0) {
        content_height = element->explicit_style.height;
    }
    // For text elements with NanoVG context
    else if (renderer && renderer->vg && !element->text_content.empty()) {
        float bounds[4];
        nvgTextBounds(renderer->vg, 0, 0, element->text_content.c_str(), nullptr, bounds);
        content_height = bounds[3] - bounds[1];
    }
    // For containers, measure children
    // IMPORTANT: Copy children locally before recursing, because nvgcssGetChildren
    // uses a static cache that gets overwritten by recursive calls
    int child_count = 0;
    NVGCSSElement** children_ptr = nvgcssGetChildren(renderer, element, &child_count);
    std::vector<NVGCSSElement*> children_copy(children_ptr, children_ptr + child_count);
    if (!children_copy.empty()) {
        // Use max height of children
        for (NVGCSSElement* child : children_copy) {
            float child_height = measure_content_height(child, renderer);
            content_height = std::max(content_height, child_height);
        }
    }

    // Add padding and border from explicit_style
    // padding[4] = top, right, bottom, left
    // border_width[4] = top, right, bottom, left
    content_height += element->explicit_style.padding[0] + element->explicit_style.padding[2];  // top + bottom
    content_height += element->explicit_style.border_width[0] + element->explicit_style.border_width[2];    // top + bottom

    return content_height;
}

/**
 * @brief Helper function to measure content size for a track
 *
 * Sprint 15: Extracted for reuse in AUTO and MINMAX track sizing
 */
float measure_track_content(
    size_t track_idx,
    const std::vector<GridItemPlacement>& items,
    NVGCSSRenderer* renderer,
    bool is_column_axis,
    const std::vector<std::pair<int, int>>& preliminary_positions)
{
    float max_content = 0;

    // Find all items that use this track
    for (size_t item_idx = 0; item_idx < items.size(); item_idx++) {
        const auto& item = items[item_idx];
        int item_track_start = preliminary_positions[item_idx].first;
        int item_track_end = preliminary_positions[item_idx].second;

        // Check if item uses this track (track_idx is 0-based, positions are 1-based)
        bool item_uses_track = (item_track_start <= (int)(track_idx + 1) &&
                               item_track_end > (int)(track_idx + 1));

        if (item_uses_track && item.element) {
            // Measure content size
            float content_size = 0;
            if (is_column_axis) {
                content_size = measure_content_width(item.element, renderer);
            } else {
                content_size = measure_content_height(item.element, renderer);
            }

            // If item spans multiple tracks, divide size proportionally
            int span = item_track_end - item_track_start;
            if (span > 1) {
                content_size = content_size / span;
            }

            max_content = std::max(max_content, content_size);
        }
    }

    return max_content;
}

/**
 * @brief Compute track sizes, resolving auto, fr, and minmax units
 *
 * Sprint 14: Added AUTO track sizing based on content measurement
 * Sprint 15: Added MINMAX track sizing with min/max constraints
 * Sizing order: FIXED → AUTO → MINMAX (min) → FRACTIONAL → MINMAX (clamp to max)
 */
void compute_track_sizes(
    std::vector<GridTrack>& tracks,
    float container_size,
    float gap,
    const std::vector<GridItemPlacement>& items,
    NVGCSSRenderer* renderer,
    bool is_column_axis)
{
    if (tracks.empty()) return;

    // Step 1: Calculate fixed space
    float fixed_space = 0;

    for (const auto& track : tracks) {
        if (track.type == GridTrack::FIXED) {
            fixed_space += track.value;
        }
    }

    // Preliminary auto-placement for items without explicit positions
    // This gives us a rough idea of which track each item will be in for sizing
    std::vector<std::pair<int, int>> preliminary_positions;  // (start, end) for each item
    size_t auto_placement_counter = 0;

    for (const auto& item : items) {
        int start, end;
        if (is_column_axis) {
            if (item.column_start != -1) {
                start = item.column_start;
                end = item.column_end;
            } else {
                // Auto-place sequentially
                start = (auto_placement_counter % tracks.size()) + 1;  // 1-based
                end = start + item.column_span();
                auto_placement_counter++;
            }
        } else {
            if (item.row_start != -1) {
                start = item.row_start;
                end = item.row_end;
            } else {
                // Auto-place sequentially
                start = (auto_placement_counter % tracks.size()) + 1;  // 1-based
                end = start + item.row_span();
                auto_placement_counter++;
            }
        }
        preliminary_positions.push_back({start, end});
    }

    // Step 2: Calculate AUTO track sizes based on content
    float auto_space = 0;

    for (size_t track_idx = 0; track_idx < tracks.size(); track_idx++) {
        if (tracks[track_idx].type == GridTrack::AUTO) {
            float content = measure_track_content(track_idx, items, renderer,
                                                  is_column_axis, preliminary_positions);
            tracks[track_idx].computed_size = content;
            auto_space += content;
        }
    }

    // Step 3: Calculate MINMAX track sizes (Sprint 15)
    float minmax_space = 0;

    for (size_t track_idx = 0; track_idx < tracks.size(); track_idx++) {
        if (tracks[track_idx].type == GridTrack::MINMAX) {
            float min_size = 0;
            float max_size = std::numeric_limits<float>::max();

            // Calculate minimum size based on min_type
            if (tracks[track_idx].minmax.min_type == GridTrack::FIXED) {
                min_size = tracks[track_idx].minmax.min_value;
            } else if (tracks[track_idx].minmax.min_type == GridTrack::AUTO) {
                min_size = measure_track_content(track_idx, items, renderer,
                                                is_column_axis, preliminary_positions);
            }
            // FR as min is treated as 0

            // Calculate maximum size based on max_type
            if (tracks[track_idx].minmax.max_type == GridTrack::FIXED) {
                max_size = tracks[track_idx].minmax.max_value;
            } else if (tracks[track_idx].minmax.max_type == GridTrack::AUTO) {
                max_size = measure_track_content(track_idx, items, renderer,
                                                is_column_axis, preliminary_positions);
            }
            // FR max handled later in fr distribution

            // For non-FR max: size to content clamped to [min, max]
            if (tracks[track_idx].minmax.max_type != GridTrack::FRACTIONAL) {
                float content_size = measure_track_content(track_idx, items, renderer,
                                                           is_column_axis, preliminary_positions);
                tracks[track_idx].computed_size = std::max(min_size, std::min(content_size, max_size));
                minmax_space += tracks[track_idx].computed_size;
            } else {
                // For FR max: start at minimum, will grow in fr distribution
                tracks[track_idx].computed_size = min_size;
                minmax_space += min_size;
            }
        }
    }

    // Step 4: Calculate gaps
    float total_gaps = (tracks.size() > 1) ? (gap * (tracks.size() - 1)) : 0;

    // Step 5: Calculate available space for fr units
    float available_space = container_size - fixed_space - auto_space - minmax_space - total_gaps;
    available_space = std::max(0.0f, available_space);  // Clamp to 0

    // Step 6: Calculate total fr (including minmax with fr max)
    float total_fr = 0;
    for (const auto& track : tracks) {
        if (track.type == GridTrack::FRACTIONAL) {
            total_fr += track.value;
        } else if (track.type == GridTrack::MINMAX &&
                   track.minmax.max_type == GridTrack::FRACTIONAL) {
            // MINMAX with FR max participates in fr distribution
            total_fr += track.minmax.max_value;
        }
    }

    // Step 7: Calculate size per fr
    float fr_size = (total_fr > 0) ? (available_space / total_fr) : 0;

    // DEBUG: Log fr calculation
    // logi("compute_track_sizes: container={}, fixed={}, auto={}, gaps={}, available={}, total_fr={}, fr_size={}",
    //      container_size, fixed_space, auto_space, total_gaps, available_space, total_fr, fr_size);

    // Step 8: Set computed sizes for all tracks
    for (auto& track : tracks) {
        if (track.type == GridTrack::FIXED) {
            track.computed_size = track.value;
        } else if (track.type == GridTrack::FRACTIONAL) {
            track.computed_size = track.value * fr_size;
        } else if (track.type == GridTrack::MINMAX) {
            // If max is FR, grow from minimum
            if (track.minmax.max_type == GridTrack::FRACTIONAL) {
                track.computed_size = track.computed_size + (track.minmax.max_value * fr_size);
            }
            // Note: AUTO and FIXED max types are handled in clamping below
        }
        // AUTO tracks already have computed_size set in Step 2
    }

    // Step 9: Clamp MINMAX tracks with FR max to their constraints (Sprint 15)
    for (size_t track_idx = 0; track_idx < tracks.size(); track_idx++) {
        if (tracks[track_idx].type == GridTrack::MINMAX &&
            tracks[track_idx].minmax.max_type == GridTrack::FRACTIONAL) {
            // Only need to handle FR max tracks here (non-FR already sized in Step 3)

            // Calculate minimum size
            float min_size = 0;
            if (tracks[track_idx].minmax.min_type == GridTrack::FIXED) {
                min_size = tracks[track_idx].minmax.min_value;
            } else if (tracks[track_idx].minmax.min_type == GridTrack::AUTO) {
                min_size = measure_track_content(track_idx, items, renderer,
                                                is_column_axis, preliminary_positions);
            }

            // FR max doesn't impose an upper limit, just ensure we meet minimum
            tracks[track_idx].computed_size = std::max(tracks[track_idx].computed_size, min_size);
        }
    }
}

// ============================================================================
// Grid Layout Algorithm
// ============================================================================

/**
 * @brief Parse grid container properties
 */
GridContainer parse_grid_container(NVGCSSElement* element)
{
    GridContainer grid;

    // Sprint 17: Get container dimensions and gaps FIRST
    // (needed for auto-fill/auto-fit calculation)

    // Container dimensions and position
    // FIX: Account for padding - children should be positioned inside the content area
    float padding_top = element->explicit_style.padding[0];
    float padding_right = element->explicit_style.padding[1];
    float padding_bottom = element->explicit_style.padding[2];
    float padding_left = element->explicit_style.padding[3];

    grid.container_x = element->computed.x + padding_left;
    grid.container_y = element->computed.y + padding_top;
    grid.container_width = (element->computed.width > 0 ? element->computed.width : 400.0f)
                           - padding_left - padding_right;
    grid.container_height = (element->computed.height > 0 ? element->computed.height : 300.0f)
                            - padding_top - padding_bottom;

    // DEBUG: Log container dimensions
    // logi("Grid container '{}': computed size = {}x{}, using size = {}x{}",
    //      element->id, element->computed.width, element->computed.height,
    //      grid.container_width, grid.container_height);

    // Parse gaps from TYPED properties (60fps refactor)
    grid.row_gap = element->style.grid_row_gap.resolve(grid.container_height, 16.0f, grid.container_height);
    grid.column_gap = element->style.grid_column_gap.resolve(grid.container_width, 16.0f, grid.container_width);

    // Read grid-template-rows from TYPED properties (60fps refactor)
    if (!element->style.grid_template_rows.empty()) {
        // Convert typed GridTrack to local GridTrack
        for (const auto& typed_track : element->style.grid_template_rows) {
            GridTrack track;
            switch (typed_track.type) {
                case nvgcss::GridTrack::Type::PX:
                    track.type = GridTrack::FIXED;
                    track.value = typed_track.value;
                    break;
                case nvgcss::GridTrack::Type::FR:
                    track.type = GridTrack::FRACTIONAL;
                    track.value = typed_track.value;
                    break;
                case nvgcss::GridTrack::Type::AUTO:
                    track.type = GridTrack::AUTO;
                    track.value = 0;
                    break;
                case nvgcss::GridTrack::Type::MINMAX:
                    track.type = GridTrack::MINMAX;
                    track.minmax.min_type = GridTrack::FIXED;
                    track.minmax.min_value = typed_track.min_val;
                    track.minmax.max_type = GridTrack::FIXED;
                    track.minmax.max_value = typed_track.max_val;
                    break;
            }
            grid.rows.push_back(track);
        }
    } else if (!element->explicit_style.grid_template_rows.empty()) {
        // Fallback: parse from string (for tests using inline_style)
        auto result = parse_track_list_with_names(element->explicit_style.grid_template_rows,
                                                  grid.container_height, grid.row_gap);
        grid.rows = result.tracks;
        grid.row_line_names = result.line_names;
    }

    // Read grid-template-columns from TYPED properties (60fps refactor)
    if (!element->style.grid_template_columns.empty()) {
        // Convert typed GridTrack to local GridTrack
        for (const auto& typed_track : element->style.grid_template_columns) {
            GridTrack track;
            switch (typed_track.type) {
                case nvgcss::GridTrack::Type::PX:
                    track.type = GridTrack::FIXED;
                    track.value = typed_track.value;
                    break;
                case nvgcss::GridTrack::Type::FR:
                    track.type = GridTrack::FRACTIONAL;
                    track.value = typed_track.value;
                    break;
                case nvgcss::GridTrack::Type::AUTO:
                    track.type = GridTrack::AUTO;
                    track.value = 0;
                    break;
                case nvgcss::GridTrack::Type::MINMAX:
                    track.type = GridTrack::MINMAX;
                    track.minmax.min_type = GridTrack::FIXED;
                    track.minmax.min_value = typed_track.min_val;
                    track.minmax.max_type = GridTrack::FIXED;
                    track.minmax.max_value = typed_track.max_val;
                    break;
            }
            grid.columns.push_back(track);
        }
    } else if (!element->explicit_style.grid_template_columns.empty()) {
        // Fallback: parse from string (for tests using inline_style)
        auto result = parse_track_list_with_names(element->explicit_style.grid_template_columns,
                                                  grid.container_width, grid.column_gap);
        grid.columns = result.tracks;
        grid.column_line_names = result.line_names;
    }

    // Default to 1x1 grid if not specified
    if (grid.rows.empty()) {
        grid.rows.push_back(GridTrack(GridTrack::FIXED, 100.0f));
    }
    if (grid.columns.empty()) {
        grid.columns.push_back(GridTrack(GridTrack::FIXED, 100.0f));
    }

    // Parse alignment properties (Sprint 8) - from typed properties
    // TODO: justify-items not yet in typed system - reading from inline_style
    if (element->inline_style.count("justify-items")) {
        grid.justify_items = element->inline_style.at("justify-items");
    }

    // align-items from typed enum
    switch (element->style.align_items) {
        case nvgcss::AlignItems::FLEX_START: grid.align_items = "flex-start"; break;
        case nvgcss::AlignItems::FLEX_END: grid.align_items = "flex-end"; break;
        case nvgcss::AlignItems::CENTER: grid.align_items = "center"; break;
        case nvgcss::AlignItems::BASELINE: grid.align_items = "baseline"; break;
        case nvgcss::AlignItems::STRETCH: grid.align_items = "stretch"; break;
    }

    // justify-content from typed enum
    switch (element->style.justify_content) {
        case nvgcss::JustifyContent::FLEX_START: grid.justify_content = "flex-start"; break;
        case nvgcss::JustifyContent::FLEX_END: grid.justify_content = "flex-end"; break;
        case nvgcss::JustifyContent::CENTER: grid.justify_content = "center"; break;
        case nvgcss::JustifyContent::SPACE_BETWEEN: grid.justify_content = "space-between"; break;
        case nvgcss::JustifyContent::SPACE_AROUND: grid.justify_content = "space-around"; break;
        case nvgcss::JustifyContent::SPACE_EVENLY: grid.justify_content = "space-evenly"; break;
    }

    // align-content from typed enum
    switch (element->style.align_content) {
        case nvgcss::AlignContent::FLEX_START: grid.align_content = "flex-start"; break;
        case nvgcss::AlignContent::FLEX_END: grid.align_content = "flex-end"; break;
        case nvgcss::AlignContent::CENTER: grid.align_content = "center"; break;
        case nvgcss::AlignContent::SPACE_BETWEEN: grid.align_content = "space-between"; break;
        case nvgcss::AlignContent::SPACE_AROUND: grid.align_content = "space-around"; break;
        case nvgcss::AlignContent::STRETCH: grid.align_content = "stretch"; break;
    }
    // Sprint 32: Grid auto-flow
    grid.grid_auto_flow = element->explicit_style.grid_auto_flow;


    return grid;
}

/**
 * @brief Collect grid items from children
 */
void collect_grid_items(
    NVGCSSElement* container,
    GridContainer& grid,
    const GridTemplateAreas& template_areas,
    NVGCSSRenderer* renderer)
{
    int child_count = 0;
    NVGCSSElement** children = nvgcssGetChildren(renderer, container, &child_count);
    for (int i = 0; i < child_count; ++i) {
        NVGCSSElement* child = children[i];
        if (!child->visible) continue;

        GridItemPlacement item;
        item.element = child;

        // Compute style for child to get raw CSS values
        auto child_style = renderer->stylesheet->compute_style(
            child->id, child->type, child->classes,
            child->attributes, child->pseudo_states, child->inline_style, {},
            child->child_index, child->total_siblings);

        // Sprint 13: Check for grid-area first
        if (!child->explicit_style.grid_area.empty() && template_areas.is_valid()) {
            std::string area_name = child->explicit_style.grid_area;

            // Look up area in template
            auto it = template_areas.areas.find(area_name);
            if (it != template_areas.areas.end()) {
                const GridArea& area = it->second;
                item.row_start = area.row_start;
                item.row_end = area.row_end;
                item.column_start = area.column_start;
                item.column_end = area.column_end;
            } else {
                // Area not found - fall back to line-based
                item.row_start = child->explicit_style.grid_row_start;
                item.row_end = child->explicit_style.grid_row_end;
                item.column_start = child->explicit_style.grid_column_start;
                item.column_end = child->explicit_style.grid_column_end;
            }
        }
        // Fall back to line-based positioning (Sprint 26: with named line resolution)
        else {
            // Initialize with defaults
            item.row_start = 0;
            item.row_end = 0;
            item.column_start = 0;
            item.column_end = 0;

            // Sprint 26: Handle grid-row shorthand (e.g., "start / end")
            auto row_it = child_style.find("grid-row");
            if (row_it != child_style.end()) {
                std::string grid_row = row_it->second;
                size_t slash_pos = grid_row.find('/');
                if (slash_pos != std::string::npos) {
                    std::string start_str = nvgcss_utils::trim(grid_row.substr(0, slash_pos));
                    std::string end_str = nvgcss_utils::trim(grid_row.substr(slash_pos + 1));

                    item.row_start = parse_grid_position_value(start_str, grid.row_line_names);
                    item.row_end = parse_grid_position_value(end_str, grid.row_line_names);
                } else {
                    // Single value, use it for start
                    item.row_start = parse_grid_position_value(grid_row, grid.row_line_names);
                }
            } else {
                // Check individual properties
                auto row_start_it = child_style.find("grid-row-start");
                if (row_start_it != child_style.end()) {
                    item.row_start = parse_grid_position_value(row_start_it->second, grid.row_line_names);
                }

                auto row_end_it = child_style.find("grid-row-end");
                if (row_end_it != child_style.end()) {
                    item.row_end = parse_grid_position_value(row_end_it->second, grid.row_line_names);
                }
            }

            // Sprint 26: Handle grid-column shorthand (e.g., "start / end")
            auto col_it = child_style.find("grid-column");
            if (col_it != child_style.end()) {
                std::string grid_column = col_it->second;
                size_t slash_pos = grid_column.find('/');
                if (slash_pos != std::string::npos) {
                    std::string start_str = nvgcss_utils::trim(grid_column.substr(0, slash_pos));
                    std::string end_str = nvgcss_utils::trim(grid_column.substr(slash_pos + 1));

                    item.column_start = parse_grid_position_value(start_str, grid.column_line_names);
                    item.column_end = parse_grid_position_value(end_str, grid.column_line_names);
                } else {
                    // Single value, use it for start
                    item.column_start = parse_grid_position_value(grid_column, grid.column_line_names);
                }
            } else {
                // Check individual properties
                auto col_start_it = child_style.find("grid-column-start");
                if (col_start_it != child_style.end()) {
                    item.column_start = parse_grid_position_value(col_start_it->second, grid.column_line_names);
                }

                auto col_end_it = child_style.find("grid-column-end");
                if (col_end_it != child_style.end()) {
                    item.column_end = parse_grid_position_value(col_end_it->second, grid.column_line_names);
                }
            }

            // Fall back to explicit_style values if still 0 (auto)
            if (item.row_start == 0) item.row_start = child->explicit_style.grid_row_start;
            if (item.row_end == 0) item.row_end = child->explicit_style.grid_row_end;
            if (item.column_start == 0) item.column_start = child->explicit_style.grid_column_start;
            if (item.column_end == 0) item.column_end = child->explicit_style.grid_column_end;
        }

        grid.items.push_back(item);
    }
}

/**
 * @brief Auto-place grid items (Sprint 32: with dense packing support)
 */
void auto_place_items(GridContainer& grid)
{
    int num_rows = grid.rows.size();
    int num_cols = grid.columns.size();

    // Create occupancy grid (larger to accommodate implicit rows/columns)
    std::vector<std::vector<bool>> occupied(
        std::max(num_rows, 100), std::vector<bool>(std::max(num_cols, 100), false));

    // Sprint 32: Parse grid-auto-flow
    bool is_dense = (grid.grid_auto_flow.find("dense") != std::string::npos);
    bool is_column = (grid.grid_auto_flow.find("column") != std::string::npos);

    // Helper: Check if span can fit at position
    auto can_place_span = [&](int row, int col, int row_span, int col_span) {
        if (col + col_span > num_cols) return false;
        for (int r = row; r < row + row_span && r < num_rows; r++) {
            for (int c = col; c < col + col_span; c++) {
                if (occupied[r][c]) return false;
            }
        }
        return true;
    };

    // Helper: Mark cells as occupied
    auto mark_occupied = [&](int row, int col, int row_span, int col_span) {
        for (int r = row; r < row + row_span; r++) {
            for (int c = col; c < col + col_span; c++) {
                if (r < (int)occupied.size() && c < (int)occupied[0].size()) {
                    occupied[r][c] = true;
                }
            }
        }
    };

    // Sprint 32: Helper for dense mode - find first fit from beginning
    auto find_first_fit = [&](int row_span, int col_span, int& out_row, int& out_col) -> bool {
        if (is_column) {
            // Column-first search
            for (int c = 0; c <= num_cols - col_span; c++) {
                for (int r = 0; r <= num_rows - row_span; r++) {
                    if (can_place_span(r, c, row_span, col_span)) {
                        out_row = r;
                        out_col = c;
                        return true;
                    }
                }
            }
        } else {
            // Row-first search (default)
            for (int r = 0; r <= num_rows - row_span; r++) {
                for (int c = 0; c <= num_cols - col_span; c++) {
                    if (can_place_span(r, c, row_span, col_span)) {
                        out_row = r;
                        out_col = c;
                        return true;
                    }
                }
            }
        }
        return false;
    };

    // First pass: Place items with explicit positions
    for (auto& item : grid.items) {
        if (item.row_start >= 1 && item.column_start >= 1) {
            int row_span = item.row_span();
            int col_span = item.column_span();

            // Calculate end positions if not set
            if (item.row_end < 1) {
                item.row_end = item.row_start + row_span;
            }
            if (item.column_end < 1) {
                item.column_end = item.column_start + col_span;
            }

            // Mark cells as occupied
            mark_occupied(item.row_start - 1, item.column_start - 1, row_span, col_span);
        }
    }

    // Second pass: Auto-place remaining items
    int current_row = 0, current_col = 0;

    for (auto& item : grid.items) {
        if (item.row_start < 1 || item.column_start < 1) {
            int row_span = item.row_span();
            int col_span = item.column_span();
            bool placed = false;

            // Case 1: Explicit column, auto row
            if (item.column_start >= 1 && item.row_start < 1) {
                int col = item.column_start - 1;

                // Calculate end column if not set
                if (item.column_end < 1) {
                    item.column_end = item.column_start + col_span;
                }

                // Find first available row at this column that fits the span
                for (int row = 0; row < num_rows; row++) {
                    if (can_place_span(row, col, row_span, col_span)) {
                        item.row_start = row + 1;
                        item.row_end = item.row_start + row_span;
                        mark_occupied(row, col, row_span, col_span);
                        placed = true;
                        break;
                    }
                }
            }
            // Case 2: Explicit row, auto column
            else if (item.row_start >= 1 && item.column_start < 1) {
                int row = item.row_start - 1;

                // Calculate end row if not set
                if (item.row_end < 1) {
                    item.row_end = item.row_start + row_span;
                }

                // Find first available column at this row that fits the span
                for (int col = 0; col < num_cols; col++) {
                    if (can_place_span(row, col, row_span, col_span)) {
                        item.column_start = col + 1;
                        item.column_end = item.column_start + col_span;
                        mark_occupied(row, col, row_span, col_span);
                        placed = true;
                        break;
                    }
                }
            }
            // Case 3: Both auto - Sprint 32: use dense or normal mode
            else {
                if (is_dense) {
                    // Dense mode: Search from beginning for first fit
                    int found_row, found_col;
                    if (find_first_fit(row_span, col_span, found_row, found_col)) {
                        item.row_start = found_row + 1;
                        item.row_end = item.row_start + row_span;
                        item.column_start = found_col + 1;
                        item.column_end = item.column_start + col_span;
                        mark_occupied(found_row, found_col, row_span, col_span);
                        placed = true;
                    }
                } else if (is_column) {
                    // Column-first normal mode: advance column first, then row
                    while (current_col < num_cols && !placed) {
                        while (current_row < num_rows) {
                            if (can_place_span(current_row, current_col, row_span, col_span)) {
                                // Found space for item
                                item.row_start = current_row + 1;
                                item.row_end = item.row_start + row_span;
                                item.column_start = current_col + 1;
                                item.column_end = item.column_start + col_span;

                                mark_occupied(current_row, current_col, row_span, col_span);
                                current_row += row_span;  // Advance by span height
                                placed = true;
                                break;
                            }
                            current_row++;
                        }

                        if (!placed) {
                            // Move to next column
                            current_col++;
                            current_row = 0;
                        }
                    }
                } else {
                    // Row-first normal mode (default): advance row first, then column
                    while (current_row < num_rows && !placed) {
                        while (current_col < num_cols) {
                            if (can_place_span(current_row, current_col, row_span, col_span)) {
                                // Found space for item
                                item.row_start = current_row + 1;
                                item.row_end = item.row_start + row_span;
                                item.column_start = current_col + 1;
                                item.column_end = item.column_start + col_span;

                                mark_occupied(current_row, current_col, row_span, col_span);
                                current_col += col_span;  // Advance by span width
                                placed = true;
                                break;
                            }
                            current_col++;
                        }

                        if (!placed) {
                            // Move to next row
                            current_row++;
                            current_col = 0;
                        }
                    }
                }
            }

            // If we ran out of space, place at end (create implicit tracks)
            if (!placed) {
                if (is_column) {
                    item.column_start = current_col + 1;
                    item.column_end = item.column_start + col_span;
                    item.row_start = current_row + 1;
                    item.row_end = item.row_start + row_span;
                } else {
                    item.row_start = current_row + 1;
                    item.row_end = item.row_start + row_span;
                    item.column_start = current_col + 1;
                    item.column_end = item.column_start + col_span;
                }
            }
        }
    }
}

/**
 * @brief Apply content alignment (align entire grid in container)
 */
void apply_content_alignment(GridContainer& grid) {
    // Calculate total grid size
    float total_width = 0;
    float total_height = 0;

    for (const auto& col : grid.columns) {
        total_width += col.computed_size;
    }
    if (grid.columns.size() > 1) {
        total_width += grid.column_gap * (grid.columns.size() - 1);
    }

    for (const auto& row : grid.rows) {
        total_height += row.computed_size;
    }
    if (grid.rows.size() > 1) {
        total_height += grid.row_gap * (grid.rows.size() - 1);
    }

    // Calculate free space
    float free_width = grid.container_width - total_width;
    float free_height = grid.container_height - total_height;

    // Apply justify-content (horizontal)
    if (free_width > 0) {
        if (grid.justify_content == "center") {
            grid.container_x += free_width / 2.0f;
        } else if (grid.justify_content == "end") {
            grid.container_x += free_width;
        } else if (grid.justify_content == "space-between") {
            // Distribute free space between columns
            if (grid.columns.size() > 1) {
                float extra_gap = free_width / (grid.columns.size() - 1);
                grid.column_gap += extra_gap;
            }
        } else if (grid.justify_content == "space-around") {
            // Distribute free space around columns
            if (grid.columns.size() > 0) {
                float space = free_width / grid.columns.size();
                grid.container_x += space / 2.0f;
                grid.column_gap += space;
            }
        } else if (grid.justify_content == "space-evenly") {
            // Distribute free space evenly
            if (grid.columns.size() > 0) {
                float space = free_width / (grid.columns.size() + 1);
                grid.container_x += space;
                grid.column_gap += space;
            }
        }
        // else "start" - no adjustment needed
    }

    // Apply align-content (vertical)
    if (free_height > 0) {
        if (grid.align_content == "center") {
            grid.container_y += free_height / 2.0f;
        } else if (grid.align_content == "end") {
            grid.container_y += free_height;
        } else if (grid.align_content == "space-between") {
            if (grid.rows.size() > 1) {
                float extra_gap = free_height / (grid.rows.size() - 1);
                grid.row_gap += extra_gap;
            }
        } else if (grid.align_content == "space-around") {
            if (grid.rows.size() > 0) {
                float space = free_height / grid.rows.size();
                grid.container_y += space / 2.0f;
                grid.row_gap += space;
            }
        } else if (grid.align_content == "space-evenly") {
            if (grid.rows.size() > 0) {
                float space = free_height / (grid.rows.size() + 1);
                grid.container_y += space;
                grid.row_gap += space;
            }
        }
        // else "start" - no adjustment needed
    }
}

/**
 * @brief Position grid items
 */
void position_grid_items(GridContainer& grid)
{
    for (auto& item : grid.items) {
        // Calculate starting position
        float x = grid.container_x;
        float y = grid.container_y;

        // Sum up column widths before this item
        for (int c = 0; c < item.column_start - 1 && c < (int)grid.columns.size(); c++) {
            x += grid.columns[c].computed_size;
            // Add gap after this column if there's another column after it
            if (c + 1 < (int)grid.columns.size()) {
                x += grid.column_gap;
            }
        }

        // Sum up row heights before this item
        for (int r = 0; r < item.row_start - 1 && r < (int)grid.rows.size(); r++) {
            y += grid.rows[r].computed_size;
            // Add gap after this row if there's another row after it
            if (r + 1 < (int)grid.rows.size()) {
                y += grid.row_gap;
            }
        }

        // Calculate item size
        float width = 0;
        float height = 0;

        // FIXED: Properly handle grid-column-span and grid-row-span
        int c_start = item.column_start - 1;
        int c_span = item.column_span();
        int c_end = item.column_end >= 1 ? item.column_end - 1 : c_start + c_span;

        // Ensure c_end doesn't exceed grid bounds
        c_end = std::min(c_end, (int)grid.columns.size());

        for (int c = c_start; c < c_end && c < (int)grid.columns.size(); c++) {
            width += grid.columns[c].computed_size;
            if (c < c_end - 1) {
                width += grid.column_gap;
            }
        }

        // FIXED: Properly handle grid-row-span
        int r_start = item.row_start - 1;
        int r_span = item.row_span();
        int r_end = item.row_end >= 1 ? item.row_end - 1 : r_start + r_span;

        // Ensure r_end doesn't exceed grid bounds
        r_end = std::min(r_end, (int)grid.rows.size());

        for (int r = r_start; r < r_end && r < (int)grid.rows.size(); r++) {
            height += grid.rows[r].computed_size;
            if (r < r_end - 1) {
                height += grid.row_gap;
            }
        }

        // Apply item alignment (Sprint 8)
        float cell_width = width;
        float cell_height = height;

        // justify-items (horizontal alignment within cell)
        if (grid.justify_items != "stretch" && item.element) {
            float item_width = item.element->explicit_style.width;
            if (item_width < 0) {  // auto
                item_width = 100.0f;  // default
            }

            if (item_width < cell_width) {
                if (grid.justify_items == "center") {
                    x += (cell_width - item_width) / 2.0f;
                } else if (grid.justify_items == "end") {
                    x += (cell_width - item_width);
                }
                // "start" - no adjustment needed

                width = item_width;  // Use item's natural width
            }
        }

        // align-items (vertical alignment within cell)
        if (grid.align_items != "stretch" && item.element) {
            float item_height = item.element->explicit_style.height;
            if (item_height < 0) {  // auto
                item_height = 100.0f;  // default
            }

            if (item_height < cell_height) {
                if (grid.align_items == "center") {
                    y += (cell_height - item_height) / 2.0f;
                } else if (grid.align_items == "end") {
                    y += (cell_height - item_height);
                }
                // "start" - no adjustment needed

                height = item_height;  // Use item's natural height
            }
        }

        // Store computed values
        item.x = x;
        item.y = y;
        item.width = width;
        item.height = height;
    }
}

/**
 * @brief Write computed layout to elements (SPRINT 4 OUTPUT)
 */
void write_computed_layout(const GridContainer& grid)
{
    for (const auto& item : grid.items) {
        // Sprint 18: Apply min/max constraints to dimensions
        float width = item.width;
        float height = item.height;

        if (item.element) {
            width = apply_dimension_constraints(
                width,
                item.element->explicit_style.min_width,
                item.element->explicit_style.max_width
            );
            height = apply_dimension_constraints(
                height,
                item.element->explicit_style.min_height,
                item.element->explicit_style.max_height
            );

            // Sprint 19: Apply box-sizing to calculate content dimensions
            // Grid assigns "total" dimensions (may include padding/border if border-box)
            float content_width, content_height;
            float border[4] = {
                item.element->explicit_style.border_width[0],  // top
                item.element->explicit_style.border_width[1],  // right
                item.element->explicit_style.border_width[2],  // bottom
                item.element->explicit_style.border_width[3]   // left
            };

            calculate_content_dimensions(
                width,
                height,
                item.element->explicit_style.padding,
                border,
                item.element->explicit_style.box_sizing,
                content_width,
                content_height
            );

            width = content_width;
            height = content_height;
        }

        // PHASE 4 SPRINT 4: Write to computed (OUTPUT)
        // Sprint 37: Apply margin offsets
        float margin_left = item.element->explicit_style.margin[3];  // left
        float margin_top = item.element->explicit_style.margin[0];   // top

        item.element->computed.x = item.x + margin_left;
        item.element->computed.y = item.y + margin_top;
        item.element->computed.width = width;
        item.element->computed.height = height;

        // DEBUG: Log what we're writing
        // logi("write_computed_layout: element '{}' -> x={}, y={}, width={}, height={}",
        //      item.element->id, item.element->computed.x, item.element->computed.y, width, height);

        // Set content dimensions
        item.element->computed.content_width = width;
        item.element->computed.content_height = height;

        // Mark as computed by grid (SPRINT 4 SOURCE TRACKING)
        item.element->computed.source = NVGCSSComputedLayout::GRID;
        item.element->computed.is_computed = true;
    }
}

/**
 * @brief Main grid layout function
 */
void compute_grid_layout(
    NVGCSSElement* element,
    NVGCSSRenderer* renderer)
{
    // Step 1: Parse grid container properties
    GridContainer grid = parse_grid_container(element);

    // Sprint 13: Parse grid-template-areas if present
    GridTemplateAreas template_areas;
    if (!element->explicit_style.grid_template_areas.empty()) {
        template_areas = parse_grid_template_areas(
            element->explicit_style.grid_template_areas);

        // Create implicit tracks if needed to match template
        if (template_areas.is_valid()) {
            // Ensure we have enough rows
            while ((int)grid.rows.size() < template_areas.num_rows) {
                grid.rows.push_back(GridTrack(GridTrack::AUTO, 0));
            }
            // Ensure we have enough columns
            while ((int)grid.columns.size() < template_areas.num_cols) {
                grid.columns.push_back(GridTrack(GridTrack::AUTO, 0));
            }
        }
    }

    // Sprint 14: Collect grid items BEFORE computing track sizes
    // (needed for auto track sizing based on content)
    collect_grid_items(element, grid, template_areas, renderer);

    if (grid.items.empty()) {
        return;  // No items to layout
    }

    // Parse grid-auto-flow to determine primary direction
    bool is_column_flow = (grid.grid_auto_flow.find("column") != std::string::npos);

    // Pre-create implicit tracks based on grid-auto-rows/columns BEFORE auto-placement
    int num_items = grid.items.size();

    if (is_column_flow) {
        // Column-first flow: estimate columns needed
        int num_rows = grid.rows.size();
        if (num_rows > 0) {
            int estimated_cols = (num_items + num_rows - 1) / num_rows;

            // Create implicit columns if we need more than explicit columns
            while ((int)grid.columns.size() < estimated_cols) {
                std::string auto_cols = element->explicit_style.grid_auto_columns;
                if (auto_cols.empty()) {
                    auto_cols = "auto";  // Default to auto sizing
                }
                GridTrack track = parse_single_track(auto_cols, grid.container_width);
                grid.columns.push_back(track);
            }
        }
    } else {
        // Row-first flow (default): estimate rows needed
        int num_cols = grid.columns.size();
        if (num_cols > 0) {
            int estimated_rows = (num_items + num_cols - 1) / num_cols;

            // Create implicit rows if we need more than explicit rows
            while ((int)grid.rows.size() < estimated_rows) {
                std::string auto_rows = element->explicit_style.grid_auto_rows;
                if (auto_rows.empty()) {
                    auto_rows = "auto";  // Default to auto sizing
                }
                GridTrack track = parse_single_track(auto_rows, grid.container_height);
                grid.rows.push_back(track);
            }
        }
    }

    // Sprint 14: Compute track sizes with items (for auto sizing)
    // Sizing order: FIXED → AUTO → FRACTIONAL
    compute_track_sizes(grid.rows, grid.container_height, grid.row_gap,
                       grid.items, renderer, false);  // false = row axis
    compute_track_sizes(grid.columns, grid.container_width, grid.column_gap,
                       grid.items, renderer, true);   // true = column axis

    // Step 3: Auto-place items
    auto_place_items(grid);

    // Step 3b: Create additional implicit rows if items still overflowed
    // Find maximum row/column used by all items
    int max_row_used = grid.rows.size();
    int max_col_used = grid.columns.size();

    for (const auto& item : grid.items) {
        if (item.row_end > max_row_used) {
            max_row_used = item.row_end;
        }
        if (item.column_end > max_col_used) {
            max_col_used = item.column_end;
        }
    }

    // Create additional implicit rows if needed
    if (max_row_used > (int)grid.rows.size()) {
        std::string auto_rows = element->explicit_style.grid_auto_rows;
        if (auto_rows.empty()) {
            auto_rows = "auto";
        }

        while ((int)grid.rows.size() < max_row_used) {
            GridTrack track = parse_single_track(auto_rows, grid.container_height);
            grid.rows.push_back(track);
        }

        // Re-compute track sizes for the new rows
        compute_track_sizes(grid.rows, grid.container_height, grid.row_gap,
                           grid.items, renderer, false);
    }

    // Create additional implicit columns if needed
    if (max_col_used > (int)grid.columns.size()) {
        std::string auto_cols = element->explicit_style.grid_auto_columns;
        if (auto_cols.empty()) {
            auto_cols = "auto";
        }

        while ((int)grid.columns.size() < max_col_used) {
            GridTrack track = parse_single_track(auto_cols, grid.container_width);
            grid.columns.push_back(track);
        }

        // Re-compute track sizes for the new columns
        compute_track_sizes(grid.columns, grid.container_width, grid.column_gap,
                           grid.items, renderer, true);
    }

    // Step 4: Apply content alignment (Sprint 8)
    apply_content_alignment(grid);

    // Step 5: Position items
    position_grid_items(grid);

    // Step 6: Write to computed layout (SPRINT 4 OUTPUT)
    write_computed_layout(grid);

    // Step 7: RECURSIVELY compute layout for nested containers
    // If any grid item is itself a flex or grid container, compute its children
    for (const auto& item : grid.items) {
        if (!item.element) continue;

        // Check if this item is a layout container (flex or grid)
        bool is_flex = (item.element->style.display == nvgcss::Display::FLEX);
        bool is_grid = (item.element->style.display == nvgcss::Display::GRID);

        if (is_flex) {
            // Recursively compute flexbox layout for this nested container
            compute_flexbox_layout(item.element, renderer);
        } else if (is_grid) {
            // Recursively compute grid layout for this nested container
            compute_grid_layout(item.element, renderer);
        }
    }
}
