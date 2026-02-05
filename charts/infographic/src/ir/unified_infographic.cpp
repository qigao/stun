#include <ir/unified_infographic.h>
#include <algorithm>

namespace flex::modules::infographic {

// Theme implementations
Theme Theme::default_theme() {
    Theme theme;
    theme.palette = {"#3b82f6", "#8b5cf6", "#f97316", "#ef4444", "#10b981", "#f59e0b"};
    return theme;
}

Theme Theme::dark() {
    Theme theme;
    theme.preset = "dark";
    theme.palette = {"#60a5fa", "#a78bfa", "#fb923c", "#f87171", "#34d399", "#fbbf24"};
    return theme;
}

Theme Theme::hand_drawn() {
    Theme theme;
    theme.preset = "hand-drawn";
    theme.stylize = "rough";
    theme.palette = {"#61DDAA", "#F6BD16", "#F08BB4", "#7B68EE", "#FF6B6B", "#4ECDC4"};
    return theme;
}

// DataItem implementations
void DataItem::set_prop(const std::string& key, const std::string& value) {
    properties[key] = value;
}

std::string DataItem::get_prop(const std::string& key, const std::string& default_value) const {
    auto it = properties.find(key);
    return it != properties.end() ? it->second : default_value;
}

std::unique_ptr<DataItem> DataItem::create(const std::string& label) {
    auto item = std::make_unique<DataItem>();
    item->label = label;
    return item;
}

std::unique_ptr<DataItem> DataItem::create_with_value(const std::string& label, double value) {
    auto item = create(label);
    item->value = value;
    return item;
}

std::unique_ptr<DataItem> DataItem::create_with_desc(const std::string& label, const std::string& desc) {
    auto item = create(label);
    item->desc = desc;
    return item;
}

// UnifiedInfographic implementations
void UnifiedInfographic::set_prop(const std::string& key, const std::string& value) {
    properties[key] = value;
}

std::string UnifiedInfographic::get_prop(const std::string& key, const std::string& default_value) const {
    auto it = properties.find(key);
    return it != properties.end() ? it->second : default_value;
}

void UnifiedInfographic::set_title(const std::string& title) {
    this->title = title;
}

void UnifiedInfographic::set_desc(const std::string& desc) {
    this->desc = desc;
}

void UnifiedInfographic::set_theme(const Theme& theme) {
    this->theme = theme;
}

std::string UnifiedInfographic::get_title() const {
    return title.value_or("");
}

std::string UnifiedInfographic::get_desc() const {
    return desc.value_or("");
}

const Theme& UnifiedInfographic::get_theme() const {
    return theme;
}

void UnifiedInfographic::add_item(std::unique_ptr<DataItem> item) {
    items.push_back(std::move(item));
}

DataItem* UnifiedInfographic::find_item(const std::string& label) {
    auto it = std::find_if(items.begin(), items.end(),
        [&label](const std::unique_ptr<DataItem>& item) {
            return item->label == label;
        });
    return it != items.end() ? it->get() : nullptr;
}

const DataItem* UnifiedInfographic::find_item(const std::string& label) const {
    auto it = std::find_if(items.begin(), items.end(),
        [&label](const std::unique_ptr<DataItem>& item) {
            return item->label == label;
        });
    return it != items.end() ? it->get() : nullptr;
}

bool UnifiedInfographic::validate() const {
    // Template-specific validation rules
    switch (template_type) {
        case TemplateType::CompareSwot:
            // SWOT must have exactly 4 items with English labels
            if (items.size() != 4) return false;
            {
                std::vector<std::string> required_labels = {"Strengths", "Weaknesses", "Opportunities", "Threats"};
                for (const auto& required : required_labels) {
                    if (!find_item(required)) return false;
                }
            }
            break;
            
        case TemplateType::CompareBinaryHorizontalUnderlineTextVs:
        case TemplateType::CompareBinaryHorizontalSimpleFold:
        case TemplateType::CompareBinaryHorizontalBadgeCardArrow:
        case TemplateType::CompareHierarchyLeftRightCircleNodePillBadge:
            // Compare templates must have exactly 2 root items
            if (items.size() != 2) return false;
            break;
            
        case TemplateType::QuadrantQuarterSimpleCard:
        case TemplateType::QuadrantQuarterCircular:
        case TemplateType::QuadrantSimpleIllus:
            // Quadrant templates must have exactly 4 items
            if (items.size() != 4) return false;
            break;
            
        default:
            // Most templates work with 2-8 items
            if (items.empty() || items.size() > 8) return false;
            break;
    }
    
    return true;
}

std::string UnifiedInfographic::get_validation_error() const {
    if (validate()) return "";
    
    switch (template_type) {
        case TemplateType::CompareSwot:
            if (items.size() != 4) {
                return "SWOT template requires exactly 4 items";
            }
            {
                std::vector<std::string> required_labels = {"Strengths", "Weaknesses", "Opportunities", "Threats"};
                for (const auto& required : required_labels) {
                    if (!find_item(required)) {
                        return "SWOT template requires items labeled: Strengths, Weaknesses, Opportunities, Threats";
                    }
                }
            }
            break;
            
        case TemplateType::CompareBinaryHorizontalUnderlineTextVs:
        case TemplateType::CompareBinaryHorizontalSimpleFold:
        case TemplateType::CompareBinaryHorizontalBadgeCardArrow:
        case TemplateType::CompareHierarchyLeftRightCircleNodePillBadge:
            return "Compare templates require exactly 2 root items";
            
        case TemplateType::QuadrantQuarterSimpleCard:
        case TemplateType::QuadrantQuarterCircular:
        case TemplateType::QuadrantSimpleIllus:
            return "Quadrant templates require exactly 4 items";
            
        default:
            if (items.empty()) {
                return "Template requires at least 1 item";
            }
            if (items.size() > 8) {
                return "Template supports maximum 8 items for visual clarity";
            }
            break;
    }
    
    return "Unknown validation error";
}

// Template utilities
TemplateType string_to_template_type(const std::string& template_name) {
    // List templates
    if (template_name == "list-grid-badge-card") return TemplateType::ListGridBadgeCard;
    if (template_name == "list-grid-candy-card-lite") return TemplateType::ListGridCandyCardLite;
    if (template_name == "list-grid-ribbon-card") return TemplateType::ListGridRibbonCard;
    if (template_name == "list-row-horizontal-icon-arrow") return TemplateType::ListRowHorizontalIconArrow;
    if (template_name == "list-row-simple-illus") return TemplateType::ListRowSimpleIllus;
    if (template_name == "list-sector-plain-text") return TemplateType::ListSectorPlainText;
    if (template_name == "list-column-done-list") return TemplateType::ListColumnDoneList;
    if (template_name == "list-column-vertical-icon-arrow") return TemplateType::ListColumnVerticalIconArrow;
    if (template_name == "list-column-simple-vertical-arrow") return TemplateType::ListColumnSimpleVerticalArrow;
    if (template_name == "list-zigzag-down-compact-card") return TemplateType::ListZigzagDownCompactCard;
    if (template_name == "list-zigzag-down-simple") return TemplateType::ListZigzagDownSimple;
    if (template_name == "list-zigzag-up-compact-card") return TemplateType::ListZigzagUpCompactCard;
    if (template_name == "list-zigzag-up-simple") return TemplateType::ListZigzagUpSimple;
    
    // Sequence templates
    if (template_name == "sequence-timeline-simple") return TemplateType::SequenceTimelineSimple;
    if (template_name == "sequence-timeline-rounded-rect-node") return TemplateType::SequenceTimelineRoundedRectNode;
    if (template_name == "sequence-timeline-simple-illus") return TemplateType::SequenceTimelineSimpleIllus;
    if (template_name == "sequence-roadmap-vertical-simple") return TemplateType::SequenceRoadmapVerticalSimple;
    if (template_name == "sequence-roadmap-vertical-plain-text") return TemplateType::SequenceRoadmapVerticalPlainText;
    if (template_name == "sequence-filter-mesh-simple") return TemplateType::SequenceFilterMeshSimple;
    if (template_name == "sequence-funnel-simple") return TemplateType::SequenceFunnelSimple;
    if (template_name == "sequence-snake-steps-simple") return TemplateType::SequenceSnakeStepsSimple;
    if (template_name == "sequence-snake-steps-compact-card") return TemplateType::SequenceSnakeStepsCompactCard;
    if (template_name == "sequence-snake-steps-underline-text") return TemplateType::SequenceSnakeStepsUnderlineText;
    if (template_name == "sequence-stairs-front-compact-card") return TemplateType::SequenceStairsFrontCompactCard;
    if (template_name == "sequence-stairs-front-pill-badge") return TemplateType::SequenceStairsFrontPillBadge;
    if (template_name == "sequence-ascending-steps") return TemplateType::SequenceAscendingSteps;
    if (template_name == "sequence-ascending-stairs-3d-underline-text") return TemplateType::SequenceAscendingStairs3dUnderlineText;
    if (template_name == "sequence-circular-simple") return TemplateType::SequenceCircularSimple;
    if (template_name == "sequence-pyramid-simple") return TemplateType::SequencePyramidSimple;
    if (template_name == "sequence-mountain-underline-text") return TemplateType::SequenceMountainUnderlineText;
    if (template_name == "sequence-cylinders-3d-simple") return TemplateType::SequenceCylinders3dSimple;
    if (template_name == "sequence-zigzag-steps-underline-text") return TemplateType::SequenceZigzagStepsUnderlineText;
    if (template_name == "sequence-zigzag-pucks-3d-simple") return TemplateType::SequenceZigzagPucks3dSimple;
    if (template_name == "sequence-horizontal-zigzag-underline-text") return TemplateType::SequenceHorizontalZigzagUnderlineText;
    if (template_name == "sequence-horizontal-zigzag-simple-illus") return TemplateType::SequenceHorizontalZigzagSimpleIllus;
    if (template_name == "sequence-color-snake-steps-horizontal-icon-line") return TemplateType::SequenceColorSnakeStepsHorizontalIconLine;
    
    // Compare templates
    if (template_name == "compare-binary-horizontal-underline-text-vs") return TemplateType::CompareBinaryHorizontalUnderlineTextVs;
    if (template_name == "compare-binary-horizontal-simple-fold") return TemplateType::CompareBinaryHorizontalSimpleFold;
    if (template_name == "compare-binary-horizontal-badge-card-arrow") return TemplateType::CompareBinaryHorizontalBadgeCardArrow;
    if (template_name == "compare-hierarchy-left-right-circle-node-pill-badge") return TemplateType::CompareHierarchyLeftRightCircleNodePillBadge;
    if (template_name == "compare-swot") return TemplateType::CompareSwot;
    
    // Hierarchy templates
    if (template_name == "hierarchy-tree-tech-style-capsule-item") return TemplateType::HierarchyTreeTechStyleCapsuleItem;
    if (template_name == "hierarchy-tree-curved-line-rounded-rect-node") return TemplateType::HierarchyTreeCurvedLineRoundedRectNode;
    if (template_name == "hierarchy-tree-tech-style-badge-card") return TemplateType::HierarchyTreeTechStyleBadgeCard;
    if (template_name == "hierarchy-structure") return TemplateType::HierarchyStructure;
    
    // Chart templates
    if (template_name == "chart-pie-plain-text") return TemplateType::ChartPiePlainText;
    if (template_name == "chart-pie-compact-card") return TemplateType::ChartPieCompactCard;
    if (template_name == "chart-pie-donut-plain-text") return TemplateType::ChartPieDonutPlainText;
    if (template_name == "chart-pie-donut-pill-badge") return TemplateType::ChartPieDonutPillBadge;
    if (template_name == "chart-bar-plain-text") return TemplateType::ChartBarPlainText;
    if (template_name == "chart-column-simple") return TemplateType::ChartColumnSimple;
    if (template_name == "chart-line-plain-text") return TemplateType::ChartLinePlainText;
    if (template_name == "chart-wordcloud") return TemplateType::ChartWordcloud;
    
    // Quadrant templates
    if (template_name == "quadrant-quarter-simple-card") return TemplateType::QuadrantQuarterSimpleCard;
    if (template_name == "quadrant-quarter-circular") return TemplateType::QuadrantQuarterCircular;
    if (template_name == "quadrant-simple-illus") return TemplateType::QuadrantSimpleIllus;
    
    // Relation templates
    if (template_name == "relation-circle-icon-badge") return TemplateType::RelationCircleIconBadge;
    if (template_name == "relation-circle-circular-progress") return TemplateType::RelationCircleCircularProgress;
    
    // Geographic templates
    if (template_name == "geographic-regional-data") return TemplateType::GeographicDataByRegion;
    if (template_name == "geographic-world-map") return TemplateType::GeographicMapWorld;
    if (template_name == "geographic-location-pins") return TemplateType::GeographicMapRegional;
    
    // Flowchart templates
    if (template_name == "flowchart-decision-tree") return TemplateType::FlowchartDecisionTree;
    if (template_name == "flowchart-process-flow") return TemplateType::FlowchartBusinessProcess;
    if (template_name == "flowchart-org-chart") return TemplateType::FlowchartSystemFlow;
    
    // Process templates
    if (template_name == "process-step-by-step") return TemplateType::ProcessStepByStep;
    if (template_name == "process-workflow") return TemplateType::ProcessWorkflow;
    if (template_name == "process-how-to-guide") return TemplateType::ProcessHowToGuide;
    if (template_name == "process-tutorial") return TemplateType::ProcessTutorial;
    
    // Default fallback
    return TemplateType::ListGridBadgeCard;
}

std::string template_type_to_string(TemplateType type) {
    switch (type) {
        // List templates
        case TemplateType::ListGridBadgeCard: return "list-grid-badge-card";
        case TemplateType::ListGridCandyCardLite: return "list-grid-candy-card-lite";
        case TemplateType::ListGridRibbonCard: return "list-grid-ribbon-card";
        case TemplateType::ListRowHorizontalIconArrow: return "list-row-horizontal-icon-arrow";
        case TemplateType::ListRowSimpleIllus: return "list-row-simple-illus";
        case TemplateType::ListSectorPlainText: return "list-sector-plain-text";
        case TemplateType::ListColumnDoneList: return "list-column-done-list";
        case TemplateType::ListColumnVerticalIconArrow: return "list-column-vertical-icon-arrow";
        case TemplateType::ListColumnSimpleVerticalArrow: return "list-column-simple-vertical-arrow";
        case TemplateType::ListZigzagDownCompactCard: return "list-zigzag-down-compact-card";
        case TemplateType::ListZigzagDownSimple: return "list-zigzag-down-simple";
        case TemplateType::ListZigzagUpCompactCard: return "list-zigzag-up-compact-card";
        case TemplateType::ListZigzagUpSimple: return "list-zigzag-up-simple";
        
        // Sequence templates
        case TemplateType::SequenceTimelineSimple: return "sequence-timeline-simple";
        case TemplateType::SequenceTimelineRoundedRectNode: return "sequence-timeline-rounded-rect-node";
        case TemplateType::SequenceTimelineSimpleIllus: return "sequence-timeline-simple-illus";
        case TemplateType::SequenceRoadmapVerticalSimple: return "sequence-roadmap-vertical-simple";
        case TemplateType::SequenceRoadmapVerticalPlainText: return "sequence-roadmap-vertical-plain-text";
        case TemplateType::SequenceFilterMeshSimple: return "sequence-filter-mesh-simple";
        case TemplateType::SequenceFunnelSimple: return "sequence-funnel-simple";
        case TemplateType::SequenceSnakeStepsSimple: return "sequence-snake-steps-simple";
        case TemplateType::SequenceSnakeStepsCompactCard: return "sequence-snake-steps-compact-card";
        case TemplateType::SequenceSnakeStepsUnderlineText: return "sequence-snake-steps-underline-text";
        case TemplateType::SequenceStairsFrontCompactCard: return "sequence-stairs-front-compact-card";
        case TemplateType::SequenceStairsFrontPillBadge: return "sequence-stairs-front-pill-badge";
        case TemplateType::SequenceAscendingSteps: return "sequence-ascending-steps";
        case TemplateType::SequenceAscendingStairs3dUnderlineText: return "sequence-ascending-stairs-3d-underline-text";
        case TemplateType::SequenceCircularSimple: return "sequence-circular-simple";
        case TemplateType::SequencePyramidSimple: return "sequence-pyramid-simple";
        case TemplateType::SequenceMountainUnderlineText: return "sequence-mountain-underline-text";
        case TemplateType::SequenceCylinders3dSimple: return "sequence-cylinders-3d-simple";
        case TemplateType::SequenceZigzagStepsUnderlineText: return "sequence-zigzag-steps-underline-text";
        case TemplateType::SequenceZigzagPucks3dSimple: return "sequence-zigzag-pucks-3d-simple";
        case TemplateType::SequenceHorizontalZigzagUnderlineText: return "sequence-horizontal-zigzag-underline-text";
        case TemplateType::SequenceHorizontalZigzagSimpleIllus: return "sequence-horizontal-zigzag-simple-illus";
        case TemplateType::SequenceColorSnakeStepsHorizontalIconLine: return "sequence-color-snake-steps-horizontal-icon-line";
        
        // Compare templates
        case TemplateType::CompareBinaryHorizontalUnderlineTextVs: return "compare-binary-horizontal-underline-text-vs";
        case TemplateType::CompareBinaryHorizontalSimpleFold: return "compare-binary-horizontal-simple-fold";
        case TemplateType::CompareBinaryHorizontalBadgeCardArrow: return "compare-binary-horizontal-badge-card-arrow";
        case TemplateType::CompareHierarchyLeftRightCircleNodePillBadge: return "compare-hierarchy-left-right-circle-node-pill-badge";
        case TemplateType::CompareSwot: return "compare-swot";
        
        // Hierarchy templates
        case TemplateType::HierarchyTreeTechStyleCapsuleItem: return "hierarchy-tree-tech-style-capsule-item";
        case TemplateType::HierarchyTreeCurvedLineRoundedRectNode: return "hierarchy-tree-curved-line-rounded-rect-node";
        case TemplateType::HierarchyTreeTechStyleBadgeCard: return "hierarchy-tree-tech-style-badge-card";
        case TemplateType::HierarchyStructure: return "hierarchy-structure";
        
        // Chart templates
        case TemplateType::ChartPiePlainText: return "chart-pie-plain-text";
        case TemplateType::ChartPieCompactCard: return "chart-pie-compact-card";
        case TemplateType::ChartPieDonutPlainText: return "chart-pie-donut-plain-text";
        case TemplateType::ChartPieDonutPillBadge: return "chart-pie-donut-pill-badge";
        case TemplateType::ChartBarPlainText: return "chart-bar-plain-text";
        case TemplateType::ChartColumnSimple: return "chart-column-simple";
        case TemplateType::ChartLinePlainText: return "chart-line-plain-text";
        case TemplateType::ChartWordcloud: return "chart-wordcloud";
        
        // Quadrant templates
        case TemplateType::QuadrantQuarterSimpleCard: return "quadrant-quarter-simple-card";
        case TemplateType::QuadrantQuarterCircular: return "quadrant-quarter-circular";
        case TemplateType::QuadrantSimpleIllus: return "quadrant-simple-illus";
        
        // Relation templates
        case TemplateType::RelationCircleIconBadge: return "relation-circle-icon-badge";
        case TemplateType::RelationCircleCircularProgress: return "relation-circle-circular-progress";
        
        // Geographic templates
        case TemplateType::GeographicDataByRegion: return "geographic-regional-data";
        case TemplateType::GeographicMapWorld: return "geographic-world-map";
        case TemplateType::GeographicMapRegional: return "geographic-location-pins";
        
        // Flowchart templates
        case TemplateType::FlowchartDecisionTree: return "flowchart-decision-tree";
        case TemplateType::FlowchartBusinessProcess: return "flowchart-process-flow";
        case TemplateType::FlowchartSystemFlow: return "flowchart-org-chart";
        
        // Process templates
        case TemplateType::ProcessStepByStep: return "process-step-by-step";
        case TemplateType::ProcessWorkflow: return "process-workflow";
        case TemplateType::ProcessHowToGuide: return "process-how-to-guide";
        case TemplateType::ProcessTutorial: return "process-tutorial";
        
        default: return "list-grid-badge-card";
    }
}

TemplateCategory get_template_category(TemplateType type) {
    switch (type) {
        case TemplateType::ListGridBadgeCard:
        case TemplateType::ListGridCandyCardLite:
        case TemplateType::ListGridRibbonCard:
        case TemplateType::ListRowHorizontalIconArrow:
        case TemplateType::ListRowSimpleIllus:
        case TemplateType::ListSectorPlainText:
        case TemplateType::ListColumnDoneList:
        case TemplateType::ListColumnVerticalIconArrow:
        case TemplateType::ListColumnSimpleVerticalArrow:
        case TemplateType::ListZigzagDownCompactCard:
        case TemplateType::ListZigzagDownSimple:
        case TemplateType::ListZigzagUpCompactCard:
        case TemplateType::ListZigzagUpSimple:
            return TemplateCategory::List;
            
        case TemplateType::SequenceTimelineSimple:
        case TemplateType::SequenceTimelineRoundedRectNode:
        case TemplateType::SequenceTimelineSimpleIllus:
        case TemplateType::SequenceRoadmapVerticalSimple:
        case TemplateType::SequenceRoadmapVerticalPlainText:
        case TemplateType::SequenceFilterMeshSimple:
        case TemplateType::SequenceFunnelSimple:
        case TemplateType::SequenceSnakeStepsSimple:
        case TemplateType::SequenceSnakeStepsCompactCard:
        case TemplateType::SequenceSnakeStepsUnderlineText:
        case TemplateType::SequenceStairsFrontCompactCard:
        case TemplateType::SequenceStairsFrontPillBadge:
        case TemplateType::SequenceAscendingSteps:
        case TemplateType::SequenceAscendingStairs3dUnderlineText:
        case TemplateType::SequenceCircularSimple:
        case TemplateType::SequencePyramidSimple:
        case TemplateType::SequenceMountainUnderlineText:
        case TemplateType::SequenceCylinders3dSimple:
        case TemplateType::SequenceZigzagStepsUnderlineText:
        case TemplateType::SequenceZigzagPucks3dSimple:
        case TemplateType::SequenceHorizontalZigzagUnderlineText:
        case TemplateType::SequenceHorizontalZigzagSimpleIllus:
        case TemplateType::SequenceColorSnakeStepsHorizontalIconLine:
            return TemplateCategory::Sequence;
            
        case TemplateType::CompareBinaryHorizontalUnderlineTextVs:
        case TemplateType::CompareBinaryHorizontalSimpleFold:
        case TemplateType::CompareBinaryHorizontalBadgeCardArrow:
        case TemplateType::CompareHierarchyLeftRightCircleNodePillBadge:
        case TemplateType::CompareSwot:
            return TemplateCategory::Compare;
            
        case TemplateType::HierarchyTreeTechStyleCapsuleItem:
        case TemplateType::HierarchyTreeCurvedLineRoundedRectNode:
        case TemplateType::HierarchyTreeTechStyleBadgeCard:
        case TemplateType::HierarchyStructure:
            return TemplateCategory::Hierarchy;
            
        case TemplateType::ChartPiePlainText:
        case TemplateType::ChartPieCompactCard:
        case TemplateType::ChartPieDonutPlainText:
        case TemplateType::ChartPieDonutPillBadge:
        case TemplateType::ChartBarPlainText:
        case TemplateType::ChartColumnSimple:
        case TemplateType::ChartLinePlainText:
        case TemplateType::ChartWordcloud:
            return TemplateCategory::Chart;
            
        case TemplateType::QuadrantQuarterSimpleCard:
        case TemplateType::QuadrantQuarterCircular:
        case TemplateType::QuadrantSimpleIllus:
            return TemplateCategory::Quadrant;
            
        case TemplateType::RelationCircleIconBadge:
        case TemplateType::RelationCircleCircularProgress:
            return TemplateCategory::Relation;
            
        case TemplateType::GeographicDataByRegion:
        case TemplateType::GeographicMapWorld:
        case TemplateType::GeographicMapRegional:
        case TemplateType::GeographicMapCountry:
            return TemplateCategory::Geographic;
            
        case TemplateType::FlowchartDecisionTree:
        case TemplateType::FlowchartBusinessProcess:
        case TemplateType::FlowchartSystemFlow:
        case TemplateType::FlowchartUserJourney:
            return TemplateCategory::Flowchart;
            
        case TemplateType::ProcessStepByStep:
        case TemplateType::ProcessWorkflow:
        case TemplateType::ProcessHowToGuide:
        case TemplateType::ProcessTutorial:
            return TemplateCategory::Process;
            
        default:
            return TemplateCategory::List;
    }
}

std::unique_ptr<UnifiedInfographic> create_infographic(TemplateType type) {
    auto infographic = std::make_unique<UnifiedInfographic>();
    infographic->template_type = type;
    infographic->category = get_template_category(type);
    infographic->theme = Theme::default_theme();
    
    return infographic;
}

LayoutType get_layout_type(TemplateType type) {
    switch (type) {
        // Grid layouts
        case TemplateType::ListGridBadgeCard:
        case TemplateType::ListGridCandyCardLite:
        case TemplateType::ListGridRibbonCard:
        case TemplateType::ListSectorPlainText:
            return LayoutType::Grid;
            
        // Row layouts
        case TemplateType::ListRowHorizontalIconArrow:
        case TemplateType::ListRowSimpleIllus:
        case TemplateType::CompareBinaryHorizontalUnderlineTextVs:
        case TemplateType::CompareBinaryHorizontalSimpleFold:
        case TemplateType::CompareBinaryHorizontalBadgeCardArrow:
            return LayoutType::Row;
            
        // Column layouts
        case TemplateType::ListColumnDoneList:
        case TemplateType::ListColumnVerticalIconArrow:
        case TemplateType::ListColumnSimpleVerticalArrow:
        case TemplateType::SequenceRoadmapVerticalSimple:
        case TemplateType::SequenceRoadmapVerticalPlainText:
            return LayoutType::Column;
            
        // Zigzag layouts
        case TemplateType::ListZigzagDownCompactCard:
        case TemplateType::ListZigzagDownSimple:
        case TemplateType::ListZigzagUpCompactCard:
        case TemplateType::ListZigzagUpSimple:
        case TemplateType::SequenceSnakeStepsSimple:
        case TemplateType::SequenceSnakeStepsCompactCard:
        case TemplateType::SequenceSnakeStepsUnderlineText:
        case TemplateType::SequenceZigzagStepsUnderlineText:
        case TemplateType::SequenceZigzagPucks3dSimple:
        case TemplateType::SequenceHorizontalZigzagUnderlineText:
        case TemplateType::SequenceHorizontalZigzagSimpleIllus:
        case TemplateType::SequenceColorSnakeStepsHorizontalIconLine:
            return LayoutType::Zigzag;
            
        // Timeline layouts
        case TemplateType::SequenceTimelineSimple:
        case TemplateType::SequenceTimelineRoundedRectNode:
        case TemplateType::SequenceTimelineSimpleIllus:
        case TemplateType::SequenceStairsFrontCompactCard:
        case TemplateType::SequenceStairsFrontPillBadge:
        case TemplateType::SequenceAscendingSteps:
        case TemplateType::SequenceAscendingStairs3dUnderlineText:
        case TemplateType::SequenceMountainUnderlineText:
        case TemplateType::SequenceCylinders3dSimple:
            return LayoutType::Timeline;
            
        // Funnel layouts
        case TemplateType::SequenceFilterMeshSimple:
        case TemplateType::SequenceFunnelSimple:
        case TemplateType::SequencePyramidSimple:
            return LayoutType::Funnel;
            
        // Circular layouts
        case TemplateType::SequenceCircularSimple:
        case TemplateType::RelationCircleIconBadge:
        case TemplateType::RelationCircleCircularProgress:
            return LayoutType::Circular;
            
        // Tree layouts
        case TemplateType::HierarchyTreeTechStyleCapsuleItem:
        case TemplateType::HierarchyTreeCurvedLineRoundedRectNode:
        case TemplateType::HierarchyTreeTechStyleBadgeCard:
        case TemplateType::HierarchyStructure:
        case TemplateType::CompareHierarchyLeftRightCircleNodePillBadge:
        case TemplateType::FlowchartDecisionTree:
        case TemplateType::FlowchartBusinessProcess:
        case TemplateType::FlowchartSystemFlow:
        case TemplateType::FlowchartUserJourney:
            return LayoutType::Tree;
            
        // Quadrant layouts
        case TemplateType::QuadrantQuarterSimpleCard:
        case TemplateType::QuadrantQuarterCircular:
        case TemplateType::QuadrantSimpleIllus:
        case TemplateType::CompareSwot:
            return LayoutType::Quadrant;
            
        // Pie layouts
        case TemplateType::ChartPiePlainText:
        case TemplateType::ChartPieCompactCard:
        case TemplateType::ChartPieDonutPlainText:
        case TemplateType::ChartPieDonutPillBadge:
        case TemplateType::ChartWordcloud:
            return LayoutType::Pie;
            
        // Bar layouts
        case TemplateType::ChartBarPlainText:
        case TemplateType::ChartColumnSimple:
        case TemplateType::ChartLinePlainText:
            return LayoutType::Bar;
            
        // Default to Grid
        default:
            return LayoutType::Grid;
    }
}

} // namespace flex::modules::infographic