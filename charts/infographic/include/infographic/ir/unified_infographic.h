#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

namespace flex::modules::infographic {

// Template categories - unified enum instead of string matching
enum class TemplateCategory {
    List,
    Sequence, 
    Compare,
    Hierarchy,
    Chart,
    Quadrant,
    Relation,
    Geographic,  // 地理类：地图、区域数据等
    Flowchart,   // 流程图类：决策树、业务流程等
    Process      // 过程类：步骤指导、教程等
};

// Template type - specific template within category
enum class TemplateType {
    // List templates
    ListGridBadgeCard,
    ListGridCandyCardLite,
    ListGridRibbonCard,
    ListRowHorizontalIconArrow,
    ListRowSimpleIllus,
    ListSectorPlainText,
    ListColumnDoneList,
    ListColumnVerticalIconArrow,
    ListColumnSimpleVerticalArrow,
    ListZigzagDownCompactCard,
    ListZigzagDownSimple,
    ListZigzagUpCompactCard,
    ListZigzagUpSimple,
    
    // Sequence templates
    SequenceTimelineSimple,
    SequenceTimelineRoundedRectNode,
    SequenceTimelineSimpleIllus,
    SequenceRoadmapVerticalSimple,
    SequenceRoadmapVerticalPlainText,
    SequenceFilterMeshSimple,
    SequenceFunnelSimple,
    SequenceSnakeStepsSimple,
    SequenceSnakeStepsCompactCard,
    SequenceSnakeStepsUnderlineText,
    SequenceStairsFrontCompactCard,
    SequenceStairsFrontPillBadge,
    SequenceAscendingSteps,
    SequenceAscendingStairs3dUnderlineText,
    SequenceCircularSimple,
    SequencePyramidSimple,
    SequenceMountainUnderlineText,
    SequenceCylinders3dSimple,
    SequenceZigzagStepsUnderlineText,
    SequenceZigzagPucks3dSimple,
    SequenceHorizontalZigzagUnderlineText,
    SequenceHorizontalZigzagSimpleIllus,
    SequenceColorSnakeStepsHorizontalIconLine,
    
    // Compare templates
    CompareBinaryHorizontalUnderlineTextVs,
    CompareBinaryHorizontalSimpleFold,
    CompareBinaryHorizontalBadgeCardArrow,
    CompareHierarchyLeftRightCircleNodePillBadge,
    CompareSwot,
    
    // Hierarchy templates
    HierarchyTreeTechStyleCapsuleItem,
    HierarchyTreeCurvedLineRoundedRectNode,
    HierarchyTreeTechStyleBadgeCard,
    HierarchyStructure,
    
    // Chart templates
    ChartPiePlainText,
    ChartPieCompactCard,
    ChartPieDonutPlainText,
    ChartPieDonutPillBadge,
    ChartBarPlainText,
    ChartColumnSimple,
    ChartLinePlainText,
    ChartWordcloud,
    
    // Quadrant templates
    QuadrantQuarterSimpleCard,
    QuadrantQuarterCircular,
    QuadrantSimpleIllus,
    
    // Relation templates
    RelationCircleIconBadge,
    RelationCircleCircularProgress,
    
    // Geographic templates
    GeographicMapRegional,
    GeographicMapCountry,
    GeographicMapWorld,
    GeographicDataByRegion,
    
    // Flowchart templates
    FlowchartDecisionTree,
    FlowchartBusinessProcess,
    FlowchartSystemFlow,
    FlowchartUserJourney,
    
    // Process templates
    ProcessStepByStep,
    ProcessHowToGuide,
    ProcessWorkflow,
    ProcessTutorial
};

// Theme configuration
struct Theme {
    std::vector<std::string> palette;
    std::optional<std::string> preset; // "dark", "hand-drawn"
    std::optional<std::string> stylize; // "rough", "pattern", "linear-gradient", "radial-gradient"
    std::unordered_map<std::string, std::string> custom_properties;
    
    static Theme default_theme();
    static Theme dark();
    static Theme hand_drawn();
};

// Data item - unified structure for all templates
struct DataItem {
    std::string label;
    std::optional<std::string> desc;
    std::optional<double> value;
    std::optional<std::string> icon;
    std::optional<std::string> illus;
    std::optional<std::string> time;
    std::optional<bool> done;
    std::vector<std::unique_ptr<DataItem>> children;
    std::unordered_map<std::string, std::string> properties;
    
    // Convenience methods
    void set_prop(const std::string& key, const std::string& value);
    std::string get_prop(const std::string& key, const std::string& default_value = "") const;
    
    // Factory methods
    static std::unique_ptr<DataItem> create(const std::string& label);
    static std::unique_ptr<DataItem> create_with_value(const std::string& label, double value);
    static std::unique_ptr<DataItem> create_with_desc(const std::string& label, const std::string& desc);
};

// Main infographic structure - unified for all templates
class UnifiedInfographic {
public:
    TemplateType template_type;
    TemplateCategory category;
    
    std::optional<std::string> title;
    std::optional<std::string> desc;
    std::vector<std::unique_ptr<DataItem>> items;
    Theme theme;
    std::unordered_map<std::string, std::string> properties;
    
    // Convenience methods
    void set_prop(const std::string& key, const std::string& value);
    std::string get_prop(const std::string& key, const std::string& default_value = "") const;
    
    void set_title(const std::string& title);
    void set_desc(const std::string& desc);
    void set_theme(const Theme& theme);
    
    std::string get_title() const;
    std::string get_desc() const;
    const Theme& get_theme() const;
    
    // Template validation
    bool validate() const;
    std::string get_validation_error() const;
    
    // Item management
    void add_item(std::unique_ptr<DataItem> item);
    DataItem* find_item(const std::string& label);
    const DataItem* find_item(const std::string& label) const;
    
    size_t item_count() const { return items.size(); }
    bool empty() const { return items.empty(); }
};

// Layout type - determines which layout algorithm to use
enum class LayoutType {
    Grid,       // Grid of cards
    Row,        // Horizontal row
    Column,     // Vertical column
    Zigzag,     // Alternating left-right
    Timeline,   // Horizontal timeline
    Funnel,     // Funnel/pyramid shape
    Circular,   // Circular arrangement
    Tree,       // Hierarchical tree
    Quadrant,   // 4-quadrant (SWOT style)
    Pie,        // Pie chart
    Bar         // Bar chart
};

// Template utilities
TemplateType string_to_template_type(const std::string& template_name);
std::string template_type_to_string(TemplateType type);
TemplateCategory get_template_category(TemplateType type);
LayoutType get_layout_type(TemplateType type);

// Factory function
std::unique_ptr<UnifiedInfographic> create_infographic(TemplateType type);

} // namespace flex::modules::infographic