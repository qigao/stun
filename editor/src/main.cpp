/*
 * Flex Editor
 *
 * A vector graphics editor built on the flex engine.
 * Uses MVVM architecture for extensibility.
 */

#include <editor/editor.h>
#include <editor/platform/sdl_platform.h>
#include <editor/view/tvg_ui_layer.h>
#include <editor/view/property_inspector.h>
#include <flex/flex.h>
#include <flex/flex.h>
#include <thorvg.h>
#include <iostream>

class EditorApp {
public:
    bool init(editor::Platform* platform, int width, int height) {
        platform_ = platform;
        width_ = width;
        height_ = height;

        // Create window
        window_ = platform->createWindow("Flex Editor", width, height);
        if (!window_) return false;

        // Initialize ThorVG
        tvg::Initializer::init(0);

        // Load fonts for text rendering
        if (!flex::load_font("Arial", "C:/Windows/Fonts/arial.ttf")) {
            if (!flex::load_font("Arial", "C:/Windows/Fonts/segoeui.ttf")) {
                std::cerr << "Warning: Could not load system font for text rendering\n";
            }
        }

        // Create ThorVG canvas targeting the window's render buffer
        createCanvas();

        // Create the editor view model
        editor_ = std::make_unique<editor::EditorViewModel>();

        // Set up initial document
        setupTestDocument();

        // Set up UI
        setupUI();

        // Set up event handlers
        setupEventHandlers();

        return true;
    }

    void run() {
        platform_->run([this]() {
            render();
        });
    }

    void cleanup() {
        flex_renderer_.reset();
        if (tvg_canvas_) {
            delete tvg_canvas_;
            tvg_canvas_ = nullptr;
        }
        tvg::Initializer::term();
        window_.reset();
    }

private:
    void createCanvas() {
        auto* target = window_->renderTarget();
        tvg_canvas_ = tvg::SwCanvas::gen();
        tvg_canvas_->target(
            target->pixels(),
            target->stride(),
            target->width(),
            target->height(),
            tvg::ColorSpace::ARGB8888);

        flex_renderer_ = flex::create_thorvg_renderer(tvg_canvas_);
    }

    void setupTestDocument() {
        auto rect = editor_->createRect(100, 100, 200, 150, editor::Color::rgb(100, 150, 200));
        rect->setName("Blue Rectangle");
        editor_->addNode(rect);

        auto circle = editor_->createCircle(400, 200, 80, editor::Color::rgb(200, 100, 100));
        circle->setName("Red Circle");
        editor_->addNode(circle);

        auto rect2 = editor_->createRect(300, 300, 150, 100, editor::Color::rgb(100, 200, 100));
        rect2->setName("Green Rectangle");
        editor_->addNode(rect2);
    }

