#include <renderer/template_renderer.h>
#include <renderer/list_renderer.h>
#include <renderer/sequence_renderer.h>
#include <renderer/compare_renderer.h>
#include <renderer/chart_renderer.h>
#include <renderer/hierarchy_renderer.h>
#include <vector>

namespace flex::modules::infographic {

// ========== TemplateRenderer 模板方法实现 ==========

std::string TemplateRenderer::render(const UnifiedInfographic& infographic) {
    SvgContext ctx;
    
    calculate_canvas_size(ctx, infographic);
    render_header(ctx);
    render_title_desc(ctx, infographic);
    render_content(ctx, infographic);
    render_footer(ctx);
    
    return ctx.svg.str();
}

void TemplateRenderer::calculate_canvas_size(SvgContext& ctx, const UnifiedInfographic& infographic) {
    // 默认尺寸，子类可覆盖
    ctx.width = 800;
    ctx.height = 600;
    ctx.content_start_y = 100;
}

void TemplateRenderer::render_header(SvgContext& ctx) {
    ctx.svg << "<svg width=\"" << ctx.width << "\" height=\"" << ctx.height 
            << "\" xmlns=\"http://www.w3.org/2000/svg\">\n"
            << "<defs><style>text{font-family:'Segoe UI',Tahoma,sans-serif;}</style></defs>\n";
}

void TemplateRenderer::render_title_desc(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int y = 40;
    
    if (!infographic.get_title().empty()) {
        svg_text(ctx.svg, ctx.width / 2, y, infographic.get_title(), 24, "#333", "middle", true);
        y += 30;
    }
    
    if (!infographic.get_desc().empty()) {
        svg_text(ctx.svg, ctx.width / 2, y, infographic.get_desc(), 16, "#666", "middle", false);
        y += 30;
    }
    
    ctx.content_start_y = y + 10;
}

void TemplateRenderer::render_footer(SvgContext& ctx) {
    ctx.svg << "</svg>";
}

// ========== SVG 工具方法 ==========

void TemplateRenderer::svg_rect(std::ostream& os, int x, int y, int w, int h,
                                const std::string& fill, int rx, float opacity,
                                const std::string& stroke, int stroke_width) {
    os << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
       << "\" fill=\"" << fill << "\"";
    if (rx > 0) os << " rx=\"" << rx << "\"";
    if (opacity < 1.0f) os << " opacity=\"" << opacity << "\"";
    if (!stroke.empty()) os << " stroke=\"" << stroke << "\" stroke-width=\"" << stroke_width << "\"";
    os << "/>\n";
}

void TemplateRenderer::svg_circle(std::ostream& os, int cx, int cy, int r,
                                  const std::string& fill, float opacity,
                                  const std::string& stroke, int stroke_width) {
    os << "<circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << r
       << "\" fill=\"" << fill << "\"";
    if (opacity < 1.0f) os << " opacity=\"" << opacity << "\"";
    if (!stroke.empty()) os << " stroke=\"" << stroke << "\" stroke-width=\"" << stroke_width << "\"";
    os << "/>\n";
}

void TemplateRenderer::svg_line(std::ostream& os, int x1, int y1, int x2, int y2,
                                const std::string& stroke, int stroke_width) {
    os << "<line x1=\"" << x1 << "\" y1=\"" << y1 << "\" x2=\"" << x2 << "\" y2=\"" << y2
       << "\" stroke=\"" << stroke << "\" stroke-width=\"" << stroke_width << "\"/>\n";
}

void TemplateRenderer::svg_text(std::ostream& os, int x, int y, const std::string& text,
                                int font_size, const std::string& fill,
                                const std::string& anchor, bool bold) {
    os << "<text x=\"" << x << "\" y=\"" << y << "\" text-anchor=\"" << anchor
       << "\" font-size=\"" << font_size << "\" fill=\"" << fill << "\"";
    if (bold) os << " font-weight=\"bold\"";
    os << ">" << escape_xml(text) << "</text>\n";
}

void TemplateRenderer::svg_path(std::ostream& os, const std::string& d,
                                const std::string& fill, const std::string& stroke,
                                int stroke_width) {
    os << "<path d=\"" << d << "\" fill=\"" << fill << "\" stroke=\"" << stroke
       << "\" stroke-width=\"" << stroke_width << "\"/>\n";
}

std::string TemplateRenderer::get_color(const Theme& theme, size_t index) {
    if (theme.palette.empty()) {
        static const std::vector<std::string> default_palette = {
            "#3b82f6", "#ef4444", "#22c55e", "#f59e0b", "#8b5cf6", "#ec4899"
        };
        return default_palette[index % default_palette.size()];
    }
    return theme.palette[index % theme.palette.size()];
}

std::string TemplateRenderer::escape_xml(const std::string& text) {
    std::string result;
    result.reserve(text.size() * 1.1);
    
    for (char c : text) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default:   result += c; break;
        }
    }
    return result;
}

