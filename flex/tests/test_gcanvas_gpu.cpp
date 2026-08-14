#include "flex.h"
#include "flex/render/engines/gcanvas.h"

#include <gcanvas/window.hpp>
#include <tinytest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int kCanvasWidth = 320;
constexpr int kCanvasHeight = 160;

const char* standard_scene_source() {
  return R"(
scene ElementalValidation {
    width: 320
    height: 160

    group row {
        x: 10
        y: 20
        width: 140
        height: 40
        layout: flex
        gap: 8
        padding: 4

        rect first {
            width: 20
            height: 10
            fill: #ff3333
        }

        rect second {
            width: 30
            height: 12
            opacity: 0.5
            fill: #33cc66
        }
    }

    text label {
        x: 200
        y: 30
        content: "Flex"
        fontSize: 16
        fontFamily: "sans-serif"
        color: #ffffff
    }

    rect marker {
        x: 180
        y: 12
        width: 10
        height: 10
        fill: #3366ff
    }
}

anim "markerMove" {
    duration: 1
    track "#marker/x" {
        keyframe 0 -> 180
        keyframe 1 -> 220
    }
}
)";
}

struct Pixel {
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
  std::uint8_t a;
};

Pixel sample_pixel(const std::vector<std::uint8_t>& pixels, gcanvas::Backend backend,
                   int x, int y) {
  if (x < 0 || y < 0 || x >= kCanvasWidth || y >= kCanvasHeight ||
      pixels.size() != static_cast<std::size_t>(kCanvasWidth * kCanvasHeight * 4)) {
    throw std::out_of_range("invalid gCanvas pixel sample");
  }
  // glReadPixels starts at the lower-left row; Vulkan copies swapchain rows top-down.
  const int storage_y = backend == gcanvas::Backend::OpenGL ? kCanvasHeight - 1 - y : y;
  const std::size_t offset =
      static_cast<std::size_t>((storage_y * kCanvasWidth + x) * 4);
  return {pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
}

bool is_background(Pixel pixel) {
  constexpr std::uint8_t tolerance = 8;
  return pixel.r <= tolerance && pixel.g <= tolerance && pixel.b <= tolerance &&
         pixel.a >= 247;
}

bool is_red(Pixel pixel) {
  return pixel.r >= 235 && pixel.g >= 35 && pixel.g <= 70 && pixel.b >= 35 &&
         pixel.b <= 70;
}

bool is_green(Pixel pixel) {
  return pixel.r < 20 && pixel.g > 235 && pixel.b < 20;
}

bool is_half_green(Pixel pixel) {
  return pixel.r >= 15 && pixel.r <= 40 && pixel.g >= 85 && pixel.g <= 120 &&
         pixel.b >= 35 && pixel.b <= 70;
}

bool is_blue(Pixel pixel) {
  return pixel.r >= 35 && pixel.r <= 70 && pixel.g >= 80 && pixel.g <= 125 &&
         pixel.b >= 235;
}

bool region_has_text(const std::vector<std::uint8_t>& pixels, gcanvas::Backend backend) {
  for (int y = 30; y < 55; ++y) {
    for (int x = 198; x < 245; ++x) {
      const Pixel pixel = sample_pixel(pixels, backend, x, y);
      if (pixel.r > 40 && pixel.g > 40 && pixel.b > 40) {
        return true;
      }
    }
  }
  return false;
}

template <typename Predicate>
bool region_has_pixel(const std::vector<std::uint8_t>& pixels, gcanvas::Backend backend,
                      int minimum_x, int minimum_y, int maximum_x, int maximum_y,
                      Predicate&& predicate) {
  for (int y = minimum_y; y < maximum_y; ++y) {
    for (int x = minimum_x; x < maximum_x; ++x) {
      if (predicate(sample_pixel(pixels, backend, x, y))) {
        return true;
      }
    }
  }
  return false;
}

std::size_t count_bright_pixels(const std::vector<std::uint8_t>& pixels,
                                gcanvas::Backend backend, int minimum_x, int minimum_y,
                                int maximum_x, int maximum_y) {
  std::size_t count = 0;
  for (int y = minimum_y; y < maximum_y; ++y) {
    for (int x = minimum_x; x < maximum_x; ++x) {
      const Pixel pixel = sample_pixel(pixels, backend, x, y);
      if (pixel.r > 180 && pixel.g > 180 && pixel.b > 180) {
        ++count;
      }
    }
  }
  return count;
}

flex::Scene::RawPtr build_clip_scene(flex::ArenaAllocator& arena) {
  auto scene = flex::Scene::create(static_cast<float>(kCanvasWidth),
                                   static_cast<float>(kCanvasHeight), arena);
  auto clipper = flex::Group::create(arena);
  clipper->set_position(80.0f, 80.0f);
  clipper->set_layout_width(20.0f);
  clipper->set_layout_height(16.0f);
  clipper->set_clip(true);
  scene->add_child(clipper);

  auto inside = flex::Shape::create(arena);
  inside->set_rect(12.0f, 12.0f);
  inside->set_fill(flex::Color{1.0f, 0.2f, 0.2f, 1.0f});
  clipper->add_child(inside);

  auto outside = flex::Shape::create(arena);
  outside->set_position(24.0f, 0.0f);
  outside->set_rect(12.0f, 12.0f);
  outside->set_fill(flex::Color{0.2f, 0.8f, 0.4f, 1.0f});
  clipper->add_child(outside);
  return scene;
}

flex::Scene::RawPtr build_shadow_scene(flex::ArenaAllocator& arena) {
  auto scene = flex::Scene::create(static_cast<float>(kCanvasWidth),
                                   static_cast<float>(kCanvasHeight), arena);

  auto rect = flex::Shape::create(arena);
  rect->set_position(40.0f, 60.0f);
  rect->set_rect(24.0f, 16.0f, 3.0f);
  rect->set_fill(flex::Color::White);
  flex::Shadow rect_shadow = flex::Shadow::drop(
      8.0f, 0.0f, 6.0f, flex::Color{1.0f, 0.0f, 0.0f, 1.0f});
  rect_shadow.spread = 2.0f;
  rect->set_shadow(rect_shadow);
  scene->add_child(rect);

  auto circle = flex::Shape::create(arena);
  circle->set_position(110.0f, 70.0f);
  circle->set_circle(10.0f);
  circle->set_fill(flex::Color::White);
  flex::Shadow circle_shadow = flex::Shadow::drop(
      0.0f, 8.0f, 5.0f, flex::Color{0.0f, 0.0f, 1.0f, 1.0f});
  circle_shadow.spread = 2.0f;
  circle->set_shadow(circle_shadow);
  scene->add_child(circle);
  return scene;
}

std::vector<std::uint8_t> render_scene(gcanvas::Context& context, flex::Renderer& renderer,
                                       flex::Scene::RawPtr scene) {
  renderer.begin_frame(static_cast<float>(kCanvasWidth),
                       static_cast<float>(kCanvasHeight), 1.0f);
  renderer.clear(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
  scene->render(renderer);
  renderer.end_frame();
  auto pixels = context.read_pixels();
  context.present_frame();
  return pixels;
}

void validate_backend(gcanvas::Backend backend) {
  gcanvas::WindowConfig config{"Flex gCanvas GPU validation", kCanvasWidth,
                             kCanvasHeight};
  config.backend = backend;
  config.visible = false;
  config.resizeable = false;
  config.vsync = false;
  config.native_pixel_size = true;

  auto window = gcanvas::Window::create(config);
  gcanvas::Context& context = window->create_context();
  auto renderer = flex::render::engines::gcanvas::create_renderer(context);

  auto definition = flex::Definition::load(standard_scene_source());
  check_true(definition != nullptr);
  check_false(definition->has_error());
  check_true(definition->scene() != nullptr);
  auto instance = flex::Instance::create(std::move(definition));
  check_true(instance != nullptr);

  auto pixels = render_scene(context, *renderer, instance->scene());
  check_true(is_red(sample_pixel(pixels, backend, 20, 28)));
  check_true(is_half_green(sample_pixel(pixels, backend, 48, 28)));
  check_true(is_blue(sample_pixel(pixels, backend, 185, 17)));
  check_true(is_background(sample_pixel(pixels, backend, 38, 28)));
  check_true(region_has_text(pixels, backend));

  auto* player = instance->play_animation("markerMove");
  check_true(player != nullptr);
  instance->advance(0.5f);
  pixels = render_scene(context, *renderer, instance->scene());
  check_true(is_background(sample_pixel(pixels, backend, 185, 17)));
  check_true(is_blue(sample_pixel(pixels, backend, 205, 17)));

  flex::ArenaAllocator arena(4096);
  pixels = render_scene(context, *renderer, build_clip_scene(arena));
  check_true(is_red(sample_pixel(pixels, backend, 85, 85)));
  check_true(is_background(sample_pixel(pixels, backend, 108, 85)));

  flex::ArenaAllocator shadow_arena(4096);
  pixels = render_scene(context, *renderer, build_shadow_scene(shadow_arena));
  const Pixel rect_shadow = sample_pixel(pixels, backend, 70, 68);
  const Pixel rect_falloff = sample_pixel(pixels, backend, 78, 68);
  const Pixel circle_shadow = sample_pixel(pixels, backend, 110, 82);
  check_true(rect_shadow.r > 235 && rect_shadow.g < 20 && rect_shadow.b < 20);
  check_true(rect_falloff.r > 20 && rect_falloff.r < 230 &&
             rect_falloff.g < 20 && rect_falloff.b < 20);
  check_true(circle_shadow.b > 235 && circle_shadow.r < 20 && circle_shadow.g < 20);
  check_true(is_background(sample_pixel(pixels, backend, 84, 68)));

  constexpr const char* shadow_svg =
      "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='8'>"
      "<rect width='16' height='8' fill='#ff0000'/></svg>";
  renderer->begin_frame(static_cast<float>(kCanvasWidth),
                        static_cast<float>(kCanvasHeight), 1.0f);
  renderer->clear(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
  renderer->set_shadow(flex::Shadow::drop(
      8.0f, 0.0f, 2.0f, flex::Color{0.0f, 1.0f, 0.0f, 1.0f}));
  renderer->fill_path("M20 108 C30 100 42 106 40 120 C36 130 22 128 20 108 Z",
                      flex::Paint::solid(flex::Color::White));
  renderer->set_shadow(flex::Shadow::drop(
      0.0f, 8.0f, 2.0f, flex::Color{0.0f, 0.0f, 1.0f, 1.0f}));
  renderer->draw_ellipse(78.0f, 114.0f, 12.0f, 6.0f,
                         flex::Paint::solid(flex::Color::White),
                         flex::Paint::none(), 0.0f);
  renderer->set_shadow(flex::Shadow::drop(
      8.0f, 0.0f, 2.0f, flex::Color{1.0f, 1.0f, 0.0f, 1.0f}));
  renderer->draw_svg_data(shadow_svg, 112.0f, 108.0f, 16.0f, 8.0f);
  renderer->set_shadow(flex::Shadow::drop(
      8.0f, 0.0f, 2.0f, flex::Color{1.0f, 0.0f, 1.0f, 1.0f}));
  renderer->draw_text("GPU", 150.0f, 102.0f, "sans-serif", 18.0f, false,
                      flex::Color::White);
  renderer->set_shadow(flex::Shadow::inner(
      4.0f, 0.0f, 6.0f, flex::Color{1.0f, 0.0f, 0.0f, 1.0f}));
  renderer->draw_rect(220.0f, 104.0f, 30.0f, 20.0f, 3.0f,
                      flex::Paint::solid(flex::Color::White),
                      flex::Paint::none(), 0.0f);
  renderer->set_shadow(flex::Shadow::inner(
      0.0f, 4.0f, 5.0f, flex::Color{0.0f, 0.0f, 1.0f, 1.0f}));
  renderer->draw_ellipse(282.0f, 114.0f, 14.0f, 9.0f,
                         flex::Paint::solid(flex::Color::White),
                         flex::Paint::none(), 0.0f);
  renderer->clear_shadow();
  renderer->end_frame();
  pixels = context.read_pixels();
  context.present_frame();
  check_true(region_has_pixel(pixels, backend, 40, 104, 52, 130, [](Pixel pixel) {
    return pixel.g > 30 && pixel.g > pixel.r * 2 && pixel.g > pixel.b * 2;
  }));
  check_true(region_has_pixel(pixels, backend, 66, 120, 90, 132, [](Pixel pixel) {
    return pixel.b > 30 && pixel.b > pixel.r * 2 && pixel.b > pixel.g * 2;
  }));
  check_true(region_has_pixel(pixels, backend, 128, 106, 140, 120, [](Pixel pixel) {
    return pixel.r > 30 && pixel.g > 30 && pixel.b < 20;
  }));
  check_true(region_has_pixel(pixels, backend, 156, 100, 205, 132, [](Pixel pixel) {
    return pixel.r > 30 && pixel.b > 30 && pixel.g < 20;
  }));
  check_true(region_has_pixel(pixels, backend, 220, 104, 250, 124, [](Pixel pixel) {
    return pixel.r > pixel.g + 20 && pixel.r > pixel.b + 20;
  }));
  check_true(region_has_pixel(pixels, backend, 268, 105, 296, 123, [](Pixel pixel) {
    return pixel.b > pixel.r + 20 && pixel.b > pixel.g + 20;
  }));
  check_true(is_background(sample_pixel(pixels, backend, 216, 114)));

  renderer->begin_frame(static_cast<float>(kCanvasWidth),
                        static_cast<float>(kCanvasHeight), 1.0f);
  renderer->clear(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
  renderer->save();
  renderer->clip_rect(20.0f, 20.0f, 25.0f, 30.0f);
  renderer->set_shadow(flex::Shadow::inner(
      7.0f, 0.0f, 2.0f, flex::Color{1.0f, 0.0f, 0.0f, 1.0f}));
  renderer->fill_path("M20 20 L62 20 L62 50 L20 50 Z",
                      flex::Paint::solid(flex::Color::White));
  renderer->restore();
  renderer->set_shadow(flex::Shadow::inner(
      7.0f, 0.0f, 2.0f, flex::Color{0.0f, 1.0f, 0.0f, 1.0f}));
  renderer->draw_svg_data(shadow_svg, 90.0f, 20.0f, 32.0f, 20.0f);
  renderer->set_shadow(flex::Shadow::inner(
      4.0f, 0.0f, 2.0f, flex::Color{0.0f, 0.0f, 1.0f, 1.0f}));
  renderer->draw_text("GPU", 150.0f, 18.0f, "sans-serif", 24.0f, false,
                      flex::Color::White);
  renderer->set_shadow(flex::Shadow::inner(
      5.0f, 0.0f, 2.0f, flex::Color{1.0f, 1.0f, 0.0f, 1.0f}));
  renderer->draw_ellipse(255.0f, 35.0f, 18.0f, 10.0f,
                         flex::Paint::none(),
                         flex::Paint::solid(flex::Color::White), 5.0f);
  renderer->clear_shadow();
  renderer->end_frame();
  pixels = context.read_pixels();
  context.present_frame();
  check_true(region_has_pixel(pixels, backend, 20, 20, 34, 50, [](Pixel pixel) {
    return pixel.r > pixel.g + 20 && pixel.r > pixel.b + 20;
  }));
  check_true(region_has_pixel(pixels, backend, 90, 20, 104, 40, [](Pixel pixel) {
    return pixel.g > pixel.r + 20 && pixel.g > pixel.b + 20;
  }));
  check_true(region_has_pixel(pixels, backend, 150, 18, 205, 50, [](Pixel pixel) {
    return pixel.b > pixel.r + 20 && pixel.b > pixel.g + 20;
  }));
  check_true(region_has_pixel(pixels, backend, 234, 23, 252, 47, [](Pixel pixel) {
    return pixel.r > 30 && pixel.g > 30 && pixel.b < 20;
  }));
  check_true(is_background(sample_pixel(pixels, backend, 18, 35)));
  check_true(is_background(sample_pixel(pixels, backend, 55, 35)));

  renderer->begin_frame(static_cast<float>(kCanvasWidth),
                        static_cast<float>(kCanvasHeight), 1.0f);
  renderer->clear(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
  flex::Shadow contracted_outer = flex::Shadow::drop(
      8.0f, 0.0f, 0.0f, flex::Color{1.0f, 0.0f, 0.0f, 1.0f});
  contracted_outer.spread = -3.0f;
  renderer->set_shadow(contracted_outer);
  renderer->fill_path("M20 20 L50 20 L50 50 L20 50 Z",
                      flex::Paint::solid(flex::Color::White));
  contracted_outer.color = flex::Color{0.0f, 1.0f, 0.0f, 1.0f};
  contracted_outer.spread = -2.0f;
  renderer->set_shadow(contracted_outer);
  renderer->draw_svg_data(shadow_svg, 80.0f, 20.0f, 24.0f, 20.0f);
  contracted_outer.color = flex::Color{0.0f, 0.0f, 1.0f, 1.0f};
  contracted_outer.spread = -1.0f;
  renderer->set_shadow(contracted_outer);
  renderer->draw_text("GPU ", 140.0f, 18.0f, "sans-serif", 24.0f, false,
                      flex::Color::White);

  flex::Shadow contracted_inset = flex::Shadow::inner(
      8.0f, 0.0f, 0.0f, flex::Color{1.0f, 0.0f, 1.0f, 1.0f});
  contracted_inset.spread = -3.0f;
  renderer->set_shadow(contracted_inset);
  renderer->fill_path("M20 80 L50 80 L50 110 L20 110 Z",
                      flex::Paint::solid(flex::Color::White));
  contracted_inset.color = flex::Color{0.0f, 1.0f, 1.0f, 1.0f};
  contracted_inset.spread = -2.0f;
  renderer->set_shadow(contracted_inset);
  renderer->draw_svg_data(shadow_svg, 80.0f, 80.0f, 24.0f, 20.0f);
  contracted_inset.color = flex::Color{1.0f, 1.0f, 0.0f, 1.0f};
  contracted_inset.spread = -1.0f;
  renderer->set_shadow(contracted_inset);
  renderer->draw_text("GPU ", 140.0f, 78.0f, "sans-serif", 24.0f, false,
                      flex::Color::White);
  renderer->clear_shadow();
  renderer->end_frame();
  pixels = context.read_pixels();
  context.present_frame();
  const Pixel contracted_path_outer = sample_pixel(pixels, backend, 53, 35);
  check_true(contracted_path_outer.r > 180 && contracted_path_outer.g < 40 &&
             contracted_path_outer.b < 40);
  const Pixel contracted_svg_outer = sample_pixel(pixels, backend, 107, 30);
  check_true(contracted_svg_outer.g > 180 && contracted_svg_outer.r < 40 &&
             contracted_svg_outer.b < 40);
  check_true(region_has_pixel(pixels, backend, 165, 18, 190, 48, [](Pixel pixel) {
    return pixel.b > pixel.r + 20 && pixel.b > pixel.g + 20;
  }));
  const Pixel contracted_path_inset = sample_pixel(pixels, backend, 22, 95);
  check_true(contracted_path_inset.r > 180 && contracted_path_inset.b > 180 &&
             contracted_path_inset.g < 40);
  check_true(is_background(sample_pixel(pixels, backend, 18, 95)));
  const Pixel contracted_path_interior = sample_pixel(pixels, backend, 28, 95);
  check_true(contracted_path_interior.r > 220 && contracted_path_interior.g > 220 &&
             contracted_path_interior.b > 220);
  check_true(region_has_pixel(pixels, backend, 80, 80, 88, 100, [](Pixel pixel) {
    return pixel.g > 120 && pixel.b > 120 && pixel.r < 80;
  }));
  check_true(region_has_pixel(pixels, backend, 140, 78, 165, 108, [](Pixel pixel) {
    return pixel.r > 120 && pixel.g > 120 && pixel.b < 80;
  }));

  constexpr const char* affine_svg =
      "<svg xmlns='http://www.w3.org/2000/svg' width='20' height='8'>"
      "<rect width='20' height='8' fill='#ff0000'/></svg>";
  renderer->begin_frame(static_cast<float>(kCanvasWidth),
                        static_cast<float>(kCanvasHeight), 1.0f);
  renderer->clear(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
  renderer->save();
  renderer->translate(50.0f, 10.0f);
  renderer->rotate(90.0f);
  renderer->draw_svg_data(affine_svg, 0.0f, 0.0f, 20.0f, 8.0f);
  renderer->restore();
  renderer->end_frame();
  pixels = context.read_pixels();
  context.present_frame();
  const Pixel affine_inside = sample_pixel(pixels, backend, 46, 20);
  check_true(affine_inside.r > 235 && affine_inside.g < 20 && affine_inside.b < 20);
  check_true(is_background(sample_pixel(pixels, backend, 20, 20)));

  renderer->begin_frame(static_cast<float>(kCanvasWidth),
                        static_cast<float>(kCanvasHeight), 1.0f);
  renderer->clear(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
  renderer->translate(140.0f, 50.0f);
  renderer->rotate(45.0f);
  renderer->clip_rect(0.0f, 0.0f, 40.0f, 40.0f);
  renderer->set_transform(flex::Transform{});
  renderer->clip_rect(130.0f, 60.0f, 20.0f, 40.0f);
  renderer->draw_rect(105.0f, 45.0f, 70.0f, 70.0f, 0.0f,
                      flex::Paint::solid(flex::Color::Red), flex::Paint::none(), 0.0f);
  renderer->fill_path("M 130 68 L 150 68 L 150 88 L 130 88 Z",
                      flex::Paint::solid(flex::Color::Green));
  renderer->draw_rect(105.0f, 45.0f, 20.0f, 20.0f, 0.0f,
                      flex::Paint::solid(flex::Color::Red), flex::Paint::none(), 0.0f);
  renderer->end_frame();
  pixels = context.read_pixels();
  context.present_frame();
  check_true(is_green(sample_pixel(pixels, backend, 140, 78)));
  check_true(is_background(sample_pixel(pixels, backend, 125, 78)));
  check_true(is_background(sample_pixel(pixels, backend, 115, 53)));

  renderer->begin_frame(static_cast<float>(kCanvasWidth),
                        static_cast<float>(kCanvasHeight), 1.0f);
  renderer->clear(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
  renderer->save();
  renderer->translate(75.0f, 5.0f);
  renderer->rotate(90.0f);
  renderer->draw_text("HI", 0.0f, 0.0f, "sans-serif", 24.0f, true,
                      flex::Color::White);
  renderer->restore();
  renderer->end_frame();
  pixels = context.read_pixels();
  context.present_frame();
  check_true(count_bright_pixels(pixels, backend, 48, 6, 73, 30) >= 8U);
  check_size_eq(count_bright_pixels(pixels, backend, 80, 6, 95, 32), 0U);

  renderer->begin_frame(static_cast<float>(kCanvasWidth),
                        static_cast<float>(kCanvasHeight), 1.0f);
  renderer->clear(flex::Color{0.0f, 0.0f, 0.0f, 1.0f});
  renderer->set_blur(flex::BlurFilter{6.0f});
  renderer->draw_rect(40.0f, 60.0f, 20.0f, 16.0f, 2.0f,
                      flex::Paint::solid(flex::Color::Red),
                      flex::Paint::none(), 0.0f);
  renderer->draw_svg_data(shadow_svg, 100.0f, 60.0f, 16.0f, 8.0f);
  renderer->draw_text("GPU", 160.0f, 56.0f, "sans-serif", 20.0f, false,
                      flex::Color::White);
  flex::LinearGradient blur_gradient(2.6f, 0.0f, 3.0f, 0.0f);
  blur_gradient.add_stop(0.0f, flex::Color::Red);
  blur_gradient.add_stop(1.0f, flex::Color::Blue);
  renderer->fill_path("M260 60 L300 60 L300 76 L260 76 Z",
                      flex::Paint(blur_gradient));
  renderer->save();
  renderer->clip_rect(30.0f, 108.0f, 20.0f, 20.0f);
  renderer->fill_path("M20 112 L60 112 L60 124 L20 124 Z",
                      flex::Paint::solid(flex::Color{0.0f, 1.0f, 1.0f, 1.0f}));
  renderer->restore();
  renderer->clear_blur();
  renderer->draw_rect(230.0f, 60.0f, 20.0f, 16.0f, 0.0f,
                      flex::Paint::solid(flex::Color::Green),
                      flex::Paint::none(), 0.0f);
  renderer->end_frame();
  pixels = context.read_pixels();
  context.present_frame();
  const Pixel blurred_center = sample_pixel(pixels, backend, 50, 68);
  const Pixel blurred_edge = sample_pixel(pixels, backend, 37, 68);
  check_true(blurred_center.r > 220 && blurred_center.g < 20 &&
             blurred_center.b < 20);
  check_true(blurred_edge.r > 5 && blurred_edge.r < 220 &&
             blurred_edge.g < 20 && blurred_edge.b < 20);
  check_true(is_background(sample_pixel(pixels, backend, 30, 68)));
  check_true(region_has_pixel(pixels, backend, 94, 56, 100, 74, [](Pixel pixel) {
    return pixel.r > 5 && pixel.r > pixel.g * 2 && pixel.r > pixel.b * 2;
  }));
  check_true(region_has_pixel(pixels, backend, 152, 52, 205, 86, [](Pixel pixel) {
    return pixel.r > 5 && pixel.g > 5 && pixel.b > 5;
  }));
  check_true(region_has_pixel(pixels, backend, 254, 62, 260, 74, [](Pixel pixel) {
    return pixel.r > 5 || pixel.b > 5;
  }));
  check_true(region_has_pixel(pixels, backend, 265, 62, 295, 74, [](Pixel pixel) {
    return pixel.r > 20 && pixel.b > 20;
  }));
  check_true(region_has_pixel(pixels, backend, 32, 112, 48, 124, [](Pixel pixel) {
    return pixel.g > 80 && pixel.b > 80 && pixel.r < 20;
  }));
  check_true(is_background(sample_pixel(pixels, backend, 26, 118)));
  check_true(is_background(sample_pixel(pixels, backend, 54, 118)));
  check_true(is_green(sample_pixel(pixels, backend, 240, 68)));
  check_true(is_background(sample_pixel(pixels, backend, 226, 68)));
}

} // namespace

suite("Flex on gCanvas GPU") {
  it("preserves Flex layout, affine clipping, text, animation, and SVG on OpenGL") {
    validate_backend(gcanvas::Backend::OpenGL);
  }

  it("preserves Flex layout, affine clipping, text, animation, and SVG on Vulkan") {
    validate_backend(gcanvas::Backend::Vulkan);
  }
}
