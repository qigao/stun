#pragma once

#include "flex/core.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace flex::test_support {

struct RecordedOp {
    std::string name;
    std::vector<float> args;
    std::string text;
    Transform transform{};
    float alpha = 1.0f;
    PaintHandle handle = nullptr;
};

class RecordingRenderer final : public Renderer {
public:
    struct State {
        Transform transform{};
        float alpha = 1.0f;
    };

    void begin_frame(float width, float height, float pixel_ratio) override {
        frame_width_ = width;
        frame_height_ = height;
        pixel_ratio_ = pixel_ratio;
        record("begin_frame", {width, height, pixel_ratio});
    }

    void end_frame() override {
        record("end_frame");
    }

    void set_retained_mode(bool enabled) override {
        retained_mode_ = enabled;
    }

    void save() override {
        stack_.push_back(state_);
        record("save");
    }

    void restore() override {
        if (!stack_.empty()) {
            state_ = stack_.back();
            stack_.pop_back();
        }
        record("restore");
    }

    void reset() override {
        stack_.clear();
        state_ = State{};
        record("reset");
    }

    void set_transform(const Transform& transform) override {
        state_.transform = transform;
        record("set_transform");
    }

    void translate(float x, float y) override {
        state_.transform.data[2] += x;
        state_.transform.data[5] += y;
        record("translate", {x, y});
    }

    void rotate(float degrees) override {
        if (degrees == 0.0f) {
            return;
        }
        float rad = deg_to_rad(degrees);
        float c = std::cos(rad);
        float s = std::sin(rad);
        float m00 = state_.transform.data[0];
        float m01 = state_.transform.data[1];
        float m10 = state_.transform.data[3];
        float m11 = state_.transform.data[4];
        state_.transform.data[0] = m00 * c + m01 * s;
        state_.transform.data[1] = -m00 * s + m01 * c;
        state_.transform.data[3] = m10 * c + m11 * s;
        state_.transform.data[4] = -m10 * s + m11 * c;
        record("rotate", {degrees});
    }

    void scale(float sx, float sy) override {
        state_.transform.data[0] *= sx;
        state_.transform.data[1] *= sy;
        state_.transform.data[3] *= sx;
        state_.transform.data[4] *= sy;
        record("scale", {sx, sy});
    }

    void clip_rect(float x, float y, float w, float h) override {
        record("clip_rect", {x, y, w, h});
    }

    void reset_clip() override {
        record("reset_clip");
    }

    void set_global_alpha(float alpha) override {
        float base_alpha = stack_.empty() ? 1.0f : stack_.back().alpha;
        state_.alpha = base_alpha * alpha;
        record("set_global_alpha", {alpha});
    }

    void set_shadow(const Shadow&) override {
        record("set_shadow");
    }

    void clear_shadow() override {
        record("clear_shadow");
    }

    void set_blur(const BlurFilter&) override {
        record("set_blur");
    }

    void clear_blur() override {
        record("clear_blur");
    }

    void fill_path(const std::string& d, const Paint&) override {
        record("fill_path", {}, d);
    }

    void stroke_path(const std::string& d, const Paint&, float width) override {
        record("stroke_path", {width}, d);
    }

    void draw_line(float x1, float y1, float x2, float y2, const Paint&, float width) override {
        Vec2 p1 = state_.transform * Vec2{x1, y1};
        Vec2 p2 = state_.transform * Vec2{x2, y2};
        record("draw_line", {p1.x, p1.y, p2.x, p2.y, width});
    }

    void draw_rect(float x, float y, float w, float h, float r,
                   const Paint&, const Paint&, float stroke_width) override {
        Vec2 origin = state_.transform * Vec2{x, y};
        record("draw_rect", {origin.x, origin.y, w, h, r, stroke_width});
    }

    void draw_circle(float cx, float cy, float r,
                     const Paint&, const Paint&, float stroke_width) override {
        Vec2 center = state_.transform * Vec2{cx, cy};
        record("draw_circle", {center.x, center.y, r, stroke_width});
    }

    void draw_ellipse(float cx, float cy, float rx, float ry,
                      const Paint&, const Paint&, float stroke_width) override {
        Vec2 center = state_.transform * Vec2{cx, cy};
        record("draw_ellipse", {center.x, center.y, rx, ry, stroke_width});
    }