    void setupUI() {
        // Setup TVG UI Layer
        ui_layer_ = std::make_unique<editor::TvgUiLayer>(tvg_canvas_);
        ui_layer_->init(width_, height_);
        
        // Property Inspector
        property_inspector_ = std::make_shared<editor::PropertyInspector>(ui_layer_->box(), ui_layer_->root());
        
        // Legacy Color Picker (for other panels if needed, though Inspector has its own)
        color_picker_.onChange([this](const editor::Color& c) {
             // Logic for legacy picker if used by other tools
             auto selection = editor_->selection().primary();
             auto shape = std::dynamic_pointer_cast<editor::ShapeNode>(selection);
             if (shape && editing_fill_ && shape->hasFill()) {
                 // Convert editor::Color to flex::Paint color
                 // selection->setFill(c); // Need proper conversion helper or method
             }
        });
        
        color_picker_.onClose([this]() {
            color_picker_.setVisible(false);
        });

        // Set up layer panel
        layer_panel_.setWidth(180);
        layer_panel_.setDocument(editor_->document());
        layer_panel_.setPosition(0, 60);

        // Set up toolbar (left side, vertical)
        toolbar_.setViewModel(editor_.get());
        toolbar_.setupDefaultTools();
        toolbar_.setPosition(10, 70);
        toolbar_.onToolSelected([this](const std::string& toolId) {
            editor_->setTool(toolId);
        });

        // Set up history panel (bottom left)
        history_panel_.setViewModel(editor_.get());
        history_panel_.setPosition(10, height_ - 210);

        // Set up align panel (top, after toolbar hint)
        align_panel_.setViewModel(editor_.get());
        align_panel_.setPosition(350, 5);

        // Set up zoom panel (bottom right)
        zoom_panel_.setViewModel(editor_.get());
        zoom_panel_.setPosition(width_ - 200, height_ - 40);

        // Set up navigator panel (bottom right, above zoom)
        navigator_panel_.setViewModel(editor_.get());
        navigator_panel_.setPosition(width_ - 210, height_ - 200);

        // Set up grid panel
        grid_panel_.setGrid(&grid_);
        grid_panel_.setPosition(width_ - 210, 60);

        // Set up context menu
        context_menu_.setViewModel(editor_.get());
        context_menu_.onItemSelected([this](const std::string& itemId) {
            editor_->shortcuts().executeAction(itemId);
        });

        // Set up ruler/guide manager
        ruler_guide_.setViewModel(editor_.get());
    }

    void setupEventHandlers() {
        window_->onMouseDown([this](const editor::PlatformMouseEvent& e) {
            handleMouseDown(e);
        });

        window_->onMouseUp([this](const editor::PlatformMouseEvent& e) {
            handleMouseUp(e);
        });

        window_->onMouseMove([this](const editor::PlatformMouseEvent& e) {
            handleMouseMove(e);
        });

        window_->onMouseWheel([this](const editor::PlatformWheelEvent& e) {
            handleMouseWheel(e);
        });

        window_->onKeyDown([this](const editor::PlatformKeyEvent& e) {
            handleKeyDown(e);
        });

        window_->onResize([this](const editor::PlatformResizeEvent& e) {
            handleResize(e);
        });

        window_->onClose([this]() {
            platform_->quit();
        });

        // Subscribe to selection changes to update property panel
        editor_->selection().addObserver([this](editor::EventType type, void*) {
            if (type == editor::EventType::SelectionChanged) {
                updatePropertyPanel();
            }
        });
    }

    editor::MouseEvent makeMouseEvent(float x, float y, int button, bool shift, bool ctrl, bool alt) {
        editor::MouseEvent e;
        e.screenPosition = {x, y};
        e.position = editor_->camera().screenToWorld(e.screenPosition);
        e.button = button;
        e.shift = shift;
        e.ctrl = ctrl;
        e.alt = alt;
        return e;
    }

    void handleMouseDown(const editor::PlatformMouseEvent& e) {
        mouse_down_ = true;
        last_mouse_ = {e.x, e.y};

        // Right click - show context menu
        if (e.button == 2) {
            if (editor_->selection().isEmpty()) {
                context_menu_.buildForCanvas();
            } else {
                context_menu_.buildForSelection();
            }
            context_menu_.show(e.x, e.y);
            return;
        }

        // Check context menu first
        if (context_menu_.isVisible() && context_menu_.onMouseDown(e.x, e.y, e.button)) {
            return;
        }
        context_menu_.hide();

        // Check color picker
        if (color_picker_.visible() && color_picker_.onMouseDown(e.x, e.y, e.button)) {
            return;
        }

        // Check toolbar
        if (toolbar_.onMouseDown(e.x, e.y, e.button)) {
            return;
        }

        // Check zoom panel
        if (zoom_panel_.onMouseDown(e.x, e.y, e.button)) {
            return;
        }

        // Check navigator panel
        if (navigator_panel_.onMouseDown(e.x, e.y, e.button)) {
            return;
        }

        // Check history panel
        if (history_panel_.onMouseDown(e.x, e.y, e.button)) {
            return;
        }

        // Check align panel
        if (align_panel_.onMouseDown(e.x, e.y, e.button)) {
            return;
        }

        // Check grid panel
        if (grid_panel_.onMouseDown(e.x, e.y, e.button)) {
            return;
        }

        // Check layer panel
        if (layer_panel_.onMouseDown(e.x, e.y, e.button)) {
            return;
        }

        // Check TvgUiLayer
        if (ui_layer_->onMouseDown(e.x, e.y, e.button)) {
            return;
        }

        // Middle button for panning
        if (e.button == 1) {
            is_panning_ = true;
            return;
        }

        auto me = makeMouseEvent(e.x, e.y, e.button, e.shift, e.ctrl, e.alt);
        editor_->onMouseDown(me);
    }

