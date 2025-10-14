#include <nanogui.h>
#include <nanovg.h>
#include <thorvg.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

using namespace nanogui;

namespace {

// Mathematical constants
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.f * kPi;
constexpr float kEpsilon = 1e-4f;

// Gauge constants
constexpr float kGaugeRadiusScale = 0.42f;
constexpr float kGaugeCenterX = 0.5f;
constexpr float kGaugeCenterY = 0.58f;
constexpr float kGaugeStartAngle = 5.f * kPi / 4.f; // 225 degrees
constexpr float kGaugeEndAngle = -kPi / 4.f;        // -45 degrees
constexpr int kGaugeMajorTicks = 10;
constexpr int kGaugeMinorTickDivisions = 5;

// Waveform constants
constexpr int kWaveformSamples = 256;
constexpr int kOscilloscopeSamples = 480;

// Default window size
constexpr int kDefaultWindowWidth = 1240;
constexpr int kDefaultWindowHeight = 640;

constexpr const char *kSampleSvg =
    R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96">
  <defs>
    <linearGradient id="orbGradient" x1="0" x2="0" y1="0" y2="1">
      <stop offset="0%" stop-color="#64B5F6"/>
      <stop offset="100%" stop-color="#283593"/>
    </linearGradient>
  </defs>
  <rect x="4" y="4" width="88" height="88" rx="18" fill="#10142A"/>
  <circle cx="32" cy="34" r="18" fill="url(#orbGradient)"/>
  <circle cx="68" cy="30" r="14" fill="#7E57C2" fill-opacity="0.85"/>
  <path d="M18 74 Q48 56 78 74 L78 86 L18 86 Z" fill="#26C6DA" fill-opacity="0.8"/>
  <path d="M24 44 C36 50 60 50 72 44" stroke="#FFCA28" stroke-width="4" stroke-linecap="round" fill="none"/>
</svg>
)SVG";

// Simple Lottie animation: bouncing circle
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

struct SvgBounds {
  Vector2f min{0.f, 0.f};
  Vector2f max{0.f, 0.f};

  Vector2f size() const { return max - min; }
};

class CircularGauge : public Widget {
public:
  CircularGauge(Widget *parent, std::string label) : Widget(parent), m_label(std::move(label)) {}

  void set_range(float min_value, float max_value) {
    if (max_value <= min_value)
      max_value = min_value + 1.f;
    if (std::abs(m_min - min_value) > kEpsilon || std::abs(m_max - max_value) > kEpsilon) {
      m_min = min_value;
      m_max = max_value;
      set_value(m_value);
    }
  }

  void set_value(float value) {
    const float clamped = std::clamp(value, m_min, m_max);
    m_value = clamped;
  }

  float value() const { return m_value; }

  void set_units(std::string units) { m_units = std::move(units); }

  Vector2i preferred_size_impl(NVGcontext *) const override { return {220, 220}; }

  void draw(NVGcontext *ctx) override {
    Widget::draw(ctx);

    const float px = static_cast<float>(m_pos.x());
    const float py = static_cast<float>(m_pos.y());
    const float pw = static_cast<float>(m_size.x());
    const float ph = static_cast<float>(m_size.y());
    const float cx = px + pw * kGaugeCenterX;
    const float cy = py + ph * kGaugeCenterY;
    const float radius = std::min(pw, ph) * kGaugeRadiusScale;

    const float sweep = kGaugeEndAngle - kGaugeStartAngle;
    const float range = m_max - m_min;
    const float t = range > kEpsilon ? (m_value - m_min) / range : 0.f;
    const float needle_angle = kGaugeStartAngle + sweep * t;

    // Drop shadow / glow
    nvgSave(ctx);
    NVGpaint glowPaint = nvgRadialGradient(ctx, cx, cy, radius * 0.65f, radius * 1.05f,
                                           nvgRGBA(10, 14, 24, 70), nvgRGBA(10, 14, 24, 0));
    nvgBeginPath(ctx);
    nvgRect(ctx, px - 40.f, py - 40.f, pw + 80.f, ph + 80.f);
    nvgCircle(ctx, cx, cy, radius * 1.12f);
    nvgPathWinding(ctx, NVG_HOLE);
    nvgFillPaint(ctx, glowPaint);
    nvgFill(ctx);
    nvgRestore(ctx);

    // Outer bezel with highlight
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius + 12.f);
    NVGpaint bezelPaint = nvgLinearGradient(ctx, cx, cy - radius - 12.f, cx, cy + radius + 12.f,
                                            nvgRGBA(90, 102, 126, 255), nvgRGBA(26, 32, 48, 255));
    nvgFillPaint(ctx, bezelPaint);
    nvgFill(ctx);
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius + 12.f);
    nvgStrokeColor(ctx, nvgRGBA(24, 30, 44, 255));
    nvgStrokeWidth(ctx, 4.f);
    nvgStroke(ctx);

    // Inner bezel
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius + 5.f);
    nvgStrokeColor(ctx, nvgRGBA(120, 136, 160, 120));
    nvgStrokeWidth(ctx, 3.f);
    nvgStroke(ctx);

    // Gauge face gradient
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius);
    NVGpaint facePaint = nvgRadialGradient(ctx, cx, cy, radius * 0.05f, radius,
                                           nvgRGBA(52, 62, 84, 240), nvgRGBA(18, 23, 35, 255));
    nvgFillPaint(ctx, facePaint);
    nvgFill(ctx);

    // Colored segments (green / amber / red)
    auto drawSegment = [&](float fromNorm, float toNorm, NVGcolor color, float thickness) {
      const float a0 = kGaugeStartAngle + sweep * fromNorm;
      const float a1 = kGaugeStartAngle + sweep * toNorm;
      nvgBeginPath(ctx);
      nvgArc(ctx, cx, cy, radius * 0.84f, a0, a1, sweep < 0.f ? NVG_CW : NVG_CCW);
      nvgStrokeColor(ctx, color);
      nvgStrokeWidth(ctx, thickness);
      nvgStroke(ctx);
    };

    drawSegment(0.f, 0.6f, nvgRGBA(80, 200, 120, 220), 8.f);
    drawSegment(0.6f, 0.85f, nvgRGBA(255, 204, 64, 220), 8.f);
    drawSegment(0.85f, 1.f, nvgRGBA(255, 92, 92, 220), 8.f);

    // Secondary arc outline
    nvgBeginPath(ctx);
    nvgArc(ctx, cx, cy, radius * 0.84f, kGaugeStartAngle, kGaugeEndAngle,
           sweep < 0.f ? NVG_CW : NVG_CCW);
    nvgStrokeColor(ctx, nvgRGBA(20, 25, 38, 255));
    nvgStrokeWidth(ctx, 2.f);
    nvgStroke(ctx);

    // Major and minor ticks
    for (int i = 0; i <= kGaugeMajorTicks * kGaugeMinorTickDivisions; ++i) {
      const bool major = (i % kGaugeMinorTickDivisions) == 0;
      const float normalized =
          static_cast<float>(i) / (kGaugeMajorTicks * kGaugeMinorTickDivisions);
      const float at = kGaugeStartAngle + sweep * normalized;
      const float cs = std::cos(at);
      const float sn = std::sin(at);
      const float inner = radius * (major ? 0.72f : 0.76f);
      const float outer = radius * 0.9f;
      const float x0 = cx + cs * inner;
      const float y0 = cy + sn * inner;
      const float x1 = cx + cs * outer;
      const float y1 = cy + sn * outer;
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, x0, y0);
      nvgLineTo(ctx, x1, y1);
      nvgStrokeColor(ctx, major ? nvgRGBA(210, 220, 246, 220) : nvgRGBA(140, 156, 186, 180));
      nvgStrokeWidth(ctx, major ? 2.4f : 1.2f);
      nvgStroke(ctx);

      if (major) {
        const float labelValue = m_min + (range * normalized);
        std::ostringstream tickStream;
        tickStream << std::fixed << std::setprecision(0) << labelValue;
        const float tx = cx + cs * (radius * 0.58f);
        const float ty = cy + sn * (radius * 0.58f);
        nvgFontFace(ctx, "sans");
        nvgFontSize(ctx, 13.f);
        nvgFillColor(ctx, nvgRGBA(214, 224, 246, 220));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(ctx, tx, ty, tickStream.str().c_str(), nullptr);
      }
    }

    // Needle tail and body
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius * 0.12f);
    nvgFillColor(ctx, nvgRGBA(26, 32, 48, 255));
    nvgFill(ctx);

    const float nx = cx + std::cos(needle_angle) * radius * 0.78f;
    const float ny = cy + std::sin(needle_angle) * radius * 0.78f;
    const float tail_x = cx - std::cos(needle_angle) * radius * 0.18f;
    const float tail_y = cy - std::sin(needle_angle) * radius * 0.18f;
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, tail_x, tail_y);
    nvgLineTo(ctx, nx, ny);
    nvgStrokeColor(ctx, nvgRGBA(255, 120, 64, 245));
    nvgStrokeWidth(ctx, 5.f);
    nvgStroke(ctx);

    // Needle hub with highlight
    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, radius * 0.07f);
    NVGpaint hubPaint =
        nvgRadialGradient(ctx, cx - radius * 0.02f, cy - radius * 0.02f, radius * 0.01f,
                          radius * 0.07f, nvgRGBA(255, 180, 120, 255), nvgRGBA(150, 60, 40, 255));
    nvgFillPaint(ctx, hubPaint);
    nvgFill(ctx);

    // Label banner
    const float bannerHeight = radius * 0.22f;
    const float bannerWidth = radius * 0.95f;
    const float bannerX = cx - bannerWidth * 0.5f;
    const float bannerY = cy + radius * 0.2f;
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, bannerX, bannerY, bannerWidth, bannerHeight, bannerHeight * 0.15f);
    NVGpaint bannerPaint = nvgLinearGradient(ctx, bannerX, bannerY, bannerX, bannerY + bannerHeight,
                                             nvgRGBA(20, 26, 40, 220), nvgRGBA(34, 44, 62, 220));
    nvgFillPaint(ctx, bannerPaint);
    nvgFill(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, bannerX, bannerY, bannerWidth, bannerHeight, bannerHeight * 0.15f);
    nvgStrokeColor(ctx, nvgRGBA(90, 102, 128, 180));
    nvgStrokeWidth(ctx, 1.4f);
    nvgStroke(ctx);

    std::ostringstream valueStream;
    valueStream << std::fixed << std::setprecision(1) << m_value;
    if (!m_units.empty())
      valueStream << ' ' << m_units;
    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, bannerHeight * 0.55f);
    nvgFillColor(ctx, nvgRGBA(240, 248, 255, 255));
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, cx, bannerY + bannerHeight * 0.5f, valueStream.str().c_str(), nullptr);

    // Label at top
    const float topTextY = py + ph * 0.16f;
    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, 20.f);
    nvgFillColor(ctx, nvgRGBA(220, 230, 255, 240));
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, cx, topTextY, m_label.c_str(), nullptr);

    // Glass highlight
    nvgSave(ctx);
    nvgBeginPath(ctx);
    nvgArc(ctx, cx, cy, radius, kGaugeStartAngle, kGaugeEndAngle, sweep < 0.f ? NVG_CW : NVG_CCW);
    nvgLineTo(ctx, cx, cy);
    nvgClosePath(ctx);
    NVGpaint glossPaint = nvgLinearGradient(ctx, cx, cy - radius, cx, cy,
                                            nvgRGBA(255, 255, 255, 55), nvgRGBA(255, 255, 255, 0));
    nvgFillPaint(ctx, glossPaint);
    nvgFill(ctx);
    nvgRestore(ctx);
  }