    void draw_text(const std::string& text, float x, float y,
                   const std::string& font_family, float font_size,
                   bool bold, const Color&) override {
        Vec2 origin = state_.transform * Vec2{x, y};
        record("draw_text", {origin.x, origin.y, font_size, bold ? 1.0f : 0.0f}, text + "|" + font_family);
    }

    void draw_image(const std::string& src, float x, float y, float width, float height) override {
        Vec2 origin = state_.transform * Vec2{x, y};
        record("draw_image", {origin.x, origin.y, width, height}, src);
    }

    void draw_svg(const std::string& src, float x, float y, float width, float height) override {
        Vec2 origin = state_.transform * Vec2{x, y};
        record("draw_svg", {origin.x, origin.y, width, height}, src);
    }

    void draw_svg_data(const std::string& data, float x, float y, float width, float height) override {
        Vec2 origin = state_.transform * Vec2{x, y};
        record("draw_svg_data", {origin.x, origin.y, width, height}, data);
    }

    void clear(const Color&) override {
        record("clear");
    }

    Bounds viewport() const override {
        return viewport_;
    }

    bool supports_retained_mode() const override {
        return retained_mode_;
    }

    void remove_cached(PaintHandle paint) override {
        record("remove_cached", {}, {}, paint);
    }

    PaintHandle push_rect(float x, float y, float w, float h, float r,
                          const Paint&, const Paint&, float stroke_width,
                          const Transform& transform, float alpha) override {
        PaintHandle handle = next_handle();
        record_with_transform("push_rect", {x, y, w, h, r, stroke_width}, transform, alpha, handle);
        return handle;
    }

    PaintHandle push_circle(float cx, float cy, float r,
                            const Paint&, const Paint&, float stroke_width,
                            const Transform& transform, float alpha) override {
        PaintHandle handle = next_handle();
        record_with_transform("push_circle", {cx, cy, r, stroke_width}, transform, alpha, handle);
        return handle;
    }

    PaintHandle push_ellipse(float cx, float cy, float rx, float ry,
                             const Paint&, const Paint&, float stroke_width,
                             const Transform& transform, float alpha) override {
        PaintHandle handle = next_handle();
        record_with_transform("push_ellipse", {cx, cy, rx, ry, stroke_width}, transform, alpha, handle);
        return handle;
    }

    PaintHandle push_polygon(int sides, float radius,
                             const Paint&, const Paint&, float stroke_width,
                             const Transform& transform, float alpha) override {
        PaintHandle handle = next_handle();
        record_with_transform("push_polygon", {static_cast<float>(sides), radius, stroke_width},
                              transform, alpha, handle);
        return handle;
    }

    PaintHandle push_star(int points, float outer_radius, float inner_radius,
                          const Paint&, const Paint&, float stroke_width,
                          const Transform& transform, float alpha) override {
        PaintHandle handle = next_handle();
        record_with_transform("push_star",
                              {static_cast<float>(points), outer_radius, inner_radius, stroke_width},
                              transform, alpha, handle);
        return handle;
    }

    PaintHandle push_path(const std::string& d,
                          const Paint&, const Paint&, float stroke_width,
                          const Transform& transform, float alpha) override {
        PaintHandle handle = next_handle();
        record_with_transform("push_path", {stroke_width}, transform, alpha, handle, d);
        return handle;
    }

    void update_transform(PaintHandle paint, const Transform& transform) override {
        record_with_transform("update_transform", {}, transform, state_.alpha, paint);
    }

    void clear_ops() {
        ops_.clear();
    }

    size_t count(const char* name) const {
        size_t matches = 0;
        for (const auto& op : ops_) {
            if (op.name == name) {
                ++matches;
            }
        }
        return matches;
    }

