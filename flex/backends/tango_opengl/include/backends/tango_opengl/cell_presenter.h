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

class CellPixelPlan final {
public:
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
};

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

private:
    std::unique_ptr<Renderer> owned_;
    Renderer* renderer_ = nullptr;
    std::thread::id owner_thread_;
};

} // namespace flex::tango_opengl