private:
  std::string m_label;
  std::string m_units;
  float m_min = 0.f;
  float m_max = 100.f;
  float m_value = 0.f;
};

class VirtualInstrumentPanel : public Widget {
public:
  VirtualInstrumentPanel(Widget *parent) : Widget(parent) {}

  void set_value(float value) { m_value = std::clamp(value, 0.f, 1.f); }

  void set_online(bool online) { m_online = online; }

  void set_warning(bool warning) { m_warning = warning; }

  Vector2i preferred_size_impl(NVGcontext *) const override { return {240, 160}; }

  void draw(NVGcontext *ctx) override {
    Widget::draw(ctx);

    const float px = static_cast<float>(m_pos.x());
    const float py = static_cast<float>(m_pos.y());
    const float pw = static_cast<float>(m_size.x());
    const float ph = static_cast<float>(m_size.y());

    // Panel background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
    NVGpaint panelPaint = nvgLinearGradient(ctx, px, py, px, py + ph, nvgRGBA(32, 38, 56, 255),
                                            nvgRGBA(18, 24, 38, 255));
    nvgFillPaint(ctx, panelPaint);
    nvgFill(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
    nvgStrokeColor(ctx, nvgRGBA(80, 92, 118, 200));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);

    // Header bar
    const float headerHeight = ph * 0.24f;
    nvgBeginPath(ctx);
    nvgRoundedRectVarying(ctx, px + 1.f, py + 1.f, pw - 2.f, headerHeight, 10.f, 10.f, 4.f, 4.f);
    NVGpaint headerPaint = nvgLinearGradient(
        ctx, px, py, px, py + headerHeight, nvgRGBA(24, 144, 255, 220), nvgRGBA(14, 104, 205, 220));
    nvgFillPaint(ctx, headerPaint);
    nvgFill(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRectVarying(ctx, px + 1.f, py + 1.f, pw - 2.f, headerHeight, 10.f, 10.f, 4.f, 4.f);
    nvgStrokeColor(ctx, nvgRGBA(122, 180, 255, 200));
    nvgStrokeWidth(ctx, 1.2f);
    nvgStroke(ctx);

    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, headerHeight * 0.45f);
    nvgFillColor(ctx, nvgRGBA(240, 248, 255, 255));
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, px + pw * 0.5f, py + headerHeight * 0.5f, "Virtual Instrument", nullptr);

    // Online / Warning indicators
    const float ledRadius = headerHeight * 0.18f;
    const float ledY = py + headerHeight * 0.5f;
    auto drawLed = [&](float cx, NVGcolor activeColor, bool state, const char *label) {
      nvgBeginPath(ctx);
      nvgCircle(ctx, cx, ledY, ledRadius + 2.f);
      nvgFillColor(ctx, nvgRGBA(12, 16, 24, 200));
      nvgFill(ctx);
      nvgBeginPath(ctx);
      nvgCircle(ctx, cx, ledY, ledRadius);
      NVGcolor base = state ? activeColor : nvgRGBA(70, 80, 98, 220);
      nvgFillColor(ctx, base);
      nvgFill(ctx);
      if (state) {
        NVGpaint ledGlow =
            nvgRadialGradient(ctx, cx, ledY, ledRadius * 0.4f, ledRadius, activeColor,
                              nvgRGBAf(activeColor.r, activeColor.g, activeColor.b, 0.f));
        nvgBeginPath(ctx);
        nvgCircle(ctx, cx, ledY, ledRadius * 1.9f);
        nvgFillPaint(ctx, ledGlow);
        nvgFill(ctx);
      }
      nvgFontFace(ctx, "sans");
      nvgFontSize(ctx, ledRadius * 0.9f);
      nvgFillColor(ctx, nvgRGBA(220, 230, 255, 220));
      nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(ctx, cx, ledY + ledRadius * 1.9f, label, nullptr);
    };

    drawLed(px + pw * 0.22f, nvgRGBA(76, 217, 100, 230), m_online, "ONLINE");
    drawLed(px + pw * 0.78f, nvgRGBA(255, 59, 48, 230), m_warning, "ALERT");

