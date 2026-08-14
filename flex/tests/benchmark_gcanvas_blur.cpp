#include "flex.h"
#include "flex/render/engines/gcanvas.h"

#include <gcanvas/window.hpp>
#include <path_tessellator.hpp>
#include <tinytest.h>

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 360;
constexpr std::size_t kWarmupFrames = 3;
constexpr std::size_t kTypicalSamples = 40;
constexpr std::size_t kStressSamples = 10;
constexpr std::size_t kPathOnlySamples = 40;
constexpr std::size_t kTypicalShapes = 16;
constexpr std::size_t kStressShapes = 96;
constexpr std::size_t kShadowShapes = 4;
constexpr float kTypicalBlurRadius = 6.0f;
constexpr float kStressBlurRadius = 12.0f;
constexpr std::size_t kPathBenchmarkSamples = 10000;
constexpr std::size_t kPathQuadLimit = 262144;
constexpr std::size_t kPaintTextureSize = 256;

const std::string kArbitraryPath =
    "M 24 24 C 36 4 64 4 76 24 C 92 50 68 76 42 72 "
    "C 18 68 4 48 24 24 Z M 36 30 L 62 30 L 58 56 L 40 56 Z";

const char* backend_name(gcanvas::Backend backend) {
  return backend == gcanvas::Backend::OpenGL ? "OpenGL" : "Vulkan";
}

enum class ShapeWorkload {
  Mixed,
  PathOnly,
};

enum class PathShadowWorkload {
  ContractedOuter,
  ContractedInset,
};

void draw_arbitrary_path(flex::Renderer& renderer, float x, float y,
                         const flex::Paint& fill) {
  renderer.save();
  renderer.translate(x, y);
  renderer.scale(0.55f, 0.55f);
  renderer.fill_path(kArbitraryPath, fill);
  renderer.restore();
}

void run_path_pipeline_benchmark() {
  const gcanvas::Path parsed = gcanvas::Path::from_svg(kArbitraryPath);
  const gcanvas::Paint paint =
      gcanvas::Paint::solid(gcanvas::color(56, 148, 245, 217));
  const gcanvas::Transform transform;
  const auto reference = gcanvas::detail::tessellate_path(
      parsed, paint, transform, 0.0f, kPathQuadLimit, kPaintTextureSize);
  const auto stroke_reference = gcanvas::detail::tessellate_path(
      parsed, paint, transform, 2.0f, kPathQuadLimit, kPaintTextureSize);
  check_not_empty(reference.mask_quads);
  check_not_empty(stroke_reference.mask_quads);

  std::size_t parsed_commands = 0;
  benchmark_batch("SVG path parse", kPathBenchmarkSamples) {
    const gcanvas::Path path = gcanvas::Path::from_svg(kArbitraryPath);
    parsed_commands += path.commands().size();
  }
  check_size_eq(parsed_commands,
                kPathBenchmarkSamples * parsed.commands().size());

  std::size_t generated_quads = 0;
  benchmark_batch("solid path tessellation", kPathBenchmarkSamples) {
    const auto geometry = gcanvas::detail::tessellate_path(
        parsed, paint, transform, 0.0f, kPathQuadLimit, kPaintTextureSize);
    generated_quads += geometry.mask_quads.size();
  }
  check_size_eq(generated_quads,
                kPathBenchmarkSamples * reference.mask_quads.size());

  std::size_t generated_stroke_quads = 0;
  benchmark_batch("solid path stroke tessellation", kPathBenchmarkSamples) {
    const auto geometry = gcanvas::detail::tessellate_path(
        parsed, paint, transform, 2.0f, kPathQuadLimit, kPaintTextureSize);
    generated_stroke_quads += geometry.mask_quads.size();
  }
  check_size_eq(generated_stroke_quads,
                kPathBenchmarkSamples * stroke_reference.mask_quads.size());
}

