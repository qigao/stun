#include "flexchart/image/image_parser.h"
#include "chart_component_internal.h"
#include "flexchart/mark_renderer_registry.h"

extern "C" void flexchart_image_force_link(void) {}

namespace flex {
namespace chart {

class ImageMarkRenderer : public MarkRenderer {
public:
    void render(const std::shared_ptr<AstMark>& mark,
                const std::vector<Record>& records,
                MarkRenderContext& ctx) override;
};

void ImageMarkRenderer::render(const std::shared_ptr<AstMark>& mark, const std::vector<Record>& records, MarkRenderContext& ctx) {
    if (!mark || records.empty() || !ctx.overlay) return;

    std::string x_field, y_field, url_field;
    for (const auto& enc : mark->encodings) {
        if (enc->channel == "x") x_field = enc->field;
        else if (enc->channel == "y") y_field = enc->field;
        else if (enc->channel == "url") url_field = enc->field;
    }
    if (x_field.empty() || y_field.empty() || url_field.empty()) return;

    float image_width = 32.0f;
    float image_height = 32.0f;
    auto width_style = mark->styles.find("img-width");
    if (width_style != mark->styles.end())
        image_width = static_cast<float>(get_double_val(width_style->second));
    auto height_style = mark->styles.find("img-height");
    if (height_style != mark->styles.end())
        image_height = static_cast<float>(get_double_val(height_style->second));

    for (size_t i = 0; i < records.size(); ++i) {
        const std::string source = get_string_val(records[i].get(url_field));
        if (source.empty()) continue;

        auto image = ctx.arena.create<Image>();
        if (!image) return;
        image->set_id("image-mark-" + std::to_string(i));
        image->set_src(source);
        image->set_width(image_width);
        image->set_height(image_height);
        image->set_fit(ImageFit::Contain);
        image->set_position_absolute(true);
        image->set_anchor(Anchor::Center);
        image->set_position(
            get_x_pos(get_string_val(records[i].get(x_field)), ctx),
            ctx.estimated_plot_h -
                static_cast<float>(get_double_val(records[i].get(y_field))) * ctx.y_scale);
        ctx.overlay->add_child(image);
    }
}

} // namespace chart
} // namespace flex
REGISTER_MARK_RENDERER("image", flex::chart::ImageMarkRenderer)