    // Digital display background
    const float displayX = px + pw * 0.08f;
    const float displayY = py + headerHeight + ph * 0.07f;
    const float displayW = pw * 0.84f;
    const float displayH = ph * 0.24f;
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, displayX, displayY, displayW, displayH, 6.f);
    NVGpaint displayPaint =
        nvgLinearGradient(ctx, displayX, displayY, displayX, displayY + displayH,
                          nvgRGBA(18, 22, 30, 230), nvgRGBA(8, 10, 16, 240));
    nvgFillPaint(ctx, displayPaint);
    nvgFill(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, displayX, displayY, displayW, displayH, 6.f);
    nvgStrokeColor(ctx, nvgRGBA(98, 110, 140, 150));
    nvgStrokeWidth(ctx, 1.f);
    nvgStroke(ctx);

    float numericValue = m_value * 100.f;
    std::ostringstream readout;
    readout << std::fixed << std::setprecision(2) << numericValue;
    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, displayH * 0.7f);
    nvgFillColor(ctx, nvgRGBA(120, 255, 190, 255));
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, displayX + displayW * 0.5f, displayY + displayH * 0.55f, readout.str().c_str(),
            nullptr);

    // Trend bar
    const float barX = px + pw * 0.08f;
    const float barY = py + headerHeight + ph * 0.45f;
    const float barW = pw * 0.84f;
    const float barH = ph * 0.18f;
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, barX, barY, barW, barH, 6.f);
    nvgFillColor(ctx, nvgRGBA(16, 20, 32, 230));
    nvgFill(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, barX, barY, barW, barH, 6.f);
    nvgStrokeColor(ctx, nvgRGBA(70, 82, 108, 160));
    nvgStrokeWidth(ctx, 1.1f);
    nvgStroke(ctx);

    const float fillW = std::max(barW * m_value, barW * 0.02f);
    NVGcolor barColor = m_warning
                            ? nvgRGBA(255, 96, 96, 230)
                            : (m_online ? nvgRGBA(82, 204, 138, 230) : nvgRGBA(90, 110, 140, 230));
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, barX + 1.f, barY + 1.f, fillW - 2.f, barH - 2.f, 5.f);
    NVGpaint fillPaint =
        nvgLinearGradient(ctx, barX, barY, barX, barY + barH, barColor,
                          nvgRGBAf(barColor.r, barColor.g, barColor.b, 180.f / 255.f));
    nvgFillPaint(ctx, fillPaint);
    nvgFill(ctx);

    // Scale markers
    const int divisions = 10;
    for (int i = 0; i <= divisions; ++i) {
      const float fx = barX + barW * (static_cast<float>(i) / divisions);
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, fx, barY + barH + 2.f);
      nvgLineTo(ctx, fx, barY + barH + (i % 2 == 0 ? 12.f : 8.f));
      nvgStrokeColor(ctx, nvgRGBA(110, 122, 150, i % 2 == 0 ? 200 : 120));
      nvgStrokeWidth(ctx, 1.f);
      nvgStroke(ctx);

      if (i % 2 == 0) {
        std::ostringstream tick;
        tick << i * 10;
        nvgFontFace(ctx, "sans");
        nvgFontSize(ctx, 12.f);
        nvgFillColor(ctx, nvgRGBA(180, 192, 216, 200));
        nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgText(ctx, fx, barY + barH + 14.f, tick.str().c_str(), nullptr);
      }
    }

    // Footer text
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, 14.f);
    nvgFillColor(ctx, nvgRGBA(200, 210, 232, 200));
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, px + pw * 0.5f, py + ph * 0.89f, "ANALOG IN", nullptr);
  }

private:
  float m_value = 0.f;
  bool m_online = true;
  bool m_warning = false;
};

class WaveformDisplay : public Widget {
public:
  WaveformDisplay(Widget *parent) : Widget(parent) {}

  void set_amplitude(float amplitude) { m_amplitude = std::clamp(amplitude, 0.f, 1.f); }

  void set_frequency(float frequency) { m_frequency = std::max(0.1f, frequency); }

  Vector2i preferred_size_impl(NVGcontext *) const override { return {460, 200}; }

  void update(float dt) { m_time += dt; }

  void draw(NVGcontext *ctx) override {
    Widget::draw(ctx);

    const float px = static_cast<float>(m_pos.x());
    const float py = static_cast<float>(m_pos.y());
    const float pw = static_cast<float>(m_size.x());
    const float ph = static_cast<float>(m_size.y());

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 10.f);
    NVGpaint bgPaint = nvgLinearGradient(ctx, px, py, px, py + ph, nvgRGBA(24, 30, 46, 255),
                                         nvgRGBA(12, 16, 26, 255));
    nvgFillPaint(ctx, bgPaint);
    nvgFill(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 10.f);
    nvgStrokeColor(ctx, nvgRGBA(70, 84, 112, 180));
    nvgStrokeWidth(ctx, 1.5f);
    nvgStroke(ctx);

    const float gridLeft = px + pw * 0.06f;
    const float gridRight = px + pw * 0.96f;
    const float gridTop = py + ph * 0.18f;
    const float gridBottom = py + ph * 0.84f;

    const int verticalLines = 10;
    for (int i = 0; i <= verticalLines; ++i) {
      float t = static_cast<float>(i) / verticalLines;
      float x = gridLeft + (gridRight - gridLeft) * t;
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, x, gridTop);
      nvgLineTo(ctx, x, gridBottom);
      nvgStrokeColor(ctx, nvgRGBA(64, 78, 108, i == 0 || i == verticalLines ? 180 : 120));
      nvgStrokeWidth(ctx, (i == 0 || i == verticalLines) ? 1.6f : 1.f);
      nvgStroke(ctx);
    }

    const int horizontalLines = 6;
    for (int i = 0; i <= horizontalLines; ++i) {
      float t = static_cast<float>(i) / horizontalLines;
      float y = gridTop + (gridBottom - gridTop) * t;
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, gridLeft, y);
      nvgLineTo(ctx, gridRight, y);
      nvgStrokeColor(ctx, nvgRGBA(64, 78, 108, i == horizontalLines / 2 ? 200 : 110));
      nvgStrokeWidth(ctx, i == horizontalLines / 2 ? 1.8f : 1.f);
      nvgStroke(ctx);
    }

    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, ph * 0.09f);
    nvgFillColor(ctx, nvgRGBA(220, 230, 255, 220));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(ctx, px + pw * 0.05f, py + ph * 0.06f, "Waveforms", nullptr);

    const float midY = (gridTop + gridBottom) * 0.5f;
    const float amp = (gridBottom - gridTop) * 0.36f * m_amplitude;
    const float omega = m_frequency * kTwoPi;

    auto drawCurve = [&](NVGcolor color, float phaseShift, float amplitudeScale) {
      nvgBeginPath(ctx);
      for (int i = 0; i <= kWaveformSamples; ++i) {
        float t = static_cast<float>(i) / kWaveformSamples;
        float x = gridLeft + (gridRight - gridLeft) * t;
        float y = midY + std::sin(omega * t + phaseShift + m_time * 0.7f) * amp * amplitudeScale;
        if (i == 0)
          nvgMoveTo(ctx, x, y);
        else
          nvgLineTo(ctx, x, y);
      }
      nvgStrokeColor(ctx, color);
      nvgStrokeWidth(ctx, 2.2f);
      nvgStroke(ctx);
    };

    drawCurve(nvgRGBA(126, 214, 255, 255), 0.f, 1.f);
    drawCurve(nvgRGBA(152, 255, 152, 240), kPi * 0.5f, 0.75f);

    // Legend
    const float legendY = gridBottom + ph * 0.08f;
    const float legendBox = ph * 0.08f;
    const float legendGap = pw * 0.12f;

    auto drawLegend = [&](float x, NVGcolor color, const char *text) {
      nvgBeginPath(ctx);
      nvgRoundedRect(ctx, x, legendY, legendBox, legendBox, 3.f);
      nvgFillColor(ctx, color);
      nvgFill(ctx);
      nvgFontFace(ctx, "sans");
      nvgFontSize(ctx, ph * 0.07f);
      nvgFillColor(ctx, nvgRGBA(210, 220, 240, 220));
      nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(ctx, x + legendBox * 1.4f, legendY + legendBox * 0.5f, text, nullptr);
    };

    drawLegend(px + pw * 0.08f, nvgRGBA(126, 214, 255, 255), "sin(x)");
    drawLegend(px + pw * 0.08f + legendGap, nvgRGBA(152, 255, 152, 240), "cos(x)");
  }

private:
  float m_amplitude = 0.6f;
  float m_frequency = 1.0f;
  float m_time = 0.f;
};

class OscilloscopeWidget : public Widget {
public:
  enum class WaveformType { Sine, Cosine, Triangle, Sawtooth, Square };

  OscilloscopeWidget(Widget *parent) : Widget(parent) {}

  void set_amplitude(float amplitude) { m_amplitude = std::clamp(amplitude, 0.f, 1.2f); }

  void set_time_scale(float scale) { m_timeScale = std::clamp(scale, 0.5f, 3.5f); }

  void set_persistence(float persistence) { m_persistence = std::clamp(persistence, 0.05f, 1.f); }

  void set_running(bool running) { m_running = running; }

  void set_waveform_types(WaveformType ch1, WaveformType ch2) {
    m_channel1Type = ch1;
    m_channel2Type = ch2;
  }

  Vector2i preferred_size_impl(NVGcontext *) const override { return {320, 220}; }

  void update(float dt) {
    if (m_running) {
      m_phase += dt * m_timeScale * kTwoPi * 0.9f;
      if (m_phase > kTwoPi)
        m_phase = std::fmod(m_phase, kTwoPi);
    }
  }

