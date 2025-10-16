#include <chrono>
#include <cstring>
#include <iostream>
#include <nanogui.h>
#include <rlottie.h>

// Inline Lottie JSON animation data
constexpr const char *kLottieAnimation = R"JSON({
  "v": "5.7.4",
  "fr": 60,
  "ip": 0,
  "op": 120,
  "w": 200,
  "h": 200,
  "nm": "Bouncing Circle",
  "ddd": 0,
  "assets": [],
  "layers": [
    {
      "ddd": 0,
      "ind": 1,
      "ty": 4,
      "nm": "Circle",
      "sr": 1,
      "ks": {
        "o": {"a": 0, "k": 100},
        "r": {"a": 0, "k": 0},
        "p": {
          "a": 1,
          "k": [
            {"i": {"x": 0.42, "y": 1}, "o": {"x": 0.58, "y": 0}, "t": 0, "s": [100, 50, 0], "to": [0, 12.5, 0], "ti": [0, 0, 0]},
            {"i": {"x": 0.42, "y": 1}, "o": {"x": 0.58, "y": 0}, "t": 30, "s": [100, 125, 0], "to": [0, 0, 0], "ti": [0, 0, 0]},
            {"i": {"x": 0.42, "y": 1}, "o": {"x": 0.58, "y": 0}, "t": 60, "s": [100, 50, 0], "to": [0, 0, 0], "ti": [0, 0, 0]},
            {"i": {"x": 0.42, "y": 1}, "o": {"x": 0.58, "y": 0}, "t": 90, "s": [100, 125, 0], "to": [0, 0, 0], "ti": [0, -12.5, 0]},
            {"t": 120, "s": [100, 50, 0]}
          ]
        },
        "a": {"a": 0, "k": [0, 0, 0]},
        "s": {"a": 0, "k": [100, 100, 100]}
      },
      "ao": 0,
      "shapes": [
        {
          "ty": "gr",
          "it": [
            {
              "d": 1,
              "ty": "el",
              "s": {"a": 0, "k": [60, 60]},
              "p": {"a": 0, "k": [0, 0]},
              "nm": "Ellipse Path 1"
            },
            {
              "ty": "fl",
              "c": {"a": 0, "k": [0.2, 0.6, 1, 1]},
              "o": {"a": 0, "k": 100},
              "r": 1,
              "bm": 0,
              "nm": "Fill 1"
            },
            {
              "ty": "tr",
              "p": {"a": 0, "k": [0, 0]},
              "a": {"a": 0, "k": [0, 0]},
              "s": {"a": 0, "k": [100, 100]},
              "r": {"a": 0, "k": 0},
              "o": {"a": 0, "k": 100},
              "sk": {"a": 0, "k": 0},
              "sa": {"a": 0, "k": 0},
              "nm": "Transform"
            }
          ],
          "nm": "Ellipse 1",
          "bm": 0
        }
      ],
      "ip": 0,
      "op": 120,
      "st": 0,
      "bm": 0
    },
    {
      "ddd": 0,
      "ind": 2,
      "ty": 4,
      "nm": "Shadow",
      "sr": 1,
      "ks": {
        "o": {
          "a": 1,
          "k": [
            {"i": {"x": [0.42], "y": [1]}, "o": {"x": [0.58], "y": [0]}, "t": 0, "s": [30]},
            {"i": {"x": [0.42], "y": [1]}, "o": {"x": [0.58], "y": [0]}, "t": 30, "s": [60]},
            {"i": {"x": [0.42], "y": [1]}, "o": {"x": [0.58], "y": [0]}, "t": 60, "s": [30]},
            {"i": {"x": [0.42], "y": [1]}, "o": {"x": [0.58], "y": [0]}, "t": 90, "s": [60]},
            {"t": 120, "s": [30]}
          ]
        },
        "r": {"a": 0, "k": 0},
        "p": {"a": 0, "k": [100, 170, 0]},
        "a": {"a": 0, "k": [0, 0, 0]},
        "s": {
          "a": 1,
          "k": [
            {"i": {"x": [0.42, 0.42, 0.42], "y": [1, 1, 1]}, "o": {"x": [0.58, 0.58, 0.58], "y": [0, 0, 0]}, "t": 0, "s": [80, 40, 100]},
            {"i": {"x": [0.42, 0.42, 0.42], "y": [1, 1, 1]}, "o": {"x": [0.58, 0.58, 0.58], "y": [0, 0, 0]}, "t": 30, "s": [120, 60, 100]},
            {"i": {"x": [0.42, 0.42, 0.42], "y": [1, 1, 1]}, "o": {"x": [0.58, 0.58, 0.58], "y": [0, 0, 0]}, "t": 60, "s": [80, 40, 100]},
            {"i": {"x": [0.42, 0.42, 0.42], "y": [1, 1, 1]}, "o": {"x": [0.58, 0.58, 0.58], "y": [0, 0, 0]}, "t": 90, "s": [120, 60, 100]},
            {"t": 120, "s": [80, 40, 100]}
          ]
        }
      },
      "ao": 0,
      "shapes": [
        {
          "ty": "gr",
          "it": [
            {
              "d": 1,
              "ty": "el",
              "s": {"a": 0, "k": [60, 20]},
              "p": {"a": 0, "k": [0, 0]},
              "nm": "Ellipse Path 1"
            },
            {
              "ty": "fl",
              "c": {"a": 0, "k": [0, 0, 0, 1]},
              "o": {"a": 0, "k": 100},
              "r": 1,
              "bm": 0,
              "nm": "Fill 1"
            },
            {
              "ty": "tr",
              "p": {"a": 0, "k": [0, 0]},
              "a": {"a": 0, "k": [0, 0]},
              "s": {"a": 0, "k": [100, 100]},
              "r": {"a": 0, "k": 0},
              "o": {"a": 0, "k": 100},
              "sk": {"a": 0, "k": 0},
              "sa": {"a": 0, "k": 0},
              "nm": "Transform"
            }
          ],
          "nm": "Ellipse 1",
          "bm": 0
        }
      ],
      "ip": 0,
      "op": 120,
      "st": 0,
      "bm": 0
    }
  ],
  "markers": []
})JSON";

