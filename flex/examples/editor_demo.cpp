/*
 * Flex Engine - Meta-Editor Demo
 *
 * Demonstrates the core editor architecture:
 * - Pan/Zoom camera
 * - Click/Drag selection
 * - Selection box gizmo
 * - Transform handles (future)
 *
 * This is the foundation for SVG Designer, UI Designer, and Whiteboard tools.
 */

#include <iostream>
#include <memory>
#include <vector>
#include <unordered_set>
#include <cmath>
#include <fstream>
#include <sstream>
#include <functional>
#include <SDL2/SDL.h>
#include <thorvg.h>
#include <flex/flex.h>
#include <flex/group.h>

// ============================================================================
// Editor State
// ============================================================================

struct Camera {
    float x = 0;
    float y = 0;
    float zoom = 1.0f;

    // Screen -> World coordinates
    void screen_to_world(float sx, float sy, float& wx, float& wy) const {
        wx = (sx - x) / zoom;
        wy = (sy - y) / zoom;
    }

    // World -> Screen coordinates
    void world_to_screen(float wx, float wy, float& sx, float& sy) const {
        sx = wx * zoom + x;
        sy = wy * zoom + y;
    }
};

struct SelectionBox {
    bool active = false;
    float start_x = 0, start_y = 0;  // World coords
    float end_x = 0, end_y = 0;
};

struct DragState {
    bool active = false;
    float start_x = 0, start_y = 0;  // World coords
    std::vector<std::pair<float, float>> node_origins;

    void begin(float wx, float wy, const std::vector<flex::Node*>& nodes) {
        active = true;
        start_x = wx;
        start_y = wy;
        node_origins.clear();
        for (auto* n : nodes) {
            node_origins.emplace_back(n->x(), n->y());
        }
    }

    void end() { active = false; }
};

struct PanState {
    bool active = false;
    float start_x = 0, start_y = 0;  // Screen coords
    float cam_x = 0, cam_y = 0;      // Camera at start

    void begin(float sx, float sy, const Camera& cam) {
        active = true;
        start_x = sx;
        start_y = sy;
        cam_x = cam.x;
        cam_y = cam.y;
    }

    void end() { active = false; }
};

enum class ResizeHandle {
    None = -1,
    TopLeft = 0,
    TopRight = 1,
    BottomLeft = 2,
    BottomRight = 3,
};

struct ResizeState {
    bool active = false;
    ResizeHandle handle = ResizeHandle::None;
    flex::Node* node = nullptr;
    float start_x = 0, start_y = 0;      // World coords at start
    float orig_x = 0, orig_y = 0;        // Node position at start
    float orig_w = 0, orig_h = 0;        // Original size

    void begin(ResizeHandle h, flex::Node* n, float wx, float wy, float x, float y, float w, float height) {
        active = true;
        handle = h;
        node = n;
        start_x = wx;
        start_y = wy;
        orig_x = x;
        orig_y = y;
        orig_w = w;
        orig_h = height;
    }

    void end() {
        active = false;
        handle = ResizeHandle::None;
        node = nullptr;
    }
};

enum class Tool {
    Select,
    Pan
};

struct MenuItem {
    std::string label;
    std::function<void()> action;
    bool enabled = true;
    bool separator = false;

    static MenuItem Separator() {
        MenuItem item;
        item.separator = true;
        return item;
    }
};

struct ContextMenu {
    bool visible = false;
    float x = 0, y = 0;          // Screen coords
    std::vector<MenuItem> items;
    int hover_index = -1;

    static constexpr float ITEM_HEIGHT = 24.0f;
    static constexpr float MENU_WIDTH = 160.0f;
    static constexpr float PADDING = 4.0f;

    void show(float sx, float sy) {
        visible = true;
        x = sx;
        y = sy;
        hover_index = -1;
    }

    void hide() {
        visible = false;
        hover_index = -1;
    }

    float height() const {
        float h = PADDING * 2;
        for (const auto& item : items) {
            h += item.separator ? 8.0f : ITEM_HEIGHT;
        }
        return h;
    }

    int hit_test(float mx, float my) const {
        if (!visible) return -1;
        if (mx < x || mx > x + MENU_WIDTH) return -1;
        if (my < y || my > y + height()) return -1;

        float cy = y + PADDING;
        for (size_t i = 0; i < items.size(); ++i) {
            float item_h = items[i].separator ? 8.0f : ITEM_HEIGHT;
            if (my >= cy && my < cy + item_h && !items[i].separator) {
                return static_cast<int>(i);
            }
            cy += item_h;
        }
        return -1;
    }
};

struct RotationState {
    bool active = false;
    flex::Node* node = nullptr;
    float start_angle = 0;      // Angle at drag start
    float orig_rotation = 0;    // Node rotation at start
    float center_x = 0, center_y = 0;  // Rotation center (world)

    void begin(flex::Node* n, float angle, float cx, float cy) {
        active = true;
        node = n;
        start_angle = angle;
        orig_rotation = n->rotation();
        center_x = cx;
        center_y = cy;
    }

    void end() {
        active = false;
        node = nullptr;
    }
};

struct DrawState {
    bool active = false;
    std::vector<std::pair<float, float>> points;  // World coordinates

    void begin(float wx, float wy) {
        active = true;
        points.clear();
        points.emplace_back(wx, wy);
    }

    void add_point(float wx, float wy) {
        if (!active) return;
        // Only add if moved enough (avoid too many points)
        if (!points.empty()) {
            float dx = wx - points.back().first;
            float dy = wy - points.back().second;
            if (dx * dx + dy * dy < 4.0f) return;  // Min distance threshold
        }
        points.emplace_back(wx, wy);
    }

    // Calculate bounding box of all points
    void get_bounds(float& min_x, float& min_y, float& max_x, float& max_y) const {
        if (points.empty()) {
            min_x = min_y = max_x = max_y = 0;
            return;
        }
        min_x = max_x = points[0].first;
        min_y = max_y = points[0].second;
        for (const auto& p : points) {
            min_x = std::min(min_x, p.first);
            min_y = std::min(min_y, p.second);
            max_x = std::max(max_x, p.first);
            max_y = std::max(max_y, p.second);
        }
    }

    // Generate path with coordinates relative to offset (for local coordinate system)
    std::string to_path_relative(float offset_x, float offset_y, bool smooth = false) const {
        if (points.size() < 2) return "";

        auto fmt = [offset_x, offset_y](float x, float y) {
            return std::to_string(x - offset_x) + " " + std::to_string(y - offset_y);
        };

        std::string d = "M " + fmt(points[0].first, points[0].second);

        if (!smooth || points.size() < 3) {
            for (size_t i = 1; i < points.size(); ++i) {
                d += " L " + fmt(points[i].first, points[i].second);
            }
        } else {
            for (size_t i = 1; i < points.size(); ++i) {
                auto p0 = (i > 1) ? points[i - 2] : points[0];
                auto p1 = points[i - 1];
                auto p2 = points[i];
                auto p3 = (i + 1 < points.size()) ? points[i + 1] : points[i];

                float cp1x = p1.first + (p2.first - p0.first) / 6.0f;
                float cp1y = p1.second + (p2.second - p0.second) / 6.0f;
                float cp2x = p2.first - (p3.first - p1.first) / 6.0f;
                float cp2y = p2.second - (p3.second - p1.second) / 6.0f;

                d += " C " + fmt(cp1x, cp1y) + " " + fmt(cp2x, cp2y) + " " + fmt(p2.first, p2.second);
            }
        }
        return d;
    }

    std::string to_path(bool smooth = false) const {
        return to_path_relative(0, 0, smooth);
    }

    void end() {
        active = false;
    }
};

struct PenSettings {
    flex::Color stroke_color = {1.0f, 1.0f, 1.0f, 1.0f};  // White default
    flex::Color fill_color = {0.3f, 0.5f, 0.9f, 1.0f};    // Blue default
    float stroke_width = 2.0f;
    bool use_fill = false;
    bool smooth_curves = true;

    // Preset colors
    static constexpr int NUM_COLORS = 8;
    flex::Color color_palette[NUM_COLORS] = {
        {1.0f, 1.0f, 1.0f, 1.0f},  // White
        {1.0f, 0.3f, 0.3f, 1.0f},  // Red
        {1.0f, 0.6f, 0.2f, 1.0f},  // Orange
        {1.0f, 1.0f, 0.3f, 1.0f},  // Yellow
        {0.3f, 0.9f, 0.3f, 1.0f},  // Green
        {0.3f, 0.7f, 1.0f, 1.0f},  // Blue
        {0.7f, 0.3f, 1.0f, 1.0f},  // Purple
        {0.2f, 0.2f, 0.2f, 1.0f},  // Dark gray
    };
};

// ============================================================================
// Shape Gizmo Info - Single source of truth for frame/handle positions
// ============================================================================

struct ShapeGizmoInfo {
    bool valid = false;

    // Screen-space corners (already transformed by camera)
    float corners[4][2];  // TL, TR, BL, BR

    // Screen-space rotation handle position
    float rot_handle_x = 0, rot_handle_y = 0;

    // Screen-space center
    float center_x = 0, center_y = 0;

    // World-space info (for resize calculations)
    float world_x = 0, world_y = 0;
    float world_w = 0, world_h = 0;
    float rotation = 0;
    float pivot_x = 0, pivot_y = 0;
};

// ============================================================================
// UI Components - Toolbar & Panel
// ============================================================================

enum class ToolType {
    Select,
    Pan,
    Rectangle,
    Circle,
    Line,
    Pen,  // Freehand drawing
};

enum class CommandType {
    Copy,
    Paste,
    Delete,
    Duplicate,
    Group,
    Ungroup,
    BringFront,
    SendBack,
};

struct ToolButton {
    ToolType tool;
    std::string icon;
    std::string tooltip;
    bool enabled = true;
};

struct CommandButton {
    CommandType cmd;
    std::string icon;
    std::string tooltip;
    bool enabled = true;
};

struct Toolbar {
    std::vector<ToolButton> tools;
    std::vector<CommandButton> commands;
    ToolType active_tool = ToolType::Select;
    int hover_tool = -1;
    int hover_cmd = -1;

    static constexpr float HEIGHT = 40.0f;
    static constexpr float BUTTON_SIZE = 32.0f;
    static constexpr float PADDING = 4.0f;
    static constexpr float SEPARATOR = 16.0f;

    void init() {
        tools = {
            {ToolType::Select,    "V", "Select (V)"},
            {ToolType::Pan,       "H", "Pan (H)"},
            {ToolType::Rectangle, "R", "Rectangle (R)"},
            {ToolType::Circle,    "O", "Circle (O)"},
            {ToolType::Line,      "L", "Line (L)"},
            {ToolType::Pen,       "B", "Pen (B)"},
        };
        commands = {
            {CommandType::Copy,      "C", "Copy (Ctrl+C)"},
            {CommandType::Paste,     "P", "Paste (Ctrl+V)"},
            {CommandType::Delete,    "X", "Delete (Del)"},
            {CommandType::Duplicate, "D", "Duplicate (D)"},
            {CommandType::Group,     "G", "Group (G)"},
            {CommandType::Ungroup,   "U", "Ungroup (Shift+G)"},
            {CommandType::BringFront,"^", "Bring to Front"},
            {CommandType::SendBack,  "v", "Send to Back"},
        };
    }

    float tools_end_x() const {
        return PADDING + tools.size() * (BUTTON_SIZE + PADDING);
    }

    float commands_start_x() const {
        return tools_end_x() + SEPARATOR;
    }

    int hit_test_tool(float x, float y) const {
        if (y < 0 || y > HEIGHT) return -1;
        float bx = PADDING;
        for (size_t i = 0; i < tools.size(); ++i) {
            if (x >= bx && x < bx + BUTTON_SIZE) {
                return static_cast<int>(i);
            }
            bx += BUTTON_SIZE + PADDING;
        }
        return -1;
    }

    int hit_test_cmd(float x, float y) const {
        if (y < 0 || y > HEIGHT) return -1;
        float bx = commands_start_x();
        for (size_t i = 0; i < commands.size(); ++i) {
            if (x >= bx && x < bx + BUTTON_SIZE) {
                return static_cast<int>(i);
            }
            bx += BUTTON_SIZE + PADDING;
        }
        return -1;
    }
};

enum class PanelSide { Left, Right };

struct PanelSection {
    std::string title;
    bool collapsed = false;
    float content_height = 100.0f;
};

struct Panel {
    PanelSide side = PanelSide::Right;
    bool visible = true;
    float width = 240.0f;
    std::vector<PanelSection> sections;

    static constexpr float HEADER_HEIGHT = 28.0f;
    static constexpr float SECTION_HEADER = 24.0f;
    static constexpr float PADDING = 8.0f;
    static constexpr float ROW_HEIGHT = 24.0f;

    void init() {
        sections = {
            {"Transform", false, 96.0f},
            {"Appearance", false, 72.0f},
            {"Pen Settings", false, 120.0f},
            {"Layers", true, 150.0f},
        };
    }

    float x(float screen_width) const {
        if (side == PanelSide::Right) {
            return screen_width - width;
        }
        return 0;
    }

    float height(float screen_height, float toolbar_height) const {
        return screen_height - toolbar_height;
    }

    int hit_test_section(float local_y) const {
        float cy = HEADER_HEIGHT;
        for (size_t i = 0; i < sections.size(); ++i) {
            float section_h = SECTION_HEADER;
            if (!sections[i].collapsed) {
                section_h += sections[i].content_height;
            }
            if (local_y >= cy && local_y < cy + SECTION_HEADER) {
                return static_cast<int>(i);  // Clicked on section header
            }
            cy += section_h;
        }
        return -1;
    }
};

// ============================================================================
// Overview Window (Minimap)
// ============================================================================

struct OverviewWindow {
    bool visible = true;
    float width = 180.0f;
    float height = 120.0f;
    float margin = 10.0f;

    // Cached document bounds
    float doc_min_x = 0, doc_min_y = 0;
    float doc_max_x = 1000, doc_max_y = 800;

    static constexpr float BORDER_WIDTH = 1.0f;

    // Get position (bottom-right corner, above panel if visible)
    float x(float screen_width, float panel_width, bool panel_visible) const {
        if (panel_visible) {
            return screen_width - panel_width - width - margin;
        }
        return screen_width - width - margin;
    }

    float y(float screen_height) const {
        return screen_height - height - margin;
    }