    void handleMouseUp(const editor::PlatformMouseEvent& e) {
        mouse_down_ = false;

        if (color_picker_.visible()) {
            color_picker_.onMouseUp(e.x, e.y, e.button);
        }

        // Check UI panels
        toolbar_.onMouseUp(e.x, e.y, e.button);
        zoom_panel_.onMouseUp(e.x, e.y, e.button);
        navigator_panel_.onMouseUp(e.x, e.y, e.button);
        grid_panel_.onMouseUp(e.x, e.y, e.button);
        layer_panel_.onMouseUp(e.x, e.y, e.button);
        history_panel_.onMouseUp(e.x, e.y, e.button);
        align_panel_.onMouseUp(e.x, e.y, e.button);
        
        if (ui_layer_->onMouseUp(e.x, e.y, e.button)) {
            return; // Consumed
        }

        if (e.button == 1) {
            is_panning_ = false;
            return;
        }

        auto me = makeMouseEvent(e.x, e.y, e.button, e.shift, e.ctrl, e.alt);
        editor_->onMouseUp(me);
    }

    void handleMouseMove(const editor::PlatformMouseEvent& e) {
        // Handle camera panning first (middle mouse button)
        if (is_panning_) {
            float dx = e.x - last_mouse_.x;
            float dy = e.y - last_mouse_.y;
            editor_->camera().pan(dx, dy);
            last_mouse_ = {e.x, e.y};
            return;
        }

        if (color_picker_.visible() && color_picker_.onMouseMove(e.x, e.y)) {
            last_mouse_ = {e.x, e.y};
            return;
        }

        // Check UI panels for mouse move
        if (toolbar_.onMouseMove(e.x, e.y)) {
            last_mouse_ = {e.x, e.y};
            return;
        }

        if (zoom_panel_.onMouseMove(e.x, e.y)) {
            last_mouse_ = {e.x, e.y};
            return;
        }

        if (navigator_panel_.onMouseMove(e.x, e.y)) {
            last_mouse_ = {e.x, e.y};
            return;
        }

        if (history_panel_.onMouseMove(e.x, e.y)) {
            last_mouse_ = {e.x, e.y};
            return;
        }

        if (align_panel_.onMouseMove(e.x, e.y)) {
            last_mouse_ = {e.x, e.y};
            return;
        }

        if (grid_panel_.onMouseMove(e.x, e.y)) {
            last_mouse_ = {e.x, e.y};
            return;
        }

        if (layer_panel_.onMouseMove(e.x, e.y)) {
            last_mouse_ = {e.x, e.y};
            return;
        }

        if (ui_layer_->onMouseMove(e.x, e.y)) {
             last_mouse_ = {e.x, e.y};
             return;
        }

        auto me = makeMouseEvent(e.x, e.y, 0, e.shift, e.ctrl, e.alt);
        if (mouse_down_) {
            editor_->onMouseDrag(me);
        } else {
            editor_->onMouseMove(me);
        }

        last_mouse_ = {e.x, e.y};
    }

    void handleMouseWheel(const editor::PlatformWheelEvent& e) {
        float zoom = editor_->camera().zoom();
        float newZoom = zoom * (e.deltaY > 0 ? 1.1f : 0.9f);
        editor_->camera().zoomTo(newZoom, {e.x, e.y});
    }

