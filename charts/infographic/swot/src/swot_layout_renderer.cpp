#include <algorithm>
#include <layout_renderer.h>
#include <render_helpers.h>


namespace flex::modules::infographic {

void SwotLayoutRenderer::render(const UnifiedInfographic &info, LayoutRenderContext &ctx) {
  const float QUAD_W = 250, QUAD_H = 180, GAP = 10, START_X = 50, START_Y = 70;
  static const flex::Color colors[] = {
      flex::Color(0.13f, 0.59f, 0.95f), // Strengths - blue
      flex::Color(0.96f, 0.26f, 0.21f), // Weaknesses - red
      flex::Color(0.30f, 0.69f, 0.31f), // Opportunities - green
      flex::Color(1.0f, 0.60f, 0.0f)    // Threats - orange
  };

  for (size_t i = 0; i < std::min((size_t)4, info.items.size()); ++i) {
    float x = 0.0f;
    float y = 0.0f;
    float w = QUAD_W;
    float h = QUAD_H;
    if (ctx.layout && ctx.layout->nodes.size() >= 4) {
      const auto& node = ctx.layout->nodes[i];
      x = node.bounds.x;
      y = node.bounds.y;
      w = node.bounds.width;
      h = node.bounds.height;
    } else {
      const size_t col = i % 2;
      const size_t row = i / 2;
      x = START_X + static_cast<float>(col) * (QUAD_W + GAP);
      y = START_Y + static_cast<float>(row) * (QUAD_H + GAP);
    }

    auto bg = ctx.arena.create<flex::Shape>();
    bg->set_rect(w, h, 8.0f);
    bg->set_fill(colors[i]);
    bg->set_position(x, y);
    ctx.root->add_child(bg);

    auto label = ctx.arena.create<flex::Text>();
    label->set_content(info.items[i]->label);
    label->set_font_size(16.0f);
    label->set_font_weight(flex::FontWeight::Bold);
    label->set_color(flex::Color(1, 1, 1));
    label->set_position(x + 15, y + 20);
    ctx.root->add_child(label);

    float cy = y + 50;
    for (const auto &child : info.items[i]->children) {
      auto bullet = ctx.arena.create<flex::Text>();
      bullet->set_content("• " + child->label);
      bullet->set_font_size(12.0f);
      bullet->set_color(flex::Color(1, 1, 1, 0.9f));
      bullet->set_position(x + 15, cy);
      ctx.root->add_child(bullet);
      cy += 22;
    }
  }
}

void VsCompareLayoutRenderer::render(const UnifiedInfographic &info, LayoutRenderContext &ctx) {
  if (info.items.size() < 2)
    return;

  const float SIDE_W = 220, H = 350, GAP = 60, START_Y = 70;
  float left_x = 50, right_x = left_x + SIDE_W + GAP;

  for (int side = 0; side < 2; ++side) {
    float x = side == 0 ? left_x : right_x;
    auto &item = info.items[side];

    auto bg = ctx.arena.create<flex::Shape>();
    bg->set_rect(SIDE_W, H, 12.0f);
    bg->set_fill(get_palette_color(info.theme, side));
    bg->set_position(x, START_Y);
    ctx.root->add_child(bg);

    auto label = ctx.arena.create<flex::Text>();
    label->set_content(item->label);
    label->set_font_size(20.0f);
    label->set_font_weight(flex::FontWeight::Bold);
    label->set_color(flex::Color(1, 1, 1));
    label->set_position(x + SIDE_W / 2, START_Y + 30);
    label->set_anchor(flex::Anchor::Top);
    ctx.root->add_child(label);

    float cy = START_Y + 70;
    for (const auto &child : item->children) {
      auto bullet = ctx.arena.create<flex::Text>();
      bullet->set_content("✓ " + child->label);
      bullet->set_font_size(13.0f);
      bullet->set_color(flex::Color(1, 1, 1, 0.95f));
      bullet->set_position(x + 20, cy);
      ctx.root->add_child(bullet);
      cy += 28;
    }
  }

  auto vs = ctx.arena.create<flex::Text>();
  vs->set_content("VS");
  vs->set_font_size(24.0f);
  vs->set_font_weight(flex::FontWeight::Bold);
  vs->set_color(flex::Color(0.4f, 0.4f, 0.4f));
  vs->set_position(left_x + SIDE_W + GAP / 2, START_Y + H / 2);
  vs->set_anchor(flex::Anchor::Center);
  ctx.root->add_child(vs);
}

} // namespace flex::modules::infographic