using namespace nanogui;

class LottieCanvas : public Canvas {
public:
  LottieCanvas(Widget *parent) : Canvas(parent, 1, false, false, true) {
    // Load Lottie animation
    m_animation = rlottie::Animation::loadFromData(std::string(kLottieAnimation), std::string(""),
                                                   rlottie::ColorFilter());

    if (!m_animation) {
      std::cerr << "Failed to load animation" << std::endl;
      return;
    }

    // Get animation properties
    m_animation->size(m_width, m_height);
    m_fps = m_animation->frameRate();
    m_total_frames = m_animation->totalFrame();

    std::cout << "Animation: " << m_width << "x" << m_height << ", " << m_fps << " FPS, "
              << m_total_frames << " frames" << std::endl;

    // Allocate buffer
    m_buffer.resize(m_width * m_height);
    m_start_time = std::chrono::high_resolution_clock::now();

    set_size(Vector2i(static_cast<int>(m_width), static_cast<int>(m_height)));
  }

  virtual void draw(NVGcontext *ctx) override {
    Canvas::draw(ctx);

    if (!m_animation)
      return;

    // Calculate current frame
    auto current_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(current_time - m_start_time).count();
    size_t current_frame = static_cast<size_t>(elapsed * m_fps) % m_total_frames;

    // Render frame to buffer
    size_t stride = m_width * 4;
    rlottie::Surface surface(m_buffer.data(), m_width, m_height, stride);
    m_animation->renderSync(current_frame, surface);

    // Create or update NanoVG image
    if (m_nvg_image == -1) {
      m_nvg_image = nvgCreateImageRGBA(ctx, static_cast<int>(m_width), static_cast<int>(m_height),
                                       0, reinterpret_cast<unsigned char *>(m_buffer.data()));
    } else {
      nvgUpdateImage(ctx, m_nvg_image, reinterpret_cast<unsigned char *>(m_buffer.data()));
    }

    // Draw the image
    if (m_nvg_image != -1) {
      NVGpaint img_paint = nvgImagePattern(ctx, m_pos.x(), m_pos.y(), static_cast<float>(m_width),
                                           static_cast<float>(m_height), 0, m_nvg_image, 1.0f);
      nvgBeginPath(ctx);
      nvgRect(ctx, m_pos.x(), m_pos.y(), static_cast<float>(m_width), static_cast<float>(m_height));
      nvgFillPaint(ctx, img_paint);
      nvgFill(ctx);
    }
  }

  ~LottieCanvas() {
    // NanoVG image cleanup is handled by NanoGUI
  }

private:
  std::unique_ptr<rlottie::Animation> m_animation;
  std::vector<uint32_t> m_buffer;
  size_t m_width = 0, m_height = 0;
  double m_fps = 0;
  size_t m_total_frames = 0;
  int m_nvg_image = -1;
  std::chrono::high_resolution_clock::time_point m_start_time;
};

int main() {
  nanogui::init();

  {
    ref<Screen> screen = new Screen(Vector2i(500, 500), "Bouncing Circle - Rlottie NanoGUI");

    ref<Window> window = new Window(screen, "Lottie Animation");
    window->set_position(Vector2i(15, 15));
    window->set_layout(new GroupLayout());

    new LottieCanvas(window);

    screen->set_visible(true);
    screen->perform_layout();

    nanogui::run(RunMode::VSync);
  }

  nanogui::shutdown();
  return 0;
}