  void draw(NVGcontext *ctx) override {
    Widget::draw(ctx);

    const float px = static_cast<float>(m_pos.x());
    const float py = static_cast<float>(m_pos.y());
    const float pw = static_cast<float>(m_size.x());
    const float ph = static_cast<float>(m_size.y());

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
    NVGpaint bgPaint = nvgLinearGradient(ctx, px, py, px, py + ph, nvgRGBA(14, 18, 28, 255),
                                         nvgRGBA(24, 30, 46, 255));
    nvgFillPaint(ctx, bgPaint);
    nvgFill(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
    nvgStrokeColor(ctx, nvgRGBA(62, 78, 116, 200));
    nvgStrokeWidth(ctx, 1.6f);
    nvgStroke(ctx);

    const float gridLeft = px + pw * 0.06f;
    const float gridRight = px + pw * 0.94f;
    const float gridTop = py + ph * 0.12f;
    const float gridBottom = py + ph * 0.88f;

    const int verticalDivs = 8;
    for (int i = 0; i <= verticalDivs; ++i) {
      const float t = static_cast<float>(i) / verticalDivs;
      const float x = gridLeft + (gridRight - gridLeft) * t;
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, x, gridTop);
      nvgLineTo(ctx, x, gridBottom);
      const bool strong = (i % 2) == 0;
      nvgStrokeColor(ctx, nvgRGBA(58, 74, 108, strong ? 190 : 110));
      nvgStrokeWidth(ctx, strong ? 1.6f : 1.f);
      nvgStroke(ctx);
    }

    const int horizontalDivs = 6;
    for (int i = 0; i <= horizontalDivs; ++i) {
      const float t = static_cast<float>(i) / horizontalDivs;
      const float y = gridTop + (gridBottom - gridTop) * t;
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, gridLeft, y);
      nvgLineTo(ctx, gridRight, y);
      const bool center = i == horizontalDivs / 2;
      nvgStrokeColor(ctx, nvgRGBA(58, 74, 108, center ? 210 : 120));
      nvgStrokeWidth(ctx, center ? 1.8f : 1.f);
      nvgStroke(ctx);
    }

    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, ph * 0.1f);
    nvgFillColor(ctx, nvgRGBA(220, 230, 255, 220));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(ctx, px + pw * 0.05f, py + ph * 0.05f, "Oscilloscope", nullptr);
    const float midY = (gridTop + gridBottom) * 0.5f;
    const float amp = (gridBottom - gridTop) * 0.38f * m_amplitude;

    // Waveform sampling function
    auto sampleWave = [&](float angle, WaveformType type) {
      float value = 0.f;
      switch (type) {
      case WaveformType::Sine:
        value = std::sin(angle);
        break;
      case WaveformType::Cosine:
        value = std::cos(angle);
        break;
      case WaveformType::Triangle: {
        // Triangle wave: linear ramp up and down, period = 2π
        float wrapped = std::fmod(angle, kTwoPi);
        if (wrapped < 0.f)
          wrapped += kTwoPi;
        value = 2.f * std::abs((wrapped / kPi) - std::floor(wrapped / kPi + 0.5f)) - 1.f;
        break;
      }
      case WaveformType::Sawtooth: {
        // Sawtooth wave: linear ramp from -1 to 1, period = 2π
        float wrapped = std::fmod(angle, kTwoPi);
        if (wrapped < 0.f)
          wrapped += kTwoPi;
        value = (wrapped / kPi) - 1.f;
        break;
      }
      case WaveformType::Square:
        value = std::sin(angle) >= 0.f ? 1.f : -1.f;
        break;
      }
      return value;
    };

    auto drawTrace = [&](NVGcolor color, float phaseOffset, float attenuation, WaveformType type) {
      nvgBeginPath(ctx);
      for (int i = 0; i <= kOscilloscopeSamples; ++i) {
        const float t = static_cast<float>(i) / kOscilloscopeSamples;
        const float x = gridLeft + (gridRight - gridLeft) * t;
        const float angle = m_phase + t * m_timeScale * 4.f * kPi + phaseOffset;
        float y = midY + sampleWave(angle, type) * amp * attenuation;
        y += std::sin(angle * 0.33f + phaseOffset * 1.1f) * amp * 0.05f;
        if (i == 0)
          nvgMoveTo(ctx, x, y);
        else
          nvgLineTo(ctx, x, y);
      }
      NVGcolor tinted = color;
      tinted.a *= m_persistence;
      nvgStrokeColor(ctx, tinted);
      nvgStrokeWidth(ctx, 2.2f);
      nvgStroke(ctx);
    };

    drawTrace(nvgRGBA(132, 255, 214, 255), 0.f, 1.f, m_channel1Type);
    drawTrace(nvgRGBA(255, 158, 112, 240), kPi * 0.5f, 0.72f, m_channel2Type);

    // Center indicator
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, gridLeft, midY);
    nvgLineTo(ctx, gridRight, midY);
    nvgStrokeColor(ctx, nvgRGBA(200, 212, 230, 60));
    nvgStrokeWidth(ctx, 1.f);
    nvgStroke(ctx);

    // Legend and status
    const float legendY = gridBottom + ph * 0.05f;
    const float boxSize = ph * 0.08f;
    auto legend = [&](float x, NVGcolor c, const char *text) {
      nvgBeginPath(ctx);
      nvgRoundedRect(ctx, x, legendY, boxSize, boxSize, 3.f);
      nvgFillColor(ctx, c);
      nvgFill(ctx);
      nvgFontFace(ctx, "sans");
      nvgFontSize(ctx, ph * 0.06f);
      nvgFillColor(ctx, nvgRGBA(200, 210, 228, 220));
      nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(ctx, x + boxSize * 1.4f, legendY + boxSize * 0.5f, text, nullptr);
    };

    legend(px + pw * 0.1f, nvgRGBA(132, 255, 214, 255),
           waveform_caption(m_channel1Type, false).c_str());
    legend(px + pw * 0.1f + pw * 0.28f, nvgRGBA(255, 158, 112, 240),
           waveform_caption(m_channel2Type, true).c_str());

    std::ostringstream status;
    status << std::fixed << std::setprecision(2) << (m_timeScale) << " ms/div";
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, ph * 0.06f);
    nvgFillColor(ctx, nvgRGBA(180, 192, 214, 200));
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, px + pw * 0.9f, legendY + boxSize * 0.5f, status.str().c_str(), nullptr);
  }

private:
  std::string waveform_caption(WaveformType type, bool channel2) const {
    const char *prefix = channel2 ? "CH2: " : "CH1: ";
    std::string name;
    switch (type) {
    case WaveformType::Sine:
      name = "Sine";
      break;
    case WaveformType::Cosine:
      name = "Cos";
      break;
    case WaveformType::Triangle:
      name = "Triangle";
      break;
    case WaveformType::Sawtooth:
      name = "Saw";
      break;
    case WaveformType::Square:
    default:
      name = "Square";
      break;
    }
    return std::string(prefix) + name;
  }

  float m_amplitude = 0.6f;
  float m_timeScale = 1.0f;
  float m_persistence = 0.8f;
  bool m_running = true;
  float m_phase = 0.f;
  WaveformType m_channel1Type = WaveformType::Sine;
  WaveformType m_channel2Type = WaveformType::Cosine;
};

class SvgCanvas : public Widget {
public:
  SvgCanvas(Widget *parent) : Widget(parent) { set_layout(nullptr); }

  ~SvgCanvas() {
    // Clear pointer to ThorVG resource (owned by parent)
    m_svg = nullptr;
  }

  void set_svg(tvg::Picture *svg, const SvgBounds &bounds) {
    m_svg = svg;
    m_bounds = bounds;
  }

  void set_scale(float scale) { m_scale = scale; }

  float scale() const { return m_scale; }

  Vector2i preferred_size_impl(NVGcontext *) const override { return {560, 480}; }

  void draw(NVGcontext *ctx) override {
    Widget::draw(ctx);

    const float px = static_cast<float>(m_pos.x());
    const float py = static_cast<float>(m_pos.y());
    const float pw = static_cast<float>(m_size.x());
    const float ph = static_cast<float>(m_size.y());

    // Draw background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 16.f);
    NVGpaint bg = nvgLinearGradient(ctx, px, py, px, py + ph, nvgRGBA(38, 46, 76, 255),
                                    nvgRGBA(16, 21, 41, 255));
    nvgFillPaint(ctx, bg);
    nvgFill(ctx);

    if (!m_svg)
      return;

    const Vector2f extent = m_bounds.size();
    if (extent.x() <= kEpsilon || extent.y() <= kEpsilon)
      return;

    const float scaledW = extent.x() * m_scale;
    const float scaledH = extent.y() * m_scale;
    const float originX = m_pos.x() + (m_size.x() - scaledW) * 0.5f;
    const float originY = m_pos.y() + (m_size.y() - scaledH) * 0.5f;

    nvgSave(ctx);
    nvgTranslate(ctx, originX - m_bounds.min.x() * m_scale, originY - m_bounds.min.y() * m_scale);
    nvgScale(ctx, m_scale, m_scale);

    // Render simplified SVG representation
    // Note: Full ThorVG integration would require traversing the scene graph
    // and converting ThorVG primitives to NanoVG calls
    render_svg_fallback(ctx);

    nvgRestore(ctx);

    // Draw border
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 16.f);
    nvgStrokeWidth(ctx, 1.2f);
    nvgStrokeColor(ctx, nvgRGBA(26, 34, 60, 220));
    nvgStroke(ctx);
  }

