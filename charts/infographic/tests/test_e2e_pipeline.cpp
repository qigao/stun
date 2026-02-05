// End-to-end test: DSL -> Parser -> Layout -> Renderer -> SVG
#include <ir/unified_infographic.h>
#include <layout/layout_engine.h>
#include <renderer/template_renderer.h>
#include <cassert>
#include <iostream>
#include <fstream>
#include <string>

namespace flex::modules::infographic {
bool parse_infographic_dsl(const char* source, UnifiedInfographic* infographic, std::string& error_msg);
}

using namespace flex::modules::infographic;

std::string render_dsl(const char* dsl) {
    UnifiedInfographic info;
    std::string error;
    
    if (!parse_infographic_dsl(dsl, &info, error)) {
        return "Parse error: " + error;
    }
    
    auto renderer = RendererFactory::create(info.template_type);
    return renderer->render(info);
}

void test_grid_infographic() {
    std::cout << "Test: Grid infographic E2E\n";
    
    const char* dsl = R"(
infographic list-grid-badge-card
data {
    title: "Key Metrics"
    desc: "Q4 2024 Performance"
    items: [
        { label: "Revenue", desc: "$12.8M", value: 12.8 }
        { label: "Users", desc: "45,000", value: 45 }
        { label: "Growth", desc: "+23%", value: 23 }
        { label: "NPS", desc: "72", value: 72 }
    ]
}
theme {
    palette: #3b82f6 #22c55e #f59e0b #ef4444
}
)";

    std::string svg = render_dsl(dsl);
    
    assert(svg.find("<svg") != std::string::npos);
    assert(svg.find("Key Metrics") != std::string::npos);
    assert(svg.find("Revenue") != std::string::npos);
    assert(svg.find("</svg>") != std::string::npos);
    
    std::ofstream("test_grid.svg") << svg;
    std::cout << "  ✓ Saved test_grid.svg (" << svg.size() << " bytes)\n";
}

void test_timeline_infographic() {
    std::cout << "Test: Timeline infographic E2E\n";
    
    const char* dsl = R"(
infographic sequence-timeline-simple
data {
    title: "Project Roadmap"
    items: [
        { label: "Q1", desc: "Planning & Design" }
        { label: "Q2", desc: "Development" }
        { label: "Q3", desc: "Testing & QA" }
        { label: "Q4", desc: "Launch" }
    ]
}
)";

    std::string svg = render_dsl(dsl);
    
    assert(svg.find("<svg") != std::string::npos);
    assert(svg.find("Project Roadmap") != std::string::npos);
    
    std::ofstream("test_timeline.svg") << svg;
    std::cout << "  ✓ Saved test_timeline.svg (" << svg.size() << " bytes)\n";
}

void test_pie_chart() {
    std::cout << "Test: Pie chart E2E\n";
    
    const char* dsl = R"(
infographic chart-pie-plain-text
data {
    title: "Market Share"
    items: [
        { label: "Product A", value: 45 }
        { label: "Product B", value: 30 }
        { label: "Product C", value: 15 }
        { label: "Others", value: 10 }
    ]
}
theme {
    palette: #3b82f6 #22c55e #f59e0b #94a3b8
}
)";

    std::string svg = render_dsl(dsl);
    
    assert(svg.find("<svg") != std::string::npos);
    assert(svg.find("Market Share") != std::string::npos);
    
    std::ofstream("test_pie.svg") << svg;
    std::cout << "  ✓ Saved test_pie.svg (" << svg.size() << " bytes)\n";
}

void test_swot_analysis() {
    std::cout << "Test: SWOT analysis E2E\n";
    
    const char* dsl = R"(
infographic compare-swot
data {
    title: "Strategic Analysis"
    items: [
        { label: "Strengths", desc: "Strong R&D, Brand recognition" }
        { label: "Weaknesses", desc: "Limited market reach" }
        { label: "Opportunities", desc: "Emerging markets, Digital transformation" }
        { label: "Threats", desc: "Competition, Regulation" }
    ]
}
)";

    std::string svg = render_dsl(dsl);
    
    assert(svg.find("<svg") != std::string::npos);
    assert(svg.find("Strategic Analysis") != std::string::npos);
    assert(svg.find("Strengths") != std::string::npos);
    
    std::ofstream("test_swot.svg") << svg;
    std::cout << "  ✓ Saved test_swot.svg (" << svg.size() << " bytes)\n";
}

void test_funnel() {
    std::cout << "Test: Funnel E2E\n";
    
    const char* dsl = R"(
infographic sequence-funnel-simple
data {
    title: "Sales Funnel"
    items: [
        { label: "Visitors", value: 10000 }
        { label: "Leads", value: 3000 }
        { label: "Qualified", value: 1000 }
        { label: "Customers", value: 300 }
    ]
}
)";

    std::string svg = render_dsl(dsl);
    
    assert(svg.find("<svg") != std::string::npos);
    assert(svg.find("Sales Funnel") != std::string::npos);
    
    std::ofstream("test_funnel.svg") << svg;
    std::cout << "  ✓ Saved test_funnel.svg (" << svg.size() << " bytes)\n";
}

void test_bar_chart() {
    std::cout << "Test: Bar chart E2E\n";
    
    const char* dsl = R"(
infographic chart-bar-plain-text
data {
    title: "Monthly Sales"
    items: [
        { label: "January", value: 120 }
        { label: "February", value: 150 }
        { label: "March", value: 180 }
        { label: "April", value: 140 }
        { label: "May", value: 200 }
    ]
}
)";

    std::string svg = render_dsl(dsl);
    
    assert(svg.find("<svg") != std::string::npos);
    assert(svg.find("Monthly Sales") != std::string::npos);
    
    std::ofstream("test_bar.svg") << svg;
    std::cout << "  ✓ Saved test_bar.svg (" << svg.size() << " bytes)\n";
}

void test_layout_integration() {
    std::cout << "Test: Layout engine integration\n";
    
    UnifiedInfographic info;
    info.template_type = TemplateType::ListGridBadgeCard;
    info.add_item(DataItem::create_with_desc("Item 1", "Desc 1"));
    info.add_item(DataItem::create_with_desc("Item 2", "Desc 2"));
    info.add_item(DataItem::create_with_desc("Item 3", "Desc 3"));
    
    auto layout_engine = create_layout_engine(info.template_type);
    StyleConfig style = parse_style_from_template("list-grid-badge-card");
    
    LayoutResult result = layout_engine->compute(info, 800, 600, style);
    
    assert(result.nodes.size() == 3);
    assert(result.canvas_width == 800);
    
    std::cout << "  Layout: " << layout_engine->name() << "\n";
    std::cout << "  Nodes: " << result.nodes.size() << "\n";
    std::cout << "  ✓ Layout engine works\n";
}

int main() {
#ifdef _WIN32
    system("chcp 65001 >nul");
#endif
    std::cout << "=== Infographic E2E Pipeline Tests ===\n\n";
    
    test_grid_infographic();
    test_timeline_infographic();
    test_pie_chart();
    test_swot_analysis();
    test_funnel();
    test_bar_chart();
    test_layout_integration();
    
    std::cout << "\n✅ All E2E tests passed!\n";
    std::cout << "\nGenerated SVG files:\n";
    std::cout << "  - test_grid.svg\n";
    std::cout << "  - test_timeline.svg\n";
    std::cout << "  - test_pie.svg\n";
    std::cout << "  - test_swot.svg\n";
    std::cout << "  - test_funnel.svg\n";
    std::cout << "  - test_bar.svg\n";
    
    return 0;
}
