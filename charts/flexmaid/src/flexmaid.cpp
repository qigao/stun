#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <map>
#include <set>
#include <cmath>
#include <queue>
#include <numeric>
#include "flexmaid/flexmaid.h"
#include "flexmaid/mermaid_component.h"
#include "flexmaid/layout/dagre_layout.h"
#include "flexmaid/render/chart_renderer.h"
#include "flexmaid/render/block_renderer.h"
#include "flexmaid/render/c4_renderer.h"
#include "flexmaid/render/class_renderer.h"
#include "flexmaid/render/er_renderer.h"
#include "flexmaid/render/flowchart_renderer.h"

namespace flex {
namespace modules {
namespace flexmaid {

std::string FlexMaid::escape_xml(const std::string& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c;
        }
    }
    return out;
}

class UniversalRenderer : public ChartRenderer {
public:
    UniversalRenderer(DagreLayoutEngine* engine) : engine_(engine) {}
protected:
    LayoutData do_layout(const UnifiedDiagram& diagram, const Theme& theme) override {
        LayoutData data;
        if (engine_) {
            // Default to flowchart layout for unknown types
            engine_->layout_flowchart(diagram, data, theme); 
        }
        return data;
    }
private:
    DagreLayoutEngine* engine_;
};

FlexMaid::FlexMaid() : theme_(Theme::light()), dagre_engine_(std::make_unique<DagreLayoutEngine>()) {
    // Strategy Pattern: Register diagram-specific renderers
    renderers_[DiagramType::Block] = std::make_unique<BlockRenderer>();
    renderers_[DiagramType::C4] = std::make_unique<C4Renderer>();
    renderers_[DiagramType::Class] = std::make_unique<ClassRenderer>();
    renderers_[DiagramType::ER] = std::make_unique<ERRenderer>();
    renderers_[DiagramType::Flowchart] = std::make_unique<FlowchartRenderer>();
    
    // Register layout functions for legacy support or special cases
    layouters_[DiagramType::Flowchart] = [this](const UnifiedDiagram& d, LayoutData& data, const Theme& t) { dagre_engine_->layout_flowchart(d, data, t); };
    layouters_[DiagramType::Sequence] = [this](const UnifiedDiagram& d, LayoutData& data, const Theme& t) { dagre_engine_->layout_sequence(d, data, t); };
    layouters_[DiagramType::Class] = [this](const UnifiedDiagram& d, LayoutData& data, const Theme& t) { dagre_engine_->layout_class(d, data, t); };
    layouters_[DiagramType::State] = [this](const UnifiedDiagram& d, LayoutData& data, const Theme& t) { dagre_engine_->layout_state(d, data, t); };
    layouters_[DiagramType::ER] = [this](const UnifiedDiagram& d, LayoutData& data, const Theme& t) { dagre_engine_->layout_er(d, data, t); };
    layouters_[DiagramType::Block] = [this](const UnifiedDiagram& d, LayoutData& data, const Theme& t) { dagre_engine_->layout_block(d, data, t); };
    layouters_[DiagramType::Architecture] = [this](const UnifiedDiagram& d, LayoutData& data, const Theme& t) { dagre_engine_->layout_flowchart(d, data, t); };
    layouters_[DiagramType::GitGraph] = layout_gitgraph;
}

FlexMaid::~FlexMaid() = default;

ParseResult FlexMaid::parse(const std::string& mermaid_text) {
    return parser_.parse(mermaid_text);
}

LayoutResult FlexMaid::layout(const UnifiedDiagram& diagram) {
    LayoutResult result;
    try {
        auto it = layouters_.find(diagram.type);
        if (it != layouters_.end()) {
            it->second(diagram, result.data, theme_);
        } else {
            dagre_engine_->layout_flowchart(diagram, result.data, theme_);
        }
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
    }
    return result;
}

std::string FlexMaid::render(const UnifiedDiagram& diagram) {
    // Strategy Pattern: Priority to specialized renderers
    if (renderers_.count(diagram.type)) {
        return renderers_.at(diagram.type)->render(diagram, theme_);
    }
    
    // Fallback: Template Method via UniversalRenderer
    UniversalRenderer universal(dagre_engine_.get());
    return universal.render(diagram, theme_);
}

std::string FlexMaid::mermaid_to_svg(const std::string& mermaid_text) {
    auto result = parse(mermaid_text);
    if (!result.success) {
        return "<svg width=\"400\" height=\"100\" xmlns=\"http://www.w3.org/2000/svg\">"
               "<text x=\"10\" y=\"50\" fill=\"red\">Parse Error: " + escape_xml(result.get_error()) + "</text></svg>";
    }
    return render(*result.diagram);
}

std::string FlexMaid::render_svg(const UnifiedDiagram& diagram) {
    return render(diagram);
}

std::string FlexMaid::render_svg(const UnifiedDiagram& diagram, const LayoutData& layout) {
    // Legacy support for direct render_svg calls with layout
    struct WrapRenderer : public ChartRenderer {
        LayoutData l;
        WrapRenderer(const LayoutData& layout) : l(layout) {}
        LayoutData do_layout(const UnifiedDiagram&, const Theme&) override { return l; }
    };
    WrapRenderer wrapper(layout);
    return wrapper.render(diagram, theme_);
}

flex::Group* FlexMaid::to_flex(const UnifiedDiagram& diagram, flex::Instance& instance) {
    return MermaidComponent::build(diagram, instance);
}

flex::Group* FlexMaid::to_flex(std::string_view source, flex::Instance& instance) {
    auto result = parse(std::string(source));
    if (!result.success) return nullptr;
    return to_flex(*result.diagram, instance);
}

void FlexMaid::set_theme(const Theme& theme) { theme_ = theme; }
const Theme& FlexMaid::get_theme() const { return theme_; }

void FlexMaid::register_layouter(DiagramType type, Layouter layouter) {
    layouters_[type] = std::move(layouter);
}

void FlexMaid::register_renderer(DiagramType type, std::unique_ptr<IChartRenderer> renderer) {
    renderers_[type] = std::move(renderer);
}

std::pair<float, float> FlexMaid::calculate_text_size(const std::string& text) const {
    // Basic estimation: 0.6 * font_size per character
    float w = text.length() * theme_.font_size * 0.6f;
    float h = theme_.font_size * 1.2f;
    return {w, h};
}

std::pair<float, float> FlexMaid::calculate_node_size(const std::string& label, NodeShape shape) const {
    auto [tw, th] = calculate_text_size(label);
    float w = std::max(80.0f, tw + 20.0f);
    float h = std::max(40.0f, th + 20.0f);
    
    if (shape == NodeShape::Circle || shape == NodeShape::CircleFilled) {
        float r = std::max(w, h) / 2.0f;
        w = h = r * 2.0f;
    }
    return {w, h};
}

void FlexMaid::layout_gitgraph(const UnifiedDiagram& diagram, LayoutData& data, const Theme& theme) {
    // Basic linear layout for gitgraph
    float x = 50, y = 100;
    for (const auto& [id, node] : diagram.nodes) {
        data.node_bounds[id] = {x, y, 40, 40};
        x += 80;
    }
    data.width = x + 50; data.height = 200;
}

} // namespace flexmaid
} // namespace modules
} // namespace flex
