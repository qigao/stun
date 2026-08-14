#include "flex/render/engines/gcanvas.h"
#include "flex/render/engines/nanovg.h"
#include "opengl_engine_test_support.h"

#include <tinytest.h>

#include <cstddef>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

namespace {

using namespace flex::test_support::opengl_engine;

constexpr std::size_t kWarmupFrames = 5U;
constexpr std::size_t kTypicalSamples = 40U;
constexpr std::size_t kStressSamples = 15U;
constexpr std::size_t kTypicalRepetitions = 1U;
constexpr std::size_t kStressRepetitions = 12U;

struct SegmentTimings {
  double record_us = 0.0;
  double submit_us = 0.0;
  double wait_us = 0.0;
  SceneRecordTimings scene;
};

void warmup(flex::Renderer& renderer, const std::string& image_path,
            std::size_t repetitions) {
  for (std::size_t frame = 0; frame < kWarmupFrames; ++frame) {
    record_common_scene(renderer, image_path, repetitions);
    glFinish();
  }
}

void benchmark_renderer(const char* engine, flex::Renderer& renderer,
                        const std::string& image_path) {
  warmup(renderer, image_path, kTypicalRepetitions);
  SegmentTimings typical_segments;
  const std::string typical = std::string(engine) + " typical frame";
  benchmark_ops(typical.c_str(), kTypicalSamples, kTypicalRepetitions) {
    const auto record_begin = std::chrono::steady_clock::now();
    record_common_scene_commands(renderer, image_path, kTypicalRepetitions,
                                 &typical_segments.scene);
    const auto submit_begin = std::chrono::steady_clock::now();
    renderer.end_frame();
    const auto wait_begin = std::chrono::steady_clock::now();
    glFinish();
    const auto wait_end = std::chrono::steady_clock::now();
    typical_segments.record_us +=
        std::chrono::duration<double, std::micro>(submit_begin - record_begin).count();
    typical_segments.submit_us +=
        std::chrono::duration<double, std::micro>(wait_begin - submit_begin).count();
    typical_segments.wait_us +=
        std::chrono::duration<double, std::micro>(wait_end - wait_begin).count();
  }

  warmup(renderer, image_path, kStressRepetitions);
  SegmentTimings stress_segments;
  const std::string stress = std::string(engine) + " stress frame";
  benchmark_ops(stress.c_str(), kStressSamples, kStressRepetitions) {
    const auto record_begin = std::chrono::steady_clock::now();
    record_common_scene_commands(renderer, image_path, kStressRepetitions,
                                 &stress_segments.scene);
    const auto submit_begin = std::chrono::steady_clock::now();
    renderer.end_frame();
    const auto wait_begin = std::chrono::steady_clock::now();
    glFinish();
    const auto wait_end = std::chrono::steady_clock::now();
    stress_segments.record_us +=
        std::chrono::duration<double, std::micro>(submit_begin - record_begin).count();
    stress_segments.submit_us +=
        std::chrono::duration<double, std::micro>(wait_begin - submit_begin).count();
    stress_segments.wait_us +=
        std::chrono::duration<double, std::micro>(wait_end - wait_begin).count();
  }

  const auto print_segments = [engine](const char* workload,
                                       const SegmentTimings& segments,
                                       std::size_t samples) {
    const double count = static_cast<double>(samples);
    std::cout << std::fixed << std::setprecision(3) << "      " << engine << ' '
              << workload << " segments: record=" << segments.record_us / count
              << " us/frame, submit=" << segments.submit_us / count
              << " us/frame, wait=" << segments.wait_us / count << " us/frame\n";
    const SceneRecordTimings& scene = segments.scene;
    std::cout << "      " << engine << ' ' << workload
              << " record detail: begin/clear=" << scene.begin_clear_us / count
              << ", solid=" << scene.solid_rect_us / count
              << ", gradient=" << scene.gradient_rect_us / count
              << ", ellipse=" << scene.ellipse_us / count
              << ", path=" << scene.path_us / count
              << ", text=" << scene.text_us / count
              << ", image=" << scene.image_us / count
              << ", svg=" << scene.svg_us / count << " us/frame\n";
  };
  print_segments("typical", typical_segments, kTypicalSamples);
  print_segments("stress", stress_segments, kStressSamples);
}

} // namespace

suite("Flex OpenGL engine performance") {
  bench("compares NanoVG and gCanvas on one GPU context and workload") {
    Window window;
    const auto fonts = test_fonts();
    char* image_path = tt_make_temp_file("flex_engine_benchmark", ".ppm");
    check_not_null(image_path);
    static constexpr unsigned char kPpm[] = {
        'P', '6', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 255, 255, 255};
    check_int_eq(tt_write_file(image_path, kPpm, sizeof(kPpm)), 0);

    auto gcanvas = flex::render::engines::gcanvas::create_external_opengl_renderer(load_proc);
    register_fonts(*gcanvas, fonts);
    benchmark_renderer("gCanvas", *gcanvas, image_path);
    gcanvas.reset();

    auto nanovg = flex::render::engines::nanovg::create_renderer(true, true, false);
    register_fonts(*nanovg, fonts);
    benchmark_renderer("NanoVG", *nanovg, image_path);
    nanovg.reset();

    check_int_eq(tt_remove_file(image_path), 0);
    std::free(image_path);
  }
}
