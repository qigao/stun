/*
 * Flex Engine - Binary Format Definition
 *
 * Defines the on-disk binary format for compiled .flex files.
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace flex {
namespace binary {

// ============================================================================
// Magic Numbers and Version
// ============================================================================

constexpr uint32_t MAGIC_NUMBER = 0x58454C46;  // "FLEX" in little-endian
constexpr uint32_t FORMAT_VERSION = 0x00010000; // Version 1.0.0

// ============================================================================
// Header Structure
// ============================================================================

#pragma pack(push, 1)

struct FileHeader {
    uint32_t magic;           // Must be MAGIC_NUMBER
    uint32_t version;         // FORMAT_VERSION
    uint32_t flags;           // Format flags (compressed, encrypted, etc.)
    uint32_t header_size;     // Size of this header

    // Offsets to major sections
    uint32_t string_table_offset;
    uint32_t string_table_size;

    uint32_t asset_table_offset;
    uint32_t asset_table_size;

    uint32_t node_data_offset;
    uint32_t node_data_size;

    uint32_t animation_data_offset;
    uint32_t animation_data_size;

    uint32_t fsm_data_offset;
    uint32_t fsm_data_size;

    // Statistics
    uint32_t node_count;
    uint32_t asset_count;
    uint32_t timeline_count;
    uint32_t fsm_count;

    // Canvas dimensions
    float canvas_width;
    float canvas_height;

    uint32_t checksum;        // CRC32 of entire file (excluding this field)
};

// ============================================================================
// Format Flags
// ============================================================================

enum FormatFlags : uint32_t {
    FLAG_NONE           = 0,
    FLAG_COMPRESSED     = 1 << 0,  // Data is zlib compressed
    FLAG_ENCRYPTED      = 1 << 1,  // Data is AES encrypted
    FLAG_DEBUG_INFO     = 1 << 2,  // Contains debug symbols
    FLAG_EMBEDDED_ASSETS = 1 << 3, // Assets embedded inline
    FLAG_OPTIMIZED      = 1 << 4,  // Aggressive optimization applied
};

// ============================================================================
// String Table (deduplicated strings)
// ============================================================================

// Strings stored as: [count: uint32_t] [offset1, offset2, ...] [string_data...]
using StringIndex = uint32_t;

// ============================================================================
// Node Types
// ============================================================================

enum class NodeType : uint8_t {
    Group = 0,
    Shape = 1,
    Text = 2,
    Image = 3,
    Svg = 4,
    Instance = 5,
    Solo = 6,
    Artboard = 7,
};

// ============================================================================
// Common Node Header
// ============================================================================

struct NodeHeader {
    NodeType type;
    uint8_t flags;            // Visibility, etc.
    uint16_t reserved;
    StringIndex id;           // Index into string table

    // Transform (6 floats: a, b, c, d, tx, ty)
    float transform[6];

    float opacity;
    float rotation;
    float scale_x;
    float scale_y;

    // Layout properties
    float layout_width;
    float layout_height;
    float flex_grow;
    float flex_shrink;
    float flex_basis;

    uint32_t child_count;     // For Group nodes
    uint32_t data_offset;     // Offset to type-specific data
};

// ============================================================================
// Shape Data
// ============================================================================

enum class ShapeType : uint8_t {
    Rect = 0,
    Circle = 1,
    Ellipse = 2,
    Polygon = 3,
    Star = 4,
    Path = 5,
    Line = 6,
    Ring = 7,
};

struct ShapeData {
    ShapeType type;
    uint8_t has_fill;
    uint8_t has_stroke;
    uint8_t reserved;

    float stroke_width;

    // Paint data offsets
    uint32_t fill_paint_offset;
    uint32_t stroke_paint_offset;

    // Geometry data offset (depends on ShapeType)
    uint32_t geometry_offset;
};

struct RectGeometry {
    float width;
    float height;
    float corner_radius;
};

struct CircleGeometry {
    float radius;
};

struct EllipseGeometry {
    float rx;
    float ry;
};

struct PolygonGeometry {
    int32_t sides;
    float radius;
};

struct StarGeometry {
    int32_t points;
    float outer_radius;
    float inner_radius;
};

struct PathGeometry {
    StringIndex path_data;  // SVG path string
};

// ============================================================================
// Paint Data
// ============================================================================

enum class PaintType : uint8_t {
    None = 0,
    Solid = 1,
    LinearGradient = 2,
    RadialGradient = 3,
};

struct ColorData {
    float r, g, b, a;
};

struct SolidPaint {
    PaintType type;  // = Solid
    uint8_t reserved[3];
    ColorData color;
};

struct ColorStop {
    float offset;
    ColorData color;
};

struct LinearGradientPaint {
    PaintType type;  // = LinearGradient
    uint8_t reserved[3];
    float x1, y1, x2, y2;
    uint32_t stop_count;
    // Followed by stop_count * ColorStop
};

struct RadialGradientPaint {
    PaintType type;  // = RadialGradient
    uint8_t reserved[3];
    float cx, cy, radius;
    uint32_t stop_count;
    // Followed by stop_count * ColorStop
};

// ============================================================================
// Text Data
// ============================================================================

struct TextData {
    StringIndex content;
    StringIndex font_family;
    float font_size;
    uint8_t bold;
    uint8_t italic;
    uint16_t reserved;
    ColorData color;
};

// ============================================================================
// Image Data
// ============================================================================

struct ImageData {
    StringIndex source;       // Path or embedded data index
    float width;
    float height;
    uint8_t embedded;         // 1 if data is embedded
    uint8_t reserved[3];
};

// ============================================================================
// Animation Data
// ============================================================================

struct TimelineHeader {
    StringIndex name;
    float duration;
    uint8_t loop;
    uint8_t auto_play;
    uint16_t reserved;
    uint32_t track_count;
};

struct TrackHeader {
    StringIndex target_id;    // Node ID
    StringIndex property;     // Property name
    uint32_t keyframe_count;
};

struct Keyframe {
    float time;
    float value;
    uint8_t easing;           // Easing function
    uint8_t reserved[3];
};

// ============================================================================
// State Machine Data
// ============================================================================

struct FSMHeader {
    StringIndex name;
    uint32_t state_count;
    uint32_t transition_count;
    uint32_t initial_state;
};

struct StateData {
    StringIndex name;
    uint32_t action_count;
};

struct TransitionData {
    uint32_t from_state;
    uint32_t to_state;
    StringIndex event;
    StringIndex condition;    // Script expression
};

#pragma pack(pop)

// ============================================================================
// Utility Functions
// ============================================================================

// Calculate CRC32 checksum
uint32_t calculate_crc32(const void* data, size_t size);

// Continue CRC32 calculation (for incremental checksums)
uint32_t calculate_crc32_continue(const void* data, size_t size, uint32_t previous_crc);

// Verify file integrity
bool verify_checksum(const void* data, size_t size, uint32_t expected_crc);

} // namespace binary
} // namespace flex
