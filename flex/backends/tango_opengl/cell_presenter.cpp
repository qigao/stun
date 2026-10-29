#include <backends/tango_opengl/cell_presenter.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace flex::tango_opengl {
namespace {

bool positive_finite(float value) noexcept {
    return std::isfinite(value) && value > 0.0f;
}

Color to_pixel_color(tui_color_t color, bool dim = false) noexcept {
    const float scale = dim ? (0.5f / 255.0f) : (1.0f / 255.0f);
    return {color.r * scale, color.g * scale, color.b * scale, 1.0f};
}

std::string encode_scalar_utf8(std::uint32_t scalar) {
    // A snapshot already passed the canonical C11 Tango Unicode policy.
    // This routine only ENCODEs its validated scalar, never parses text.
    std::string result;
    if (scalar <= 0x7f) {
        result.push_back(static_cast<char>(scalar));
    } else if (scalar <= 0x7ff) {
        result.push_back(static_cast<char>(0xc0u | (scalar >> 6)));
        result.push_back(static_cast<char>(0x80u | (scalar & 0x3fu)));
    } else if (scalar <= 0xffff) {
        result.push_back(static_cast<char>(0xe0u | (scalar >> 12)));
        result.push_back(static_cast<char>(0x80u | ((scalar >> 6) & 0x3fu)));
        result.push_back(static_cast<char>(0x80u | (scalar & 0x3fu)));
    } else {
        result.push_back(static_cast<char>(0xf0u | (scalar >> 18)));
        result.push_back(static_cast<char>(0x80u | ((scalar >> 12) & 0x3fu)));
        result.push_back(static_cast<char>(0x80u | ((scalar >> 6) & 0x3fu)));
        result.push_back(static_cast<char>(0x80u | (scalar & 0x3fu)));
    }
    return result;
}

void consume_op(std::size_t& current, std::size_t limit) {
    if (current >= limit)
        throw std::length_error("TangoOpenGL cell-pixel draw-op budget exceeded");
    ++current;
}

void consume_text(std::size_t& used, std::size_t added, std::size_t limit) {
    if (added > limit || used > limit - added)
        throw std::length_error("TangoOpenGL cell-pixel UTF-8 byte budget exceeded");
    used += added;
}

} // namespace

CellPixelPlan prepare_cell_pixel_plan(const tui_grid_snapshot_t* snapshot,
                                      const CellPixelMetrics& metrics) {
    if (!snapshot || !metrics.expected_grid_revision ||
        !positive_finite(metrics.cell_width) ||
        !positive_finite(metrics.cell_height) ||
        !positive_finite(metrics.font_size) ||
        !positive_finite(metrics.pixel_ratio) ||
        !std::isfinite(metrics.baseline) ||
        metrics.baseline <= 0.0f || metrics.baseline > metrics.cell_height ||
        metrics.font_family.empty() || metrics.max_cells == 0 ||
        metrics.max_draw_ops == 0 || metrics.max_text_bytes == 0)
        throw std::invalid_argument("TangoOpenGL requires a complete finite cell-pixel profile");

    if (tui_grid_snapshot_revision(snapshot) != metrics.expected_grid_revision)
        throw std::logic_error("TangoOpenGL input cell-grid revision is stale");

    const auto count = tui_grid_snapshot_count(snapshot);
    const int columns = tui_grid_snapshot_width(snapshot);
    const int rows = tui_grid_snapshot_height(snapshot);
    if (columns <= 0 || rows <= 0 ||
        count > metrics.max_cells ||
        static_cast<std::size_t>(columns) > metrics.max_cells ||
        static_cast<std::size_t>(rows) > metrics.max_cells)
        throw std::length_error("TangoOpenGL cell-grid dimensions exceed budget");
    if (static_cast<std::size_t>(columns) >
        (std::numeric_limits<std::size_t>::max)() /
            static_cast<std::size_t>(rows) ||
        count != static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows))
        throw std::invalid_argument("TangoOpenGL cell-grid geometry is inconsistent");

    const double width = static_cast<double>(columns) * metrics.cell_width;
    const double height = static_cast<double>(rows) * metrics.cell_height;
    const double max_dimension = static_cast<double>((std::numeric_limits<int>::max)());
    if (!std::isfinite(width) || !std::isfinite(height) ||
        width <= 0.0 || height <= 0.0 || width > max_dimension ||
        height > max_dimension || width * metrics.pixel_ratio > max_dimension ||
        height * metrics.pixel_ratio > max_dimension)
        throw std::length_error("TangoOpenGL framebuffer extent exceeds device coordinate domain");

    CellPixelPlan plan;
    plan.width_ = static_cast<float>(width);
    plan.height_ = static_cast<float>(height);
    plan.pixel_ratio_ = metrics.pixel_ratio;
    plan.font_family_ = metrics.font_family;
    plan.font_size_ = metrics.font_size;
    plan.grid_revision_ = metrics.expected_grid_revision;

    std::size_t draw_ops = 0;
    std::size_t text_bytes = 0;
    const tui_cell_t* const cells = tui_grid_snapshot_cells(snapshot);
    if (!cells) throw std::invalid_argument("TangoOpenGL grid has no owned cells");

    // Stage ALL operations before any native begin_frame/clear. The snapshot
    // already owns canonical wide-cell continuation and UTF-8 bytes.
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const tui_cell_t& cell =
                cells[static_cast<std::size_t>(y) * static_cast<std::size_t>(columns) +
                      static_cast<std::size_t>(x)];
            const std::uint8_t supported =
                TUI_STYLE_BOLD | TUI_STYLE_DIM | TUI_STYLE_UNDERLINE |
                TUI_STYLE_REVERSE | TUI_STYLE_STRIKE;
            if ((cell.style & static_cast<std::uint8_t>(~supported)) != 0u)
                throw std::logic_error("TangoOpenGL cell attribute lacks a qualified pixel implementation");
            // This M3 profile does not invent a terminal blink clock or italic
            // font, and does not silently discard those attributes.
            const bool reversed = (cell.style & TUI_STYLE_REVERSE) != 0u;
            const bool dimmed = (cell.style & TUI_STYLE_DIM) != 0u;
            const tui_color_t bg = reversed ? cell.fg : cell.bg;
            const tui_color_t fg = reversed ? cell.bg : cell.fg;
            const float px = x * metrics.cell_width;
            const float py = y * metrics.cell_height;

            consume_op(draw_ops, metrics.max_draw_ops);
            plan.backgrounds_.push_back(
                {px, py, metrics.cell_width, metrics.cell_height,
                 to_pixel_color(bg)});

            if (cell.span == TUI_CELL_SPAN_CONTINUATION) continue;

            const float glyph_width = cell.span == TUI_CELL_SPAN_WIDE
                ? metrics.cell_width * 2.0f : metrics.cell_width;
            const bool blank = cell.grapheme_len == 0 && cell.ch == ' ';
            if (!blank) {
                const auto utf8 = cell.grapheme_len
                    ? std::string(cell.grapheme, cell.grapheme_len)
                    : encode_scalar_utf8(cell.ch);
                consume_text(text_bytes, utf8.size(), metrics.max_text_bytes);
                consume_op(draw_ops, metrics.max_draw_ops);
                plan.glyphs_.push_back(
                    {px, py + metrics.baseline, glyph_width, metrics.cell_height,
                     to_pixel_color(fg, dimmed), utf8,
                     (cell.style & TUI_STYLE_BOLD) != 0u});
            }

            const float line_height = (std::max)(1.0f, metrics.cell_height * 0.07f);
            if (cell.style & TUI_STYLE_UNDERLINE) {
                consume_op(draw_ops, metrics.max_draw_ops);
                plan.decorations_.push_back(
                    {px, py + metrics.cell_height - line_height,
                     glyph_width, line_height, to_pixel_color(fg, dimmed)});
            }
            if (cell.style & TUI_STYLE_STRIKE) {
                consume_op(draw_ops, metrics.max_draw_ops);
                plan.decorations_.push_back(
                    {px, py + metrics.cell_height * 0.52f,
                     glyph_width, line_height, to_pixel_color(fg, dimmed)});
            }
        }
    }
    return plan;
}

