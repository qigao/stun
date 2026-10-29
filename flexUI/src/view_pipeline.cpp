#include <flexUI/view_pipeline.h>

#include <stdexcept>

namespace flexUI {

ViewPipelineResult ViewPipeline::update(ViewPipelineHost& host) {
  ViewPipelineResult result{};

  if (!host.has_view_root()) {
    set_state(ViewLifecycleState::Idle);
    last_result_ = result;
    return last_result_;
  }

  for (int pass = 0; pass < 4; ++pass) {
    if (host.needs_style_stage()) {
      set_state(ViewLifecycleState::Styling);
      host.run_style_stage();
      result.styled = true;
      set_state(ViewLifecycleState::Styled);
    }

    if (host.needs_layout_stage()) {
      set_state(ViewLifecycleState::Layout);
      host.run_layout_stage();
      result.laid_out = true;
      set_state(ViewLifecycleState::LaidOut);
    }

    host.run_layout_semantics_stage();

    if (host.needs_style_stage() || host.needs_layout_stage()) {
      continue;
    }

    if (pass == 0 && host.run_container_query_stage()) {
      continue;
    }
    break;
  }

  // The pass budget bounds recursive widget/state invalidations. Never send a
  // frame using styles or layout that are still known to be pending.
  if (host.needs_style_stage() || host.needs_layout_stage()) {
    throw std::logic_error(
        "ViewPipeline style/layout did not stabilize within four passes");
  }

  host.run_positioning_stage();
  if (host.needs_style_stage() || host.needs_layout_stage()) {
    throw std::logic_error(
        "ViewPipeline positioning invalidated style/layout before render");
  }

  if (!host.needs_render_stage()) {
    if (state_ != ViewLifecycleState::Styled) {
      set_state(ViewLifecycleState::LaidOut);
    }
    result.state = state_;
    last_result_ = result;
    return last_result_;
  }

  set_state(ViewLifecycleState::Rendering);
  host.run_render_stage();
  result.rendered = true;
  set_state(ViewLifecycleState::Rendered);

  result.state = state_;
  last_result_ = result;
  return last_result_;
}

} // namespace flexUI
