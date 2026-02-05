#pragma once

#include "renderer.h"
#include <thorvg.h>
#include <memory>

namespace flex::modules::flexmaid {

class ThorVGRenderer : public Renderer {
public:
    ThorVGRenderer();
    virtual ~ThorVGRenderer();

    void render(const DiagramLayout& layout, const Theme& theme) override;
    bool save_svg(const std::string& path) override;
    bool save_png(const std::string& path, int width, int height) override;
    std::string to_svg_string() override;

private:
    void render_nodes();
    void render_edges();
    void render_subgraphs();
    
    // Shape helpers
    void draw_rectangle(const NodeLayout& node);
    void draw_circle(const NodeLayout& node);
    void draw_diamond(const NodeLayout& node);
    void draw_text(float x, float y, const std::vector<std::string>& lines);

    tvg::Scene* root_scene_ = nullptr;
    DiagramLayout layout_;
    Theme theme_;
};

} // namespace flex::modules::flexmaid
