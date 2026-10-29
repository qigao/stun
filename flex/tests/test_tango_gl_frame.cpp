#include <backends/tango_opengl/cell_presenter.h>
#include <tinytest.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace tp = flex::tango_opengl;

struct PixelRecorder final : flex::Renderer {
    int begins = 0;
    int ends = 0;
    int clears = 0;
    int rects = 0;
    int text_draws = 0;
    int measurements = 0;
    bool reject_metrics = false;
    std::vector<std::string> text;

    void begin_frame(float, float, float) override { ++begins; }
    void end_frame() override { ++ends; }
    void set_retained_mode(bool enabled) override {
        if (enabled) throw std::logic_error("retained mode unsupported");
    }
    void save() override {}
    void restore() override {}
    void reset() override {}
    void set_transform(const flex::Transform&) override {}
    void translate(float, float) override {}
    void rotate(float) override {}
    void scale(float, float) override {}
    void clip_rect(float, float, float, float) override {}
    void reset_clip() override {}
    void set_global_alpha(float) override {}
    void set_shadow(const flex::Shadow&) override {}
    void clear_shadow() override {}
    void set_blur(const flex::BlurFilter&) override {}
    void clear_blur() override {}
    void fill_path(const std::string&, const flex::Paint&) override {}
    void stroke_path(const std::string&, const flex::Paint&, float) override {}
    void draw_line(float, float, float, float, const flex::Paint&, float) override {}
    void draw_rect(float, float, float, float, float, const flex::Paint&,
                   const flex::Paint&, float) override { ++rects; }
    void draw_circle(float, float, float, const flex::Paint&,
                     const flex::Paint&, float) override {}
    void draw_ellipse(float, float, float, float, const flex::Paint&,
                      const flex::Paint&, float) override {}
    void draw_text(const std::string& value, float, float,
                   const std::string&, float, bool,
                   const flex::Color&) override {
        ++text_draws;
        text.push_back(value);
    }
    bool measure_text(const std::string& value, const std::string&, float,
                      bool, flex::TextMetrics& out) override {
        ++measurements;
        if (reject_metrics) return false;
        out.width = value.size() > 2 ? 15.0f : 7.0f;
        out.height = 12.0f;
        return true;
    }
    void draw_image(const std::string&, float, float, float, float) override {}
    void draw_svg(const std::string&, float, float, float, float) override {}
    void draw_svg_data(const std::string&, float, float, float, float) override {}
    void clear(const flex::Color&) override { ++clears; }
    flex::Bounds viewport() const override { return {0, 0, 32, 16}; }
    bool supports_retained_mode() const override { return false; }
    void remove_cached(flex::PaintHandle) override {}
    flex::PaintHandle push_rect(float, float, float, float, float,
                                const flex::Paint&, const flex::Paint&, float,
                                const flex::Transform&, float) override { return nullptr; }
    flex::PaintHandle push_circle(float, float, float,
                                  const flex::Paint&, const flex::Paint&, float,
                                  const flex::Transform&, float) override { return nullptr; }
    flex::PaintHandle push_ellipse(float, float, float, float,
                                   const flex::Paint&, const flex::Paint&, float,
                                   const flex::Transform&, float) override { return nullptr; }
    flex::PaintHandle push_polygon(int, float, const flex::Paint&,
                                   const flex::Paint&, float,
                                   const flex::Transform&, float) override { return nullptr; }
    flex::PaintHandle push_star(int, float, float, const flex::Paint&,
                                const flex::Paint&, float,
                                const flex::Transform&, float) override { return nullptr; }
    flex::PaintHandle push_path(const std::string&, const flex::Paint&,
                                const flex::Paint&, float,
                                const flex::Transform&, float) override { return nullptr; }
    void update_transform(flex::PaintHandle, const flex::Transform&) override {}
};

struct Snapshot {
    tui_buffer_t* buffer = nullptr;
    tui_grid_snapshot_t* frame = nullptr;
    Snapshot() {
        buffer = tui_buffer_create(4, 1);
        if (!buffer) throw std::bad_alloc();
        tui_buffer_set(buffer, 0, 0,
                       tui_cell_styled('A', TUI_YELLOW, TUI_BLUE,
                           TUI_STYLE_REVERSE | TUI_STYLE_UNDERLINE));
        if (!tui_buffer_set_grapheme(buffer, 1, 0, "\xE4\xB8\xAD", 3,
                                     TUI_WHITE, TUI_BLACK, TUI_STYLE_BOLD))
            throw std::runtime_error("Tango fixture failed to add wide glyph");
        tui_buffer_set(buffer, 3, 0, tui_cell('B', TUI_GREEN, TUI_BLACK));
        if (tui_grid_snapshot_capture(buffer, 5, 4, 2048, &frame) !=
            TUI_GRID_SNAPSHOT_OK)
            throw std::runtime_error("Tango fixture snapshot failed");
    }
    ~Snapshot() {
        tui_grid_snapshot_destroy(frame);
        tui_buffer_destroy(buffer);
    }
    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
};

