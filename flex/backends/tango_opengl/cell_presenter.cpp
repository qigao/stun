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

std::optional<CellPixelHit> CellPixelPlan::hit_test_projected(
    double logical_x, double logical_y) const noexcept {
    if (!std::isfinite(logical_x) || !std::isfinite(logical_y) ||
        logical_x < 0.0 || logical_y < 0.0 ||
        logical_x >= static_cast<double>(width_) ||
        logical_y >= static_cast<double>(height_) ||
        !positive_finite(cell_width_) || !positive_finite(cell_height_) ||
        columns_ <= 0 || rows_ <= 0) return std::nullopt;
    const double col = logical_x / static_cast<double>(cell_width_);
    const double row = logical_y / static_cast<double>(cell_height_);
    // Reject at the actual painted cell geometry rather than clamping into
    // another cell when float framebuffer metrics are inconsistent.
    if (col < 0.0 || row < 0.0 ||
        col >= static_cast<double>(columns_) ||
        row >= static_cast<double>(rows_)) return std::nullopt;
    const int column = static_cast<int>(col);
    const int cell_row = static_cast<int>(row);
    const std::size_t offset =
        static_cast<std::size_t>(cell_row) *
        static_cast<std::size_t>(columns_) +
        static_cast<std::size_t>(column);
    if (offset >= leading_column_by_cell_.size())
        return std::nullopt;
    const int lead = leading_column_by_cell_[offset];
    if (lead < 0 || lead > column || column - lead > 1)
        return std::nullopt;
    return CellPixelHit{
        column, cell_row, lead, column != lead, grid_revision_
    };
}

std::optional<CellPixelHit> CellPixelPlan::hit_test_logical(
    double x, double y) const noexcept {
    return hit_test_projected(x, y);
}

std::optional<CellPixelHit> CellPixelPlan::hit_test_framebuffer(
    double framebuffer_x, double framebuffer_y) const noexcept {
    if (!std::isfinite(framebuffer_x) || !std::isfinite(framebuffer_y) ||
        !positive_finite(pixel_ratio_)) return std::nullopt;
    return hit_test_projected(
        framebuffer_x / static_cast<double>(pixel_ratio_),
        framebuffer_y / static_cast<double>(pixel_ratio_));
}

std::optional<CellPixelHit> hit_test_window_pointer(
    const CellPixelPlan& plan, double window_x, double window_y,
    int window_width, int window_height,
    int framebuffer_width, int framebuffer_height) {
    if (window_width <= 0 || window_height <= 0 ||
        framebuffer_width <= 0 || framebuffer_height <= 0)
        throw std::invalid_argument(
            "TangoOpenGL window and framebuffer extents must be positive");

    const double expected_width =
        static_cast<double>(plan.width()) * plan.pixel_ratio();
    const double expected_height =
        static_cast<double>(plan.height()) * plan.pixel_ratio();
    if (!std::isfinite(expected_width) || !std::isfinite(expected_height) ||
        std::lround(expected_width) != framebuffer_width ||
        std::lround(expected_height) != framebuffer_height)
        throw std::logic_error(
            "TangoOpenGL pointer extents do not match the last painted cell frame");

    if (!std::isfinite(window_x) || !std::isfinite(window_y) ||
        window_x < 0.0 || window_y < 0.0 ||
        window_x >= window_width || window_y >= window_height)
        return std::nullopt;

    return plan.hit_test_framebuffer(
        window_x * (static_cast<double>(framebuffer_width) / window_width),
        window_y * (static_cast<double>(framebuffer_height) / window_height));
}

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
    plan.columns_ = columns;
    plan.rows_ = rows;
    plan.cell_width_ = metrics.cell_width;
    plan.cell_height_ = metrics.cell_height;
    plan.leading_column_by_cell_.resize(count);

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

            const std::size_t cell_index =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(columns) +
                static_cast<std::size_t>(x);
            plan.leading_column_by_cell_[cell_index] =
                cell.span == TUI_CELL_SPAN_CONTINUATION ? x - 1 : x;

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

std::uint64_t CellPixelPresenter::submission_id() const {
    if (!on_owner_thread())
        throw std::logic_error(
            "TangoOpenGL submission identity accessed off its GL owner");
    return submitted_plan_ ? submission_id_ : 0;
}

std::optional<CellPixelHit> CellPixelPresenter::hit_test_window_pointer(
    double window_x, double window_y,
    int window_width, int window_height,
    int framebuffer_width, int framebuffer_height) const {
    if (!on_owner_thread())
        throw std::logic_error(
            "TangoOpenGL pointer input accessed outside its GL owner thread");
    if (!submitted_plan_) return std::nullopt;
    return tango_opengl::hit_test_window_pointer(
        *submitted_plan_, window_x, window_y,
        window_width, window_height, framebuffer_width, framebuffer_height);
}

const CellPixelPlan* CellPixelPresenter::last_submitted_plan() const {
    if (!on_owner_thread())
        throw std::logic_error(
            "TangoOpenGL hit geometry accessed outside its GL owner thread");
    return submitted_plan_ ? &*submitted_plan_ : nullptr;
}

void CellPixelPresenter::present(const tui_grid_snapshot_t* snapshot,
                                 const CellPixelMetrics& metrics) {
    if (!on_owner_thread())
        throw std::logic_error(
            "TangoOpenGL presenter accessed outside its GL owner thread");
    if (submission_id_ == (std::numeric_limits<std::uint64_t>::max)())
        throw std::overflow_error(
            "TangoOpenGL submitted pixel frame generation exhausted");
    auto plan = prepare_cell_pixel_plan(snapshot, metrics);

    // Invalid font state is still a PRE-submit error: keep the previously
    // submitted frame/hit plan. Once actual backend drawing begins, any
    // error may have partially altered the external framebuffer, so never
    // retain old pointer-hit geometry as if the old image were guaranteed.
    validate_cell_pixel_font(*renderer_, plan);
    submitted_plan_.reset();
    paint_cell_pixel_plan(*renderer_, plan);
    submitted_plan_.emplace(std::move(plan));
    ++submission_id_;
}

} // namespace flex::tango_opengl
