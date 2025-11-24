#include <iostream>
#include <lunasvg.h>
#include <nanogui.h>
#include <vector>

using namespace nanogui;
using namespace lunasvg;

class SVGCanvas : public Canvas {
public:
  SVGCanvas(Widget *parent, const std::string &svg_path) : Canvas(parent, 1, false, false, true) {
    // Load SVG
    m_document = Document::loadFromFile(svg_path);
    if (!m_document) {
      std::cerr << "Failed to load SVG: " << svg_path << std::endl;
      return;
    }

    // Render to bitmap at 512x512
    m_bitmap = m_document->renderToBitmap(512, 512);
    if (m_bitmap.isNull()) {
      std::cerr << "Failed to render bitmap" << std::endl;
      return;
    }

    m_width = m_bitmap.width();
    m_height = m_bitmap.height();

    std::cout << "SVG rendered: " << m_width << "x" << m_height << std::endl;

    set_size(Vector2i(static_cast<int>(m_width), static_cast<int>(m_height)));
  }

  virtual void draw(NVGcontext *ctx) override {
    Canvas::draw(ctx);

    if (m_bitmap.isNull())
      return;

    // Create NanoVG image on first draw
    if (m_nvg_image == -1) {
      m_nvg_image = nvgCreateImageRGBA(ctx, static_cast<int>(m_width), static_cast<int>(m_height),
                                       0, m_bitmap.data());
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

private:
  std::unique_ptr<Document> m_document;
  Bitmap m_bitmap;
  uint32_t m_width = 0, m_height = 0;
  int m_nvg_image = -1;
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <svg_file>" << std::endl;
    return 1;
  }

  nanogui::init();

  {
    ref<Screen> screen = new Screen(Vector2i(800, 600), "LunaSVG + NanoGUI");

    ref<Window> window = new Window(screen, "SVG Viewer");
    window->set_position(Vector2i(15, 15));
    window->set_layout(new GroupLayout());

    new SVGCanvas(window, argv[1]);

    screen->set_visible(true);
    screen->perform_layout();

    nanogui::run(RunMode::VSync);
  }

  nanogui::shutdown();
  return 0;
}