    int first_index(const char* name) const {
        for (size_t i = 0; i < ops_.size(); ++i) {
            if (ops_[i].name == name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::vector<RecordedOp> find_all(const char* name) const {
        std::vector<RecordedOp> matches;
        for (const auto& op : ops_) {
            if (op.name == name) {
                matches.push_back(op);
            }
        }
        return matches;
    }

    const std::vector<RecordedOp>& ops() const {
        return ops_;
    }

private:
    PaintHandle next_handle() {
        return reinterpret_cast<PaintHandle>(static_cast<uintptr_t>(++handle_counter_));
    }

    void record(const std::string& name,
                std::vector<float> args = {},
                std::string text = {},
                PaintHandle handle = nullptr) {
        ops_.push_back(RecordedOp{name, std::move(args), std::move(text), state_.transform, state_.alpha, handle});
    }

    void record_with_transform(const std::string& name,
                               std::vector<float> args,
                               const Transform& transform,
                               float alpha,
                               PaintHandle handle,
                               std::string text = {}) {
        ops_.push_back(RecordedOp{name, std::move(args), std::move(text), transform, alpha, handle});
    }

    float frame_width_ = 0.0f;
    float frame_height_ = 0.0f;
    float pixel_ratio_ = 1.0f;
    Bounds viewport_{-10000.0f, -10000.0f, 20000.0f, 20000.0f};
    State state_{};
    std::vector<State> stack_;
    std::vector<RecordedOp> ops_;
    std::uintptr_t handle_counter_ = 0;
};

inline std::string format_trace_float(float value) {
    if (std::fabs(value) < 0.0005f) {
        value = 0.0f;
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    return buffer;
}

inline std::string stable_trace_entry(const RecordedOp& op) {
    auto append_alpha = [&](std::string base) {
        base += " alpha=";
        base += format_trace_float(op.alpha);
        return base;
    };

    if (op.name == "draw_rect" && op.args.size() >= 4) {
        std::string trace = "draw_rect ";
        trace += format_trace_float(op.args[0]);
        trace += " ";
        trace += format_trace_float(op.args[1]);
        trace += " ";
        trace += format_trace_float(op.args[2]);
        trace += " ";
        trace += format_trace_float(op.args[3]);
        return append_alpha(std::move(trace));
    }

    if (op.name == "draw_text" && op.args.size() >= 3) {
        std::string trace = "draw_text ";
        trace += format_trace_float(op.args[0]);
        trace += " ";
        trace += format_trace_float(op.args[1]);
        trace += " ";
        trace += format_trace_float(op.args[2]);
        trace = append_alpha(std::move(trace));
        trace += " text=";
        trace += op.text;
        return trace;
    }

    if ((op.name == "draw_svg" || op.name == "draw_svg_data") && op.args.size() >= 4) {
        std::string trace = op.name;
        trace += " ";
        trace += format_trace_float(op.args[0]);
        trace += " ";
        trace += format_trace_float(op.args[1]);
        trace += " ";
        trace += format_trace_float(op.args[2]);
        trace += " ";
        trace += format_trace_float(op.args[3]);
        trace = append_alpha(std::move(trace));
        trace += " svg=";
        trace += op.text;
        return trace;
    }

    if (op.name == "clip_rect" && op.args.size() >= 4) {
        Vec2 origin = op.transform * Vec2{op.args[0], op.args[1]};
        std::string trace = "clip_rect ";
        trace += format_trace_float(origin.x);
        trace += " ";
        trace += format_trace_float(origin.y);
        trace += " ";
        trace += format_trace_float(op.args[2]);
        trace += " ";
        trace += format_trace_float(op.args[3]);
        return trace;
    }

    if (op.name == "push_rect" && op.args.size() >= 4) {
        Vec2 origin = op.transform * Vec2{op.args[0], op.args[1]};
        std::string trace = "push_rect ";
        trace += format_trace_float(origin.x);
        trace += " ";
        trace += format_trace_float(origin.y);
        trace += " ";
        trace += format_trace_float(op.args[2]);
        trace += " ";
        trace += format_trace_float(op.args[3]);
        return append_alpha(std::move(trace));
    }

    if (op.name == "update_transform") {
        std::string trace = "update_transform ";
        trace += format_trace_float(tx(op.transform));
        trace += " ";
        trace += format_trace_float(ty(op.transform));
        return trace;
    }

    return {};
}

inline std::vector<std::string> stable_trace(const RecordingRenderer& renderer) {
    std::vector<std::string> trace;
    for (const auto& op : renderer.ops()) {
        std::string entry = stable_trace_entry(op);
        if (!entry.empty()) {
            trace.push_back(std::move(entry));
        }
    }
    return trace;
}

} // namespace flex::test_support
