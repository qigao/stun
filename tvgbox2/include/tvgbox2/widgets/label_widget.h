/*
 * tvgbox2 - LabelWidget
 *
 * 文本标签控件 - 所有行为由 CSS 定义
 */

#ifndef TVGBOX2_LABEL_WIDGET_H
#define TVGBOX2_LABEL_WIDGET_H

#include "../widget.h"
#include <string>

namespace tvgbox2 {

/**
 * LabelWidget - 文本标签
 *
 * 设计理念：纯渲染，无交互
 *
 * CSS 变量支持：
 *   --text-overflow: "ellipsis" | "clip"  // 文本溢出处理
 *   --white-space: "normal" | "nowrap"    // 空白处理
 *   --max-lines: "3"                       // 最大行数
 *   --line-clamp: "2"                      // 行数限制（现代 CSS）
 *   --text-align: "left" | "center" | "right"
 *   --vertical-align: "top" | "middle" | "bottom"
 *
 * 示例用法（CSS）：
 *   label.title {
 *     font-size: 24px;
 *     font-weight: 700;
 *     color: #000;
 *   }
 *   label.description {
 *     --text-overflow: ellipsis;
 *     --max-lines: 3;
 *     color: #666;
 *   }
 */
class LabelWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param text 文本内容（可选，也可以通过 Element.text_content 设置）
   */
  explicit LabelWidget(const std::string& text = "");

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "LabelWidget"; }

  // ========================================================================
  // 文本访问
  // ========================================================================

  const std::string& text() const { return text_; }
  void set_text(const std::string& text) { text_ = text; dirty_ = true; }

private:
  // ========== 渲染辅助 ==========

  void render_text(tvg::Scene* scene, const Element& elem);

  // ========== 文本处理 ==========

  std::string process_text(const std::string& text, const Element& elem);
  std::string apply_ellipsis(const std::string& text, float max_width, float font_size);

  // ========== 状态 ==========

  std::string text_;
};

} // namespace tvgbox2

#endif // TVGBOX2_LABEL_WIDGET_H