    // Convert screen coords in overview to world coords
    void overview_to_world(float ox, float oy, float overview_x, float overview_y,
                           float& wx, float& wy) const {
        float scale_x = (doc_max_x - doc_min_x) / (width - 2 * BORDER_WIDTH);
        float scale_y = (doc_max_y - doc_min_y) / (height - 2 * BORDER_WIDTH);
        float scale = std::max(scale_x, scale_y);

        wx = doc_min_x + (ox - overview_x - BORDER_WIDTH) * scale;
        wy = doc_min_y + (oy - overview_y - BORDER_WIDTH) * scale;
    }

    bool hit_test(float mx, float my, float overview_x, float overview_y) const {
        return mx >= overview_x && mx < overview_x + width &&
               my >= overview_y && my < overview_y + height;
    }
};

// ============================================================================
// Editor Demo Class
// ============================================================================

class EditorDemo {
public:
    EditorDemo() = default;
    ~EditorDemo() { cleanup(); }

    bool init(const char* file = nullptr) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
            return false;
        }

        window_ = SDL_CreateWindow(
            "Flex Meta-Editor Demo",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCREEN_WIDTH, SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN
        );
        if (!window_) return false;

        sdl_renderer_ = SDL_CreateRenderer(window_, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!sdl_renderer_) return false;

        // Create texture for ThorVG output
        texture_ = SDL_CreateTexture(sdl_renderer_,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            SCREEN_WIDTH, SCREEN_HEIGHT);
        if (!texture_) return false;

        // Initialize ThorVG
        if (tvg::Initializer::init(0) != tvg::Result::Success) {
            std::cerr << "ThorVG init failed\n";
            return false;
        }

        // Create ThorVG canvas
        tvg_canvas_ = tvg::SwCanvas::gen();
        if (!tvg_canvas_) return false;

        tvg_buffer_.resize(SCREEN_WIDTH * SCREEN_HEIGHT);
        tvg_canvas_->target(tvg_buffer_.data(), SCREEN_WIDTH, SCREEN_WIDTH, SCREEN_HEIGHT, tvg::ColorSpace::ARGB8888);

        // Initialize Flex
        flex::init();

        // Load fonts for text rendering
        if (!flex::load_font("Segoe UI", "C:/Windows/Fonts/segoeui.ttf")) {
            if (!flex::load_font("Segoe UI", "C:/Windows/Fonts/arial.ttf")) {
                std::cerr << "Warning: Could not load system font for text rendering\n";
            }
        }

        // Create flex ThorVG renderer
        flex_renderer_ = flex::create_thorvg_renderer(tvg_canvas_);

        // Load file or create default document
        if (file) {
            current_file_ = file;
            auto definition = flex::Definition::load_file(file);
            if (definition && !definition->has_error()) {
                instance_ = flex::Instance::create(definition);
                std::cout << "Loaded: " << file << "\n";
            } else {
                std::cerr << "Failed to load " << file << ", creating empty document\n";
                instance_ = flex::Instance::create(SCREEN_WIDTH, SCREEN_HEIGHT);
                createDocument();
            }
        } else {
            instance_ = flex::Instance::create(SCREEN_WIDTH, SCREEN_HEIGHT);
            createDocument();
        }

        // Initialize UI components
        toolbar_.init();
        panel_.init();

        printHelp();
        return true;
    }

    void run() {
        running_ = true;
        Uint32 last_time = SDL_GetTicks();

        while (running_) {
            Uint32 now = SDL_GetTicks();
            float dt = (now - last_time) / 1000.0f;
            last_time = now;

            handleEvents();
            update(dt);
            render();
            SDL_Delay(16);  // Limit to ~60 FPS
        }
    }