void record_blurred_frame(flex::Renderer& renderer, std::size_t shape_count,
                          float blur_radius, ShapeWorkload workload) {
  const flex::Paint fill = flex::Paint::solid(
      flex::Color{0.22f, 0.58f, 0.96f, 0.85f});
  renderer.begin_frame(static_cast<float>(kCanvasWidth),
                       static_cast<float>(kCanvasHeight), 1.0f);
  renderer.clear(flex::Color{0.01f, 0.015f, 0.025f, 1.0f});
  renderer.set_blur(flex::BlurFilter{blur_radius});

  for (std::size_t index = 0; index < shape_count; ++index) {
    const float x = 12.0f + static_cast<float>((index * 37U) % 560U);
    const float y = 12.0f + static_cast<float>((index * 53U) % 280U);
    if (workload == ShapeWorkload::PathOnly) {
      draw_arbitrary_path(renderer, x, y, fill);
      continue;
    }
    switch (index % 3U) {
    case 0:
      renderer.draw_rect(x, y, 42.0f, 28.0f, 5.0f, fill,
                         flex::Paint::none(), 0.0f);
      break;
    case 1:
      renderer.draw_ellipse(x + 20.0f, y + 14.0f, 20.0f, 14.0f, fill,
                            flex::Paint::none(), 0.0f);
      break;
    default:
      draw_arbitrary_path(renderer, x, y, fill);
      break;
    }
  }

  renderer.clear_blur();
}

void record_path_shadow_frame(flex::Renderer& renderer, std::size_t shape_count,
                              PathShadowWorkload workload) {
  const flex::Paint fill = flex::Paint::solid(flex::Color::White);
  flex::Shadow shadow = workload == PathShadowWorkload::ContractedOuter
                            ? flex::Shadow::drop(
                                  3.0f, 2.0f, kTypicalBlurRadius,
                                  flex::Color{0.22f, 0.58f, 0.96f, 0.85f})
                            : flex::Shadow::inner(
                                  3.0f, 2.0f, kTypicalBlurRadius,
                                  flex::Color{0.22f, 0.58f, 0.96f, 0.85f});
  shadow.spread = -2.0f;

  renderer.begin_frame(static_cast<float>(kCanvasWidth),
                       static_cast<float>(kCanvasHeight), 1.0f);
  renderer.clear(flex::Color{0.01f, 0.015f, 0.025f, 1.0f});
  renderer.set_shadow(shadow);
  for (std::size_t index = 0; index < shape_count; ++index) {
    const float x = 12.0f + static_cast<float>((index * 37U) % 560U);
    const float y = 12.0f + static_cast<float>((index * 53U) % 280U);
    draw_arbitrary_path(renderer, x, y, fill);
  }
  renderer.clear_shadow();
}

void render_blurred_frame(gcanvas::Context& context, flex::Renderer& renderer,
                          std::size_t shape_count, float blur_radius,
                          ShapeWorkload workload) {
  record_blurred_frame(renderer, shape_count, blur_radius, workload);
  renderer.end_frame();
  context.present_frame();
}

struct SegmentTimings {
  double record_us = 0.0;
  double submit_us = 0.0;
  double present_us = 0.0;
};

template <typename Clock>
double elapsed_us(typename Clock::time_point begin,
                  typename Clock::time_point end) {
  return std::chrono::duration<double, std::micro>(end - begin).count();
}

void render_timed_blurred_frame(gcanvas::Context& context, flex::Renderer& renderer,
                                std::size_t shape_count, float blur_radius,
                                ShapeWorkload workload, SegmentTimings& timings) {
  using Clock = std::chrono::steady_clock;
  const auto record_begin = Clock::now();
  record_blurred_frame(renderer, shape_count, blur_radius, workload);
  const auto submit_begin = Clock::now();
  renderer.end_frame();
  const auto present_begin = Clock::now();
  context.present_frame();
  const auto frame_end = Clock::now();

  timings.record_us += elapsed_us<Clock>(record_begin, submit_begin);
  timings.submit_us += elapsed_us<Clock>(submit_begin, present_begin);
  timings.present_us += elapsed_us<Clock>(present_begin, frame_end);
}

void render_timed_path_shadow_frame(gcanvas::Context& context,
                                    flex::Renderer& renderer,
                                    std::size_t shape_count,
                                    PathShadowWorkload workload,
                                    SegmentTimings& timings) {
  using Clock = std::chrono::steady_clock;
  const auto record_begin = Clock::now();
  record_path_shadow_frame(renderer, shape_count, workload);
  const auto submit_begin = Clock::now();
  renderer.end_frame();
  const auto present_begin = Clock::now();
  context.present_frame();
  const auto frame_end = Clock::now();

  timings.record_us += elapsed_us<Clock>(record_begin, submit_begin);
  timings.submit_us += elapsed_us<Clock>(submit_begin, present_begin);
  timings.present_us += elapsed_us<Clock>(present_begin, frame_end);
}

