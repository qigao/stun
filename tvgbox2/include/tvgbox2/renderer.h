/*
 * tvgbox2 - Renderer (Placeholder)
 *
 * ThorVG 渲染辅助类（未来实现）
 */

#ifndef TVGBOX2_RENDERER_H
#define TVGBOX2_RENDERER_H

namespace tvg {
  class Canvas;
  class Shape;
  class Text;
}

namespace tvgbox2 {

/**
 * Renderer - ThorVG 渲染辅助类
 *
 * 提供便捷的 ThorVG Paint 创建方法
 */
class Renderer {
public:
  explicit Renderer(tvg::Canvas* canvas) : canvas_(canvas) {}

  tvg::Canvas* canvas() { return canvas_; }

  // TODO: 添加便捷的 Paint 创建方法
  // tvg::Shape* create_rect(float x, float y, float w, float h);
  // tvg::Text* create_text(const std::string& text, float x, float y);

private:
  tvg::Canvas* canvas_;
};

} // namespace tvgbox2

#endif // TVGBOX2_RENDERER_H