void validate_cell_pixel_font(Renderer& renderer, const CellPixelPlan& plan) {
    for (const auto& glyph : plan.glyphs_) {
        TextMetrics measure{};
        if (!renderer.measure_text(glyph.utf8, plan.font_family_,
                                   plan.font_size_, glyph.bold, measure) ||
            !std::isfinite(measure.width) || !std::isfinite(measure.height) ||
            measure.width < 0.0f || measure.height < 0.0f ||
            measure.width > glyph.max_width + 0.001f ||
            measure.height > glyph.max_height + 0.001f)
            throw std::logic_error(
                "TangoOpenGL glyph has no qualified font/size within its cell span");
    }
}

void paint_cell_pixel_plan(Renderer& renderer, const CellPixelPlan& plan) {
    if (!positive_finite(plan.width_) || !positive_finite(plan.height_) ||
        !positive_finite(plan.pixel_ratio_) || !positive_finite(plan.font_size_) ||
        plan.font_family_.empty() || !plan.grid_revision_)
        throw std::invalid_argument("TangoOpenGL pixel plan must be prepared before painting");

    validate_cell_pixel_font(renderer, plan);
    renderer.begin_frame(plan.width_, plan.height_, plan.pixel_ratio_);
    renderer.clear({0.0f, 0.0f, 0.0f, 1.0f});
    for (const auto& rect : plan.backgrounds_)
        renderer.draw_rect(rect.x, rect.y, rect.width, rect.height, 0.0f,
                           Paint::solid(rect.color), Paint::none(), 0.0f);
    for (const auto& glyph : plan.glyphs_)
        renderer.draw_text(glyph.utf8, glyph.x, glyph.baseline,
                           plan.font_family_, plan.font_size_, glyph.bold, glyph.color);
    for (const auto& rect : plan.decorations_)
        renderer.draw_rect(rect.x, rect.y, rect.width, rect.height, 0.0f,
                           Paint::solid(rect.color), Paint::none(), 0.0f);
    renderer.end_frame();
}

CellPixelPresenter::CellPixelPresenter(Renderer& renderer) noexcept
    : renderer_(&renderer), owner_thread_(std::this_thread::get_id()) {}

CellPixelPresenter::CellPixelPresenter(std::unique_ptr<Renderer> renderer)
    : owned_(std::move(renderer)), renderer_(owned_.get()),
      owner_thread_(std::this_thread::get_id()) {
    if (!renderer_) throw std::invalid_argument("TangoOpenGL presenter requires a renderer");
}

bool CellPixelPresenter::on_owner_thread() const noexcept {
    return std::this_thread::get_id() == owner_thread_;
}

void CellPixelPresenter::present(const tui_grid_snapshot_t* snapshot,
                                 const CellPixelMetrics& metrics) {
    if (!on_owner_thread())
        throw std::logic_error("TangoOpenGL presenter accessed outside its GL owner thread");
    const auto plan = prepare_cell_pixel_plan(snapshot, metrics);
    paint_cell_pixel_plan(*renderer_, plan);
}

} // namespace flex::tango_opengl