private:
    static constexpr int SCREEN_WIDTH = 1280;
    static constexpr int SCREEN_HEIGHT = 800;

    bool running_ = false;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    // ThorVG rendering
    tvg::SwCanvas* tvg_canvas_ = nullptr;
    std::vector<uint32_t> tvg_buffer_;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    flex::Instance::Ptr instance_;

    // Editor state
    Camera camera_;
    Tool current_tool_ = Tool::Select;
    SelectionBox selection_box_;
    std::vector<flex::Node*> selected_nodes_;
    std::unordered_set<flex::Node*> selected_set_;  // O(1) lookup
    DragState drag_;
    PanState pan_;
    ResizeState resize_;
    RotationState rotation_;
    DrawState draw_;
    PenSettings pen_settings_;
    ContextMenu context_menu_;
    std::string current_file_;  // Current .flex file path

    // UI Components
    Toolbar toolbar_;
    Panel panel_;
    OverviewWindow overview_;

    // Clipboard for copy/paste
    struct ClipboardItem {
        flex::GeometryType geo_type;
        float width = 0, height = 0, radius = 0, corner_radius = 0;
        flex::Color fill_color;
        flex::Color stroke_color;
        float stroke_width = 1.0f;
        float rotation = 0;
    };
    std::vector<ClipboardItem> clipboard_;

    void printHelp() {
        std::cout << "\n=== Flex Meta-Editor Demo ===\n";
        std::cout << "Controls:\n";
        std::cout << "  Left Click     - Select node\n";
        std::cout << "  Left Drag      - Move selected / Box select\n";
        std::cout << "  Corner Drag    - Resize selected shape\n";
        std::cout << "  Rotation Handle- Drag circle above selection to rotate\n";
        std::cout << "  Middle Drag    - Pan canvas\n";
        std::cout << "  Right Click    - Context menu\n";
        std::cout << "  Scroll         - Zoom in/out\n";
        std::cout << "  ESC            - Deselect all\n";
        std::cout << "  Delete         - Delete selected\n";
        std::cout << "  R / Shift+R    - Rotate 15 CW / CCW\n";
        std::cout << "  G              - Group selected\n";
        std::cout << "  Shift+G        - Ungroup selected\n";
        std::cout << "  D              - Duplicate selected\n";
        std::cout << "  B              - Pen/Brush tool (freehand draw)\n";
        std::cout << "  V              - Select tool\n";
        std::cout << "  P              - Toggle panel\n";
        std::cout << "  M              - Toggle minimap/overview\n";
        std::cout << "  N              - New rectangle\n";
        std::cout << "  C              - New circle\n";
        std::cout << "  L              - Load .flex file\n";
        std::cout << "  S              - Save .flex file\n";
        std::cout << "  Q              - Quit\n\n";
    }

    void createDocument() {
        auto* artboard = instance_->artboard();
        artboard->set_background(flex::Color(0.12f, 0.12f, 0.14f, 1.0f));

        // Create some sample shapes
        auto rect1 = flex::Shape::create();
        rect1->set_id("rect1");
        rect1->set_position(100, 100);
        rect1->set_rect(200, 150, 8);
        rect1->set_fill(flex::Color(0.3f, 0.5f, 0.9f, 1.0f));
        rect1->set_stroke(flex::Color(1.0f, 1.0f, 1.0f, 0.3f), 1.0f);
        artboard->add_child(rect1);

        auto rect2 = flex::Shape::create();
        rect2->set_id("rect2");
        rect2->set_position(350, 200);
        rect2->set_rect(120, 180, 4);
        rect2->set_fill(flex::Color(0.9f, 0.4f, 0.3f, 1.0f));
        rect2->set_stroke(flex::Color(1.0f, 1.0f, 1.0f, 0.3f), 1.0f);
        artboard->add_child(rect2);

        auto circle1 = flex::Shape::create();
        circle1->set_id("circle1");
        circle1->set_position(600, 300);
        circle1->set_circle(60);
        circle1->set_fill(flex::Color(0.4f, 0.8f, 0.4f, 1.0f));
        circle1->set_stroke(flex::Color(1.0f, 1.0f, 1.0f, 0.3f), 1.0f);
        artboard->add_child(circle1);

        auto rect3 = flex::Shape::create();
        rect3->set_id("rect3");
        rect3->set_position(150, 400);
        rect3->set_rect(250, 100, 12);
        rect3->set_fill(flex::Color(0.8f, 0.6f, 0.2f, 1.0f));
        rect3->set_stroke(flex::Color(1.0f, 1.0f, 1.0f, 0.3f), 1.0f);
        artboard->add_child(rect3);

        // Center camera on content
        camera_.x = 50;
        camera_.y = 50;
    }

    // ========================================================================
    // File I/O
    // ========================================================================

    bool loadDocument(const char* path) {
        auto definition = flex::Definition::load_file(path);
        if (!definition || definition->has_error()) {
            std::cerr << "Failed to load: " << path;
            if (definition) {
                std::cerr << " - " << definition->error_message();
            }
            std::cerr << "\n";
            return false;
        }

        // Clear current selection
        selected_nodes_.clear();
        selected_set_.clear();

        // Create new instance from definition
        instance_ = flex::Instance::create(definition);
        current_file_ = path;

        std::cout << "Loaded: " << path << "\n";
        return true;
    }

    std::string exportToFlex() {
        std::ostringstream out;
        auto* artboard = instance_->artboard();

        // Header
        out << "// Generated by Flex Editor\n\n";

        // Artboard
        out << "Artboard \"main\" (" << SCREEN_WIDTH << ", " << SCREEN_HEIGHT << ") {\n";

        // Export each child node
        auto* root = artboard->root();
        for (size_t i = 0; i < root->child_count(); ++i) {
            exportNode(out, root->child_at(i), 1);
        }

        out << "}\n";
        return out.str();
    }

    void exportNode(std::ostringstream& out, flex::Node* node, int indent) {
        if (!node) return;

        std::string pad(indent * 4, ' ');
        auto* shape = dynamic_cast<flex::Shape*>(node);

        if (shape) {
            out << pad << "Shape";
            if (!node->id().empty()) {
                out << " \"" << node->id() << "\"";
            }
            out << " {\n";

            // Position
            if (node->x() != 0 || node->y() != 0) {
                out << pad << "    x: " << node->x() << "\n";
                out << pad << "    y: " << node->y() << "\n";
            }

            // Geometry
            if (shape->geometry_type() == flex::GeometryType::Rect) {
                auto r = shape->rect();
                out << pad << "    Rect {\n";
                out << pad << "        width: " << r.width << "\n";
                out << pad << "        height: " << r.height << "\n";
                if (r.corner_radius > 0) {
                    out << pad << "        cornerRadius: " << r.corner_radius << "\n";
                }
                out << pad << "    }\n";
            }
            else if (shape->geometry_type() == flex::GeometryType::Circle) {
                auto c = shape->circle();
                out << pad << "    Circle { radius: " << c.radius << " }\n";
            }
            else if (shape->geometry_type() == flex::GeometryType::Ellipse) {
                auto e = shape->ellipse();
                out << pad << "    Ellipse { rx: " << e.rx << ", ry: " << e.ry << " }\n";
            }

            // Fill
            if (shape->has_fill()) {
                auto f = shape->fill();
                out << pad << "    Fill { color: " << colorToHex(f.color) << " }\n";
            }

            // Stroke
            if (shape->has_stroke()) {
                auto s = shape->stroke();
                out << pad << "    Stroke { color: " << colorToHex(s.color);
                if (s.width != 1.0f) {
                    out << ", width: " << s.width;
                }
                out << " }\n";
            }

            out << pad << "}\n\n";
        }
    }

    std::string colorToHex(const flex::Color& c) {
        char buf[16];
        int r = static_cast<int>(c.r * 255);
        int g = static_cast<int>(c.g * 255);
        int b = static_cast<int>(c.b * 255);
        snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
        return buf;
    }

    bool saveDocument(const char* path) {
        std::string content = exportToFlex();

        std::ofstream file(path);
        if (!file) {
            std::cerr << "Failed to save: " << path << "\n";
            return false;
        }

        file << content;
        current_file_ = path;
        std::cout << "Saved: " << path << "\n";
        return true;
    }

    // ========================================================================
    // Hit Testing
    // ========================================================================

    flex::Node* hitTest(float wx, float wy) {
        auto* root = instance_->artboard()->root();

        // Iterate in reverse order (top-most first)
        for (int i = static_cast<int>(root->child_count()) - 1; i >= 0; --i) {
            auto* node = root->child_at(i);
            if (nodeContainsPoint(node, wx, wy)) {
                return node;
            }
        }
        return nullptr;
    }

    bool nodeContainsPoint(flex::Node* node, float wx, float wy) {
        if (!node || !node->visible()) return false;

        auto* shape = dynamic_cast<flex::Shape*>(node);
        if (!shape) return false;

        float nx = node->x();
        float ny = node->y();
        float rotation = node->rotation();

        // Transform world point to local coordinates (inverse rotation around pivot)
        float lx = wx, ly = wy;
        if (rotation != 0) {
            // Pivot point is where node position is:
            // - Rect: top-left corner (nx, ny)
            // - Circle: center (nx, ny)
            float px = nx, py = ny;

            // Rotate point around pivot by -rotation
            float rad = -rotation * 3.14159f / 180.0f;
            float cos_a = std::cos(rad);
            float sin_a = std::sin(rad);
            float dx = wx - px;
            float dy = wy - py;
            lx = px + dx * cos_a - dy * sin_a;
            ly = py + dx * sin_a + dy * cos_a;
        }

        if (shape->geometry_type() == flex::GeometryType::Rect) {
            auto r = shape->rect();
            return lx >= nx && lx <= nx + r.width &&
                   ly >= ny && ly <= ny + r.height;
        }
        else if (shape->geometry_type() == flex::GeometryType::Circle) {
            float dx = lx - nx;
            float dy = ly - ny;
            float radius = shape->circle().radius;
            return (dx * dx + dy * dy) <= (radius * radius);
        }
        else if (shape->geometry_type() == flex::GeometryType::Path) {
            // For paths, use bounding box approximation
            auto bounds = shape->bounds();
            return lx >= bounds.x && lx <= bounds.x + bounds.width &&
                   ly >= bounds.y && ly <= bounds.y + bounds.height;
        }

        return false;
    }

    bool nodeIntersectsBox(flex::Node* node, float x1, float y1, float x2, float y2) {
        if (!node || !node->visible()) return false;

        auto* shape = dynamic_cast<flex::Shape*>(node);
        if (!shape) return false;

        // Normalize box
        float bx1 = std::min(x1, x2);
        float by1 = std::min(y1, y2);
        float bx2 = std::max(x1, x2);
        float by2 = std::max(y1, y2);

        float nx = node->x();
        float ny = node->y();

        if (shape->geometry_type() == flex::GeometryType::Rect) {
            auto r = shape->rect();
            float rx2 = nx + r.width;
            float ry2 = ny + r.height;
            return !(rx2 < bx1 || nx > bx2 || ry2 < by1 || ny > by2);
        }
        else if (shape->geometry_type() == flex::GeometryType::Circle) {
            float radius = shape->circle().radius;
            // Simple AABB test for circle
            return !(nx + radius < bx1 || nx - radius > bx2 ||
                     ny + radius < by1 || ny - radius > by2);
        }

        return false;
    }

    // Get shape bounds in world coordinates (x, y, width, height)
    bool getShapeBounds(flex::Node* node, float& x, float& y, float& w, float& h) {
        auto* shape = dynamic_cast<flex::Shape*>(node);
        if (!shape) return false;

        // Use bounds() for consistent behavior with scale
        auto b = shape->bounds();
        if (!b.valid()) return false;

        x = b.x;
        y = b.y;
        w = b.width;
        h = b.height;
        return true;
    }

    // ========================================================================
    // Unified Gizmo Calculation - Single source of truth
    // ========================================================================

    // Calculate all gizmo info for a shape (screen-space positions)
    ShapeGizmoInfo getShapeGizmoInfo(flex::Node* node) {
        ShapeGizmoInfo info;

        auto* shape = dynamic_cast<flex::Shape*>(node);
        if (!shape) return info;

        info.rotation = node->rotation();
        info.pivot_x = node->x();
        info.pivot_y = node->y();

        // Use bounds() consistently for all shape types
        // bounds() returns world coordinates with scale applied
        auto bounds = shape->bounds();
        if (!bounds.valid()) return info;

        float nx = bounds.x;
        float ny = bounds.y;
        float w = bounds.width;
        float h = bounds.height;

        info.world_x = nx;
        info.world_y = ny;
        info.world_w = w;
        info.world_h = h;

        // Calculate 4 corners in world coords (before rotation)
        float corners_w[4][2] = {
            {nx, ny},           // TL
            {nx + w, ny},       // TR
            {nx, ny + h},       // BL
            {nx + w, ny + h}    // BR
        };

        // Rotate corners around pivot and convert to screen
        float rad = info.rotation * 3.14159f / 180.0f;
        float cos_r = std::cos(rad);
        float sin_r = std::sin(rad);

        for (int i = 0; i < 4; ++i) {
            // Rotate around pivot
            float dx = corners_w[i][0] - info.pivot_x;
            float dy = corners_w[i][1] - info.pivot_y;
            float rot_x = info.pivot_x + dx * cos_r - dy * sin_r;
            float rot_y = info.pivot_y + dx * sin_r + dy * cos_r;

            // Convert to screen
            camera_.world_to_screen(rot_x, rot_y, info.corners[i][0], info.corners[i][1]);
        }

        // Calculate screen-space center
        info.center_x = (info.corners[0][0] + info.corners[3][0]) / 2;
        info.center_y = (info.corners[0][1] + info.corners[3][1]) / 2;

        // Calculate rotation handle position (above top edge center)
        float top_center_x = nx + w / 2;
        float top_center_y = ny - 30.0f / camera_.zoom;

        float dx = top_center_x - info.pivot_x;
        float dy = top_center_y - info.pivot_y;
        float rot_x = info.pivot_x + dx * cos_r - dy * sin_r;
        float rot_y = info.pivot_y + dx * sin_r + dy * cos_r;

        camera_.world_to_screen(rot_x, rot_y, info.rot_handle_x, info.rot_handle_y);

        info.valid = true;
        return info;
    }

    // Hit test resize handles using unified gizmo info
    ResizeHandle hitTestHandle(float sx, float sy, flex::Node*& out_node) {
        const float hs = 8.0f;

        for (auto* node : selected_nodes_) {
            ShapeGizmoInfo info = getShapeGizmoInfo(node);
            if (!info.valid) continue;

            ResizeHandle handle_types[] = {
                ResizeHandle::TopLeft,
                ResizeHandle::TopRight,
                ResizeHandle::BottomLeft,
                ResizeHandle::BottomRight,
            };

            for (int i = 0; i < 4; ++i) {
                if (std::abs(sx - info.corners[i][0]) <= hs &&
                    std::abs(sy - info.corners[i][1]) <= hs) {
                    out_node = node;
                    return handle_types[i];
                }
            }
        }

        out_node = nullptr;
        return ResizeHandle::None;
    }

    // Hit test rotation handle using unified gizmo info
    bool hitTestRotationHandle(float sx, float sy) {
        if (selected_nodes_.empty()) return false;

        ShapeGizmoInfo info = getShapeGizmoInfo(selected_nodes_[0]);
        if (!info.valid) return false;

        const float hs = 12.0f;
        return std::abs(sx - info.rot_handle_x) <= hs &&
               std::abs(sy - info.rot_handle_y) <= hs;
    }

    // Get selection bounds and center for rotation
    bool getSelectionBounds(float& min_x, float& min_y, float& max_x, float& max_y, float& center_x, float& center_y) {
        if (selected_nodes_.empty()) return false;

        min_x = 1e9f; min_y = 1e9f;
        max_x = -1e9f; max_y = -1e9f;

        for (auto* node : selected_nodes_) {
            float bx, by, bw, bh;
            if (getShapeBounds(node, bx, by, bw, bh)) {
                min_x = std::min(min_x, bx);
                min_y = std::min(min_y, by);
                max_x = std::max(max_x, bx + bw);
                max_y = std::max(max_y, by + bh);
            }
        }

        center_x = (min_x + max_x) / 2;
        center_y = (min_y + max_y) / 2;
        return max_x > min_x;
    }

    // Begin rotation drag
    void beginRotation(float sx, float sy) {
        if (selected_nodes_.empty()) return;

        float min_x, min_y, max_x, max_y, cx, cy;
        if (!getSelectionBounds(min_x, min_y, max_x, max_y, cx, cy)) return;

        // Convert center to screen
        float scx, scy;
        camera_.world_to_screen(cx, cy, scx, scy);

        // Calculate initial angle
        float angle = std::atan2(sy - scy, sx - scx) * 180.0f / 3.14159f;

        // For multi-selection, store first node (we rotate all)
        rotation_.begin(selected_nodes_[0], angle, cx, cy);
    }

    // Apply rotation during drag
    void applyRotation(float sx, float sy) {
        if (!rotation_.active) return;

        // Convert center to screen
        float scx, scy;
        camera_.world_to_screen(rotation_.center_x, rotation_.center_y, scx, scy);

        // Calculate current angle
        float current_angle = std::atan2(sy - scy, sx - scx) * 180.0f / 3.14159f;
        float delta = current_angle - rotation_.start_angle;

        // Apply to all selected nodes
        for (auto* node : selected_nodes_) {
            node->set_rotation(node->rotation() + delta);
        }

        rotation_.start_angle = current_angle;
    }

    // Apply resize to shape (rotation-aware)
    void resizeShape(flex::Node* node, ResizeHandle handle, float dx, float dy) {
        auto* shape = dynamic_cast<flex::Shape*>(node);
        if (!shape) return;

        float rotation = node->rotation();
        float rad = rotation * 3.14159f / 180.0f;
        float cos_r = std::cos(rad);
        float sin_r = std::sin(rad);

        // Transform world delta to local (shape's coordinate system)
        float ldx = dx * cos_r + dy * sin_r;
        float ldy = -dx * sin_r + dy * cos_r;

        float new_w = resize_.orig_w;
        float new_h = resize_.orig_h;

        // Position deltas in local space (for handles that move the origin)
        float local_pos_dx = 0, local_pos_dy = 0;

        // Adjust based on which handle is being dragged
        switch (handle) {
            case ResizeHandle::TopLeft:
                local_pos_dx = ldx;
                local_pos_dy = ldy;
                new_w -= ldx;
                new_h -= ldy;
                break;
            case ResizeHandle::TopRight:
                local_pos_dy = ldy;
                new_w += ldx;
                new_h -= ldy;
                break;
            case ResizeHandle::BottomLeft:
                local_pos_dx = ldx;
                new_w -= ldx;
                new_h += ldy;
                break;
            case ResizeHandle::BottomRight:
                new_w += ldx;
                new_h += ldy;
                break;
            default:
                break;
        }

        // Clamp minimum size
        const float min_size = 10.0f;
        if (new_w < min_size) {
            float diff = min_size - new_w;
            new_w = min_size;
            if (handle == ResizeHandle::TopLeft || handle == ResizeHandle::BottomLeft) {
                local_pos_dx -= diff;
            }
        }
        if (new_h < min_size) {
            float diff = min_size - new_h;
            new_h = min_size;
            if (handle == ResizeHandle::TopLeft || handle == ResizeHandle::TopRight) {
                local_pos_dy -= diff;
            }
        }

        // Transform position delta back to world space
        float world_pos_dx = local_pos_dx * cos_r - local_pos_dy * sin_r;
        float world_pos_dy = local_pos_dx * sin_r + local_pos_dy * cos_r;

        float new_x = resize_.orig_x + world_pos_dx;
        float new_y = resize_.orig_y + world_pos_dy;

        // Apply changes
        if (shape->geometry_type() == flex::GeometryType::Rect) {
            node->set_position(new_x, new_y);
            shape->set_rect(new_w, new_h, shape->rect().corner_radius);
        }
        else if (shape->geometry_type() == flex::GeometryType::Circle) {
            float radius = std::min(new_w, new_h) / 2.0f;
            // For circles, adjust position to keep center at right place
            float center_offset_x = (resize_.orig_w / 2.0f - radius) * cos_r;
            float center_offset_y = (resize_.orig_w / 2.0f - radius) * sin_r;
            node->set_position(resize_.orig_x + center_offset_x + radius,
                             resize_.orig_y + center_offset_y + radius);
            shape->set_circle(radius);
        }
    }

    // ========================================================================
    // Selection
    // ========================================================================

    bool isSelected(flex::Node* node) {
        return selected_set_.count(node) > 0;
    }

    void selectNode(flex::Node* node, bool add_to_selection = false) {
        if (!add_to_selection) {
            selected_nodes_.clear();
            selected_set_.clear();
        }
        if (node && selected_set_.insert(node).second) {
            selected_nodes_.push_back(node);
            std::cout << "Selected: " << node->id() << "\n";
        }
    }

    void deselectAll() {
        selected_nodes_.clear();
        selected_set_.clear();
        std::cout << "Deselected all\n";
    }

    void selectInBox(float x1, float y1, float x2, float y2) {
        selected_nodes_.clear();
        selected_set_.clear();
        auto* root = instance_->artboard()->root();

        for (size_t i = 0; i < root->child_count(); ++i) {
            auto* node = root->child_at(i);
            if (nodeIntersectsBox(node, x1, y1, x2, y2)) {
                selected_nodes_.push_back(node);
                selected_set_.insert(node);
            }
        }

        if (!selected_nodes_.empty()) {
            std::cout << "Box selected " << selected_nodes_.size() << " nodes\n";
        }
    }

    void deleteSelected() {
        if (selected_nodes_.empty()) return;

        auto* root = instance_->artboard()->root();
        for (auto* node : selected_nodes_) {
            root->remove_child(node);
            std::cout << "Deleted: " << node->id() << "\n";
        }
        selected_nodes_.clear();
        selected_set_.clear();
    }

    // ========================================================================
    // Event Handling
    // ========================================================================

    void handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running_ = false;
                    break;

                case SDL_KEYDOWN:
                    handleKeyDown(event.key);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    handleMouseDown(event.button);
                    break;

                case SDL_MOUSEBUTTONUP:
                    handleMouseUp(event.button);
                    break;

                case SDL_MOUSEMOTION:
                    handleMouseMove(event.motion);
                    break;

                case SDL_MOUSEWHEEL:
                    handleMouseWheel(event.wheel);
                    break;
            }
        }
    }

    void handleKeyDown(const SDL_KeyboardEvent& key) {
        // Hide context menu on any key press
        if (context_menu_.visible) {
            context_menu_.hide();
        }

        bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;

        switch (key.keysym.sym) {
            case SDLK_ESCAPE:
                deselectAll();
                break;

            case SDLK_DELETE:
            case SDLK_BACKSPACE:
                deleteSelected();
                break;

            case SDLK_q:
                running_ = false;
                break;

            case SDLK_n:
                createShape(false);
                break;

            case SDLK_c:
                createShape(true);
                break;

            case SDLK_l:
                loadDocument(current_file_.empty() ? "document.flex" : current_file_.c_str());
                break;

            case SDLK_s:
                saveDocument(current_file_.empty() ? "document.flex" : current_file_.c_str());
                break;

            case SDLK_r:
                // R = rotate 15° CW, Shift+R = rotate 15° CCW
                rotateSelection(shift ? -15.0f : 15.0f);
                break;

            case SDLK_g:
                // G = group, Shift+G = ungroup
                if (shift) {
                    ungroupSelection();
                } else {
                    groupSelection();
                }
                break;

            case SDLK_d:
                duplicateSelection();
                break;

            case SDLK_p:
                // Toggle panel visibility
                panel_.visible = !panel_.visible;
                std::cout << "Panel: " << (panel_.visible ? "visible" : "hidden") << "\n";
                break;

            case SDLK_b:
                // Pen/Brush tool
                toolbar_.active_tool = ToolType::Pen;
                std::cout << "Tool: Pen\n";
                break;

            case SDLK_v:
                // Select tool
                toolbar_.active_tool = ToolType::Select;
                std::cout << "Tool: Select\n";
                break;

            case SDLK_m:
                // Toggle minimap/overview visibility
                overview_.visible = !overview_.visible;
                std::cout << "Overview: " << (overview_.visible ? "visible" : "hidden") << "\n";
                break;
        }
    }

    void handleMouseDown(const SDL_MouseButtonEvent& btn) {
        float mx = static_cast<float>(btn.x);
        float my = static_cast<float>(btn.y);
        float wx, wy;
        camera_.screen_to_world(mx, my, wx, wy);

        // Context menu click handling
        if (context_menu_.visible) {
            if (btn.button == SDL_BUTTON_LEFT) {
                int idx = context_menu_.hit_test(mx, my);
                if (idx >= 0 && context_menu_.items[idx].enabled) {
                    context_menu_.items[idx].action();
                }
            }
            context_menu_.hide();
            return;
        }

        // Toolbar click handling
        if (btn.button == SDL_BUTTON_LEFT && my < Toolbar::HEIGHT) {
            // Check tool buttons
            int tool_idx = toolbar_.hit_test_tool(mx, my);
            if (tool_idx >= 0 && toolbar_.tools[tool_idx].enabled) {
                toolbar_.active_tool = toolbar_.tools[tool_idx].tool;
                std::cout << "Tool: " << toolbar_.tools[tool_idx].tooltip << "\n";
                return;
            }

            // Check command buttons
            int cmd_idx = toolbar_.hit_test_cmd(mx, my);
            if (cmd_idx >= 0 && toolbar_.commands[cmd_idx].enabled) {
                executeCommand(toolbar_.commands[cmd_idx].cmd);
                return;
            }
            return;
        }

        // Panel click handling
        if (panel_.visible && btn.button == SDL_BUTTON_LEFT) {
            float px = panel_.x(SCREEN_WIDTH);
            if (mx >= px && my >= Toolbar::HEIGHT) {
                float local_x = mx - px;
                float local_y = my - Toolbar::HEIGHT;

                // Check section headers first
                int sec_idx = panel_.hit_test_section(local_y);
                if (sec_idx >= 0) {
                    panel_.sections[sec_idx].collapsed = !panel_.sections[sec_idx].collapsed;
                    return;
                }

                // Check pen settings if section is open
                if (!panel_.sections[2].collapsed) {  // Pen Settings is index 2
                    if (handlePenSettingsClick(local_x, local_y)) {
                        return;
                    }
                }
                return;
            }
        }

        // Overview window click handling
        if (overview_.visible && btn.button == SDL_BUTTON_LEFT) {
            float ox = overview_.x(SCREEN_WIDTH, panel_.width, panel_.visible);
            float oy = overview_.y(SCREEN_HEIGHT);
            if (overview_.hit_test(mx, my, ox, oy)) {
                navigateFromOverview(mx, my, ox, oy);
                return;
            }
        }

        // Bottom toolbar zoom buttons
        if (btn.button == SDL_BUTTON_LEFT) {
            int zoom_dir = hitTestZoomButtons(mx, my);
            if (zoom_dir != 0) {
                float zoom_factor = 1.2f;
                float old_zoom = camera_.zoom;
                if (zoom_dir > 0) {
                    camera_.zoom *= zoom_factor;
                } else {
                    camera_.zoom /= zoom_factor;
                }
                camera_.zoom = std::max(0.1f, std::min(10.0f, camera_.zoom));
                // Zoom towards screen center
                float cx = SCREEN_WIDTH / 2.0f;
                float cy = SCREEN_HEIGHT / 2.0f;
                float scale_change = camera_.zoom / old_zoom;
                camera_.x = cx - (cx - camera_.x) * scale_change;
                camera_.y = cy - (cy - camera_.y) * scale_change;
                return;
            }
        }

        // Middle button = Pan
        if (btn.button == SDL_BUTTON_MIDDLE) {
            pan_.begin(mx, my, camera_);
            return;
        }

        // Right button = Context menu
        if (btn.button == SDL_BUTTON_RIGHT) {
            buildContextMenu(mx, my);
            return;
        }

        // Left button = Rotation/Resize/Select/Drag or Draw
        if (btn.button == SDL_BUTTON_LEFT) {
            // Pen tool - start drawing
            if (toolbar_.active_tool == ToolType::Pen) {
                draw_.begin(wx, wy);
                return;
            }

            // First, check rotation handle
            if (hitTestRotationHandle(mx, my)) {
                beginRotation(mx, my);
                return;
            }

            // Check resize handles
            flex::Node* handle_node = nullptr;
            ResizeHandle handle = hitTestHandle(mx, my, handle_node);

            if (handle != ResizeHandle::None && handle_node) {
                // Start resize
                float bx, by, bw, bh;
                if (getShapeBounds(handle_node, bx, by, bw, bh)) {
                    resize_.begin(handle, handle_node, wx, wy, bx, by, bw, bh);
                    return;
                }
            }

            // Otherwise, normal select/drag
            auto* hit = hitTest(wx, wy);

            if (hit) {
                // If not already selected, select it
                bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                if (!isSelected(hit)) {
                    selectNode(hit, shift);
                }
                drag_.begin(wx, wy, selected_nodes_);
            } else {
                // Start selection box
                deselectAll();
                selection_box_.active = true;
                selection_box_.start_x = wx;
                selection_box_.start_y = wy;
                selection_box_.end_x = wx;
                selection_box_.end_y = wy;
            }
        }
    }

    void handleMouseUp(const SDL_MouseButtonEvent& btn) {
        if (btn.button == SDL_BUTTON_MIDDLE) {
            pan_.end();
        }

        if (btn.button == SDL_BUTTON_LEFT) {
            // Finish drawing with Pen tool
            if (draw_.active) {
                finishDrawing();
            }
            if (rotation_.active) {
                rotation_.end();
                std::cout << "Rotation complete\n";
            }
            if (resize_.active) {
                resize_.end();
            }
            if (selection_box_.active) {
                selectInBox(
                    selection_box_.start_x, selection_box_.start_y,
                    selection_box_.end_x, selection_box_.end_y
                );
                selection_box_.active = false;
            }
            drag_.end();
        }
    }

    void handleMouseMove(const SDL_MouseMotionEvent& motion) {
        float mx = static_cast<float>(motion.x);
        float my = static_cast<float>(motion.y);
        float wx, wy;
        camera_.screen_to_world(mx, my, wx, wy);

        // Update context menu hover
        if (context_menu_.visible) {
            context_menu_.hover_index = context_menu_.hit_test(mx, my);
        }

        // Update toolbar hover
        if (my < Toolbar::HEIGHT) {
            toolbar_.hover_tool = toolbar_.hit_test_tool(mx, my);
            toolbar_.hover_cmd = toolbar_.hit_test_cmd(mx, my);
        } else {
            toolbar_.hover_tool = -1;
            toolbar_.hover_cmd = -1;
        }

        if (pan_.active) {
            camera_.x = pan_.cam_x + (mx - pan_.start_x);
            camera_.y = pan_.cam_y + (my - pan_.start_y);
        }

        // Add points while drawing
        if (draw_.active) {
            draw_.add_point(wx, wy);
        }

        if (rotation_.active) {
            applyRotation(mx, my);
        }

        if (resize_.active && resize_.node) {
            float dx = wx - resize_.start_x;
            float dy = wy - resize_.start_y;
            resizeShape(resize_.node, resize_.handle, dx, dy);
        }

        if (selection_box_.active) {
            selection_box_.end_x = wx;
            selection_box_.end_y = wy;
        }

        if (drag_.active && !selected_nodes_.empty()) {
            float dx = wx - drag_.start_x;
            float dy = wy - drag_.start_y;

            for (size_t i = 0; i < selected_nodes_.size(); ++i) {
                auto& origin = drag_.node_origins[i];
                selected_nodes_[i]->set_position(origin.first + dx, origin.second + dy);
            }
        }
    }

    void handleMouseWheel(const SDL_MouseWheelEvent& wheel) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);

        float old_zoom = camera_.zoom;
        float zoom_factor = 1.1f;

        if (wheel.y > 0) {
            camera_.zoom *= zoom_factor;
        } else if (wheel.y < 0) {
            camera_.zoom /= zoom_factor;
        }

        // Clamp zoom
        camera_.zoom = std::max(0.1f, std::min(10.0f, camera_.zoom));

        // Zoom towards mouse position
        float scale_change = camera_.zoom / old_zoom;
        camera_.x = mx - (mx - camera_.x) * scale_change;
        camera_.y = my - (my - camera_.y) * scale_change;
    }

    // ========================================================================
    // Create New Shapes
    // ========================================================================

    void finishDrawing() {
        if (!draw_.active) return;

        // Calculate path bounds to set proper position
        float min_x, min_y, max_x, max_y;
        draw_.get_bounds(min_x, min_y, max_x, max_y);
        float w = max_x - min_x;
        float h = max_y - min_y;

        // Generate path with coordinates relative to bounding box origin
        std::string path_d = draw_.to_path_relative(min_x, min_y, pen_settings_.smooth_curves);
        draw_.end();

        if (path_d.empty()) return;

        static int path_count = 0;
        auto shape = flex::Shape::create();
        shape->set_id("path" + std::to_string(++path_count));

        // Set position to path bounds origin - path coordinates are local
        shape->set_position(min_x, min_y);
        // Use set_path with explicit bounds for proper gizmo display
        shape->set_path(path_d, w, h);

        // Apply pen settings
        shape->set_stroke(pen_settings_.stroke_color, pen_settings_.stroke_width);
        if (pen_settings_.use_fill) {
            shape->set_fill(pen_settings_.fill_color);
        }

        instance_->artboard()->add_child(shape);
        selectNode(shape.get());

        std::cout << "Created path: " << shape->id() << " at (" << min_x << ", " << min_y
                  << ") size " << w << "x" << h << "\n";
    }

    void createShape(bool is_circle) {
        static int rect_count = 4;
        static int circle_count = 1;

        auto shape = flex::Shape::create();
        if (is_circle) {
            shape->set_id("circle" + std::to_string(++circle_count));
            shape->set_circle(30 + rand() % 40);
        } else {
            shape->set_id("rect" + std::to_string(++rect_count));
            shape->set_rect(100 + rand() % 100, 80 + rand() % 80, 4);
        }

        shape->set_position(
            SCREEN_WIDTH / 2 - camera_.x / camera_.zoom,
            SCREEN_HEIGHT / 2 - camera_.y / camera_.zoom
        );
        shape->set_fill(flex::Color(
            0.3f + (rand() % 50) / 100.0f,
            0.3f + (rand() % 50) / 100.0f,
            0.3f + (rand() % 50) / 100.0f,
            1.0f
        ));
        shape->set_stroke(flex::Color(1.0f, 1.0f, 1.0f, 0.3f), 1.0f);

        instance_->artboard()->add_child(shape);
        selectNode(shape.get());
        std::cout << "Created: " << shape->id() << "\n";
    }

    // ========================================================================
    // Update & Render
    // ========================================================================

    void update(float dt) {
        instance_->advance(dt);
    }

    void render() {
        // === ThorVG Scene Rendering ===
        tvg_canvas_->remove();

        auto* artboard = instance_->artboard();
        flex_renderer_->begin_frame(SCREEN_WIDTH, SCREEN_HEIGHT, 1.0f);

        // Clear with background
        flex_renderer_->clear(artboard->background());

        // Draw grid (via ThorVG)
        drawGridTVG();

        // Apply camera transform
        flex_renderer_->save();
        flex_renderer_->translate(camera_.x, camera_.y);
        flex_renderer_->scale(camera_.zoom, camera_.zoom);

        // Render all nodes
        instance_->render(*flex_renderer_);

        // Draw gizmos in world space (same coordinate system as shapes)
        drawGizmosTVG();

        // Draw selection box in world space
        if (selection_box_.active) {
            drawSelectionBoxTVG();
        }

        // Draw current path being drawn (preview)
        if (draw_.active && draw_.points.size() >= 2) {
            std::string preview_path = draw_.to_path(pen_settings_.smooth_curves);
            flex::Color preview_color = pen_settings_.stroke_color;
            preview_color.a *= 0.8f;  // Slightly transparent preview
            flex_renderer_->stroke_path(preview_path,
                flex::Paint::solid(preview_color), pen_settings_.stroke_width);
        }

        flex_renderer_->restore();

        // Draw UI components (screen space, via ThorVG)
        drawToolbarTVG();
        drawPanelTVG();
        drawOverviewTVG();
        drawBottomToolbarTVG();
        drawContextMenuTVG();

        flex_renderer_->end_frame();

        // Sync ThorVG to buffer
        tvg_canvas_->draw(true);
        tvg_canvas_->sync();

        // === SDL Overlay ===
        // Copy ThorVG buffer to SDL texture
        SDL_UpdateTexture(texture_, nullptr, tvg_buffer_.data(), SCREEN_WIDTH * sizeof(uint32_t));
        SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);

        // All rendering now via ThorVG - no SDL overlays needed
        SDL_RenderPresent(sdl_renderer_);
    }

    void drawGridTVG() {
        // Draw grid using ThorVG for anti-aliased lines
        flex::Color grid_color(0.18f, 0.18f, 0.20f, 1.0f);
        flex::Paint paint = flex::Paint::solid(grid_color);

        float grid_size = 50.0f;
        float world_left = -camera_.x / camera_.zoom;
        float world_top = -camera_.y / camera_.zoom;
        float world_right = (SCREEN_WIDTH - camera_.x) / camera_.zoom;
        float world_bottom = (SCREEN_HEIGHT - camera_.y) / camera_.zoom;

        // Snap to grid
        float start_x = std::floor(world_left / grid_size) * grid_size;
        float start_y = std::floor(world_top / grid_size) * grid_size;

        flex_renderer_->save();
        flex_renderer_->translate(camera_.x, camera_.y);
        flex_renderer_->scale(camera_.zoom, camera_.zoom);

        // Vertical lines
        for (float x = start_x; x <= world_right; x += grid_size) {
            std::string path = "M " + std::to_string(x) + " " + std::to_string(world_top) +
                              " L " + std::to_string(x) + " " + std::to_string(world_bottom);
            flex_renderer_->stroke_path(path, paint, 1.0f / camera_.zoom);
        }

        // Horizontal lines
        for (float y = start_y; y <= world_bottom; y += grid_size) {
            std::string path = "M " + std::to_string(world_left) + " " + std::to_string(y) +
                              " L " + std::to_string(world_right) + " " + std::to_string(y);
            flex_renderer_->stroke_path(path, paint, 1.0f / camera_.zoom);
        }

        flex_renderer_->restore();
    }

    // Helper: rotate point (px,py) around center (cx,cy) by angle in degrees
    void rotatePoint(float px, float py, float cx, float cy, float angle_deg,
                     float& out_x, float& out_y) {
        float rad = angle_deg * 3.14159f / 180.0f;
        float cos_a = std::cos(rad);
        float sin_a = std::sin(rad);
        float dx = px - cx;
        float dy = py - cy;
        out_x = cx + dx * cos_a - dy * sin_a;
        out_y = cy + dx * sin_a + dy * cos_a;
    }

    // Helper: generate SVG rect path
    std::string rect_path(float x, float y, float w, float h) {
        return "M " + std::to_string(x) + " " + std::to_string(y) +
               " H " + std::to_string(x + w) +
               " V " + std::to_string(y + h) +
               " H " + std::to_string(x) + " Z";
    }

    // Helper: generate SVG line path
    std::string line_path(float x1, float y1, float x2, float y2) {
        return "M " + std::to_string(x1) + " " + std::to_string(y1) +
               " L " + std::to_string(x2) + " " + std::to_string(y2);
    }

    // Helper: convert 0-255 color to flex::Color (0-1 range)
    flex::Color rgb(int r, int g, int b, int a = 255) {
        return flex::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }

    // Update document bounds for overview window
    void updateDocumentBounds() {
        auto* root = instance_->artboard()->root();
        if (root->child_count() == 0) {
            overview_.doc_min_x = 0;
            overview_.doc_min_y = 0;
            overview_.doc_max_x = SCREEN_WIDTH;
            overview_.doc_max_y = SCREEN_HEIGHT;
            return;
        }

        float min_x = 1e9f, min_y = 1e9f;
        float max_x = -1e9f, max_y = -1e9f;

        for (size_t i = 0; i < root->child_count(); ++i) {
            auto* node = root->child_at(i);
            float bx, by, bw, bh;
            if (getShapeBounds(node, bx, by, bw, bh)) {
                min_x = std::min(min_x, bx);
                min_y = std::min(min_y, by);
                max_x = std::max(max_x, bx + bw);
                max_y = std::max(max_y, by + bh);
            }
        }

        // Add padding
        float padding = 50.0f;
        overview_.doc_min_x = min_x - padding;
        overview_.doc_min_y = min_y - padding;
        overview_.doc_max_x = max_x + padding;
        overview_.doc_max_y = max_y + padding;

        // Ensure minimum size
        if (overview_.doc_max_x - overview_.doc_min_x < 200) {
            overview_.doc_max_x = overview_.doc_min_x + 200;
        }
        if (overview_.doc_max_y - overview_.doc_min_y < 200) {
            overview_.doc_max_y = overview_.doc_min_y + 200;
        }
    }

    void drawOverviewTVG() {
        if (!overview_.visible) return;

        updateDocumentBounds();

        float ox = overview_.x(SCREEN_WIDTH, panel_.width, panel_.visible);
        float oy = overview_.y(SCREEN_HEIGHT);
        float ow = overview_.width;
        float oh = overview_.height;

        // Background with shadow
        flex_renderer_->fill_path(rect_path(ox + 3, oy + 3, ow, oh),
                                  flex::Paint::solid(rgb(0, 0, 0, 60)));
        flex_renderer_->fill_path(rect_path(ox, oy, ow, oh),
                                  flex::Paint::solid(rgb(30, 30, 35, 230)));
        flex_renderer_->stroke_path(rect_path(ox, oy, ow, oh),
                                    flex::Paint::solid(rgb(60, 60, 70)), 1.0f);

        // Calculate scale to fit document in overview
        float doc_w = overview_.doc_max_x - overview_.doc_min_x;
        float doc_h = overview_.doc_max_y - overview_.doc_min_y;
        float scale_x = (ow - 4) / doc_w;
        float scale_y = (oh - 4) / doc_h;
        float scale = std::min(scale_x, scale_y);

        // Offset to center the content
        float content_w = doc_w * scale;
        float content_h = doc_h * scale;
        float offset_x = ox + 2 + (ow - 4 - content_w) / 2;
        float offset_y = oy + 2 + (oh - 4 - content_h) / 2;

        // Draw simplified shapes
        auto* root = instance_->artboard()->root();
        for (size_t i = 0; i < root->child_count(); ++i) {
            auto* node = root->child_at(i);
            if (!node->visible()) continue;

            float bx, by, bw, bh;
            if (!getShapeBounds(node, bx, by, bw, bh)) continue;

            // Transform to overview coords
            float rx = offset_x + (bx - overview_.doc_min_x) * scale;
            float ry = offset_y + (by - overview_.doc_min_y) * scale;
            float rw = std::max(2.0f, bw * scale);
            float rh = std::max(2.0f, bh * scale);

            // Get shape color
            flex::Color col(0.5f, 0.5f, 0.5f, 0.8f);
            auto* shape = dynamic_cast<flex::Shape*>(node);
            if (shape && shape->has_fill()) {
                col = shape->fill().color;
                col.a = 0.8f;
            }

            // Highlight selected
            if (isSelected(node)) {
                flex_renderer_->fill_path(rect_path(rx - 1, ry - 1, rw + 2, rh + 2),
                                          flex::Paint::solid(rgb(0, 153, 255, 150)));
            }

            flex_renderer_->fill_path(rect_path(rx, ry, rw, rh),
                                      flex::Paint::solid(col));
        }

        // Draw viewport rectangle (what's currently visible on screen)
        float vp_left = -camera_.x / camera_.zoom;
        float vp_top = -camera_.y / camera_.zoom;
        float vp_right = (SCREEN_WIDTH - camera_.x) / camera_.zoom;
        float vp_bottom = (SCREEN_HEIGHT - camera_.y) / camera_.zoom;

        float vx = offset_x + (vp_left - overview_.doc_min_x) * scale;
        float vy = offset_y + (vp_top - overview_.doc_min_y) * scale;
        float vw = (vp_right - vp_left) * scale;
        float vh = (vp_bottom - vp_top) * scale;

        // Clamp to overview bounds
        vx = std::max(ox + 2, std::min(vx, ox + ow - 4));
        vy = std::max(oy + 2, std::min(vy, oy + oh - 4));
        vw = std::max(4.0f, std::min(vw, ow - 4));
        vh = std::max(4.0f, std::min(vh, oh - 4));

        // Semi-transparent viewport fill
        flex_renderer_->fill_path(rect_path(vx, vy, vw, vh),
                                  flex::Paint::solid(rgb(0, 120, 215, 40)));
        // Viewport border
        flex_renderer_->stroke_path(rect_path(vx, vy, vw, vh),
                                    flex::Paint::solid(rgb(0, 153, 255)), 1.5f);
    }

    void navigateFromOverview(float mx, float my, float ox, float oy) {
        float ow = overview_.width;
        float oh = overview_.height;

        // Calculate scale (same as in drawOverviewTVG)
        float doc_w = overview_.doc_max_x - overview_.doc_min_x;
        float doc_h = overview_.doc_max_y - overview_.doc_min_y;
        float scale_x = (ow - 4) / doc_w;
        float scale_y = (oh - 4) / doc_h;
        float scale = std::min(scale_x, scale_y);

        // Offset (same as in drawOverviewTVG)
        float content_w = doc_w * scale;
        float content_h = doc_h * scale;
        float offset_x = ox + 2 + (ow - 4 - content_w) / 2;
        float offset_y = oy + 2 + (oh - 4 - content_h) / 2;

        // Convert click position to world coordinates
        float world_x = overview_.doc_min_x + (mx - offset_x) / scale;
        float world_y = overview_.doc_min_y + (my - offset_y) / scale;

        // Center camera on clicked position
        camera_.x = SCREEN_WIDTH / 2 - world_x * camera_.zoom;
        camera_.y = SCREEN_HEIGHT / 2 - world_y * camera_.zoom;
    }

    // Bottom toolbar with zoom controls
    static constexpr float BOTTOM_BAR_HEIGHT = 28.0f;

    void drawBottomToolbarTVG() {
        float bx = 10.0f;
        float by = SCREEN_HEIGHT - BOTTOM_BAR_HEIGHT - 5.0f;
        float btn_size = 24.0f;
        float spacing = 4.0f;

        // Background
        flex_renderer_->fill_path(rect_path(bx - 4, by - 2, 140, BOTTOM_BAR_HEIGHT),
                                  flex::Paint::solid(rgb(35, 35, 40, 220)));
        flex_renderer_->stroke_path(rect_path(bx - 4, by - 2, 140, BOTTOM_BAR_HEIGHT),
                                    flex::Paint::solid(rgb(60, 60, 70)), 1.0f);

        // Zoom out button [-]
        flex_renderer_->fill_path(rect_path(bx, by, btn_size, btn_size),
                                  flex::Paint::solid(rgb(50, 50, 55)));
        flex_renderer_->stroke_path(rect_path(bx, by, btn_size, btn_size),
                                    flex::Paint::solid(rgb(70, 70, 80)), 1.0f);
        flex_renderer_->stroke_path(line_path(bx + 6, by + btn_size/2, bx + btn_size - 6, by + btn_size/2),
                                    flex::Paint::solid(rgb(200, 200, 200)), 2.0f);

        // Zoom percentage text
        int zoom_pct = static_cast<int>(camera_.zoom * 100);
        std::string zoom_text = std::to_string(zoom_pct) + "%";
        float text_x = bx + btn_size + spacing + 25;
        flex_renderer_->draw_text(zoom_text, text_x, by + 17, "Segoe UI", 12.0f, false, rgb(180, 180, 180));

        // Zoom in button [+]
        float plus_x = bx + btn_size + spacing + 80;
        flex_renderer_->fill_path(rect_path(plus_x, by, btn_size, btn_size),
                                  flex::Paint::solid(rgb(50, 50, 55)));
        flex_renderer_->stroke_path(rect_path(plus_x, by, btn_size, btn_size),
                                    flex::Paint::solid(rgb(70, 70, 80)), 1.0f);
        // Plus sign
        flex_renderer_->stroke_path(line_path(plus_x + 6, by + btn_size/2, plus_x + btn_size - 6, by + btn_size/2),
                                    flex::Paint::solid(rgb(200, 200, 200)), 2.0f);
        flex_renderer_->stroke_path(line_path(plus_x + btn_size/2, by + 6, plus_x + btn_size/2, by + btn_size - 6),
                                    flex::Paint::solid(rgb(200, 200, 200)), 2.0f);
    }

    // Hit test for bottom toolbar zoom buttons
    int hitTestZoomButtons(float mx, float my) {
        float bx = 10.0f;
        float by = SCREEN_HEIGHT - BOTTOM_BAR_HEIGHT - 5.0f;
        float btn_size = 24.0f;
        float spacing = 4.0f;

        // Zoom out button
        if (mx >= bx && mx < bx + btn_size && my >= by && my < by + btn_size) {
            return -1;  // Zoom out
        }

        // Zoom in button
        float plus_x = bx + btn_size + spacing + 80;
        if (mx >= plus_x && mx < plus_x + btn_size && my >= by && my < by + btn_size) {
            return 1;  // Zoom in
        }

        return 0;  // No hit
    }

    // Draw gizmos using ThorVG in world coordinates (called inside camera transform)
    void drawGizmosTVG() {
        if (selected_nodes_.empty()) return;

        flex::Color select_color = rgb(0, 153, 255);
        flex::Color handle_fill = rgb(255, 255, 255);
        flex::Color rot_handle_fill = rgb(100, 200, 100);

        // Zoom-independent sizes (divide by zoom to keep constant screen size)
        float line_width = 1.5f / camera_.zoom;
        float handle_size = 8.0f / camera_.zoom;
        float rot_handle_size = 10.0f / camera_.zoom;
        float rot_handle_offset = 30.0f / camera_.zoom;

        bool first_shape = true;

        for (auto* node : selected_nodes_) {
            auto* shape = dynamic_cast<flex::Shape*>(node);
            if (!shape) continue;

            // Use bounds() consistently for all shape types
            // bounds() returns world coordinates with scale applied
            auto bounds = shape->bounds();
            if (!bounds.valid()) continue;

            float nx = bounds.x;
            float ny = bounds.y;
            float w = bounds.width;
            float h = bounds.height;

            float rotation = node->rotation();
            float rad = rotation * 3.14159f / 180.0f;
            float cos_r = std::cos(rad);
            float sin_r = std::sin(rad);
            float pivot_x = node->x();
            float pivot_y = node->y();

            // Calculate rotated corners in world space
            float corners[4][2] = {
                {nx, ny},           // TL
                {nx + w, ny},       // TR
                {nx, ny + h},       // BL
                {nx + w, ny + h}    // BR
            };

            // Rotate corners around pivot
            for (int i = 0; i < 4; ++i) {
                float dx = corners[i][0] - pivot_x;
                float dy = corners[i][1] - pivot_y;
                corners[i][0] = pivot_x + dx * cos_r - dy * sin_r;
                corners[i][1] = pivot_y + dx * sin_r + dy * cos_r;
            }

            // Draw selection frame (4 lines)
            std::string frame_path =
                "M " + std::to_string(corners[0][0]) + " " + std::to_string(corners[0][1]) +
                " L " + std::to_string(corners[1][0]) + " " + std::to_string(corners[1][1]) +
                " L " + std::to_string(corners[3][0]) + " " + std::to_string(corners[3][1]) +
                " L " + std::to_string(corners[2][0]) + " " + std::to_string(corners[2][1]) +
                " Z";
            flex_renderer_->stroke_path(frame_path, flex::Paint::solid(select_color), line_width);

            // Draw corner handles
            float hs = handle_size / 2;
            for (int i = 0; i < 4; ++i) {
                std::string handle_path = rect_path(corners[i][0] - hs, corners[i][1] - hs, handle_size, handle_size);
                flex_renderer_->fill_path(handle_path, flex::Paint::solid(handle_fill));
                flex_renderer_->stroke_path(handle_path, flex::Paint::solid(select_color), line_width);
            }

            // Draw rotation handle (first shape only)
            if (first_shape) {
                // Calculate rotation handle position (above top edge center)
                float top_mid_x = nx + w / 2;
                float top_mid_y = ny - rot_handle_offset;

                // Rotate around pivot
                float dx = top_mid_x - pivot_x;
                float dy = top_mid_y - pivot_y;
                float rot_x = pivot_x + dx * cos_r - dy * sin_r;
                float rot_y = pivot_y + dx * sin_r + dy * cos_r;

                // Line from top edge center to rotation handle
                float edge_mid_x = (corners[0][0] + corners[1][0]) / 2;
                float edge_mid_y = (corners[0][1] + corners[1][1]) / 2;

                flex_renderer_->stroke_path(
                    line_path(edge_mid_x, edge_mid_y, rot_x, rot_y),
                    flex::Paint::solid(select_color), line_width);

                // Rotation handle square
                float rhs = rot_handle_size / 2;
                std::string rot_path = rect_path(rot_x - rhs, rot_y - rhs, rot_handle_size, rot_handle_size);
                flex_renderer_->fill_path(rot_path, flex::Paint::solid(rot_handle_fill));
                flex_renderer_->stroke_path(rot_path, flex::Paint::solid(handle_fill), line_width);

                first_shape = false;
            }
        }
    }

    // Draw selection box using ThorVG in world coordinates
    void drawSelectionBoxTVG() {
        float x1 = std::min(selection_box_.start_x, selection_box_.end_x);
        float y1 = std::min(selection_box_.start_y, selection_box_.end_y);
        float x2 = std::max(selection_box_.start_x, selection_box_.end_x);
        float y2 = std::max(selection_box_.start_y, selection_box_.end_y);
        float w = x2 - x1;
        float h = y2 - y1;

        float line_width = 1.0f / camera_.zoom;

        // Semi-transparent fill
        flex_renderer_->fill_path(rect_path(x1, y1, w, h),
                                  flex::Paint::solid(flex::Color(0, 0.6f, 1.0f, 0.12f)));
        // Border
        flex_renderer_->stroke_path(rect_path(x1, y1, w, h),
                                    flex::Paint::solid(rgb(0, 153, 255)), line_width);
    }

    void buildContextMenu(float sx, float sy) {
        context_menu_.items.clear();
        bool has_selection = !selected_nodes_.empty();
        bool has_group = false;

        // Check if selection contains a group
        for (auto* node : selected_nodes_) {
            if (node->is_group()) {
                has_group = true;
                break;
            }
        }

        // Rotate submenu items
        MenuItem rot_cw{"Rotate 90° CW", [this]() { rotateSelection(90); }, has_selection};
        MenuItem rot_ccw{"Rotate 90° CCW", [this]() { rotateSelection(-90); }, has_selection};
        MenuItem rot_45{"Rotate 45°", [this]() { rotateSelection(45); }, has_selection};

        context_menu_.items.push_back(rot_cw);
        context_menu_.items.push_back(rot_ccw);
        context_menu_.items.push_back(rot_45);
        context_menu_.items.push_back(MenuItem::Separator());

        // Group/Ungroup
        MenuItem group_item{"Group (G)", [this]() { groupSelection(); }, selected_nodes_.size() > 1};
        MenuItem ungroup_item{"Ungroup (Shift+G)", [this]() { ungroupSelection(); }, has_group};

        context_menu_.items.push_back(group_item);
        context_menu_.items.push_back(ungroup_item);
        context_menu_.items.push_back(MenuItem::Separator());

        // Edit operations
        MenuItem dup_item{"Duplicate (D)", [this]() { duplicateSelection(); }, has_selection};
        MenuItem del_item{"Delete", [this]() { deleteSelected(); }, has_selection};

        context_menu_.items.push_back(dup_item);
        context_menu_.items.push_back(del_item);

        context_menu_.show(sx, sy);
    }

    void drawContextMenuTVG() {
        if (!context_menu_.visible) return;

        float x = context_menu_.x;
        float y = context_menu_.y;
        float w = ContextMenu::MENU_WIDTH;
        float h = context_menu_.height();

        // Shadow
        flex_renderer_->fill_path(rect_path(x + 3, y + 3, w, h),
                                  flex::Paint::solid(rgb(0, 0, 0, 60)));

        // Background
        flex_renderer_->fill_path(rect_path(x, y, w, h),
                                  flex::Paint::solid(rgb(45, 45, 50, 245)));

        // Border
        flex_renderer_->stroke_path(rect_path(x, y, w, h),
                                    flex::Paint::solid(rgb(80, 80, 90)), 1.0f);

        // Items
        float cy = y + ContextMenu::PADDING;
        for (size_t i = 0; i < context_menu_.items.size(); ++i) {
            const auto& item = context_menu_.items[i];

            if (item.separator) {
                flex_renderer_->stroke_path(line_path(x + 8, cy + 4, x + w - 8, cy + 4),
                                            flex::Paint::solid(rgb(80, 80, 90)), 1.0f);
                cy += 8;
                continue;
            }

            float item_h = ContextMenu::ITEM_HEIGHT;

            // Highlight on hover
            if ((int)i == context_menu_.hover_index && item.enabled) {
                flex_renderer_->fill_path(rect_path(x + 2, cy, w - 4, item_h),
                                          flex::Paint::solid(rgb(0, 120, 215, 200)));
            }

            // Item text
            flex::Color text_color = item.enabled ? rgb(220, 220, 220) : rgb(100, 100, 100);
            flex_renderer_->draw_text(item.label, x + 12, cy + 16, "Segoe UI", 11.0f, false, text_color);

            cy += item_h;
        }
    }

    void drawContextMenu() {
        if (!context_menu_.visible) return;

        SDL_SetRenderDrawBlendMode(sdl_renderer_, SDL_BLENDMODE_BLEND);

        float x = context_menu_.x;
        float y = context_menu_.y;
        float w = ContextMenu::MENU_WIDTH;
        float h = context_menu_.height();

        // Shadow
        SDL_SetRenderDrawColor(sdl_renderer_, 0, 0, 0, 60);
        SDL_Rect shadow{(int)x + 3, (int)y + 3, (int)w, (int)h};
        SDL_RenderFillRect(sdl_renderer_, &shadow);

        // Background
        SDL_SetRenderDrawColor(sdl_renderer_, 45, 45, 50, 245);
        SDL_Rect bg{(int)x, (int)y, (int)w, (int)h};
        SDL_RenderFillRect(sdl_renderer_, &bg);

        // Border
        SDL_SetRenderDrawColor(sdl_renderer_, 80, 80, 90, 255);
        SDL_RenderDrawRect(sdl_renderer_, &bg);

        // Items
        float cy = y + ContextMenu::PADDING;
        for (size_t i = 0; i < context_menu_.items.size(); ++i) {
            const auto& item = context_menu_.items[i];

            if (item.separator) {
                SDL_SetRenderDrawColor(sdl_renderer_, 80, 80, 90, 255);
                SDL_RenderDrawLine(sdl_renderer_,
                    (int)x + 8, (int)cy + 4,
                    (int)(x + w - 8), (int)cy + 4);
                cy += 8;
                continue;
            }

            float item_h = ContextMenu::ITEM_HEIGHT;

            // Highlight on hover
            if ((int)i == context_menu_.hover_index && item.enabled) {
                SDL_SetRenderDrawColor(sdl_renderer_, 0, 120, 215, 200);
                SDL_Rect highlight{(int)x + 2, (int)cy, (int)w - 4, (int)item_h};
                SDL_RenderFillRect(sdl_renderer_, &highlight);
            }

            // Item text indicator (simple colored bar)
            if (item.enabled) {
                SDL_SetRenderDrawColor(sdl_renderer_, 220, 220, 220, 255);
            } else {
                SDL_SetRenderDrawColor(sdl_renderer_, 100, 100, 100, 255);
            }
            SDL_Rect text_indicator{(int)x + 10, (int)cy + 8, (int)(item.label.length() * 6), 8};
            SDL_RenderFillRect(sdl_renderer_, &text_indicator);

            cy += item_h;
        }
    }

    void drawToolbarTVG() {
        // Background
        flex_renderer_->fill_path(rect_path(0, 0, SCREEN_WIDTH, Toolbar::HEIGHT),
                                  flex::Paint::solid(rgb(35, 35, 40, 250)));

        // Bottom border
        flex_renderer_->stroke_path(line_path(0, Toolbar::HEIGHT - 1, SCREEN_WIDTH, Toolbar::HEIGHT - 1),
                                    flex::Paint::solid(rgb(60, 60, 70)), 1.0f);

        float by = (Toolbar::HEIGHT - Toolbar::BUTTON_SIZE) / 2;

        // Draw tool buttons
        float bx = Toolbar::PADDING;
        for (size_t i = 0; i < toolbar_.tools.size(); ++i) {
            const auto& btn = toolbar_.tools[i];
            bool is_active = (btn.tool == toolbar_.active_tool);
            bool is_hover = ((int)i == toolbar_.hover_tool);

            flex::Color btn_bg;
            if (is_active) {
                btn_bg = rgb(0, 120, 215);
            } else if (is_hover && btn.enabled) {
                btn_bg = rgb(60, 60, 70);
            } else {
                btn_bg = rgb(45, 45, 50);
            }

            std::string path = rect_path(bx, by, Toolbar::BUTTON_SIZE, Toolbar::BUTTON_SIZE);
            flex_renderer_->fill_path(path, flex::Paint::solid(btn_bg));

            flex::Color border_color = is_active ? rgb(0, 150, 255) : rgb(70, 70, 80);
            flex_renderer_->stroke_path(path, flex::Paint::solid(border_color), 1.0f);

            // Draw tool icon
            flex::Color icon_color = btn.enabled ? rgb(220, 220, 220) : rgb(100, 100, 100);
            drawToolIcon(btn.tool, bx, by, Toolbar::BUTTON_SIZE, icon_color);

            bx += Toolbar::BUTTON_SIZE + Toolbar::PADDING;
        }

        // Separator line
        float sep_x = toolbar_.tools_end_x() + Toolbar::SEPARATOR / 2;
        flex_renderer_->stroke_path(line_path(sep_x, by + 4, sep_x, by + Toolbar::BUTTON_SIZE - 4),
                                    flex::Paint::solid(rgb(60, 60, 70)), 1.0f);

        // Draw command buttons
        bx = toolbar_.commands_start_x();
        bool has_selection = !selected_nodes_.empty();

        for (size_t i = 0; i < toolbar_.commands.size(); ++i) {
            const auto& btn = toolbar_.commands[i];
            bool is_hover = ((int)i == toolbar_.hover_cmd);

            // Enable/disable based on selection state
            bool enabled = true;
            if (btn.cmd == CommandType::Copy || btn.cmd == CommandType::Delete ||
                btn.cmd == CommandType::Duplicate || btn.cmd == CommandType::BringFront ||
                btn.cmd == CommandType::SendBack) {
                enabled = has_selection;
            } else if (btn.cmd == CommandType::Paste) {
                enabled = !clipboard_.empty();
            } else if (btn.cmd == CommandType::Group) {
                enabled = selected_nodes_.size() > 1;
            } else if (btn.cmd == CommandType::Ungroup) {
                enabled = has_selection;
            }

            flex::Color btn_bg;
            if (is_hover && enabled) {
                btn_bg = rgb(60, 60, 70);
            } else {
                btn_bg = rgb(45, 45, 50);
            }

            std::string path = rect_path(bx, by, Toolbar::BUTTON_SIZE, Toolbar::BUTTON_SIZE);
            flex_renderer_->fill_path(path, flex::Paint::solid(btn_bg));
            flex_renderer_->stroke_path(path, flex::Paint::solid(rgb(70, 70, 80)), 1.0f);

            // Draw command icon
            flex::Color icon_color = enabled ? rgb(200, 200, 200) : rgb(80, 80, 80);
            drawCommandIcon(btn.cmd, bx, by, Toolbar::BUTTON_SIZE, icon_color);

            bx += Toolbar::BUTTON_SIZE + Toolbar::PADDING;
        }
    }

    // Draw tool icons using shapes
    void drawToolIcon(ToolType tool, float x, float y, float size, const flex::Color& color) {
        float cx = x + size / 2;
        float cy = y + size / 2;
        float s = size * 0.3f;  // Icon scale

        switch (tool) {
            case ToolType::Select: {
                // Arrow cursor
                std::string arrow = "M " + std::to_string(cx - s) + " " + std::to_string(cy - s) +
                                   " L " + std::to_string(cx + s*0.3f) + " " + std::to_string(cy + s*0.8f) +
                                   " L " + std::to_string(cx) + " " + std::to_string(cy + s*0.3f) +
                                   " L " + std::to_string(cx + s*0.8f) + " " + std::to_string(cy + s*0.3f) + " Z";
                flex_renderer_->fill_path(arrow, flex::Paint::solid(color));
                break;
            }
            case ToolType::Pan: {
                // Hand/move icon (cross arrows)
                flex_renderer_->stroke_path(line_path(cx - s, cy, cx + s, cy), flex::Paint::solid(color), 2.0f);
                flex_renderer_->stroke_path(line_path(cx, cy - s, cx, cy + s), flex::Paint::solid(color), 2.0f);
                // Arrow heads
                flex_renderer_->stroke_path(line_path(cx + s - 3, cy - 3, cx + s, cy), flex::Paint::solid(color), 2.0f);
                flex_renderer_->stroke_path(line_path(cx + s - 3, cy + 3, cx + s, cy), flex::Paint::solid(color), 2.0f);
                break;
            }
            case ToolType::Rectangle: {
                // Rectangle
                flex_renderer_->stroke_path(rect_path(cx - s, cy - s*0.7f, s*2, s*1.4f),
                                           flex::Paint::solid(color), 2.0f);
                break;
            }
            case ToolType::Circle: {
                // Circle
                std::string circle = "M " + std::to_string(cx + s) + " " + std::to_string(cy) +
                                    " A " + std::to_string(s) + " " + std::to_string(s) + " 0 1 1 " +
                                    std::to_string(cx - s) + " " + std::to_string(cy) +
                                    " A " + std::to_string(s) + " " + std::to_string(s) + " 0 1 1 " +
                                    std::to_string(cx + s) + " " + std::to_string(cy);
                flex_renderer_->stroke_path(circle, flex::Paint::solid(color), 2.0f);
                break;
            }
            case ToolType::Line: {
                // Diagonal line
                flex_renderer_->stroke_path(line_path(cx - s, cy + s, cx + s, cy - s),
                                           flex::Paint::solid(color), 2.0f);
                break;
            }
            case ToolType::Pen: {
                // Pen/pencil icon - wavy line
                std::string wavy = "M " + std::to_string(cx - s) + " " + std::to_string(cy) +
                                  " Q " + std::to_string(cx - s*0.5f) + " " + std::to_string(cy - s*0.5f) +
                                  " " + std::to_string(cx) + " " + std::to_string(cy) +
                                  " Q " + std::to_string(cx + s*0.5f) + " " + std::to_string(cy + s*0.5f) +
                                  " " + std::to_string(cx + s) + " " + std::to_string(cy);
                flex_renderer_->stroke_path(wavy, flex::Paint::solid(color), 2.0f);
                break;
            }
        }
    }

    // Draw command icons using shapes
    void drawCommandIcon(CommandType cmd, float x, float y, float size, const flex::Color& color) {
        float cx = x + size / 2;
        float cy = y + size / 2;
        float s = size * 0.25f;

        switch (cmd) {
            case CommandType::Copy: {
                // Two overlapping rectangles
                flex_renderer_->stroke_path(rect_path(cx - s*0.8f, cy - s, s*1.4f, s*1.4f),
                                           flex::Paint::solid(color), 1.5f);
                flex_renderer_->stroke_path(rect_path(cx - s*0.3f, cy - s*0.5f, s*1.4f, s*1.4f),
                                           flex::Paint::solid(color), 1.5f);
                break;
            }
            case CommandType::Paste: {
                // Clipboard
                flex_renderer_->stroke_path(rect_path(cx - s, cy - s*0.5f, s*2, s*2),
                                           flex::Paint::solid(color), 1.5f);
                flex_renderer_->fill_path(rect_path(cx - s*0.5f, cy - s, s, s*0.6f),
                                         flex::Paint::solid(color));
                break;
            }
            case CommandType::Delete: {
                // X mark
                flex_renderer_->stroke_path(line_path(cx - s, cy - s, cx + s, cy + s),
                                           flex::Paint::solid(color), 2.0f);
                flex_renderer_->stroke_path(line_path(cx + s, cy - s, cx - s, cy + s),
                                           flex::Paint::solid(color), 2.0f);
                break;
            }
            case CommandType::Duplicate: {
                // Plus sign
                flex_renderer_->stroke_path(line_path(cx - s, cy, cx + s, cy),
                                           flex::Paint::solid(color), 2.0f);
                flex_renderer_->stroke_path(line_path(cx, cy - s, cx, cy + s),
                                           flex::Paint::solid(color), 2.0f);
                break;
            }
            case CommandType::Group: {
                // Bracket [ ]
                std::string left = "M " + std::to_string(cx - s*0.3f) + " " + std::to_string(cy - s) +
                                  " L " + std::to_string(cx - s) + " " + std::to_string(cy - s) +
                                  " L " + std::to_string(cx - s) + " " + std::to_string(cy + s) +
                                  " L " + std::to_string(cx - s*0.3f) + " " + std::to_string(cy + s);
                std::string right = "M " + std::to_string(cx + s*0.3f) + " " + std::to_string(cy - s) +
                                   " L " + std::to_string(cx + s) + " " + std::to_string(cy - s) +
                                   " L " + std::to_string(cx + s) + " " + std::to_string(cy + s) +
                                   " L " + std::to_string(cx + s*0.3f) + " " + std::to_string(cy + s);
                flex_renderer_->stroke_path(left, flex::Paint::solid(color), 1.5f);
                flex_renderer_->stroke_path(right, flex::Paint::solid(color), 1.5f);
                break;
            }
            case CommandType::Ungroup: {
                // Exploding brackets
                std::string left = "M " + std::to_string(cx - s*0.5f) + " " + std::to_string(cy - s*0.8f) +
                                  " L " + std::to_string(cx - s*1.2f) + " " + std::to_string(cy - s*0.8f) +
                                  " L " + std::to_string(cx - s*1.2f) + " " + std::to_string(cy + s*0.8f) +
                                  " L " + std::to_string(cx - s*0.5f) + " " + std::to_string(cy + s*0.8f);
                std::string right = "M " + std::to_string(cx + s*0.5f) + " " + std::to_string(cy - s*0.8f) +
                                   " L " + std::to_string(cx + s*1.2f) + " " + std::to_string(cy - s*0.8f) +
                                   " L " + std::to_string(cx + s*1.2f) + " " + std::to_string(cy + s*0.8f) +
                                   " L " + std::to_string(cx + s*0.5f) + " " + std::to_string(cy + s*0.8f);
                flex_renderer_->stroke_path(left, flex::Paint::solid(color), 1.5f);
                flex_renderer_->stroke_path(right, flex::Paint::solid(color), 1.5f);
                break;
            }
            case CommandType::BringFront: {
                // Up arrow
                std::string arrow = "M " + std::to_string(cx) + " " + std::to_string(cy - s) +
                                   " L " + std::to_string(cx - s) + " " + std::to_string(cy + s*0.5f) +
                                   " L " + std::to_string(cx + s) + " " + std::to_string(cy + s*0.5f) + " Z";
                flex_renderer_->fill_path(arrow, flex::Paint::solid(color));
                break;
            }
            case CommandType::SendBack: {
                // Down arrow
                std::string arrow = "M " + std::to_string(cx) + " " + std::to_string(cy + s) +
                                   " L " + std::to_string(cx - s) + " " + std::to_string(cy - s*0.5f) +
                                   " L " + std::to_string(cx + s) + " " + std::to_string(cy - s*0.5f) + " Z";
                flex_renderer_->fill_path(arrow, flex::Paint::solid(color));
                break;
            }
        }
    }

    // Old SDL drawToolbar/drawPanel removed - using ThorVG versions above

    void drawPanelTVG() {
        if (!panel_.visible) return;

        float px = panel_.x(SCREEN_WIDTH);
        float py = Toolbar::HEIGHT;
        float pw = panel_.width;
        float ph = panel_.height(SCREEN_HEIGHT, Toolbar::HEIGHT);

        // Panel background
        flex_renderer_->fill_path(rect_path(px, py, pw, ph),
                                  flex::Paint::solid(rgb(35, 35, 40, 245)));

        // Left border
        flex_renderer_->stroke_path(line_path(px, py, px, py + ph),
                                    flex::Paint::solid(rgb(60, 60, 70)), 1.0f);

        // Panel header
        flex_renderer_->fill_path(rect_path(px, py, pw, Panel::HEADER_HEIGHT),
                                  flex::Paint::solid(rgb(45, 45, 50)));

        // Header text - show selection info
        std::string header_text = "Inspector";
        if (selected_nodes_.size() == 1) {
            header_text = selected_nodes_[0]->id().empty() ? "Shape" : selected_nodes_[0]->id();
        } else if (selected_nodes_.size() > 1) {
            header_text = std::to_string(selected_nodes_.size()) + " objects";
        }
        flex_renderer_->draw_text(header_text, px + 10, py + 18, "Segoe UI", 12.0f, true, rgb(180, 180, 180));

        float sy = py + Panel::HEADER_HEIGHT;
        size_t section_idx = 0;

        for (const auto& section : panel_.sections) {
            // Section header
            flex_renderer_->fill_path(rect_path(px, sy, pw, Panel::SECTION_HEADER),
                                      flex::Paint::solid(rgb(50, 50, 55)));

            // Collapse indicator (triangle)
            float tri_x = px + 8;
            float tri_y = sy + 8;
            std::string tri_path;
            if (section.collapsed) {
                tri_path = "M " + std::to_string(tri_x) + " " + std::to_string(tri_y) +
                          " L " + std::to_string(tri_x + 6) + " " + std::to_string(tri_y + 4) +
                          " L " + std::to_string(tri_x) + " " + std::to_string(tri_y + 8) + " Z";
            } else {
                tri_path = "M " + std::to_string(tri_x) + " " + std::to_string(tri_y) +
                          " L " + std::to_string(tri_x + 8) + " " + std::to_string(tri_y) +
                          " L " + std::to_string(tri_x + 4) + " " + std::to_string(tri_y + 6) + " Z";
            }
            flex_renderer_->fill_path(tri_path, flex::Paint::solid(rgb(150, 150, 150)));

            // Section title
            flex_renderer_->draw_text(section.title, px + 20, sy + 16, "Segoe UI", 11.0f, false, rgb(180, 180, 180));

            sy += Panel::SECTION_HEADER;

            // Section content
            if (!section.collapsed) {
                flex_renderer_->fill_path(rect_path(px + 4, sy, pw - 8, section.content_height),
                                          flex::Paint::solid(rgb(40, 40, 45)));

                float cy = sy + 8;  // Content y position

                if (section_idx == 0) {  // Transform section
                    drawTransformProperties(px + 8, cy, pw - 16);
                } else if (section_idx == 1) {  // Appearance section
                    drawAppearanceProperties(px + 8, cy, pw - 16);
                } else if (section_idx == 2) {  // Pen Settings section
                    drawPenSettings(px + 8, cy, pw - 16);
                } else if (section_idx == 3) {  // Layers section
                    drawLayersSection(px + 8, cy, pw - 16, section.content_height - 16);
                }

                sy += section.content_height;
            }
            section_idx++;
        }
    }

    void drawTransformProperties(float x, float y, float w) {
        if (selected_nodes_.empty()) {
            flex_renderer_->draw_text("No selection", x, y + 12, "Segoe UI", 10.0f, false, rgb(100, 100, 100));
            return;
        }

        auto* node = selected_nodes_[0];
        float row_h = Panel::ROW_HEIGHT;

        // X position
        flex_renderer_->draw_text("X", x, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));
        flex_renderer_->draw_text(std::to_string((int)node->x()), x + 30, y + 12, "Segoe UI", 10.0f, false, rgb(200, 200, 200));

        // Y position
        flex_renderer_->draw_text("Y", x + w/2, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));
        flex_renderer_->draw_text(std::to_string((int)node->y()), x + w/2 + 30, y + 12, "Segoe UI", 10.0f, false, rgb(200, 200, 200));

        y += row_h;

        // Width/Height (for shapes)
        auto* shape = dynamic_cast<flex::Shape*>(node);
        if (shape) {
            if (shape->geometry_type() == flex::GeometryType::Rect) {
                auto r = shape->rect();
                flex_renderer_->draw_text("W", x, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));
                flex_renderer_->draw_text(std::to_string((int)r.width), x + 30, y + 12, "Segoe UI", 10.0f, false, rgb(200, 200, 200));
                flex_renderer_->draw_text("H", x + w/2, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));
                flex_renderer_->draw_text(std::to_string((int)r.height), x + w/2 + 30, y + 12, "Segoe UI", 10.0f, false, rgb(200, 200, 200));
            } else if (shape->geometry_type() == flex::GeometryType::Circle) {
                auto c = shape->circle();
                flex_renderer_->draw_text("R", x, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));
                flex_renderer_->draw_text(std::to_string((int)c.radius), x + 30, y + 12, "Segoe UI", 10.0f, false, rgb(200, 200, 200));
            }
        }

        y += row_h;

        // Rotation
        flex_renderer_->draw_text("Rot", x, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));
        flex_renderer_->draw_text(std::to_string((int)node->rotation()) + "°", x + 30, y + 12, "Segoe UI", 10.0f, false, rgb(200, 200, 200));
    }

    void drawAppearanceProperties(float x, float y, float w) {
        if (selected_nodes_.empty()) {
            flex_renderer_->draw_text("No selection", x, y + 12, "Segoe UI", 10.0f, false, rgb(100, 100, 100));
            return;
        }

        auto* shape = dynamic_cast<flex::Shape*>(selected_nodes_[0]);
        if (!shape) return;

        float row_h = Panel::ROW_HEIGHT;

        // Fill color
        flex_renderer_->draw_text("Fill", x, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));
        if (shape->has_fill()) {
            auto f = shape->fill();
            // Draw color swatch
            flex_renderer_->fill_path(rect_path(x + 40, y + 2, 60, 16),
                                      flex::Paint::solid(f.color));
            flex_renderer_->stroke_path(rect_path(x + 40, y + 2, 60, 16),
                                        flex::Paint::solid(rgb(80, 80, 80)), 1.0f);
        }

        y += row_h;

        // Stroke
        flex_renderer_->draw_text("Stroke", x, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));
        if (shape->has_stroke()) {
            auto s = shape->stroke();
            flex_renderer_->fill_path(rect_path(x + 40, y + 2, 60, 16),
                                      flex::Paint::solid(s.color));
            flex_renderer_->stroke_path(rect_path(x + 40, y + 2, 60, 16),
                                        flex::Paint::solid(rgb(80, 80, 80)), 1.0f);
        }
    }

    void drawPenSettings(float x, float y, float w) {
        float row_h = Panel::ROW_HEIGHT;
        float swatch_size = 20.0f;
        float spacing = 4.0f;

        // Stroke color label
        flex_renderer_->draw_text("Stroke", x, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));

        // Color palette
        float cx = x + 50;
        for (int i = 0; i < PenSettings::NUM_COLORS; ++i) {
            const auto& col = pen_settings_.color_palette[i];
            bool is_selected = (col.r == pen_settings_.stroke_color.r &&
                               col.g == pen_settings_.stroke_color.g &&
                               col.b == pen_settings_.stroke_color.b);

            flex_renderer_->fill_path(rect_path(cx, y, swatch_size, swatch_size),
                                      flex::Paint::solid(col));
            if (is_selected) {
                flex_renderer_->stroke_path(rect_path(cx, y, swatch_size, swatch_size),
                                            flex::Paint::solid(rgb(255, 255, 255)), 2.0f);
            } else {
                flex_renderer_->stroke_path(rect_path(cx, y, swatch_size, swatch_size),
                                            flex::Paint::solid(rgb(60, 60, 60)), 1.0f);
            }
            cx += swatch_size + spacing;
            if (i == 3) {  // New row after 4 colors
                cx = x + 50;
                y += swatch_size + spacing;
            }
        }

        y += swatch_size + spacing + 8;

        // Stroke width
        flex_renderer_->draw_text("Width", x, y + 12, "Segoe UI", 10.0f, false, rgb(140, 140, 140));

        // Width buttons: 1, 2, 4, 8
        float widths[] = {1.0f, 2.0f, 4.0f, 8.0f};
        cx = x + 50;
        for (float ww : widths) {
            bool is_selected = (pen_settings_.stroke_width == ww);
            flex::Color btn_bg = is_selected ? rgb(0, 120, 215) : rgb(60, 60, 70);

            flex_renderer_->fill_path(rect_path(cx, y, 30, 20), flex::Paint::solid(btn_bg));
            flex_renderer_->stroke_path(rect_path(cx, y, 30, 20),
                                        flex::Paint::solid(rgb(80, 80, 80)), 1.0f);
            flex_renderer_->draw_text(std::to_string((int)ww), cx + 10, y + 14, "Segoe UI", 10.0f, false, rgb(200, 200, 200));
            cx += 34;
        }

        y += row_h + 4;

        // Fill toggle
        flex::Color fill_btn_bg = pen_settings_.use_fill ? rgb(0, 120, 215) : rgb(60, 60, 70);
        flex_renderer_->fill_path(rect_path(x, y, 60, 20), flex::Paint::solid(fill_btn_bg));
        flex_renderer_->stroke_path(rect_path(x, y, 60, 20), flex::Paint::solid(rgb(80, 80, 80)), 1.0f);
        flex_renderer_->draw_text("Fill", x + 20, y + 14, "Segoe UI", 10.0f, false, rgb(200, 200, 200));

        // Smooth toggle
        flex::Color smooth_btn_bg = pen_settings_.smooth_curves ? rgb(0, 120, 215) : rgb(60, 60, 70);
        flex_renderer_->fill_path(rect_path(x + 70, y, 70, 20), flex::Paint::solid(smooth_btn_bg));
        flex_renderer_->stroke_path(rect_path(x + 70, y, 70, 20), flex::Paint::solid(rgb(80, 80, 80)), 1.0f);
        flex_renderer_->draw_text("Smooth", x + 82, y + 14, "Segoe UI", 10.0f, false, rgb(200, 200, 200));
    }

    // Calculate pen settings section Y offset in panel
    float getPenSettingsSectionY() const {
        float y = Panel::HEADER_HEIGHT;
        for (size_t i = 0; i < 2; ++i) {  // Skip first 2 sections (Transform, Appearance)
            y += Panel::SECTION_HEADER;
            if (!panel_.sections[i].collapsed) {
                y += panel_.sections[i].content_height;
            }
        }
        y += Panel::SECTION_HEADER;  // Pen Settings header
        return y;
    }

    bool handlePenSettingsClick(float local_x, float local_y) {
        float section_y = getPenSettingsSectionY();
        float content_y = local_y - section_y;

        if (content_y < 0 || content_y > panel_.sections[2].content_height) {
            return false;  // Not in pen settings content area
        }

        float x = Panel::PADDING;
        float y = 8.0f;  // Starting y offset in content area
        float swatch_size = 20.0f;
        float spacing = 4.0f;

        // Check color palette (2 rows of 4)
        float color_start_x = x + 50;
        float color_start_y = y;
        for (int i = 0; i < PenSettings::NUM_COLORS; ++i) {
            int row = i / 4;
            int col = i % 4;
            float cx = color_start_x + col * (swatch_size + spacing);
            float cy = color_start_y + row * (swatch_size + spacing);

            if (local_x >= cx && local_x < cx + swatch_size &&
                content_y >= cy && content_y < cy + swatch_size) {
                pen_settings_.stroke_color = pen_settings_.color_palette[i];
                std::cout << "Pen color: " << i << "\n";
                return true;
            }
        }

        // Y offset for width buttons (after 2 rows of colors)
        float width_y = color_start_y + 2 * (swatch_size + spacing) + 8;

        // Check width buttons
        float widths[] = {1.0f, 2.0f, 4.0f, 8.0f};
        float wx = x + 50;
        for (int i = 0; i < 4; ++i) {
            if (local_x >= wx && local_x < wx + 30 &&
                content_y >= width_y && content_y < width_y + 20) {
                pen_settings_.stroke_width = widths[i];
                std::cout << "Pen width: " << widths[i] << "\n";
                return true;
            }
            wx += 34;
        }

        // Y offset for toggle buttons
        float toggle_y = width_y + Panel::ROW_HEIGHT + 4;

        // Check fill toggle
        if (local_x >= x && local_x < x + 60 &&
            content_y >= toggle_y && content_y < toggle_y + 20) {
            pen_settings_.use_fill = !pen_settings_.use_fill;
            std::cout << "Pen fill: " << (pen_settings_.use_fill ? "on" : "off") << "\n";
            return true;
        }

        // Check smooth toggle
        if (local_x >= x + 70 && local_x < x + 140 &&
            content_y >= toggle_y && content_y < toggle_y + 20) {
            pen_settings_.smooth_curves = !pen_settings_.smooth_curves;
            std::cout << "Pen smooth: " << (pen_settings_.smooth_curves ? "on" : "off") << "\n";
            return true;
        }

        return false;
    }

    void drawLayersSection(float x, float y, float w, float h) {
        auto* root = instance_->artboard()->root();
        float row_h = 20.0f;
        float cy = y;

        // Draw layers in reverse order (top to bottom = front to back)
        for (int i = static_cast<int>(root->child_count()) - 1; i >= 0 && cy < y + h - row_h; --i) {
            auto* node = root->child_at(i);
            bool is_selected = isSelected(node);

            // Highlight selected
            if (is_selected) {
                flex_renderer_->fill_path(rect_path(x - 4, cy, w + 8, row_h),
                                          flex::Paint::solid(rgb(0, 100, 180, 100)));
            }

            // Type icon
            std::string icon = "■";
            auto* shape = dynamic_cast<flex::Shape*>(node);
            if (shape) {
                if (shape->geometry_type() == flex::GeometryType::Circle) {
                    icon = "●";
                }
            }
            flex_renderer_->draw_text(icon, x, cy + 14, "Segoe UI", 10.0f, false, rgb(150, 150, 150));

            // Name
            std::string name = node->id().empty() ? "Shape" : node->id();
            flex::Color text_color = is_selected ? rgb(255, 255, 255) : rgb(180, 180, 180);
            flex_renderer_->draw_text(name, x + 16, cy + 14, "Segoe UI", 10.0f, false, text_color);

            cy += row_h;
        }
    }

    // ========================================================================
    // Actions
    // ========================================================================

    void executeCommand(CommandType cmd) {
        switch (cmd) {
            case CommandType::Copy:      copySelection(); break;
            case CommandType::Paste:     pasteClipboard(); break;
            case CommandType::Delete:    deleteSelected(); break;
            case CommandType::Duplicate: duplicateSelection(); break;
            case CommandType::Group:     groupSelection(); break;
            case CommandType::Ungroup:   ungroupSelection(); break;
            case CommandType::BringFront: bringToFront(); break;
            case CommandType::SendBack:  sendToBack(); break;
        }
    }

    void copySelection() {
        clipboard_.clear();
        for (auto* node : selected_nodes_) {
            auto* shape = dynamic_cast<flex::Shape*>(node);
            if (!shape) continue;

            ClipboardItem item;
            item.geo_type = shape->geometry_type();
            item.rotation = node->rotation();

            if (item.geo_type == flex::GeometryType::Rect) {
                auto r = shape->rect();
                item.width = r.width;
                item.height = r.height;
                item.corner_radius = r.corner_radius;
            } else if (item.geo_type == flex::GeometryType::Circle) {
                item.radius = shape->circle().radius;
            }

            if (shape->has_fill()) {
                item.fill_color = shape->fill().color;
            }
            if (shape->has_stroke()) {
                auto s = shape->stroke();
                item.stroke_color = s.color;
                item.stroke_width = s.width;
            }
            clipboard_.push_back(item);
        }
        std::cout << "Copied " << clipboard_.size() << " items\n";
    }

    void pasteClipboard() {
        if (clipboard_.empty()) return;

        auto* root = instance_->artboard()->root();
        std::vector<flex::Node*> new_nodes;

        float offset_x = 30, offset_y = 30;
        for (const auto& item : clipboard_) {
            auto shape = flex::Shape::create();
            shape->set_id("pasted_" + std::to_string(rand() % 10000));

            // Position at screen center with offset
            shape->set_position(
                SCREEN_WIDTH / 2 - camera_.x / camera_.zoom + offset_x,
                SCREEN_HEIGHT / 2 - camera_.y / camera_.zoom + offset_y
            );
            shape->set_rotation(item.rotation);

            if (item.geo_type == flex::GeometryType::Rect) {
                shape->set_rect(item.width, item.height, item.corner_radius);
            } else if (item.geo_type == flex::GeometryType::Circle) {
                shape->set_circle(item.radius);
            }

            shape->set_fill(item.fill_color);
            shape->set_stroke(item.stroke_color, item.stroke_width);

            root->add_child(shape);
            new_nodes.push_back(shape.get());
            offset_x += 20;
            offset_y += 20;
        }

        // Select pasted nodes
        selected_nodes_ = new_nodes;
        selected_set_.clear();
        for (auto* n : new_nodes) selected_set_.insert(n);

        std::cout << "Pasted " << new_nodes.size() << " items\n";
    }

    void bringToFront() {
        if (selected_nodes_.empty()) return;

        auto* root = instance_->artboard()->root();
        for (auto* node : selected_nodes_) {
            // Find shared_ptr in children
            for (const auto& child : root->children()) {
                if (child.get() == node) {
                    root->remove_child(node);
                    root->add_child(child);
                    break;
                }
            }
        }
        std::cout << "Brought " << selected_nodes_.size() << " nodes to front\n";
    }

    void sendToBack() {
        if (selected_nodes_.empty()) return;

        auto* root = instance_->artboard()->root();
        std::vector<flex::Node::Ptr> to_move;

        // Collect shared_ptrs
        for (auto* node : selected_nodes_) {
            for (const auto& child : root->children()) {
                if (child.get() == node) {
                    to_move.push_back(child);
                    break;
                }
            }
        }

        // Remove all and re-add at beginning (before other children)
        for (const auto& ptr : to_move) {
            root->remove_child(ptr.get());
        }

        // Get current children and re-add in order (selected first, then rest)
        auto remaining = root->children();
        root->clear_children();
        for (const auto& ptr : to_move) {
            root->add_child(ptr);
        }
        for (const auto& ptr : remaining) {
            root->add_child(ptr);
        }

        std::cout << "Sent " << selected_nodes_.size() << " nodes to back\n";
    }

    void rotateSelection(float degrees) {
        for (auto* node : selected_nodes_) {
            node->set_rotation(node->rotation() + degrees);
        }
        std::cout << "Rotated " << selected_nodes_.size() << " nodes by " << degrees << "°\n";
    }

    void groupSelection() {
        if (selected_nodes_.size() < 2) return;

        // Calculate bounding box
        float min_x = 1e9f, min_y = 1e9f;
        for (auto* node : selected_nodes_) {
            float bx, by, bw, bh;
            if (getShapeBounds(node, bx, by, bw, bh)) {
                min_x = std::min(min_x, bx);
                min_y = std::min(min_y, by);
            }
        }

        // Create group at min corner
        auto group = flex::Group::create();
        group->set_id("group" + std::to_string(rand() % 1000));
        group->set_position(min_x, min_y);

        auto* root = instance_->artboard()->root();

        // Collect shared_ptrs from root's children
        std::vector<flex::Node::Ptr> nodes_to_move;
        for (auto* sel_node : selected_nodes_) {
            for (const auto& child : root->children()) {
                if (child.get() == sel_node) {
                    nodes_to_move.push_back(child);
                    break;
                }
            }
        }

        // Move nodes into group, adjusting their positions
        for (const auto& node_ptr : nodes_to_move) {
            root->remove_child(node_ptr.get());
            node_ptr->set_position(node_ptr->x() - min_x, node_ptr->y() - min_y);
            group->add_child(node_ptr);
        }

        root->add_child(group);

        // Select the new group
        selected_nodes_.clear();
        selected_set_.clear();
        selectNode(group.get());

        std::cout << "Grouped " << group->child_count() << " nodes into " << group->id() << "\n";
    }

    void ungroupSelection() {
        auto* root = instance_->artboard()->root();
        std::vector<flex::Node*> new_selection;

        for (auto* node : selected_nodes_) {
            auto* group = dynamic_cast<flex::Group*>(node);
            if (!group || group == root) continue;

            float gx = group->x();
            float gy = group->y();

            // Collect children as shared_ptrs
            std::vector<flex::Node::Ptr> children_to_move;
            for (const auto& child : group->children()) {
                children_to_move.push_back(child);
            }

            // Move children to root, adjusting positions
            for (const auto& child_ptr : children_to_move) {
                group->remove_child(child_ptr.get());
                child_ptr->set_position(child_ptr->x() + gx, child_ptr->y() + gy);
                root->add_child(child_ptr);
                new_selection.push_back(child_ptr.get());
            }

            // Remove empty group
            root->remove_child(group);
            std::cout << "Ungrouped: " << group->id() << "\n";
        }

        // Select ungrouped children
        selected_nodes_ = new_selection;
        selected_set_.clear();
        for (auto* n : new_selection) {
            selected_set_.insert(n);
        }
    }

    void duplicateSelection() {
        if (selected_nodes_.empty()) return;

        auto* root = instance_->artboard()->root();
        std::vector<flex::Node*> new_nodes;

        for (auto* node : selected_nodes_) {
            auto* shape = dynamic_cast<flex::Shape*>(node);
            if (!shape) continue;

            auto clone = flex::Shape::create();
            clone->set_id(node->id() + "_copy");
            clone->set_position(node->x() + 20, node->y() + 20);
            clone->set_rotation(node->rotation());

            // Copy geometry
            if (shape->geometry_type() == flex::GeometryType::Rect) {
                auto r = shape->rect();
                clone->set_rect(r.width, r.height, r.corner_radius);
            } else if (shape->geometry_type() == flex::GeometryType::Circle) {
                clone->set_circle(shape->circle().radius);
            }

            // Copy paint
            if (shape->has_fill()) {
                clone->set_fill(shape->fill().color);
            }
            if (shape->has_stroke()) {
                auto s = shape->stroke();
                clone->set_stroke(s.color, s.width);
            }

            root->add_child(clone);
            new_nodes.push_back(clone.get());
        }

        // Select clones
        selected_nodes_ = new_nodes;
        selected_set_.clear();
        for (auto* n : new_nodes) {
            selected_set_.insert(n);
        }

        std::cout << "Duplicated " << new_nodes.size() << " nodes\n";
    }

    void cleanup() {
        flex_renderer_.reset();
        instance_.reset();
        flex::unload_font("Segoe UI");
        flex::shutdown();

        if (tvg_canvas_) {
            delete tvg_canvas_;
            tvg_canvas_ = nullptr;
        }
        tvg::Initializer::term();

        if (texture_) SDL_DestroyTexture(texture_);
        if (sdl_renderer_) SDL_DestroyRenderer(sdl_renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }
};

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    EditorDemo editor;

    const char* file = (argc > 1) ? argv[1] : nullptr;

    if (!editor.init(file)) {
        std::cerr << "Failed to initialize editor\n";
        return 1;
    }

    editor.run();
    return 0;
}