    void handleKeyDown(const editor::PlatformKeyEvent& e) {
        // Close color picker on Escape
        if (e.key == 27 && color_picker_.visible()) {
            color_picker_.setVisible(false);
            return;
        }

        // File operations
        if (e.ctrl && !e.alt) {
            if (e.key == 's' || e.key == 'S') {
                saveDocument();
                return;
            }
            if (e.key == 'o' || e.key == 'O') {
                loadDocument();
                return;
            }
        }

        // Boolean operations (Ctrl+Alt combinations)
        if (e.ctrl && e.alt) {
            switch (e.key) {
                case 'u': case 'U': editor_->booleanUnion(); return;
                case 'i': case 'I': editor_->booleanIntersect(); return;
                case 's': case 'S': editor_->booleanSubtract(); return;
                case 'x': case 'X': editor_->booleanExclude(); return;
            }
        }

        // Tool shortcuts
        if (!e.ctrl && !e.alt) {
            switch (e.key) {
                case 'v': case 'V': editor_->setTool("select"); return;
                case 'h': case 'H': editor_->setTool("pan"); return;
                case 'r': case 'R': editor_->setTool("rectangle"); return;
                case 'e': case 'E': editor_->setTool("ellipse"); return;
                case 'l': case 'L': editor_->setTool("line"); return;
                case 'p': case 'P': editor_->setTool("pen"); return;
                case 't': case 'T': editor_->setTool("text"); return;
            }
        }

        editor::KeyEvent ke;
        ke.key = e.key;
        ke.shift = e.shift;
        ke.ctrl = e.ctrl;
        ke.alt = e.alt;
        editor_->onKeyDown(ke);
    }

    void handleResize(const editor::PlatformResizeEvent& e) {
        width_ = e.width;
        height_ = e.height;

        // Recreate canvas for new size
        if (tvg_canvas_) {
            delete tvg_canvas_;
        }
        createCanvas();

        // Reposition panels for new window size
        history_panel_.setPosition(10, height_ - 210);
        zoom_panel_.setPosition(width_ - 200, height_ - 40);
        navigator_panel_.setPosition(width_ - 210, height_ - 200);
        grid_panel_.setPosition(width_ - 210, 60);

        ui_layer_->resize(width_, height_);
    }

    void updatePropertyPanel() {
        if (editor_->selection().isEmpty()) {
            property_inspector_->setTarget(nullptr);
        } else {
            property_inspector_->setTarget(editor_->selection().primary());
        }
    }

    void saveDocument() {
        std::string filepath = "document.flexdoc";
        if (editor::DocumentSerializer::save(editor_->document(), filepath)) {
            printf("Document saved to: %s\n", filepath.c_str());
        } else {
            printf("Failed to save document\n");
        }
    }

    void loadDocument() {
        std::string filepath = "document.flexdoc";
        if (editor::DocumentSerializer::load(editor_->document(), filepath)) {
            printf("Document loaded from: %s\n", filepath.c_str());
            editor_->selection().clear();
            updatePropertyPanel();
        } else {
            printf("Failed to load document (file may not exist)\n");
        }
    }