// ========== RendererFactory 实现 ==========

std::unique_ptr<TemplateRenderer> RendererFactory::create(TemplateType type) {
    TemplateCategory category = get_template_category(type);
    return create(category, type);
}

std::unique_ptr<TemplateRenderer> RendererFactory::create(TemplateCategory category, TemplateType type) {
    switch (category) {
        case TemplateCategory::List:      return create_list_renderer(type);
        case TemplateCategory::Sequence:  return create_sequence_renderer(type);
        case TemplateCategory::Compare:   return create_compare_renderer(type);
        case TemplateCategory::Chart:     return create_chart_renderer(type);
        case TemplateCategory::Hierarchy: return create_hierarchy_renderer(type);
        case TemplateCategory::Quadrant:  return create_quadrant_renderer(type);
        case TemplateCategory::Relation:  return create_relation_renderer(type);
        case TemplateCategory::Flowchart: return create_flowchart_renderer(type);
        case TemplateCategory::Process:   return create_process_renderer(type);
        default:                          return create_list_renderer(type);
    }
}

std::unique_ptr<TemplateRenderer> RendererFactory::create_list_renderer(TemplateType type) {
    switch (type) {
        case TemplateType::ListRowHorizontalIconArrow:
        case TemplateType::ListRowSimpleIllus:
            return std::make_unique<ListRowsRenderer>();
        case TemplateType::ListColumnDoneList:
        case TemplateType::ListColumnVerticalIconArrow:
        case TemplateType::ListColumnSimpleVerticalArrow:
            return std::make_unique<ListColumnRenderer>();
        case TemplateType::ListGridCandyCardLite:
            return std::make_unique<ListCandyGridRenderer>();
        default:
            return std::make_unique<ListGridRenderer>();
    }
}

std::unique_ptr<TemplateRenderer> RendererFactory::create_sequence_renderer(TemplateType type) {
    switch (type) {
        case TemplateType::SequenceFunnelSimple:
        case TemplateType::SequenceFilterMeshSimple:
            return std::make_unique<FunnelRenderer>();
        case TemplateType::SequenceCircularSimple:
            return std::make_unique<CircularRenderer>();
        case TemplateType::SequenceRoadmapVerticalSimple:
        case TemplateType::SequenceRoadmapVerticalPlainText:
            return std::make_unique<VerticalRoadmapRenderer>();
        default:
            return std::make_unique<TimelineRenderer>();
    }
}

std::unique_ptr<TemplateRenderer> RendererFactory::create_compare_renderer(TemplateType type) {
    if (type == TemplateType::CompareSwot) {
        return std::make_unique<SwotRenderer>();
    }
    return std::make_unique<VsLayoutRenderer>();
}

std::unique_ptr<TemplateRenderer> RendererFactory::create_chart_renderer(TemplateType type) {
    switch (type) {
        case TemplateType::ChartPieDonutPlainText:
        case TemplateType::ChartPieDonutPillBadge:
            return std::make_unique<DonutChartRenderer>();
        case TemplateType::ChartBarPlainText:
        case TemplateType::ChartColumnSimple:
            return std::make_unique<BarChartRenderer>();
        default:
            return std::make_unique<PieChartRenderer>();
    }
}

std::unique_ptr<TemplateRenderer> RendererFactory::create_hierarchy_renderer(TemplateType) {
    return std::make_unique<HierarchyTreeRenderer>();
}

std::unique_ptr<TemplateRenderer> RendererFactory::create_quadrant_renderer(TemplateType) {
    return std::make_unique<SwotRenderer>();  // 四象限复用 SWOT 布局
}

std::unique_ptr<TemplateRenderer> RendererFactory::create_relation_renderer(TemplateType) {
    return std::make_unique<CircularRenderer>();  // 关系图复用圆形布局
}

std::unique_ptr<TemplateRenderer> RendererFactory::create_flowchart_renderer(TemplateType) {
    return std::make_unique<VerticalRoadmapRenderer>();  // 流程图复用垂直路线图
}

std::unique_ptr<TemplateRenderer> RendererFactory::create_process_renderer(TemplateType) {
    return std::make_unique<TimelineRenderer>();  // 过程图复用时间线
}

} // namespace flex::modules::infographic