private:
  void render_svg_fallback(NVGcontext *ctx) {
    // Simplified rendering of the embedded SVG
    // Background rect
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, 4.0f, 4.0f, 88.0f, 88.0f, 18.0f);
    nvgFillColor(ctx, nvgRGBA(16, 20, 42, 255));
    nvgFill(ctx);

    // Blue orb with gradient
    nvgBeginPath(ctx);
    nvgCircle(ctx, 32.0f, 34.0f, 18.0f);
    NVGpaint orbGradient = nvgLinearGradient(
        ctx, 32.0f, 16.0f, 32.0f, 52.0f, nvgRGBA(100, 181, 246, 255), nvgRGBA(40, 53, 147, 255));
    nvgFillPaint(ctx, orbGradient);
    nvgFill(ctx);

    // Purple orb
    nvgBeginPath(ctx);
    nvgCircle(ctx, 68.0f, 30.0f, 14.0f);
    nvgFillColor(ctx, nvgRGBA(126, 87, 194, 217));
    nvgFill(ctx);

    // Cyan wave path
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, 18.0f, 74.0f);
    nvgQuadTo(ctx, 48.0f, 56.0f, 78.0f, 74.0f);
    nvgLineTo(ctx, 78.0f, 86.0f);
    nvgLineTo(ctx, 18.0f, 86.0f);
    nvgClosePath(ctx);
    nvgFillColor(ctx, nvgRGBA(38, 198, 218, 204));
    nvgFill(ctx);

    // Yellow arc
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, 24.0f, 44.0f);
    nvgBezierTo(ctx, 30.0f, 47.0f, 42.0f, 47.0f, 48.0f, 47.0f);
    nvgBezierTo(ctx, 54.0f, 47.0f, 66.0f, 47.0f, 72.0f, 44.0f);
    nvgStrokeColor(ctx, nvgRGBA(255, 202, 40, 255));
    nvgStrokeWidth(ctx, 4.0f);
    nvgLineCap(ctx, NVG_ROUND);
    nvgStroke(ctx);
  }

  tvg::Picture *m_svg = nullptr;
  SvgBounds m_bounds;
  float m_scale = 1.f;
};

class LottieCanvas : public Widget {
public:
  LottieCanvas(Widget *parent) : Widget(parent) { set_layout(nullptr); }

  ~LottieCanvas() {
    if (m_animation) {
      // WORKAROUND: Don't call reset() - it causes heap corruption in ThorVG
      // Instead, release ownership and let ThorVG clean up during term()
      (void)m_animation.release();
    }
  }

  bool load_animation(const char *json_data) {
    std::cout << "Loading Lottie animation..." << std::endl;

    m_animation = tvg::Animation::gen();
    if (!m_animation) {
      std::cerr << "ERROR: Failed to generate ThorVG Animation object" << std::endl;
      return false;
    }

    auto picture = m_animation->picture();
    if (!picture) {
      std::cerr << "ERROR: Failed to get picture from animation" << std::endl;
      return false;
    }

    tvg::Result loadResult = picture->load(json_data, std::strlen(json_data), "lottie", true);
    if (loadResult != tvg::Result::Success) {
      std::cerr << "ERROR: Failed to load Lottie data. Result code: "
                << static_cast<int>(loadResult) << std::endl;
      return false;
    }

    std::cout << "Lottie animation loaded successfully!" << std::endl;

    // Get animation properties
    float duration = m_animation->duration();
    float totalFrames = m_animation->totalFrame();
    std::cout << "Animation duration: " << duration << "s, Total frames: " << totalFrames
              << std::endl;

    float x = 0, y = 0, w = 0, h = 0;
    if (picture->bounds(&x, &y, &w, &h) == tvg::Result::Success) {
      m_bounds.min = {x, y};
      m_bounds.max = {x + w, y + h};
      std::cout << "Animation bounds: x=" << x << ", y=" << y << ", w=" << w << ", h=" << h
                << std::endl;
    }

    m_loaded = true;
    return true;
  }

  void set_playing(bool playing) { m_playing = playing; }
  bool is_playing() const { return m_playing; }

  void set_loop(bool loop) { m_loop = loop; }

  void update(float dt) {
    if (!m_loaded || !m_playing || !m_animation)
      return;

    m_currentTime += dt;
    float duration = m_animation->duration();

    if (m_currentTime >= duration) {
      if (m_loop) {
        m_currentTime = std::fmod(m_currentTime, duration);
      } else {
        m_currentTime = duration;
        m_playing = false;
      }
    }

    // Set animation frame based on current time
    float totalFrames = m_animation->totalFrame();
    float frame = (m_currentTime / duration) * totalFrames;
    m_animation->frame(frame);
  }

  Vector2i preferred_size_impl(NVGcontext *) const override { return {280, 280}; }

  void draw(NVGcontext *ctx) override {
    Widget::draw(ctx);

    const float px = static_cast<float>(m_pos.x());
    const float py = static_cast<float>(m_pos.y());
    const float pw = static_cast<float>(m_size.x());
    const float ph = static_cast<float>(m_size.y());

    // Draw background
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
    NVGpaint bg = nvgLinearGradient(ctx, px, py, px, py + ph, nvgRGBA(28, 34, 52, 255),
                                    nvgRGBA(16, 20, 36, 255));
    nvgFillPaint(ctx, bg);
    nvgFill(ctx);

    if (!m_loaded || !m_animation)
      return;

    const Vector2f extent = m_bounds.size();
    if (extent.x() <= kEpsilon || extent.y() <= kEpsilon)
      return;

    // Calculate scale to fit
    const float scaleX = (pw * 0.8f) / extent.x();
    const float scaleY = (ph * 0.8f) / extent.y();
    const float scale = std::min(scaleX, scaleY);

    const float scaledW = extent.x() * scale;
    const float scaledH = extent.y() * scale;
    const float originX = px + (pw - scaledW) * 0.5f;
    const float originY = py + (ph - scaledH) * 0.5f;

    nvgSave(ctx);
    nvgTranslate(ctx, originX - m_bounds.min.x() * scale, originY - m_bounds.min.y() * scale);
    nvgScale(ctx, scale, scale);

    // Render Lottie animation
    // Note: This is a simplified rendering. Full implementation would require
    // converting ThorVG's rendered output to NanoVG commands
    render_lottie_fallback(ctx);

    nvgRestore(ctx);

    // Draw border
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
    nvgStrokeWidth(ctx, 1.2f);
    nvgStrokeColor(ctx, nvgRGBA(52, 62, 88, 220));
    nvgStroke(ctx);

    // Draw play/pause indicator
    const float indicatorSize = 24.f;
    const float indicatorX = px + pw - indicatorSize - 10.f;
    const float indicatorY = py + 10.f;

    nvgBeginPath(ctx);
    nvgCircle(ctx, indicatorX + indicatorSize * 0.5f, indicatorY + indicatorSize * 0.5f,
              indicatorSize * 0.5f);
    nvgFillColor(ctx, nvgRGBA(40, 50, 70, 200));
    nvgFill(ctx);

    if (m_playing) {
      // Pause icon
      nvgBeginPath(ctx);
      nvgRect(ctx, indicatorX + 8.f, indicatorY + 6.f, 3.f, 12.f);
      nvgRect(ctx, indicatorX + 13.f, indicatorY + 6.f, 3.f, 12.f);
      nvgFillColor(ctx, nvgRGBA(100, 200, 255, 255));
      nvgFill(ctx);
    } else {
      // Play icon
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, indicatorX + 9.f, indicatorY + 6.f);
      nvgLineTo(ctx, indicatorX + 9.f, indicatorY + 18.f);
      nvgLineTo(ctx, indicatorX + 17.f, indicatorY + 12.f);
      nvgClosePath(ctx);
      nvgFillColor(ctx, nvgRGBA(100, 200, 255, 255));
      nvgFill(ctx);
    }
  }

