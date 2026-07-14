#pragma once

#include <ir/unified_infographic.h>
#include <vector>
#include <string>
#include <memory>

namespace flex::modules::infographic {

// ============================================================================
// Layout Result - 布局计算结果
// ============================================================================

struct LayoutRect {
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;
};

struct LayoutNode {
    LayoutRect bounds;
    const DataItem* item = nullptr;
    int index = 0;
    std::vector<LayoutNode> children;
};

struct LayoutResult {
    int canvas_width = 800;
    int canvas_height = 600;
    int content_start_y = 100;
    std::vector<LayoutNode> nodes;
    std::vector<int> parent_index;
};

// ============================================================================
// Style Config - 从模板名解析的样式配置
// ============================================================================

struct StyleConfig {
    // Card appearance
    int card_radius = 8;
    bool show_border = true;
    float card_opacity = 1.0f;
    
    // Content display
    bool show_icon = false;
    bool show_badge = false;
    bool show_illus = false;
    std::string icon_position = "left";  // left, top, center
    
    // Typography
    int title_font_size = 16;
    int desc_font_size = 12;
    int value_font_size = 20;
    bool title_bold = true;
    
    // Layout hints
    int max_columns = 3;
    int item_spacing = 20;
    int card_width = 200;
    int card_height = 120;
    
    // Special styles
    bool is_3d = false;
    bool is_compact = false;
    bool show_connector = true;
    std::string connector_style = "line";  // line, arrow, curve
};

// ============================================================================
// Layout Engine - 布局算法基类
// ============================================================================

class LayoutEngine {
public:
    virtual ~LayoutEngine() = default;
    
    // 计算布局
    virtual LayoutResult compute(const UnifiedInfographic& infographic, 
                                 int width, int height,
                                 const StyleConfig& style) = 0;
    
    // 布局类型名称
    virtual const char* name() const = 0;
};

// ============================================================================
// Concrete Layout Engines
// ============================================================================

class GridLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Grid"; }
};

class RowLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Row"; }
};

class ColumnLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Column"; }
};

class ZigzagLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Zigzag"; }
};

class TimelineLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Timeline"; }
};

class FunnelLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Funnel"; }
};

class CircularLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Circular"; }
};

class TreeLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Tree"; }
};

class QuadrantLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Quadrant"; }
};

class PieLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Pie"; }
};

class BarLayoutEngine : public LayoutEngine {
public:
    LayoutResult compute(const UnifiedInfographic& infographic, int width, int height,
                        const StyleConfig& style) override;
    const char* name() const override { return "Bar"; }
};

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<LayoutEngine> create_layout_engine(TemplateType type);
StyleConfig parse_style_from_template(const std::string& template_name);

} // namespace flex::modules::infographic
