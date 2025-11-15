#include <nanogui/canvas.h>
#include <lunasvg.h>
#include <iostream>

using namespace nanogui;

class TestSVGWidget : public Canvas {
public:
  TestSVGWidget(Widget *parent) : Canvas(parent, 1, false, false, true) {
    // Create a simple test SVG
    std::string svg_data = R"(<svg width="200" height="200" xmlns="http://www.w3.org/2000/svg">
      <rect x="10" y="10" width="180" height="180" fill="red" stroke="blue" stroke-width="4"/>
      <circle cx="100" cy="100" r="50" fill="yellow"/>
    </svg>)";
    
    // Load SVG - exactly like the working example
    m_document = lunasvg::Document::loadFromData(svg_data);
    if (!m_document) {
      std::cerr << "Failed to load SVG" << std::endl;
      return;
    }
    
    // Render to bitmap - exactly like the working example
    m_bitmap = m_document->renderToBitmap(200, 200);
    if (m_bitmap.isNull()) {
      std::cerr << "Failed to render bitmap" << std::endl;
      return;
    }
    
    m_width = m_bitmap.width();
    m_height = m_bitmap.height();
    
    std::cout << "TestSVGWidget: SVG rendered " << m_width << "x" << m_height << std::endl;
    
    set_size(Vector2i(static_cast<int>(m_width), static_cast<int>(m_height)));
  }
  
  virtual void draw(NVGcontext *ctx) override {
    Canvas::draw(ctx);
    
    if (m_bitmap.isNull())
      return;
    
    // Create NanoVG image on first draw - exactly like the working example
    if (m_nvg_image == -1) {
      m_nvg_image = nvgCreateImageRGBA(ctx, static_cast<int>(m_width), static_cast<int>(m_height),
                                       0, m_bitmap.data());
      std::cout << "TestSVGWidget: Created NanoVG image: " << m_nvg_image << std::endl;
    }
    
    // Draw the image - exactly like the working example
    if (m_nvg_image != -1) {
      NVGpaint img_paint = nvgImagePattern(ctx, m_pos.x(), m_pos.y(), static_cast<float>(m_width),
                                           static_cast<float>(m_height), 0, m_nvg_image, 1.0f);
      nvgBeginPath(ctx);
      nvgRect(ctx, m_pos.x(), m_pos.y(), static_cast<float>(m_width), static_cast<float>(m_height));
      nvgFillPaint(ctx, img_paint);
      nvgFill(ctx);
      
      std::cout << "TestSVGWidget: Drew image at (" << m_pos.x() << ", " << m_pos.y() << ")" << std::endl;
    }
  }
  
private:
  std::unique_ptr<lunasvg::Document> m_document;
  lunasvg::Bitmap m_bitmap;
  uint32_t m_width = 0, m_height = 0;
  int m_nvg_image = -1;
};