tp::CellPixelMetrics metrics() {
    tp::CellPixelMetrics result;
    result.expected_grid_revision = 5;
    result.font_family = "tango-test-font";
    return result;
}
} // namespace

suite("Tango OpenGL cell-to-pixel staging on native Tango grid") {
    it("stages a full immutable cell frame and renders the explicit pixel plan") {
        Snapshot src;
        auto m = metrics();
        const auto plan = tp::prepare_cell_pixel_plan(src.frame, m);
        check_equal(plan.width(), 32.0f);
        check_equal(plan.height(), 16.0f);
        check_equal(plan.grid_revision(), 5u);
        check_equal(plan.backgrounds().size(), size_t{4});
        check_equal(plan.glyphs().size(), size_t{3});
        check_equal(plan.decorations().size(), size_t{1});
        check_equal(plan.glyphs()[1].max_width, 16.0f);
        check_equal(plan.glyphs()[1].utf8, std::string("\xE4\xB8\xAD", 3));
        // Reverse swaps the foreground/background instead of dropping style.
        check(plan.backgrounds()[0].color.r > 0.9f);
        PixelRecorder target;
        tp::paint_cell_pixel_plan(target, plan);
        check_equal(target.begins, 1);
        check_equal(target.ends, 1);
        check_equal(target.clears, 1);
        check_equal(target.rects, 5);
        check_equal(target.text_draws, 3);
        check_equal(target.measurements, 3);
        check_equal(target.text[1], std::string("\xE4\xB8\xAD", 3));
    }

    it("refuses stale, unsupported and overbudget frames without starting output") {
        Snapshot src;
        auto m = metrics();
        PixelRecorder target;
        m.expected_grid_revision = 6;
        check_throws_as(tp::prepare_cell_pixel_plan(src.frame, m), std::logic_error);
        m.expected_grid_revision = 5;
        m.max_cells = 3;
        check_throws_as(tp::prepare_cell_pixel_plan(src.frame, m), std::length_error);
        m.max_cells = 4;
        m.max_draw_ops = 7;
        check_throws_as(tp::prepare_cell_pixel_plan(src.frame, m), std::length_error);
        m.max_draw_ops = 100;
        m.max_text_bytes = 2;
        check_throws_as(tp::prepare_cell_pixel_plan(src.frame, m), std::length_error);
        m.max_text_bytes = 1000;
        m.cell_width = 0.0f;
        check_throws_as(tp::prepare_cell_pixel_plan(src.frame, m), std::invalid_argument);
        m.cell_width = 8.0f;

        src.buffer->cells[3].style = TUI_STYLE_ITALIC;
        tui_grid_snapshot_t* rejected = nullptr;
        check(tui_grid_snapshot_capture(src.buffer, 6, 4, 2048, &rejected) ==
              TUI_GRID_SNAPSHOT_OK);
        m.expected_grid_revision = 6;
        check_throws_as(tp::prepare_cell_pixel_plan(rejected, m), std::logic_error);
        tui_grid_snapshot_destroy(rejected);
        check_equal(target.begins, 0);
        check_equal(target.clears, 0);
        check_equal(target.text_draws, 0);
    }

    it("requires actual glyph metrics and does not begin a frame on font failure") {
        Snapshot src;
        const auto plan = tp::prepare_cell_pixel_plan(src.frame, metrics());
        PixelRecorder target;
        target.reject_metrics = true;
        check_throws_as(tp::paint_cell_pixel_plan(target, plan), std::logic_error);
        check_equal(target.begins, 0);
        check_equal(target.clears, 0);
        check_equal(target.text_draws, 0);
    }

    it("keeps the grid independent of source resize and enforces owner-thread presentation") {
        Snapshot src;
        PixelRecorder backend;
        tp::CellPixelPresenter presenter(backend);
        check(presenter.on_owner_thread());
        tui_buffer_resize(src.buffer, 2, 1);
        presenter.present(src.frame, metrics());
        check_equal(backend.begins, 1);
        std::thread foreign([&] {
            try {
                presenter.present(src.frame, metrics());
            } catch (const std::logic_error&) {
                return;
            }
            backend.begins = -99; // Only on unexpected cross-thread success.
        });
        foreign.join();
        check_equal(backend.begins, 1);
    }
}
