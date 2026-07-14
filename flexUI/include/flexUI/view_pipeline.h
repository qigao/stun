/*
 * flexUI - ViewPipeline
 *
 * Coordinates the CSS-driven UI pipeline:
 * Element tree -> computed style -> layout tree -> backend draw stream.
 */

#ifndef FLEXUI_VIEW_PIPELINE_H
#define FLEXUI_VIEW_PIPELINE_H

namespace flexUI {

enum class ViewLifecycleState {
  Idle,
  Styling,
  Styled,
  Layout,
  LaidOut,
  Rendering,
  Rendered,
};

struct ViewPipelineResult {
  ViewLifecycleState state = ViewLifecycleState::Idle;
  bool styled = false;
  bool laid_out = false;
  bool rendered = false;
};

class ViewPipelineHost {
public:
  virtual ~ViewPipelineHost() = default;

  virtual bool has_view_root() const = 0;
  virtual bool needs_style_stage() const = 0;
  virtual bool needs_layout_stage() const = 0;
  virtual bool needs_render_stage() const = 0;
  virtual void run_style_stage() = 0;
  virtual void run_layout_stage() = 0;
  virtual void run_layout_semantics_stage() = 0;
  virtual bool run_container_query_stage() = 0;
  virtual void run_positioning_stage() = 0;
  virtual void run_render_stage() = 0;
};

class ViewPipeline {
public:
  ViewPipelineResult update(ViewPipelineHost& host);

  ViewLifecycleState state() const { return state_; }
  const ViewPipelineResult& last_result() const { return last_result_; }

private:
  void set_state(ViewLifecycleState state) { state_ = state; }

  ViewLifecycleState state_ = ViewLifecycleState::Idle;
  ViewPipelineResult last_result_{};
};

} // namespace flexUI

#endif // FLEXUI_VIEW_PIPELINE_H