private:
  void render_lottie_fallback(NVGcontext *ctx) {
    // Simplified rendering of the bouncing ball animation
    // Calculate ball position based on current time
    float duration = m_animation ? m_animation->duration() : 2.0f;
    float t = duration > 0.f ? m_currentTime / duration : 0.f;
    t = std::fmod(t, 1.0f);

    // Bounce animation (0 to 1 and back)
    float bounce = std::abs(std::sin(t * kTwoPi));
    float y = 50.f + bounce * 75.f;

    // Shadow
    float shadowScale = 0.8f + (1.0f - bounce) * 0.4f;
    float shadowOpacity = 30.f + (1.0f - bounce) * 30.f;
    nvgSave(ctx);
    nvgTranslate(ctx, 100.f, 170.f);
    nvgScale(ctx, shadowScale, shadowScale * 0.5f);
    nvgBeginPath(ctx);
    nvgEllipse(ctx, 0.f, 0.f, 30.f, 10.f);
    nvgFillColor(ctx, nvgRGBA(0, 0, 0, static_cast<int>(shadowOpacity)));
    nvgFill(ctx);
    nvgRestore(ctx);

    // Ball
    nvgBeginPath(ctx);
    nvgCircle(ctx, 100.f, y, 30.f);
    NVGpaint ballGradient = nvgRadialGradient(
        ctx, 95.f, y - 5.f, 5.f, 35.f, nvgRGBA(100, 180, 255, 255), nvgRGBA(30, 100, 200, 255));
    nvgFillPaint(ctx, ballGradient);
    nvgFill(ctx);

    // Highlight
    nvgBeginPath(ctx);
    nvgCircle(ctx, 90.f, y - 10.f, 8.f);
    nvgFillColor(ctx, nvgRGBA(200, 230, 255, 150));
    nvgFill(ctx);
  }

  std::unique_ptr<tvg::Animation> m_animation;
  SvgBounds m_bounds;
  bool m_loaded = false;
  bool m_playing = true;
  bool m_loop = true;
  float m_currentTime = 0.f;
};

class LabXScreen : public Screen {
public:
  LabXScreen()
      : Screen({kDefaultWindowWidth, kDefaultWindowHeight}, "labX NanoGUI Demo"), m_minScale(0.45f),
        m_maxScale(2.6f) {
    inc_ref();
    if (!load_svg())
      throw std::runtime_error("Failed to parse embedded SVG data.");

    set_background(Color(22, 25, 38, 255));

    setup_ui();
    initialize_values();
    perform_layout();
  }

  ~LabXScreen() {
    // ThorVG resources should already be cleaned up by cleanup_thorvg_resources()
    if (m_svg || m_canvas || m_lottieCanvas) {
      cleanup_thorvg_resources();
    }
  }

  void draw_all() override {
    update_animations();
    Screen::draw_all();
  }

  void cleanup_thorvg_resources() {
    // Destroy LottieCanvas which holds Animation
    if (m_lottieCanvas) {
      auto it = std::find(m_children.begin(), m_children.end(), m_lottieCanvas);
      if (it != m_children.end()) {
        m_children.erase(it);
      }
      m_lottieCanvas->dec_ref();
      m_lottieCanvas = nullptr;
    }

    // Destroy SvgCanvas which holds Picture pointer
    if (m_canvas) {
      auto it = std::find(m_children.begin(), m_children.end(), m_canvas);
      if (it != m_children.end()) {
        m_children.erase(it);
      }
      m_canvas->dec_ref();
      m_canvas = nullptr;
    }

    // Release ThorVG Picture - don't call reset(), let ThorVG clean up during term()
    if (m_svg) {
      (void)m_svg.release();
      m_svg = nullptr;
    }
  }

private:
  void setup_ui() {
    setup_controls();
    setup_widgets();
  }

  void setup_controls() {

    auto *controls = new Window(this, "Controls");
    controls->set_position({20, 20});
    controls->set_layout(new GroupLayout(10, 8, 4, 0));

    new Label(controls, "SVG inspector", "sans-bold");

    auto *scaleRow = new Widget(controls);
    scaleRow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    new Label(scaleRow, "Scale", "sans");

    m_slider = new Slider(scaleRow);
    m_slider->set_value(0.0f);
    m_slider->set_final_callback([this](float value) { apply_scale(value); });
    m_slider->set_callback([this](float value) { apply_scale(value); });

    m_scaleLabel = new Label(scaleRow, "", "sans-bold");

    m_canvas = new SvgCanvas(this);
    m_canvas->set_svg(m_svg.get(), m_bounds);
    m_canvas->set_position({240, 20});
    m_canvas->set_size({420, 380});

    new Label(controls, "Gauge", "sans-bold");
    Widget *gaugeRow = new Widget(controls);
    gaugeRow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    new Label(gaugeRow, "Value", "sans");
    m_gaugeSlider = new Slider(gaugeRow);
    m_gaugeSlider->set_fixed_width(140);
    m_gaugeValueLabel = new Label(gaugeRow, "", "sans-bold");
    m_gaugeValueLabel->set_fixed_width(80);

    m_gauge = new CircularGauge(this, "Chamber Pressure");
    m_gauge->set_units("kPa");
    m_gauge->set_range(m_gaugeMin, m_gaugeMax);
    m_gauge->set_position({720, 140});
    m_gauge->set_size({260, 260});

    new Label(controls, "Instrument", "sans-bold");
    Widget *instrumentRow = new Widget(controls);
    instrumentRow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    new Label(instrumentRow, "Level", "sans");
    m_instrumentSlider = new Slider(instrumentRow);
    m_instrumentSlider->set_fixed_width(140);
    m_instrumentValueLabel = new Label(instrumentRow, "", "sans-bold");
    m_instrumentValueLabel->set_fixed_width(80);

    Widget *indicatorRow = new Widget(controls);
    indicatorRow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 12));
    m_onlineCheck = new CheckBox(indicatorRow, "Online");
    m_warningCheck = new CheckBox(indicatorRow, "Alert");

    m_instrument = new VirtualInstrumentPanel(this);
    m_instrument->set_position({680, 20});
    m_instrument->set_size({300, 180});

    m_waveform = new WaveformDisplay(this);
    m_waveform->set_position({240, 420});
    m_waveform->set_size({460, 220});

    m_oscilloscope = new OscilloscopeWidget(this);
    m_oscilloscope->set_position({720, 420});
    m_oscilloscope->set_size({300, 220});

    new Label(controls, "Waveforms", "sans-bold");
    Widget *waveAmpRow = new Widget(controls);
    waveAmpRow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    new Label(waveAmpRow, "Amplitude", "sans");
    m_waveAmplitudeSlider = new Slider(waveAmpRow);
    m_waveAmplitudeSlider->set_fixed_width(140);
    m_waveAmplitudeLabel = new Label(waveAmpRow, "", "sans-bold");
    m_waveAmplitudeLabel->set_fixed_width(80);

    Widget *waveFreqRow = new Widget(controls);
    waveFreqRow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    new Label(waveFreqRow, "Frequency", "sans");
    m_waveFrequencySlider = new Slider(waveFreqRow);
    m_waveFrequencySlider->set_fixed_width(140);
    m_waveFrequencyLabel = new Label(waveFreqRow, "", "sans-bold");
    m_waveFrequencyLabel->set_fixed_width(80);

    const std::vector<std::string> waveOptions = {"Sine", "Cosine", "Triangle", "Saw", "Square"};
    Widget *ch1Row = new Widget(controls);
    ch1Row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    new Label(ch1Row, "CH1", "sans");
    m_scopeCh1Combo = new ComboBox(ch1Row, waveOptions);
    m_scopeCh1Combo->set_fixed_width(160);

    Widget *ch2Row = new Widget(controls);
    ch2Row->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    new Label(ch2Row, "CH2", "sans");
    m_scopeCh2Combo = new ComboBox(ch2Row, waveOptions);
    m_scopeCh2Combo->set_fixed_width(160);

    Widget *scopeTimeRow = new Widget(controls);
    scopeTimeRow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    new Label(scopeTimeRow, "Time/div", "sans");
    m_scopeTimeSlider = new Slider(scopeTimeRow);
    m_scopeTimeSlider->set_fixed_width(140);
    m_scopeTimeLabel = new Label(scopeTimeRow, "", "sans-bold");
    m_scopeTimeLabel->set_fixed_width(80);

    Widget *scopePersistRow = new Widget(controls);
    scopePersistRow->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 6));
    new Label(scopePersistRow, "Persistence", "sans");
    m_scopePersistenceSlider = new Slider(scopePersistRow);
    m_scopePersistenceSlider->set_fixed_width(140);
    m_scopePersistenceLabel = new Label(scopePersistRow, "", "sans-bold");
    m_scopePersistenceLabel->set_fixed_width(80);

    m_scopeRunCheck = new CheckBox(controls, "Run");

    const float initialScale = 1.0f;
    const float sliderValue = to_slider(initialScale);
    m_slider->set_value(sliderValue);
    apply_scale(sliderValue);

    const float initialGaugeValue = 42.f;
    const float gaugeSliderValue = (initialGaugeValue - m_gaugeMin) / (m_gaugeMax - m_gaugeMin);
    m_gaugeSlider->set_value(std::clamp(gaugeSliderValue, 0.f, 1.f));
    update_gauge_from_slider(m_gaugeSlider->value());

    m_gaugeSlider->set_callback([this](float value) { update_gauge_from_slider(value); });
    m_gaugeSlider->set_final_callback([this](float value) { update_gauge_from_slider(value); });

    const float initialInstrumentValue = 0.38f;
    m_instrumentSlider->set_value(std::clamp(initialInstrumentValue, 0.f, 1.f));
    update_instrument_from_slider(m_instrumentSlider->value());
    m_instrumentSlider->set_callback([this](float v) { update_instrument_from_slider(v); });
    m_instrumentSlider->set_final_callback([this](float v) { update_instrument_from_slider(v); });

    m_onlineCheck->set_callback([this](bool state) {
      if (m_instrument)
        m_instrument->set_online(state);
    });
    m_onlineCheck->set_checked(true);

    m_warningCheck->set_callback([this](bool state) {
      if (m_instrument)
        m_instrument->set_warning(state);
    });

    const float initialWaveAmplitude = 0.65f;
    const float initialWaveFrequency = 1.4f;
    m_waveAmplitudeSlider->set_value(std::clamp(initialWaveAmplitude, 0.f, 1.f));
    m_waveFrequencySlider->set_value(std::clamp((initialWaveFrequency - m_waveFrequencyMin) /
                                                    (m_waveFrequencyMax - m_waveFrequencyMin),
                                                0.f, 1.f));
    update_waveform_amplitude(m_waveAmplitudeSlider->value());
    update_waveform_frequency(m_waveFrequencySlider->value());
    m_waveAmplitudeSlider->set_callback([this](float v) { update_waveform_amplitude(v); });
    m_waveAmplitudeSlider->set_final_callback([this](float v) { update_waveform_amplitude(v); });
    m_waveFrequencySlider->set_callback([this](float v) { update_waveform_frequency(v); });
    m_waveFrequencySlider->set_final_callback([this](float v) { update_waveform_frequency(v); });

    const float initialScopeTime = 1.2f;
    const float initialScopePersistence = 0.8f;
    m_scopeTimeSlider->set_value(std::clamp(
        (initialScopeTime - m_scopeTimeMin) / (m_scopeTimeMax - m_scopeTimeMin), 0.f, 1.f));
    m_scopePersistenceSlider->set_value(std::clamp(initialScopePersistence, 0.f, 1.f));
    update_scope_time(m_scopeTimeSlider->value());
    update_scope_persistence(m_scopePersistenceSlider->value());
    m_scopeTimeSlider->set_callback([this](float v) { update_scope_time(v); });
    m_scopeTimeSlider->set_final_callback([this](float v) { update_scope_time(v); });
    m_scopePersistenceSlider->set_callback([this](float v) { update_scope_persistence(v); });
    m_scopePersistenceSlider->set_final_callback([this](float v) { update_scope_persistence(v); });

    m_scopeCh1Combo->set_selected_index(0);
    m_scopeCh2Combo->set_selected_index(1);
    update_scope_waveforms();
    m_scopeCh1Combo->set_callback([this](int) { update_scope_waveforms(); });
    m_scopeCh2Combo->set_callback([this](int) { update_scope_waveforms(); });

    m_scopeRunCheck->set_checked(true);
    m_scopeRunCheck->set_callback([this](bool state) {
      if (m_oscilloscope)
        m_oscilloscope->set_running(state);
    });

    if (m_oscilloscope)
      m_oscilloscope->set_running(true);

    perform_layout();
  }

