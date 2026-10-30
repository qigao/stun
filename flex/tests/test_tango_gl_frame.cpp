#include <backends/tango_opengl/cell_presenter.h>
#include <backends/tango_opengl/cell_input.h>
#include <tinytest.hpp>

#include <limits>
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
    bool fail_draw = false;
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
        if (fail_draw) throw std::runtime_error("pixel test renderer rejected draw");
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


    it("hit-tests the same frozen cells painted by the pixel plan, normalizing CJK continuation") {
        Snapshot src;
        const auto plan = tp::prepare_cell_pixel_plan(src.frame, metrics());
        check_equal(plan.columns(), 4);
        check_equal(plan.rows(), 1);
        check_equal(plan.cell_width(), 8.0f);
        check_equal(plan.cell_height(), 16.0f);
        const auto first = plan.hit_test_logical(0.0, 0.0);
        check(first.has_value());
        if (first) {
            check_equal(first->column, 0);
            check_equal(first->leading_column, 0);
            check(!first->continuation);
            check_equal(first->grid_revision, 5u);
        }
        const auto lead = plan.hit_test_logical(8.0, 3.0);
        const auto continuation = plan.hit_test_logical(16.0, 3.0);
        check(lead.has_value());
        check(continuation.has_value());
        if (lead && continuation) {
            check_equal(lead->column, 1);
            check_equal(lead->leading_column, 1);
            check(!lead->continuation);
            check_equal(continuation->column, 2);
            check_equal(continuation->leading_column, 1);
            check(continuation->continuation);
            check_equal(continuation->grid_revision, lead->grid_revision);
        }
        const auto last = plan.hit_test_logical(24.0, 15.99);
        check(last.has_value());
        if (last) check_equal(last->column, 3);
        check(!plan.hit_test_logical(-0.00001, 0.0));
        check(!plan.hit_test_logical(0.0, -1.0));
        check(!plan.hit_test_logical(32.0, 0.0));
        check(!plan.hit_test_logical(0.0, 16.0));
        check(!plan.hit_test_logical(
            (std::numeric_limits<double>::quiet_NaN)(), 1.0));
        check(!plan.hit_test_logical(
            (std::numeric_limits<double>::infinity)(), 1.0));
        check(!plan.hit_test_framebuffer(32.0, 0.0));
        check(!plan.hit_test_framebuffer(0.0, 16.0));
        check(plan.hit_test_framebuffer(16.0, 3.0).has_value());
    }

    it("maps logical GLFW pointer positions through observed HiDPI framebuffer metrics") {
        Snapshot src;
        auto doubled = metrics();
        doubled.pixel_ratio = 2.0f;
        const auto plan = tp::prepare_cell_pixel_plan(src.frame, doubled);
        // Retina-style 32x16 logical window -> 64x32 real framebuffer.
        const auto hit = tp::hit_test_window_pointer(
            plan, 20.0, 4.0, 32, 16, 64, 32);
        check(hit.has_value());
        if (hit) {
            check_equal(hit->column, 2);
            check_equal(hit->leading_column, 1);
            check_equal(hit->row, 0);
        }
        // Window of 64 units with the *same* 64-pixel physical framebuffer.
        const auto rescaled = tp::hit_test_window_pointer(
            plan, 40.0, 8.0, 64, 32, 64, 32);
        check(rescaled.has_value());
        if (rescaled) {
            check_equal(rescaled->column, 2);
            check_equal(rescaled->leading_column, 1);
        }
        const auto outside = tp::hit_test_window_pointer(
            plan, 32.0, 0.0, 32, 16, 64, 32);
        check(!outside);
        check(!tp::hit_test_window_pointer(
            plan, (std::numeric_limits<double>::infinity)(),
            0.0, 32, 16, 64, 32));
        check_throws_as(tp::hit_test_window_pointer(
            plan, 20.0, 4.0, 0, 16, 64, 32), std::invalid_argument);
        check_throws_as(tp::hit_test_window_pointer(
            plan, 20.0, 4.0, 32, 16, 63, 32), std::logic_error);
        check_throws_as(tp::hit_test_window_pointer(
            plan, 20.0, 4.0, 32, 16, 64, 31), std::logic_error);
    }

    it("keeps the last submitted hit geometry on preflight refusal, but retires it after backend failure") {
        Snapshot src;
        PixelRecorder backend;
        tp::CellPixelPresenter presenter(backend);
        check(presenter.last_submitted_plan() == nullptr);
        presenter.present(src.frame, metrics());
        const auto actual_input = presenter.hit_test_window_pointer(
            16.5, 5.0, 32, 16, 32, 16);
        check(actual_input.has_value());
        if (actual_input) {
            check_equal(actual_input->column, 2);
            check_equal(actual_input->leading_column, 1);
        }
        const auto* first_plan = presenter.last_submitted_plan();
        check(first_plan != nullptr);
        if (first_plan) {
            const auto hit = first_plan->hit_test_logical(16.5, 5.0);
            check(hit.has_value());
            if (hit) check_equal(hit->leading_column, 1);
        }
        const int frames = backend.begins;
        auto stale = metrics();
        ++stale.expected_grid_revision;
        check_throws_as(presenter.present(src.frame, stale), std::logic_error);
        check_equal(backend.begins, frames);
        check(presenter.last_submitted_plan() == first_plan);
        check(presenter.hit_test_window_pointer(
            16.5, 5.0, 32, 16, 32, 16).has_value());

        backend.reject_metrics = true;
        check_throws_as(presenter.present(src.frame, metrics()), std::logic_error);
        check_equal(backend.begins, frames);
        check(presenter.last_submitted_plan() == first_plan);

        backend.reject_metrics = false;
        backend.fail_draw = true;
        check_throws_as(presenter.present(src.frame, metrics()), std::runtime_error);
        check_equal(backend.begins, frames + 1);
        check(presenter.last_submitted_plan() == nullptr);
        check(!presenter.hit_test_window_pointer(
            16.5, 5.0, 32, 16, 32, 16));
        backend.fail_draw = false;

        presenter.present(src.frame, metrics());
        check_equal(backend.begins, frames + 2);
        check(presenter.last_submitted_plan() != nullptr);

        bool foreign_rejected = false;
        std::thread foreign([&] {
            try {
                (void)presenter.last_submitted_plan();
            } catch (const std::logic_error&) {
                foreign_rejected = true;
            }
        });
        foreign.join();
        check(foreign_rejected);
        bool foreign_input_rejected = false;
        std::thread foreign_input([&] {
            try {
                (void)presenter.hit_test_window_pointer(
                    8.0, 5.0, 32, 16, 32, 16);
            } catch (const std::logic_error&) {
                foreign_input_rejected = true;
            }
        });
        foreign_input.join();
        check(foreign_input_rejected);
    }


    it("routes GLFW-sized pointer callbacks into canonical C11 Tango mouse events") {
        Snapshot src;
        PixelRecorder backend;
        tp::CellPixelPresenter presenter(backend);
        const tp::CellWindowExtents window{32, 16, 64, 32};
        check(!tp::cell_input_pointer(
            presenter, 20.0, 4.0, window, tp::CellMouseAction::Press,
            tp::CellMouseButton::Left, TUI_MOD_NONE));
        presenter.present(src.frame, metrics());
        // A submitted 1x plan must reject a 2x physical framebuffer.
        check_throws_as(tp::cell_input_pointer(
            presenter, 20.0, 4.0, window, tp::CellMouseAction::Press,
            tp::CellMouseButton::Left, TUI_MOD_NONE), std::logic_error);

        auto doubled = metrics();
        doubled.pixel_ratio = 2.0f;
        presenter.present(src.frame, doubled);
        const auto press = tp::cell_input_pointer(
            presenter, 20.0, 4.0, window, tp::CellMouseAction::Press,
            tp::CellMouseButton::Left, TUI_MOD_SHIFT);
        check(press.has_value());
        if (press) {
            check_equal(press->event.type, TUI_EVENT_MOUSE_PRESS);
            check_equal(press->event.key, TUI_KEY_MOUSE_LEFT);
            check_equal(press->event.x, 2);
            check_equal(press->event.y, 0);
            check_equal(press->event.mod, static_cast<std::uint8_t>(TUI_MOD_SHIFT));
            check_equal(press->leading_column, 1);
            check(press->continuation);
            check_equal(press->grid_revision, 5u);
        }
        const auto release = tp::cell_input_pointer(
            presenter, 20.0, 4.0, window, tp::CellMouseAction::Release,
            tp::CellMouseButton::Left, TUI_MOD_NONE);
        check(release.has_value());
        if (release) check_equal(release->event.type, TUI_EVENT_MOUSE_RELEASE);
        const auto wheel = tp::cell_input_pointer(
            presenter, 20.0, 4.0, window, tp::CellMouseAction::WheelDown,
            tp::CellMouseButton::None, TUI_MOD_CTRL);
        check(wheel.has_value());
        if (wheel) check_equal(wheel->event.key, TUI_KEY_MOUSE_WHEEL_DOWN);
        const auto move = tp::cell_input_pointer(
            presenter, 20.0, 4.0, window, tp::CellMouseAction::Move,
            tp::CellMouseButton::None, TUI_MOD_NONE);
        check(move.has_value());
        if (move) check_equal(move->event.type, TUI_EVENT_MOUSE_MOVE);
        check(!tp::cell_input_pointer(
            presenter, 32.0, 4.0, window, tp::CellMouseAction::Press,
            tp::CellMouseButton::Left, TUI_MOD_NONE));
        check_throws_as(tp::cell_input_pointer(
            presenter, 20.0, 4.0, window, tp::CellMouseAction::Press,
            tp::CellMouseButton::None, TUI_MOD_NONE), std::invalid_argument);
        check_throws_as(tp::cell_input_pointer(
            presenter, 20.0, 4.0, window, tp::CellMouseAction::Press,
            tp::CellMouseButton::Left, 0x80u), std::invalid_argument);
    }

    it("delivers special keys and committed Unicode once, on the displayed frame owner") {
        Snapshot src;
        PixelRecorder backend;
        tp::CellPixelPresenter presenter(backend);
        check(!tp::cell_input_key(presenter, TUI_KEY_ESCAPE, TUI_MOD_NONE));
        check(!tp::cell_input_character(presenter, 'A'));
        presenter.present(src.frame, metrics());

        const auto escape = tp::cell_input_key(
            presenter, TUI_KEY_ESCAPE, TUI_MOD_NONE);
        check(escape.has_value());
        if (escape) {
            check_equal(escape->event.key, TUI_KEY_ESCAPE);
            check_equal(escape->event.ch, 27u);
            check_equal(escape->grid_revision, 5u);
        }
        check(!tp::cell_input_key(presenter, static_cast<tui_key_t>('A'), TUI_MOD_NONE));
        const auto text = tp::cell_input_character(presenter, 'A');
        check(text.has_value());
        if (text) {
            check_equal(text->event.type, TUI_EVENT_KEY);
            check_equal(text->event.ch, static_cast<std::uint32_t>('A'));
            check_equal(text->event.key, static_cast<tui_key_t>('A'));
        }
        const auto cjk = tp::cell_input_character(presenter, 0x4e2du);
        check(cjk.has_value());
        if (cjk) {
            check_equal(cjk->event.ch, 0x4e2du);
            check_equal(cjk->event.key, TUI_KEY_NONE);
        }
        const auto emoji = tp::cell_input_character(presenter, 0x1f642u);
        check(emoji.has_value());
        check(!tp::cell_input_character(presenter, 0xd800u));
        check(!tp::cell_input_character(presenter, 0x110000u));
        check(!tp::cell_input_character(presenter, 0x81u));
        check(!tp::cell_input_character(presenter, 0u));
        check_throws_as(tp::cell_input_key(
            presenter, TUI_KEY_LEFT, 0x80u), std::invalid_argument);

        bool rejected = false;
        std::thread foreign([&] {
            try {
                (void)tp::cell_input_character(presenter, 'Z');
            } catch (const std::logic_error&) { rejected = true; }
        });
        foreign.join();
        check(rejected);
        backend.fail_draw = true;
        check_throws_as(presenter.present(src.frame, metrics()), std::runtime_error);
        check(!tp::cell_input_key(presenter, TUI_KEY_ESCAPE, TUI_MOD_NONE));
        check(!tp::cell_input_character(presenter, 'A'));
    }


    it("refits the same canonical C11 grid onto actual positive host resize and 2x/1.5x DPI metrics") {
        Snapshot src;
        const auto original = metrics();
        const tp::CellWindowExtents one{40, 20, 40, 20};
        const auto fitted = tp::fit_cell_metrics_to_window(src.frame, one, original);
        check_equal(fitted.cell_width, 10.0f);
        check_equal(fitted.cell_height, 20.0f);
        check_equal(fitted.pixel_ratio, 1.0f);
        check_equal(fitted.font_size, original.font_size * 1.25f);
        check_equal(fitted.baseline, original.baseline * 1.25f);
        const auto plan = tp::prepare_cell_pixel_plan(src.frame, fitted);
        check_equal(plan.columns(), 4);
        check_equal(plan.rows(), 1);
        check_equal(plan.grid_revision(), 5u);
        const auto hit = tp::hit_test_window_pointer(plan, 23.0, 4.0, 40, 20, 40, 20);
        check(hit.has_value());
        if (hit) {
            check_equal(hit->column, 2);
            check_equal(hit->leading_column, 1);
            check(hit->continuation);
        }
        check(!plan.hit_test_logical(40.0, 0.0));
        const auto last = plan.hit_test_logical(39.9, 19.9);
        check(last.has_value());
        if (last) check_equal(last->column, 3);

        const tp::CellWindowExtents retina{40, 20, 80, 40};
        const auto doubled = tp::fit_cell_metrics_to_window(src.frame, retina, original);
        check_equal(doubled.pixel_ratio, 2.0f);
        const auto retina_plan = tp::prepare_cell_pixel_plan(src.frame, doubled);
        const auto hi_hit = tp::hit_test_window_pointer(
            retina_plan, 23.0, 4.0, 40, 20, 80, 40);
        check(hi_hit.has_value());
        if (hi_hit) {
            check_equal(hi_hit->column, 2);
            check_equal(hi_hit->leading_column, 1);
        }
        const tp::CellWindowExtents fractional{40, 20, 60, 30};
        const auto one_and_half = tp::fit_cell_metrics_to_window(
            src.frame, fractional, original);
        check_equal(one_and_half.pixel_ratio, 1.5f);
        const auto fractional_plan = tp::prepare_cell_pixel_plan(src.frame, one_and_half);
        check(tp::hit_test_window_pointer(
            fractional_plan, 23.0, 4.0, 40, 20, 60, 30).has_value());

        auto bad = retina;
        bad.framebuffer_width = 81;
        check_throws_as(tp::fit_cell_metrics_to_window(
            src.frame, bad, original), std::logic_error);
        bad = retina; bad.window_width = 0;
        check_throws_as(tp::fit_cell_metrics_to_window(
            src.frame, bad, original), std::invalid_argument);
        auto stale = original;
        ++stale.expected_grid_revision;
        check_throws_as(tp::fit_cell_metrics_to_window(
            src.frame, retina, stale), std::logic_error);
        auto invalid_font = original;
        invalid_font.font_size = (std::numeric_limits<float>::infinity)();
        check_throws_as(tp::fit_cell_metrics_to_window(
            src.frame, retina, invalid_font), std::invalid_argument);
    }

    it("retires input permanently before orderly GL context owner destruction") {
        Snapshot src;
        PixelRecorder renderer;
        auto old_presenter = std::make_unique<tp::CellPixelPresenter>(renderer);
        auto old_gate = std::make_unique<tp::CellHostFrameGate>(*old_presenter);
        const tp::CellWindowExtents observed{32, 16, 32, 16};
        old_gate->set_focus(true);
        old_presenter->present(src.frame, metrics());
        old_gate->accept_presented_frame(observed);
        check(old_gate->key(TUI_KEY_LEFT, TUI_MOD_NONE, observed).has_value());
        const auto old_id = old_presenter->submission_id();
        check(old_id != 0u);

        bool foreign_rejected = false;
        std::thread foreign([&] {
            try {
                old_gate->retire_device();
            } catch (const std::logic_error&) {
                foreign_rejected = true;
            }
        });
        foreign.join();
        check(foreign_rejected);
        check(old_gate->key(TUI_KEY_LEFT, TUI_MOD_NONE, observed).has_value());

        // An old gate may remain allocated after the GPU owner is destroyed;
        // every operation must refuse without calling the freed presenter.
        old_gate->retire_device();
        old_gate->retire_device(); // Idempotent on the same owner.
        old_presenter.reset();
        old_gate->set_focus(true);
        old_gate->invalidate_geometry();
        check(!old_gate->key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        check(!old_gate->character('A', observed));
        check(!old_gate->pointer(
            16.5, 5.0, observed, tp::CellMouseAction::Press,
            tp::CellMouseButton::Left, TUI_MOD_NONE));
        check_throws_as(old_gate->accept_presented_frame(observed),
                        std::logic_error);

        // A *new* context/presenter may accept the SAME C11 source revision,
        // but only its own freshly drawn frame and its own gate qualify.
        tp::CellPixelPresenter replacement(renderer);
        tp::CellHostFrameGate new_gate(replacement);
        new_gate.set_focus(true);
        check(!new_gate.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        replacement.present(src.frame, metrics());
        new_gate.accept_presented_frame(observed);
        const auto new_key = new_gate.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed);
        check(new_key.has_value());
        if (new_key) check_equal(new_key->grid_revision, 5u);
        check(!old_gate->key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        new_gate.retire_device();
        check(!new_gate.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
    }

    it("retires stale host input on focus, metrics and independent native draw generations") {
        Snapshot src;
        PixelRecorder backend;
        tp::CellPixelPresenter presenter(backend);
        tp::CellHostFrameGate host(presenter);
        const tp::CellWindowExtents observed{32, 16, 32, 16};
        host.set_focus(true);
        check(!host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        check_throws_as(host.accept_presented_frame(observed), std::logic_error);
        presenter.present(src.frame, metrics());
        check(presenter.submission_id() != 0u);
        host.accept_presented_frame(observed);

        const auto press = host.pointer(
            16.5, 5.0, observed, tp::CellMouseAction::Press,
            tp::CellMouseButton::Left, TUI_MOD_NONE);
        check(press.has_value());
        if (press) {
            check_equal(press->event.type, TUI_EVENT_MOUSE_PRESS);
            check_equal(press->event.x, 2);
            check_equal(press->leading_column, 1);
            check_equal(press->grid_revision, 5u);
        }
        check(host.character('a', observed).has_value());
        const auto initial_id = presenter.submission_id();

        // A fresh draw with the SAME C11 grid revision cannot borrow a prior
        // host admission: only the just-submitted frame can be input-active.
        presenter.present(src.frame, metrics());
        check(presenter.submission_id() > initial_id);
        check(!host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        host.accept_presented_frame(observed);
        check(host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed).has_value());

        auto stale = metrics();
        ++stale.expected_grid_revision;
        const auto before_refusal = presenter.submission_id();
        check_throws_as(presenter.present(src.frame, stale), std::logic_error);
        check_equal(presenter.submission_id(), before_refusal);
        // Pre-submit refusal does not touch the last actually painted frame.
        check(host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed).has_value());

        host.set_focus(false);
        check(!host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        check(!host.character('a', observed));
        host.set_focus(true);
        check(!host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        check_throws_as(host.accept_presented_frame(observed), std::logic_error);
        presenter.present(src.frame, metrics());
        host.accept_presented_frame(observed);
        check(host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed).has_value());

        // Even logical-only host resize retires input if the framebuffer
        // dimensions happen not to change. Do not guess the same frame.
        auto resized = observed;
        ++resized.window_width;
        check(!host.key(TUI_KEY_LEFT, TUI_MOD_NONE, resized));
        check(!host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        check_throws_as(host.accept_presented_frame(observed), std::logic_error);
        presenter.present(src.frame, metrics());
        host.accept_presented_frame(observed);
        check(host.pointer(
            16.5, 5.0, observed, tp::CellMouseAction::Press,
            tp::CellMouseButton::Left, TUI_MOD_NONE).has_value());

        host.invalidate_geometry();
        check(!host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        check_throws_as(host.accept_presented_frame(observed), std::logic_error);
        presenter.present(src.frame, metrics());
        host.accept_presented_frame(observed);
        check(host.character(0x4e2du, observed).has_value());

        bool foreign_rejected = false;
        std::thread foreign([&] {
            try {
                host.set_focus(false);
            } catch (const std::logic_error&) {
                foreign_rejected = true;
            }
        });
        foreign.join();
        check(foreign_rejected);
        // Real draw failure is not an admissible frame; old input is retired.
        backend.fail_draw = true;
        check_throws_as(presenter.present(src.frame, metrics()), std::runtime_error);
        check_equal(presenter.submission_id(), 0u);
        check(!host.key(TUI_KEY_LEFT, TUI_MOD_NONE, observed));
        check(!host.pointer(
            16.5, 5.0, observed, tp::CellMouseAction::Press,
            tp::CellMouseButton::Left, TUI_MOD_NONE));
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
