/*
 * flexUI - RenderManager
 *
 * 渲染系统：元素渲染、overlay 渲染
 */

#ifndef FLEXUI_RENDER_MANAGER_H
#define FLEXUI_RENDER_MANAGER_H

#include "render_command.h"
#include "render_frame.h"
#include <flex/runtime/types.h>

namespace flex {
  class Renderer;
  struct RendererCapabilities;
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

  // 渲染一帧
  void render_frame(const RenderFrame& frame);

  // 渲染整个树
  void render_tree(Element* root);

  // 渲染 overlay 层
  void render_overlays(Element* root);

private:
  void render_element(Element* elem, const flex::Transform& parent_transform,
                      const flex::RendererCapabilities& capabilities,
                      const flex::Bounds& viewport, RenderCommandList& commands);
  void render_overlays(Element* root,
                       const flex::RendererCapabilities& capabilities,
                       RenderCommandList& commands);

  Renderer* renderer_;
  RenderCommandCache retained_cache_;
};

} // namespace flexUI

#endif // FLEXUI_RENDER_MANAGER_H
