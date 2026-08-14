#include "flex/render/engines/gcanvas.h"
#include "flex/render/engines/nanovg.h"
#include "opengl_engine_test_support.h"

#include <tinytest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace {

using namespace flex::test_support::opengl_engine;

struct ImageFixture {
  ImageFixture() {
    path = tt_make_temp_file("flex_engine_parity", ".ppm");
    if (!path) {
      throw std::runtime_error("could not allocate parity image path");
    }
    static constexpr unsigned char kPpm[] = {
        'P', '6', '\n', '2', ' ', '2', '\n', '2', '5', '5', '\n',
        255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
    if (tt_write_file(path, kPpm, sizeof(kPpm)) != 0) {
      throw std::runtime_error("could not write parity image");
    }
  }

  ~ImageFixture() {
    if (path) {
      tt_remove_file(path);
      std::free(path);
    }
  }

  char* path = nullptr;
};

struct Difference {
  double mean_absolute = 0.0;
  double changed_pixel_ratio = 0.0;
};

Difference compare_images(const std::vector<std::uint8_t>& first,
                          const std::vector<std::uint8_t>& second) {
  check_size_eq(first.size(), second.size());
  std::uint64_t absolute_sum = 0;
  std::size_t changed_pixels = 0;
  for (std::size_t offset = 0; offset < first.size(); offset += 4U) {
    int maximum_delta = 0;
    for (std::size_t channel = 0; channel < 4U; ++channel) {
      const int delta = std::abs(static_cast<int>(first[offset + channel]) -
                                 static_cast<int>(second[offset + channel]));
      absolute_sum += static_cast<std::uint64_t>(delta);
      maximum_delta = (std::max)(maximum_delta, delta);
    }
    if (maximum_delta > 24) {
      ++changed_pixels;
    }
  }
  return {static_cast<double>(absolute_sum) / static_cast<double>(first.size()),
          static_cast<double>(changed_pixels) /
              static_cast<double>(first.size() / 4U)};
}

bool is_bright(const std::array<std::uint8_t, 4>& pixel) {
  return pixel[0] > 80 || pixel[1] > 80 || pixel[2] > 80;
}

std::size_t bright_pixels_in(const std::vector<std::uint8_t>& pixels,
                             int left, int top, int right, int bottom) {
  std::size_t count = 0;
  for (int y = top; y < bottom; ++y)
    for (int x = left; x < right; ++x)
      if (is_bright(pixel_at(pixels, x, y)))
        ++count;
  return count;
}

} // namespace

suite("Flex OpenGL engine migration parity") {
  it("keeps the canonical NanoVG and gCanvas images within the golden tolerance") {
    Window window;
    ImageFixture image;
    const auto fonts = test_fonts();

    auto gcanvas = flex::render::engines::gcanvas::create_external_opengl_renderer(load_proc);
    register_fonts(*gcanvas, fonts);
    record_common_scene(*gcanvas, image.path);
    const auto gcanvas_pixels = read_pixels();
    gcanvas.reset();

    auto nanovg = flex::render::engines::nanovg::create_renderer(true, true, false);
    register_fonts(*nanovg, fonts);
    record_common_scene(*nanovg, image.path);
    const auto nanovg_pixels = read_pixels();
    nanovg.reset();

    const Difference difference = compare_images(gcanvas_pixels, nanovg_pixels);
    check_double_le(difference.mean_absolute, 18.0);
    check_double_le(difference.changed_pixel_ratio, 0.18);

    for (const auto& pixels : {&gcanvas_pixels, &nanovg_pixels}) {
      check_true(is_bright(pixel_at(*pixels, 30, 25)));
      check_true(is_bright(pixel_at(*pixels, 118, 25)));
      check_true(is_bright(pixel_at(*pixels, 198, 27)));
      check_true(is_bright(pixel_at(*pixels, 275, 25)));
      check_true(is_bright(pixel_at(*pixels, 222, 70)));
      check_true(is_bright(pixel_at(*pixels, 280, 70)));
      check_true(bright_pixels_in(*pixels, 12, 62, 112, 92) > std::size_t{20});
      check_true(bright_pixels_in(*pixels, 126, 62, 196, 92) > std::size_t{20});
    }
  }

  it("keeps gCanvas image errors fail-fast after the migration") {
    Window window;
    auto renderer =
        flex::render::engines::gcanvas::create_external_opengl_renderer(load_proc);
    renderer->begin_frame(static_cast<float>(kWidth), static_cast<float>(kHeight), 1.0f);
    check_throws_as(renderer->draw_image("", 0.0f, 0.0f, 10.0f, 10.0f),
                    std::invalid_argument);
    check_throws_as(renderer->draw_image("missing-flex-parity-image.png", 0.0f, 0.0f,
                                         10.0f, 10.0f),
                    std::runtime_error);
    renderer->end_frame();
  }
}
