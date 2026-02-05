// Test for infographic DSL parser (re2c + lemon)
#include <ir/unified_infographic.h>
#include <cassert>
#include <iostream>
#include <string>

namespace flex::modules::infographic {
bool parse_infographic_dsl(const char* source, UnifiedInfographic* infographic, std::string& error_msg);
}

using namespace flex::modules::infographic;

void test_basic_dsl() {
    std::cout << "Test: Basic DSL parsing\n";
    
    const char* dsl = R"(
infographic list-grid-badge-card
data {
    title: "Key Metrics"
    desc: "Annual overview"
    items: [
        { label: "Revenue", desc: "12.8M", value: 12.8 }
        { label: "Users", desc: "45K", value: 45 }
        { label: "Growth", desc: "+23%", value: 23 }
    ]
}
)";

    UnifiedInfographic info;
    std::string error;
    bool ok = parse_infographic_dsl(dsl, &info, error);
    
    if (!ok) {
        std::cerr << "  Parse error: " << error << "\n";
    }
    assert(ok);
    assert(info.template_type == TemplateType::ListGridBadgeCard);
    assert(info.get_title() == "Key Metrics");
    assert(info.get_desc() == "Annual overview");
    assert(info.items.size() == 3);
    assert(info.items[0]->label == "Revenue");
    assert(info.items[0]->value.value_or(0) == 12.8);
    
    std::cout << "  ✓ Passed\n";
}

void test_theme_dsl() {
    std::cout << "Test: Theme DSL parsing\n";
    
    const char* dsl = R"(
infographic chart-pie-plain-text
data {
    title: "Market Share"
    items: [
        { label: "Product A", value: 45 }
        { label: "Product B", value: 30 }
        { label: "Product C", value: 25 }
    ]
}
theme {
    palette: #3b82f6 #8b5cf6 #f97316
    preset: dark
    stylize: rough
}
)";

    UnifiedInfographic info;
    std::string error;
    bool ok = parse_infographic_dsl(dsl, &info, error);
    
    if (!ok) std::cerr << "  Parse error: " << error << "\n";
    assert(ok);
    assert(info.template_type == TemplateType::ChartPiePlainText);
    assert(info.theme.palette.size() == 3);
    assert(info.theme.palette[0] == "#3b82f6");
    assert(info.theme.preset.value_or("") == "dark");
    assert(info.theme.stylize.value_or("") == "rough");
    
    std::cout << "  ✓ Passed\n";
}

void test_nested_children() {
    std::cout << "Test: Nested children parsing\n";
    
    const char* dsl = R"(
infographic hierarchy-tree-tech-style-capsule-item
data {
    title: "Organization"
    items: [
        {
            label: "CEO"
            children: [
                { label: "CTO", desc: "Technology" }
                { label: "CFO", desc: "Finance" }
            ]
        }
    ]
}
)";

    UnifiedInfographic info;
    std::string error;
    bool ok = parse_infographic_dsl(dsl, &info, error);
    
    if (!ok) std::cerr << "  Parse error: " << error << "\n";
    assert(ok);
    assert(info.items.size() == 1);
    assert(info.items[0]->label == "CEO");
    assert(info.items[0]->children.size() == 2);
    assert(info.items[0]->children[0]->label == "CTO");
    
    std::cout << "  ✓ Passed\n";
}

void test_timeline_template() {
    std::cout << "Test: Timeline template\n";
    
    const char* dsl = R"(
infographic sequence-timeline-simple
data {
    title: "Project Roadmap"
    items: [
        { label: "Q1", desc: "Planning" }
        { label: "Q2", desc: "Development" }
        { label: "Q3", desc: "Testing" }
        { label: "Q4", desc: "Launch" }
    ]
}
)";

    UnifiedInfographic info;
    std::string error;
    bool ok = parse_infographic_dsl(dsl, &info, error);
    
    assert(ok);
    assert(info.template_type == TemplateType::SequenceTimelineSimple);
    assert(info.category == TemplateCategory::Sequence);
    assert(info.items.size() == 4);
    
    std::cout << "  ✓ Passed\n";
}

void test_swot_template() {
    std::cout << "Test: SWOT template\n";
    
    const char* dsl = R"(
infographic compare-swot
data {
    title: "Strategic Analysis"
    items: [
        { label: "Strengths", desc: "Internal positive" }
        { label: "Weaknesses", desc: "Internal negative" }
        { label: "Opportunities", desc: "External positive" }
        { label: "Threats", desc: "External negative" }
    ]
}
)";

    UnifiedInfographic info;
    std::string error;
    bool ok = parse_infographic_dsl(dsl, &info, error);
    
    assert(ok);
    assert(info.template_type == TemplateType::CompareSwot);
    assert(info.validate());
    
    std::cout << "  ✓ Passed\n";
}

void test_item_fields() {
    std::cout << "Test: All item fields\n";
    
    const char* dsl = R"(
infographic list-column-done-list
data {
    items: [
        {
            label: "Task 1"
            desc: "First task"
            value: 100
            icon: "check"
            illus: "task.png"
            done: true
        }
        {
            label: "Task 2"
            done: false
        }
    ]
}
)";

    UnifiedInfographic info;
    std::string error;
    bool ok = parse_infographic_dsl(dsl, &info, error);
    
    assert(ok);
    assert(info.items.size() == 2);
    
    auto& item = info.items[0];
    assert(item->label == "Task 1");
    assert(item->desc.value_or("") == "First task");
    assert(item->value.value_or(0) == 100);
    assert(item->icon.value_or("") == "check");
    assert(item->illus.value_or("") == "task.png");
    assert(item->done.value_or(false) == true);
    
    assert(info.items[1]->done.value_or(true) == false);
    
    std::cout << "  ✓ Passed\n";
}

void test_theme_preset_shorthand() {
    std::cout << "Test: Theme preset shorthand\n";
    
    const char* dsl = R"(
infographic list-grid-badge-card
data {
    items: [
        { label: "Item 1" }
    ]
}
theme dark
)";

    UnifiedInfographic info;
    std::string error;
    bool ok = parse_infographic_dsl(dsl, &info, error);
    
    assert(ok);
    assert(info.theme.preset.value_or("") == "dark");
    
    std::cout << "  ✓ Passed\n";
}

void test_error_handling() {
    std::cout << "Test: Error handling\n";
    
    // Invalid character
    const char* bad_dsl = "infographic list-grid-badge-card\n@invalid";
    
    UnifiedInfographic info;
    std::string error;
    bool ok = parse_infographic_dsl(bad_dsl, &info, error);
    
    assert(!ok);
    assert(!error.empty());
    std::cout << "  Expected error: " << error << "\n";
    
    std::cout << "  ✓ Passed\n";
}

int main() {
#ifdef _WIN32
    system("chcp 65001 >nul");
#endif
    std::cout << "=== Infographic DSL Parser Tests ===\n\n";
    
    test_basic_dsl();
    test_theme_dsl();
    test_nested_children();
    test_timeline_template();
    test_swot_template();
    test_item_fields();
    test_theme_preset_shorthand();
    test_error_handling();
    
    std::cout << "\n✅ All DSL parser tests passed!\n";
    return 0;
}
