#include <layout_renderer.h>
#include <render_helpers.h>

namespace flex::modules::infographic {

void BarLayoutRenderer::render(const UnifiedInfographic &info, LayoutRenderContext &ctx) {
  const float START_X = 120, START_Y = 80, BAR_H = 35, GAP = 15, MAX_W = 350;

  double max_val = 0;
  for (const auto &item : info.items) {
    if (item->value)
      max_val = std::max(max_val, *item->value);
  }
  if (max_val <= 0)
    max_val = 100;

  size_t idx = 0;
  for (const auto &item : info.items) {
    float y = START_Y + idx * (BAR_H + GAP);
    float w = item->value ? (float)(*item->value / max_val * MAX_W) : 0;

    auto label = ctx.arena.create<flex::Text>();
    label->set_content(item->label);
    label->set_font_size(13.0f);
    label->set_color(flex::Color(0.3f, 0.3f, 0.3f));
    label->set_position(START_X - 10, y + BAR_H / 2);
    label->set_anchor(flex::Anchor::Right);
    ctx.root->add_child(label);

    auto bar = ctx.arena.create<flex::Shape>();
    bar->set_rect(w, BAR_H, 4.0f);
    bar->set_fill(get_palette_color(info.theme, idx));
    bar->set_position(START_X, y);
    ctx.root->add_child(bar);

    if (item->value) {
      auto val = ctx.arena.create<flex::Text>();
      val->set_content(std::to_string((int)*item->value));
      val->set_font_size(12.0f);
      val->set_color(flex::Color(1, 1, 1));
      val->set_position(START_X + w - 8, y + BAR_H / 2);
      val->set_anchor(flex::Anchor::Right);
      ctx.root->add_child(val);
    }
    idx++;
  }
}

} // namespace flex::modules::infographic
