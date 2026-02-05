/*
 * flexUI - RenderManager
 *
 * 渲染系统：元素渲染、overlay 渲染
 */

#ifndef FLEXUI_RENDER_MANAGER_H
#define FLEXUI_RENDER_MANAGER_H

namespace flex {
  class Renderer;
}

namespace flexUI {

class Element;
class Renderer;

/**
 * RenderManager - 渲染管理器
 *
 * 职责：
 * 1. 渲染元素树
 * 2. 渲染 overlay（dropdown、tooltip 等）
 */
class RenderManager {
public:
  explicit RenderManager(Renderer* renderer) : renderer_(renderer) {}

  // 渲染整个树
  void render_tree(Element* root);

  // 渲染 overlay 层
  void render_overlays(Element* root);

private:
  void render_element(Element* elem);

  Renderer* renderer_;
};

} // namespace flexUI

#endif // FLEXUI_RENDER_MANAGER_H
