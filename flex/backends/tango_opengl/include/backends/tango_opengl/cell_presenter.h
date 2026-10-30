/*
 * Tango OpenGL cell/pixel presenter - pixel frame planning.
 *
 * This C++17 adapter consumes ONLY an immutable Tango C11 cell snapshot.
 * It does not parse VT/Markdown or create a second Unicode/layout system.
 */
#pragma once

#include <flex/core/renderer.h>
#include <tui_grid_snapshot.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace flex::tango_opengl {

struct CellPixelMetrics {
    float cell_width = 8.0f;
    float cell_height = 16.0f;
    float baseline = 13.0f;
    float font_size = 14.0f;
    float pixel_ratio = 1.0f;
    std::string font_family = "tango-monospace";
    // The source/cell-grid generation is chosen and maintained by its UI owner.
    std::uint64_t expected_grid_revision = 0;
    std::size_t max_cells = 16384;
    std::size_t max_draw_ops = 65536;
    std::size_t max_text_bytes = 1024 * 1024;
};

struct PixelRect {
    float x = 0, y = 0, width = 0, height = 0;
    Color color{};
};

struct PixelGlyph {
    float x = 0, baseline = 0;
    float max_width = 0, max_height = 0;
    Color color{};
    std::string utf8;
    bool bold = false;
};

struct CellPixelHit final {
    int column = 0;         // Actual cell under the pointer.
    int row = 0;
    int leading_column = 0; // Leading cell for a wide grapheme.
    bool continuation = false;
    std::uint64_t grid_revision = 0;
};

// The same immutable plan used for GPU cell painting owns the hit geometry;
// no second layout computation or borrowed grid is allowed.
class CellPixelPlan final {
public:
    int columns() const noexcept { return columns_; }
    int rows() const noexcept { return rows_; }
    float cell_width() const noexcept { return cell_width_; }
    float cell_height() const noexcept { return cell_height_; }

    // Both coordinates are top-left-origin (matching Tango and GLFW).
    // Logical coordinates are the existing draw_rect/draw_text coordinate
    // space; framebuffer coordinates are device pixels. A boundary at the
    // exclusive right/bottom edge is NOT a cell hit.
    std::optional<CellPixelHit> hit_test_logical(
        double x, double y) const noexcept;
    std::optional<CellPixelHit> hit_test_framebuffer(
        double framebuffer_x, double framebuffer_y) const noexcept;
    float width() const noexcept { return width_; }
    float height() const noexcept { return height_; }
    float pixel_ratio() const noexcept { return pixel_ratio_; }
    float font_size() const noexcept { return font_size_; }
    const std::string& font_family() const noexcept { return font_family_; }
    std::uint64_t grid_revision() const noexcept { return grid_revision_; }
    const std::vector<PixelRect>& backgrounds() const noexcept { return backgrounds_; }
    const std::vector<PixelGlyph>& glyphs() const noexcept { return glyphs_; }
    const std::vector<PixelRect>& decorations() const noexcept { return decorations_; }

private:
    friend CellPixelPlan prepare_cell_pixel_plan(
        const tui_grid_snapshot_t*, const CellPixelMetrics&);
    friend void validate_cell_pixel_font(Renderer&, const CellPixelPlan&);
    friend void paint_cell_pixel_plan(Renderer&, const CellPixelPlan&);
    float width_ = 0, height_ = 0, pixel_ratio_ = 1;
    float font_size_ = 0;
    std::string font_family_;
    std::uint64_t grid_revision_ = 0;
    std::vector<PixelRect> backgrounds_;
    std::vector<PixelGlyph> glyphs_;
    std::vector<PixelRect> decorations_;
    // The source grid is no longer borrowed after plan construction. A
    // continuation always maps to its canonical wide glyph's leading cell.
    std::vector<int> leading_column_by_cell_;
    int columns_ = 0;
    int rows_ = 0;
    float cell_width_ = 0;
    float cell_height_ = 0;
    std::optional<CellPixelHit> hit_test_projected(
        double logical_x, double logical_y) const noexcept;
};

// Map host window logical points into the same device pixels actually painted
// by this plan. Reject stale/mismatched framebuffer sizing (including HiDPI)
// instead of inventing a guessed cursor-to-cell transform. The host must
// obtain window/framebuffer dimensions from ONE coherent resize generation.
std::optional<CellPixelHit> hit_test_window_pointer(
    const CellPixelPlan& plan, double window_x, double window_y,
    int window_width, int window_height,
    int framebuffer_width, int framebuffer_height);

// Stage a bounded copy of every draw operation, preserving the single Tango
// grid and grapheme/continuation policy. This is a CPU plan with no native
// handles, shader code, CSS, or GL calls. Unsupported attributes fail closed.
CellPixelPlan prepare_cell_pixel_plan(const tui_grid_snapshot_t* snapshot,
                                      const CellPixelMetrics& metrics);

// Measure ALL glyphs against the already selected pixel font before beginning
// the GPU frame. No alternate font, fallback renderer, or guessed width.
void validate_cell_pixel_font(Renderer& renderer, const CellPixelPlan& plan);

// Render an already-validated plan into the explicitly supplied pixel backend.
// If a native GPU driver fails *after* begin_frame, this method cannot undo
// external present; an owner-managed RenderPlan/fence/reaper remains required.
void paint_cell_pixel_plan(Renderer& renderer, const CellPixelPlan& plan);

// Owns or borrows the selected native pixel renderer; presentation is always
// confined to the creating thread. The GL context must be current for create,
// present and destroy, and the context host remains responsible for swap.
class CellPixelPresenter final {
public:
    explicit CellPixelPresenter(Renderer& renderer) noexcept;
    explicit CellPixelPresenter(std::unique_ptr<Renderer> renderer);
    CellPixelPresenter(const CellPixelPresenter&) = delete;
    CellPixelPresenter& operator=(const CellPixelPresenter&) = delete;
    void present(const tui_grid_snapshot_t* snapshot,
                 const CellPixelMetrics& metrics);
    bool on_owner_thread() const noexcept;
    // Generation of the last successfully completed native draw. Zero means
    // no submitted plan is available. This distinguishes two presents that
    // happen to share the same C11 source revision.
    std::uint64_t submission_id() const;
    // Exact renderer-owned GPU object identities, not estimated glyph
    // handles. Available only on the GL/UI owner for a submitted frame;
    // requires the native renderer's current context and a qualified
    // resource-inventory implementation.
    std::vector<NativeRendererResourceId> native_gpu_resource_ids() const;
    // Hit testing is available only for the last frame whose synchronous
    // end_frame() succeeded, and only on the GL owner. An invalid input
    // rejected before rendering preserves the prior submitted plan.
    // A failure during backend drawing invalidates it: painted GPU pixels
    // cannot be rolled back or trusted until the owner recovers.
    // Direct, owner-thread-only input path. It never returns a borrowed
    // plan pointer to input callbacks, and produces no hit after an
    // incomplete/failed GPU submission.
    std::optional<CellPixelHit> hit_test_window_pointer(
        double window_x, double window_y,
        int window_width, int window_height,
        int framebuffer_width, int framebuffer_height) const;
    // Diagnostic borrowed plan view: valid until the next present/reset
    // or presenter destruction, and only on its owner thread.
    const CellPixelPlan* last_submitted_plan() const;

private:
    std::unique_ptr<Renderer> owned_;
    Renderer* renderer_ = nullptr;
    std::thread::id owner_thread_;
    std::optional<CellPixelPlan> submitted_plan_;
    std::uint64_t submission_id_ = 0;
};

} // namespace flex::tango_opengl