    void render() {
        auto* target = window_->renderTarget();

        // Begin frame
        flex_renderer_->begin_frame(
            static_cast<float>(target->width()),
            static_cast<float>(target->height()),
            1.0f);
        flex_renderer_->clear(flex::Color{0.94f, 0.94f, 0.94f, 1.0f});

        // Apply camera transform
        flex_renderer_->save();
        flex_renderer_->translate(editor_->camera().panX(), editor_->camera().panY());
        flex_renderer_->scale(editor_->camera().zoom(), editor_->camera().zoom());

        // Draw grid in world space (follows camera transform)
        if (grid_.settings().visible) {
            editor::Rect visibleArea = {
                -editor_->camera().panX() / editor_->camera().zoom(),
                -editor_->camera().panY() / editor_->camera().zoom(),
                static_cast<float>(width_) / editor_->camera().zoom(),
                static_cast<float>(height_) / editor_->camera().zoom()
            };
            grid_.render(*flex_renderer_, visibleArea, editor_->camera().zoom());
        }

        // Render editor
        editor_->render(*flex_renderer_);

        flex_renderer_->restore();

        // Draw rulers (screen space)
        editor::Rect viewport = {0, 0, static_cast<float>(width_), static_cast<float>(height_)};
        ruler_guide_.render(*flex_renderer_, viewport, editor_->camera().zoom());

        // Draw UI overlay
        drawToolbarHint();
        toolbar_.render(*flex_renderer_);
        layer_panel_.render(*flex_renderer_);
        // property_panel_.render(*flex_renderer_); // Replaced
        history_panel_.render(*flex_renderer_);
        align_panel_.render(*flex_renderer_);
        zoom_panel_.render(*flex_renderer_);
        navigator_panel_.render(*flex_renderer_);
        grid_panel_.render(*flex_renderer_);
        color_picker_.render(*flex_renderer_);
        context_menu_.render(*flex_renderer_);
        
        // Render TVG UI Layer
        ui_layer_->render(tvg_canvas_);

        // End frame
        flex_renderer_->end_frame();

        // Push to ThorVG and present
        tvg_canvas_->draw();
        tvg_canvas_->sync();
        window_->present();
    }

    void drawToolbarHint() {
        auto* tool = editor_->currentTool();
        if (tool) {
            flex_renderer_->draw_text(
                std::string("Tool: ") + tool->name() + " | V=Select H=Pan R=Rect E=Ellipse L=Line P=Pen T=Text",
                10, 20, "Arial", 14, false, {0.3f, 0.3f, 0.3f, 1.0f});
            flex_renderer_->draw_text(
                "Ctrl+Z=Undo Ctrl+Y=Redo | Ctrl+S=Save Ctrl+O=Open | Ctrl+Alt+U/I/S/X=Boolean",
                10, 40, "Arial", 12, false, {0.5f, 0.5f, 0.5f, 1.0f});
        }
    }

    // Platform
    editor::Platform* platform_ = nullptr;
    std::unique_ptr<editor::Window> window_;
    int width_, height_;

    // Rendering
    tvg::SwCanvas* tvg_canvas_ = nullptr;
    std::unique_ptr<flex::Renderer> flex_renderer_;

    // Editor
    std::unique_ptr<editor::EditorViewModel> editor_;

    // Input state
    bool mouse_down_ = false;
    bool is_panning_ = false;
    editor::Point last_mouse_;

    // UI panels
    // editor::PropertyPanel property_panel_; // Removed
    std::unique_ptr<editor::TvgUiLayer> ui_layer_;
    std::shared_ptr<editor::PropertyInspector> property_inspector_;
    editor::ColorPicker color_picker_; // Legacy for other panels
    editor::LayerPanel layer_panel_;

    // New panels
    editor::Toolbar toolbar_;
    editor::HistoryPanel history_panel_;
    editor::AlignPanel align_panel_;
    editor::ZoomPanel zoom_panel_;
    editor::NavigatorPanel navigator_panel_;
    editor::GridPanel grid_panel_;
    editor::ContextMenu context_menu_;
    editor::RulerGuideManager ruler_guide_;
    editor::Grid grid_;

    bool editing_fill_ = false;
    bool show_rulers_ = true;
    bool show_grid_ = true;
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Create platform
    auto platform = editor::createPlatform();
    if (!platform->init()) {
        printf("Failed to initialize platform\n");
        return 1;
    }

    // Create and run app
    EditorApp app;
    if (!app.init(platform.get(), 1280, 720)) {
        platform->shutdown();
        return 1;
    }

    app.run();
    app.cleanup();
    platform->shutdown();

    return 0;
}
