/*
 * Editor Core Types
 *
 * Forward declarations and common types for the editor framework.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace editor {

// Forward declarations - Model
class Document;
class Layer;
class EditorNode;

// Forward declarations - ViewModel
class EditorViewModel;
class ToolManager;
class SelectionManager;
class HistoryManager;

// Forward declarations - Command
class Command;
class CommandGroup;

// Forward declarations - Tool
class Tool;
class SelectTool;
class DrawTool;
class TransformTool;

// Smart pointer aliases
using DocumentPtr = std::shared_ptr<Document>;
using LayerPtr = std::shared_ptr<Layer>;
using EditorNodePtr = std::shared_ptr<EditorNode>;
using CommandPtr = std::unique_ptr<Command>;
using ToolPtr = std::unique_ptr<Tool>;

// Common types
struct Point {
    float x = 0, y = 0;

    Point() = default;
    Point(float x_, float y_) : x(x_), y(y_) {}

    Point operator+(const Point& o) const { return {x + o.x, y + o.y}; }
    Point operator-(const Point& o) const { return {x - o.x, y - o.y}; }
    Point operator*(float s) const { return {x * s, y * s}; }
};

struct Rect {
    float x = 0, y = 0, width = 0, height = 0;

    Rect() = default;
    Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), width(w_), height(h_) {}

    bool contains(const Point& p) const {
        return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + height;
    }

    Point center() const { return {x + width / 2, y + height / 2}; }
};

struct Color {
    float r = 0, g = 0, b = 0, a = 1;

    Color() = default;
    Color(float r_, float g_, float b_, float a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}

    static Color rgb(uint8_t r, uint8_t g, uint8_t b) {
        return {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f};
    }
    static Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }
};

// Event types for observer pattern
enum class EventType {
    DocumentChanged,
    SelectionChanged,
    LayerChanged,
    NodeAdded,
    NodeRemoved,
    NodeModified,
    ToolChanged,
    HistoryChanged,
};

// Callback types
using EventCallback = std::function<void(EventType, void*)>;
using PropertyChangeCallback = std::function<void(const std::string&)>;

} // namespace editor