void print_segment_timings(const char* backend, const char* workload,
                           const SegmentTimings& timings, std::size_t samples,
                           std::size_t shapes) {
  const double sample_count = static_cast<double>(samples);
  const double operation_count = sample_count * static_cast<double>(shapes);
  const auto previous_flags = std::cout.flags();
  const auto previous_precision = std::cout.precision();
  std::cout << std::fixed << std::setprecision(3)
            << "      " << backend << ' ' << workload
            << " segments: record=" << timings.record_us / sample_count
            << " us/frame (" << timings.record_us / operation_count
            << " us/shape), submit=" << timings.submit_us / sample_count
            << " us/frame, present=" << timings.present_us / sample_count
            << " us/frame\n";
  std::cout.flags(previous_flags);
  std::cout.precision(previous_precision);
}

void run_blur_benchmark(gcanvas::Backend backend) {
  gcanvas::WindowConfig config{"gCanvas blur benchmark", kCanvasWidth,
                               kCanvasHeight};
  config.backend = backend;
  config.visible = false;
  config.resizeable = false;
  config.vsync = false;
  config.native_pixel_size = true;

  auto window = gcanvas::Window::create(config);
  gcanvas::Context& context = window->create_context();
  auto renderer = flex::render::engines::gcanvas::create_renderer(context);
  check_true(renderer->capabilities().blur);

  for (std::size_t frame = 0; frame < kWarmupFrames; ++frame) {
    render_blurred_frame(context, *renderer, kTypicalShapes, kTypicalBlurRadius,
                         ShapeWorkload::Mixed);
  }

  SegmentTimings typical_timings;
  const std::string typical_name =
      std::string(backend_name(backend)) + " typical blur end-to-end";
  benchmark_ops(typical_name.c_str(), kTypicalSamples, kTypicalShapes) {
    render_timed_blurred_frame(context, *renderer, kTypicalShapes,
                               kTypicalBlurRadius, ShapeWorkload::Mixed,
                               typical_timings);
  }
  print_segment_timings(backend_name(backend), "typical", typical_timings,
                        kTypicalSamples, kTypicalShapes);

  SegmentTimings stress_timings;
  const std::string stress_name =
      std::string(backend_name(backend)) + " stress blur end-to-end";
  benchmark_ops(stress_name.c_str(), kStressSamples, kStressShapes) {
    render_timed_blurred_frame(context, *renderer, kStressShapes,
                               kStressBlurRadius, ShapeWorkload::Mixed,
                               stress_timings);
  }
  print_segment_timings(backend_name(backend), "stress", stress_timings,
                        kStressSamples, kStressShapes);

  for (std::size_t frame = 0; frame < kWarmupFrames; ++frame) {
    render_blurred_frame(context, *renderer, kTypicalShapes, kTypicalBlurRadius,
                         ShapeWorkload::PathOnly);
  }
  SegmentTimings path_only_timings;
  const std::string path_only_name =
      std::string(backend_name(backend)) + " path-only blur end-to-end";
  benchmark_ops(path_only_name.c_str(), kPathOnlySamples, kTypicalShapes) {
    render_timed_blurred_frame(context, *renderer, kTypicalShapes,
                               kTypicalBlurRadius, ShapeWorkload::PathOnly,
                               path_only_timings);
  }
  print_segment_timings(backend_name(backend), "path-only", path_only_timings,
                        kPathOnlySamples, kTypicalShapes);

  for (const PathShadowWorkload workload : {PathShadowWorkload::ContractedOuter,
                                            PathShadowWorkload::ContractedInset}) {
    SegmentTimings shadow_timings;
    const char* workload_name = workload == PathShadowWorkload::ContractedOuter
                                    ? "contracted-outer"
                                    : "contracted-inset";
    for (std::size_t frame = 0; frame < kWarmupFrames; ++frame) {
      SegmentTimings ignored;
      render_timed_path_shadow_frame(context, *renderer, kShadowShapes, workload,
                                     ignored);
    }
    const std::string benchmark_name = std::string(backend_name(backend)) + ' ' +
                                       workload_name + " path shadow end-to-end";
    benchmark_ops(benchmark_name.c_str(), kPathOnlySamples, kTypicalShapes) {
      render_timed_path_shadow_frame(context, *renderer, kShadowShapes, workload,
                                     shadow_timings);
    }
    print_segment_timings(backend_name(backend), workload_name, shadow_timings,
                          kPathOnlySamples, kShadowShapes);
  }
}

} // namespace

suite("Flex on gCanvas blur performance") {
  bench("measures the CPU path pipeline") {
    run_path_pipeline_benchmark();
  }

  bench("measures bounded sampled blur on real GPU backends") {
    run_blur_benchmark(gcanvas::Backend::OpenGL);
    run_blur_benchmark(gcanvas::Backend::Vulkan);
  }
}
