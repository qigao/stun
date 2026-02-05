#pragma once

#include <ir/unified_infographic.h>
#include <string>
#include <sstream>
#include <memory>

namespace flex::modules::infographic {

// SVG 构建上下文 - 消除重复的 SVG 生成代码
struct SvgContext {
    std::ostringstream svg;
    int width = 800;
    int height = 600;
    int content_start_y = 100;  // 标题/描述后的内容起始位置
    
    // 便捷方法
    void set_size(int w, int h) { width = w; height = h; }
};

// 布局参数 - 统一的布局配置
struct LayoutParams {
    int margin = 20;
    int padding = 15;
    int item_spacing = 10;
    int card_width = 200;
    int card_height = 120;
    int node_radius = 20;
    int line_width = 2;
};

/**
 * 模板渲染器基类 - 模板方法模式
 * 
 * 设计原则：
 * 1. render() 是 final 的，定义了渲染流程骨架
 * 2. 子类只需实现 render_content() 来定义具体布局
 * 3. 公共逻辑（header/footer/title/desc）在基类中实现一次
 */
class TemplateRenderer {
public:
    virtual ~TemplateRenderer() = default;
    
    // 模板方法 - 定义渲染流程骨架，不可覆盖
    virtual std::string render(const UnifiedInfographic& infographic) final;
    
    // 获取渲染器名称（用于调试）
    virtual std::string name() const = 0;
    
protected:
    // 子类必须实现：渲染具体内容
    virtual void render_content(SvgContext& ctx, const UnifiedInfographic& infographic) = 0;
    
    // 子类可选覆盖：计算画布尺寸
    virtual void calculate_canvas_size(SvgContext& ctx, const UnifiedInfographic& infographic);
    
    // 子类可选覆盖：获取布局参数
    virtual LayoutParams get_layout_params() const { return LayoutParams{}; }
    
    // ========== 受保护的工具方法 ==========
    
    // SVG 基础元素
    static void svg_rect(std::ostream& os, int x, int y, int w, int h, 
                        const std::string& fill, int rx = 0, float opacity = 1.0f,
                        const std::string& stroke = "", int stroke_width = 0);
    
    static void svg_circle(std::ostream& os, int cx, int cy, int r,
                          const std::string& fill, float opacity = 1.0f,
                          const std::string& stroke = "", int stroke_width = 0);
    
    static void svg_line(std::ostream& os, int x1, int y1, int x2, int y2,
                        const std::string& stroke, int stroke_width = 2);
    
    static void svg_text(std::ostream& os, int x, int y, const std::string& text,
                        int font_size = 14, const std::string& fill = "#333",
                        const std::string& anchor = "start", bool bold = false);
    
    static void svg_path(std::ostream& os, const std::string& d,
                        const std::string& fill = "none", 
                        const std::string& stroke = "#333", int stroke_width = 2);
    
    // 颜色工具
    static std::string get_color(const Theme& theme, size_t index);
    
    // XML 转义
    static std::string escape_xml(const std::string& text);
    
private:
    // 私有方法 - 渲染流程的固定部分
    void render_header(SvgContext& ctx);
    void render_title_desc(SvgContext& ctx, const UnifiedInfographic& infographic);
    void render_footer(SvgContext& ctx);
};

/**
 * 渲染器工厂 - 策略模式
 * 
 * 根据模板类型创建对应的渲染器实例
 */
class RendererFactory {
public:
    static std::unique_ptr<TemplateRenderer> create(TemplateType type);
    static std::unique_ptr<TemplateRenderer> create(TemplateCategory category, TemplateType type);
    
    // 注册自定义渲染器（扩展点）
    using RendererCreator = std::unique_ptr<TemplateRenderer>(*)();
    static void register_renderer(TemplateType type, RendererCreator creator);
    
private:
    static std::unique_ptr<TemplateRenderer> create_list_renderer(TemplateType type);
    static std::unique_ptr<TemplateRenderer> create_sequence_renderer(TemplateType type);
    static std::unique_ptr<TemplateRenderer> create_compare_renderer(TemplateType type);
    static std::unique_ptr<TemplateRenderer> create_hierarchy_renderer(TemplateType type);
    static std::unique_ptr<TemplateRenderer> create_chart_renderer(TemplateType type);
    static std::unique_ptr<TemplateRenderer> create_quadrant_renderer(TemplateType type);
    static std::unique_ptr<TemplateRenderer> create_relation_renderer(TemplateType type);
    static std::unique_ptr<TemplateRenderer> create_flowchart_renderer(TemplateType type);
    static std::unique_ptr<TemplateRenderer> create_process_renderer(TemplateType type);
};

} // namespace flex::modules::infographic