private:
  bool load_svg() {
    std::cout << "Loading SVG data..." << std::endl;
    std::cout << "SVG length: " << std::strlen(kSampleSvg) << " bytes" << std::endl;

    m_svg = tvg::Picture::gen();
    if (!m_svg) {
      std::cerr << "ERROR: Failed to generate ThorVG Picture object" << std::endl;
      return false;
    }

    std::cout << "Attempting to load SVG from string..." << std::endl;
    tvg::Result loadResult = m_svg->load(kSampleSvg, std::strlen(kSampleSvg), "svg", true);

    if (loadResult != tvg::Result::Success) {
      std::cerr << "ERROR: Failed to load SVG data. ThorVG Result code: "
                << static_cast<int>(loadResult) << std::endl;

      // Try alternative loading method
      std::cout << "Trying alternative load method without length..." << std::endl;
      m_svg = tvg::Picture::gen();
      loadResult = m_svg->load(kSampleSvg);

      if (loadResult != tvg::Result::Success) {
        std::cerr << "ERROR: Alternative load also failed. Result code: "
                  << static_cast<int>(loadResult) << std::endl;
        return false;
      }
    }

    std::cout << "SVG loaded successfully!" << std::endl;

    float x = 0, y = 0, w = 0, h = 0;
    tvg::Result boundsResult = m_svg->bounds(&x, &y, &w, &h);

    if (boundsResult != tvg::Result::Success) {
      std::cerr << "ERROR: Failed to get SVG bounds. Result code: "
                << static_cast<int>(boundsResult) << std::endl;
      m_bounds = {};
      return false;
    }

    std::cout << "SVG bounds: x=" << x << ", y=" << y << ", w=" << w << ", h=" << h << std::endl;

    m_bounds.min = {x, y};
    m_bounds.max = {x + w, y + h};
    return true;
  }

  void apply_scale(float sliderValue) {
    sliderValue = std::clamp(sliderValue, 0.f, 1.f);
    const float scale = m_minScale + sliderValue * (m_maxScale - m_minScale);
    if (m_canvas)
      m_canvas->set_scale(scale);
    if (m_scaleLabel) {
      std::ostringstream oss;
      oss << "x" << std::fixed << std::setprecision(2) << scale;
      m_scaleLabel->set_caption(oss.str());
    }
  }

  float to_slider(float scale) const {
    return std::clamp((scale - m_minScale) / (m_maxScale - m_minScale), 0.f, 1.f);
  }

  void update_gauge_from_slider(float sliderValue) {
    sliderValue = std::clamp(sliderValue, 0.f, 1.f);
    const float gaugeValue = m_gaugeMin + sliderValue * (m_gaugeMax - m_gaugeMin);
    if (m_gauge)
      m_gauge->set_value(gaugeValue);
    if (m_gaugeValueLabel) {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(1) << gaugeValue << " kPa";
      m_gaugeValueLabel->set_caption(oss.str());
    }
  }

  void update_instrument_from_slider(float sliderValue) {
    sliderValue = std::clamp(sliderValue, 0.f, 1.f);
    if (m_instrument)
      m_instrument->set_value(sliderValue);
    if (m_instrumentValueLabel) {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(2) << sliderValue * 100.f << "%";
      m_instrumentValueLabel->set_caption(oss.str());
    }
  }

  void update_waveform_amplitude(float sliderValue) {
    sliderValue = std::clamp(sliderValue, 0.f, 1.f);
    if (m_waveform)
      m_waveform->set_amplitude(sliderValue);
    if (m_oscilloscope)
      m_oscilloscope->set_amplitude(std::max(0.2f, sliderValue));
    if (m_waveAmplitudeLabel) {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(0) << sliderValue * 100.f << "%";
      m_waveAmplitudeLabel->set_caption(oss.str());
    }
  }

  void update_waveform_frequency(float sliderValue) {
    sliderValue = std::clamp(sliderValue, 0.f, 1.f);
    const float frequency =
        m_waveFrequencyMin + sliderValue * (m_waveFrequencyMax - m_waveFrequencyMin);
    if (m_waveform)
      m_waveform->set_frequency(frequency);
    if (m_waveFrequencyLabel) {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(2) << frequency << " Hz";
      m_waveFrequencyLabel->set_caption(oss.str());
    }
  }

  void update_scope_time(float sliderValue) {
    sliderValue = std::clamp(sliderValue, 0.f, 1.f);
    const float timeDiv = m_scopeTimeMin + sliderValue * (m_scopeTimeMax - m_scopeTimeMin);
    if (m_oscilloscope)
      m_oscilloscope->set_time_scale(timeDiv);
    if (m_scopeTimeLabel) {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(2) << timeDiv << " ms";
      m_scopeTimeLabel->set_caption(oss.str());
    }
  }

  void update_scope_persistence(float sliderValue) {
    sliderValue = std::clamp(sliderValue, 0.f, 1.f);
    const float persistence = std::max(0.05f, sliderValue);
    if (m_oscilloscope)
      m_oscilloscope->set_persistence(persistence);
    if (m_scopePersistenceLabel) {
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(0) << persistence * 100.f << "%";
      m_scopePersistenceLabel->set_caption(oss.str());
    }
  }

  void update_scope_waveforms() {
    if (!m_oscilloscope)
      return;
    auto toType = [](int idx) {
      switch (idx) {
      case 0:
        return OscilloscopeWidget::WaveformType::Sine;
      case 1:
        return OscilloscopeWidget::WaveformType::Cosine;
      case 2:
        return OscilloscopeWidget::WaveformType::Triangle;
      case 3:
        return OscilloscopeWidget::WaveformType::Sawtooth;
      case 4:
      default:
        return OscilloscopeWidget::WaveformType::Square;
      }
    };
    OscilloscopeWidget::WaveformType ch1 =
        toType(m_scopeCh1Combo ? m_scopeCh1Combo->selected_index() : 0);
    OscilloscopeWidget::WaveformType ch2 =
        toType(m_scopeCh2Combo ? m_scopeCh2Combo->selected_index() : 1);
    m_oscilloscope->set_waveform_types(ch1, ch2);
  }

  void setup_widgets() {
    // SVG canvas
    m_canvas = new SvgCanvas(this);
    m_canvas->set_svg(m_svg.get(), m_bounds);
    m_canvas->set_position({240, 20});
    m_canvas->set_size({420, 380});

    // Gauge
    m_gauge = new CircularGauge(this, "Chamber Pressure");
    m_gauge->set_units("kPa");
    m_gauge->set_range(m_gaugeMin, m_gaugeMax);
    m_gauge->set_position({720, 140});
    m_gauge->set_size({260, 260});

    // Instrument panel
    m_instrument = new VirtualInstrumentPanel(this);
    m_instrument->set_position({680, 20});
    m_instrument->set_size({300, 180});

    // Waveform display
    m_waveform = new WaveformDisplay(this);
    m_waveform->set_position({240, 420});
    m_waveform->set_size({420, 200});

    // Lottie animation
    m_lottieCanvas = new LottieCanvas(this);
    m_lottieCanvas->set_position({680, 420});
    m_lottieCanvas->set_size({240, 200});
    if (!m_lottieCanvas->load_animation(kLottieAnimation)) {
      std::cerr << "WARNING: Failed to load Lottie animation" << std::endl;
    }

    // Oscilloscope
    m_oscilloscope = new OscilloscopeWidget(this);
    m_oscilloscope->set_position({940, 420});
    m_oscilloscope->set_size({280, 200});
  }

  void initialize_values() {
    // SVG scale
    const float initialScale = 1.0f;
    m_slider->set_value(to_slider(initialScale));
    apply_scale(m_slider->value());

    // Gauge
    const float initialGaugeValue = 42.f;
    m_gaugeSlider->set_value((initialGaugeValue - m_gaugeMin) / (m_gaugeMax - m_gaugeMin));
    update_gauge_from_slider(m_gaugeSlider->value());

    // Instrument
    m_instrumentSlider->set_value(0.38f);
    update_instrument_from_slider(m_instrumentSlider->value());
    m_onlineCheck->set_checked(true);

    // Waveforms
    m_waveAmplitudeSlider->set_value(0.65f);
    m_waveFrequencySlider->set_value((1.4f - m_waveFrequencyMin) /
                                     (m_waveFrequencyMax - m_waveFrequencyMin));
    update_waveform_amplitude(m_waveAmplitudeSlider->value());
    update_waveform_frequency(m_waveFrequencySlider->value());

    // Oscilloscope
    m_scopeTimeSlider->set_value((1.2f - m_scopeTimeMin) / (m_scopeTimeMax - m_scopeTimeMin));
    m_scopePersistenceSlider->set_value(0.8f);
    update_scope_time(m_scopeTimeSlider->value());
    update_scope_persistence(m_scopePersistenceSlider->value());

    m_scopeCh1Combo->set_selected_index(0);
    m_scopeCh2Combo->set_selected_index(1);
    update_scope_waveforms();

    m_scopeRunCheck->set_checked(true);
    if (m_oscilloscope)
      m_oscilloscope->set_running(true);
  }

  void update_animations() {
    float timeNow = static_cast<float>(Screen::get_time());
    if (m_lastTime < 0.f)
      m_lastTime = timeNow;

    const float dt = std::clamp(timeNow - m_lastTime, 0.f, 0.05f);
    m_lastTime = timeNow;

    if (m_waveform)
      m_waveform->update(dt);
    if (m_oscilloscope)
      m_oscilloscope->update(dt);
    if (m_lottieCanvas)
      m_lottieCanvas->update(dt);
  }

  std::unique_ptr<tvg::Picture> m_svg;
  SvgBounds m_bounds;
  SvgCanvas *m_canvas = nullptr;
  LottieCanvas *m_lottieCanvas = nullptr;
  Slider *m_slider = nullptr;
  Label *m_scaleLabel = nullptr;
  CircularGauge *m_gauge = nullptr;
  Slider *m_gaugeSlider = nullptr;
  Label *m_gaugeValueLabel = nullptr;
  VirtualInstrumentPanel *m_instrument = nullptr;
  Slider *m_instrumentSlider = nullptr;
  Label *m_instrumentValueLabel = nullptr;
  CheckBox *m_onlineCheck = nullptr;
  CheckBox *m_warningCheck = nullptr;
  WaveformDisplay *m_waveform = nullptr;
  Slider *m_waveAmplitudeSlider = nullptr;
  Slider *m_waveFrequencySlider = nullptr;
  Label *m_waveAmplitudeLabel = nullptr;
  Label *m_waveFrequencyLabel = nullptr;
  OscilloscopeWidget *m_oscilloscope = nullptr;
  Slider *m_scopeTimeSlider = nullptr;
  Slider *m_scopePersistenceSlider = nullptr;
  Label *m_scopeTimeLabel = nullptr;
  Label *m_scopePersistenceLabel = nullptr;
  CheckBox *m_scopeRunCheck = nullptr;
  ComboBox *m_scopeCh1Combo = nullptr;
  ComboBox *m_scopeCh2Combo = nullptr;
  const float m_minScale;
  const float m_maxScale;
  const float m_gaugeMin = 0.f;
  const float m_gaugeMax = 100.f;
  const float m_waveFrequencyMin = 0.5f;
  const float m_waveFrequencyMax = 3.0f;
  const float m_scopeTimeMin = 0.5f;
  const float m_scopeTimeMax = 3.0f;
  float m_lastTime = -1.f;
};

} // namespace

