#include <flexinfographic.h>
#include <iostream>
#include <fstream>

using namespace flex::modules::infographic;

int main() {
    std::cout << "FlexInfographic Simple Example\n";
    std::cout << "==============================\n\n";
    
    FlexInfographic flex;
    
    // 示例 1: KPI 仪表板
    std::cout << "1. Creating KPI Dashboard...\n";
    
    auto kpi_dashboard = create_kpi_dashboard({
        {"Total Revenue", "12.8M | +23.5%"},
        {"New Customers", "3,280 | +45%"},
        {"Customer Satisfaction", "94.6% | Industry leading"},
        {"Market Share", "18.5% | Rank #2"}
    });
    
    std::cout << "Generated infographic syntax:\n";
    std::cout << kpi_dashboard << "\n";
    
    std::string svg1 = flex.infographic_to_svg(kpi_dashboard);
    std::ofstream file1("kpi_dashboard.svg");
    file1 << svg1;
    file1.close();
    std::cout << "✓ Saved to kpi_dashboard.svg\n\n";
    
    // 示例 2: 产品路线图
    std::cout << "2. Creating Product Roadmap...\n";
    
    auto roadmap = create_timeline({
        {"Q1 2024", "Research & Planning"},
        {"Q2 2024", "Core Development"},
        {"Q3 2024", "Beta Testing"},
        {"Q4 2024", "Public Launch"}
    });
    
    std::cout << "Generated infographic syntax:\n";
    std::cout << roadmap << "\n";
    
    std::string svg2 = flex.infographic_to_svg(roadmap);
    std::ofstream file2("product_roadmap.svg");
    file2 << svg2;
    file2.close();
    std::cout << "✓ Saved to product_roadmap.svg\n\n";
    
    // 示例 3: SWOT 分析
    std::cout << "3. Creating SWOT Analysis...\n";
    
    auto swot = create_swot_analysis(
        {"Strong R&D team", "Established brand", "Financial stability"},
        {"Limited market presence", "High operational costs"},
        {"Digital transformation", "Emerging markets", "Strategic partnerships"},
        {"Intense competition", "Economic uncertainty", "Regulatory changes"}
    );
    
    std::cout << "Generated infographic syntax:\n";
    std::cout << swot << "\n";
    
    std::string svg3 = flex.infographic_to_svg(swot);
    std::ofstream file3("swot_analysis.svg");
    file3 << svg3;
    file3.close();
    std::cout << "✓ Saved to swot_analysis.svg\n\n";
    
    // 示例 4: 产品比较
    std::cout << "4. Creating Product Comparison...\n";
    
    auto comparison = create_comparison(
        "Cloud vs On-Premise",
        "Cloud Solution",
        {"Scalable on demand", "Pay as you go", "Automatic updates", "Global availability"},
        "On-Premise Solution", 
        {"Full control", "One-time cost", "Custom configuration", "Data sovereignty"}
    );
    
    std::cout << "Generated infographic syntax:\n";
    std::cout << comparison << "\n";
    
    std::string svg4 = flex.infographic_to_svg(comparison);
    std::ofstream file4("product_comparison.svg");
    file4 << svg4;
    file4.close();
    std::cout << "✓ Saved to product_comparison.svg\n\n";
    
    // 示例 5: 自定义主题
    std::cout << "5. Creating Custom Themed Infographic...\n";
    
    std::string custom_themed = R"(
infographic list-grid-badge-card
data
  title Q4 2024 Results
  desc Outstanding performance across all metrics
  items
    - label Revenue Growth
      desc +28.5% YoY
    - label Customer Acquisition
      desc +45% New Users
    - label Product Launches
      desc 3 Major Releases
    - label Team Expansion
      desc +12 New Hires
theme
  palette #2563eb #7c3aed #dc2626 #059669
)";
    
    std::cout << "Custom themed infographic:\n";
    std::cout << custom_themed << "\n";
    
    std::string svg5 = flex.infographic_to_svg(custom_themed);
    std::ofstream file5("custom_themed.svg");
    file5 << svg5;
    file5.close();
    std::cout << "✓ Saved to custom_themed.svg\n\n";
    
    // 显示可用模板信息
    std::cout << "6. Available Templates Information...\n";
    
    auto templates = FlexInfographic::get_available_templates();
    std::cout << "Total available templates: " << templates.size() << "\n";
    
    auto list_templates = FlexInfographic::get_templates_by_category(TemplateCategory::List);
    std::cout << "List templates: " << list_templates.size() << "\n";
    
    auto chart_templates = FlexInfographic::get_templates_by_category(TemplateCategory::Chart);
    std::cout << "Chart templates: " << chart_templates.size() << "\n";
    
    std::cout << "\nPopular templates:\n";
    std::vector<TemplateType> popular = {
        TemplateType::ListGridBadgeCard,
        TemplateType::SequenceTimelineSimple,
        TemplateType::SequenceFilterMeshSimple,
        TemplateType::CompareBinaryHorizontalUnderlineTextVs,
        TemplateType::CompareSwot,
        TemplateType::ChartPieDonutPlainText
    };
    
    for (auto type : popular) {
        std::cout << "  • " << template_type_to_string(type) 
                  << " - " << FlexInfographic::get_template_description(type) << "\n";
    }
    
    std::cout << "\n🎉 All examples completed successfully!\n";
    std::cout << "Check the generated SVG files to see the results.\n";
    
    return 0;
}