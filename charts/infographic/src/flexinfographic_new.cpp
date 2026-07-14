/**
 * FlexInfographic - 重构后的实现
 */

#include <flexinfographic.h>
#include <infographic_component.h>
#include <renderer/template_renderer.h>
#include <algorithm>
#include <unordered_set>

namespace flex::modules::infographic {

namespace {

std::unique_ptr<DataItem> clone_item(const DataItem& source) {
    auto result = std::make_unique<DataItem>();
    result->label = source.label;
    result->desc = source.desc;
    result->value = source.value;
    result->icon = source.icon;
    result->illus = source.illus;
    result->time = source.time;
    result->done = source.done;
    result->properties = source.properties;
    result->children.reserve(source.children.size());
    for (const auto& child : source.children) result->children.push_back(clone_item(*child));
    return result;
}

UnifiedInfographic clone_with_theme(const UnifiedInfographic& source, const Theme& theme) {
    UnifiedInfographic result;
    result.template_type = source.template_type;
    result.category = source.category;
    result.title = source.title;
    result.desc = source.desc;
    result.theme = theme;
    result.properties = source.properties;
    result.items.reserve(source.items.size());
    for (const auto& item : source.items) result.items.push_back(clone_item(*item));
    return result;
}

} // namespace

FlexInfographic::FlexInfographic() 
    : parser_(std::make_unique<UnifiedParser>())
    , current_theme_(Theme::default_theme()) {
}

FlexInfographic::~FlexInfographic() = default;

// ========== 核心 API ==========

ParseResult FlexInfographic::parse(const std::string& infographic_text) {
    return parser_->parse(infographic_text);
}

std::string FlexInfographic::infographic_to_svg(const std::string& infographic_text) {
    auto result = parse(infographic_text);
    if (!result.success) {
        return "<!-- Parse Error: " + result.get_error() + " -->";
    }
    return render_svg(*result.infographic);
}

std::string FlexInfographic::render_svg(const UnifiedInfographic& infographic) {
    auto renderer = RendererFactory::create(infographic.template_type);
    if (!has_theme_override_) return renderer->render(infographic);
    auto themed = clone_with_theme(infographic, current_theme_);
    return renderer->render(themed);
}

flex::Group* FlexInfographic::to_flex(const UnifiedInfographic& infographic, flex::Instance& instance) {
    if (!has_theme_override_) return InfographicComponent::build(infographic, instance);
    auto themed = clone_with_theme(infographic, current_theme_);
    return InfographicComponent::build(themed, instance);
}

flex::Group* FlexInfographic::to_flex(std::string_view source, flex::Instance& instance) {
    auto result = parse(std::string(source));
    if (!result.success) return nullptr;
    return to_flex(*result.infographic, instance);
}

// ========== 主题管理 ==========

void FlexInfographic::set_theme(const Theme& theme) {
    current_theme_ = theme;
    has_theme_override_ = true;
}

const Theme& FlexInfographic::get_theme() const {
    return current_theme_;
}

// ========== 模板信息 ==========

std::vector<std::string> FlexInfographic::get_available_templates() {
    return {
        // List
        "list-grid-badge-card", "list-grid-candy-card-lite", "list-column-done-list",
        "list-row-horizontal-icon-arrow", "list-grid-ribbon-card",
        // Sequence
        "sequence-timeline-simple", "sequence-roadmap-vertical-simple",
        "sequence-funnel-simple", "sequence-circular-simple",
        // Compare
        "compare-binary-horizontal-underline-text-vs", "compare-swot",
        // Hierarchy
        "hierarchy-tree-tech-style-capsule-item", "hierarchy-structure",
        // Chart
        "chart-pie-donut-plain-text", "chart-pie-plain-text", "chart-bar-plain-text",
        // Quadrant
        "quadrant-quarter-simple-card",
        // Relation
        "relation-circle-icon-badge"
    };
}

std::vector<std::string> FlexInfographic::get_templates_by_category(TemplateCategory category) {
    auto all = get_available_templates();
    std::vector<std::string> result;
    
    for (const auto& name : all) {
        if (get_template_category(string_to_template_type(name)) == category) {
            result.push_back(name);
        }
    }
    return result;
}

std::string FlexInfographic::get_template_description(TemplateType type) {
    switch (type) {
        case TemplateType::ListGridBadgeCard:
            return "Grid cards with badges - Best for KPI cards, metrics";
        case TemplateType::SequenceTimelineSimple:
            return "Simple timeline - Best for history, milestones";
        case TemplateType::CompareSwot:
            return "SWOT analysis (4 quadrants) - Best for strategic analysis";
        case TemplateType::ChartPieDonutPlainText:
            return "Donut chart - Best for distribution";
        default:
            return "Infographic template";
    }
}

bool FlexInfographic::validate_template_name(const std::string& template_name) {
    static const std::unordered_set<std::string> valid_templates = []() {
        auto vec = get_available_templates();
        return std::unordered_set<std::string>(vec.begin(), vec.end());
    }();
    return valid_templates.find(template_name) != valid_templates.end();
}

std::string FlexInfographic::get_template_requirements(TemplateType type) {
    switch (type) {
        case TemplateType::CompareSwot:
            return "Requires exactly 4 items: Strengths, Weaknesses, Opportunities, Threats";
        case TemplateType::CompareBinaryHorizontalUnderlineTextVs:
            return "Requires exactly 2 root items with children for comparison";
        default:
            return "Supports 2-8 items for optimal visual clarity";
    }
}

// ========== 便利函数 ==========

std::string quick_infographic(const std::string& template_name, 
                             const std::vector<std::string>& labels,
                             const std::vector<double>& values) {
    std::ostringstream ss;
    ss << "infographic " << template_name << "\ndata\n  items\n";
    
    for (size_t i = 0; i < labels.size(); ++i) {
        ss << "    - label " << labels[i] << "\n";
        if (i < values.size()) {
            ss << "      value " << values[i] << "\n";
        }
    }
    return ss.str();
}

std::string create_kpi_dashboard(const std::vector<std::pair<std::string, std::string>>& kpis) {
    std::ostringstream ss;
    ss << "infographic list-grid-badge-card\ndata\n  title Key Performance Indicators\n  items\n";
    
    for (const auto& kpi : kpis) {
        ss << "    - label " << kpi.first << "\n      desc " << kpi.second << "\n";
    }
    return ss.str();
}

std::string create_timeline(const std::vector<std::pair<std::string, std::string>>& events) {
    std::ostringstream ss;
    ss << "infographic sequence-timeline-simple\ndata\n  title Timeline\n  items\n";
    
    for (const auto& event : events) {
        ss << "    - label " << event.first << "\n      desc " << event.second << "\n";
    }
    return ss.str();
}

std::string create_comparison(const std::string& title,
                             const std::string& option_a, const std::vector<std::string>& features_a,
                             const std::string& option_b, const std::vector<std::string>& features_b) {
    std::ostringstream ss;
    ss << "infographic compare-binary-horizontal-underline-text-vs\ndata\n  title " << title << "\n  items\n";
    
    ss << "    - label " << option_a << "\n      children\n";
    for (const auto& f : features_a) ss << "        - label " << f << "\n";
    
    ss << "    - label " << option_b << "\n      children\n";
    for (const auto& f : features_b) ss << "        - label " << f << "\n";
    
    return ss.str();
}

std::string create_swot_analysis(const std::vector<std::string>& strengths,
                                const std::vector<std::string>& weaknesses,
                                const std::vector<std::string>& opportunities,
                                const std::vector<std::string>& threats) {
    std::ostringstream ss;
    ss << "infographic compare-swot\ndata\n  title SWOT Analysis\n  items\n";
    
    auto add_section = [&ss](const std::string& label, const std::vector<std::string>& items) {
        ss << "    - label " << label << "\n      children\n";
        for (const auto& item : items) ss << "        - label " << item << "\n";
    };
    
    add_section("Strengths", strengths);
    add_section("Weaknesses", weaknesses);
    add_section("Opportunities", opportunities);
    add_section("Threats", threats);
    
    return ss.str();
}

} // namespace flex::modules::infographic