static int labx_entry(int argc, char **argv) {
  try {
    std::cout << "=== labX NanoGUI Demo Starting ===" << std::endl;
    std::cout << "Initializing ThorVG (Software Canvas Engine, 4 threads)..." << std::endl;

    tvg::Result initResult = tvg::Initializer::init(tvg::CanvasEngine::Sw, 4);
    if (initResult != tvg::Result::Success) {
      std::cerr << "ERROR: ThorVG initialization failed. Result code: "
                << static_cast<int>(initResult) << std::endl;
      return -1;
    }
    std::cout << "ThorVG initialized successfully!" << std::endl;

    std::cout << "Initializing NanoGUI..." << std::endl;
    nanogui::init();
    {
      std::cout << "Creating application window..." << std::endl;
      ref<LabXScreen> app = new LabXScreen();
      app->dec_ref();
      app->set_visible(true);
      std::cout << "Starting main loop..." << std::endl;
      nanogui::run(nanogui::RunMode::VSync);
      app->cleanup_thorvg_resources();
    }
    std::cout << "Shutting down NanoGUI..." << std::endl;
    nanogui::shutdown();
    std::cout << "Terminating ThorVG..." << std::endl;
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
    std::cout << "=== Application exited cleanly ===" << std::endl;
  } catch (const std::exception &e) {
    std::string message = std::string("Fatal error: ") + e.what();
    std::cerr << message << std::endl;
#if defined(_WIN32)
    MessageBoxA(nullptr, message.c_str(), "labX demo", MB_OK | MB_ICONERROR);
#else
    std::cerr << message << std::endl;
#endif
    return -1;
  } catch (...) {
    std::cerr << "Fatal error: unknown exception" << std::endl;
#if defined(_WIN32)
    MessageBoxA(nullptr, "Fatal error: unknown exception", "labX demo", MB_OK | MB_ICONERROR);
#else
    std::cerr << "Fatal error: unknown exception" << std::endl;
#endif
    return -1;
  }
  return 0;
}

int main(int argc, char **argv) { return labx_entry(argc, argv); }
