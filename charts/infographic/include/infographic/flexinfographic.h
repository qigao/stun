#pragma once

#include <ir/unified_infographic.h>
#include <parser/unified_parser.h>
#include <string>
#include <string_view>
#include <memory>

namespace flex {
class Instance;
class Group;
}

namespace flex::modules::infographic {

/**
 * FlexInfographic - 信息图渲染引擎
 * 
 * 设计原则：
 * 1. 保持 API 稳定 (Never break userspace)
 * 2. 内部使用策略模式委托给具体渲染器
 * 3. 简洁的公共接口，复杂性封装在内部
 */
class FlexInfographic {
public:
    FlexInfographic();
    ~FlexInfographic();
    
    // ========== 核心 API (保持不变) ==========
    ParseResult parse(const std::string& infographic_text);
    std::string infographic_to_svg(const std::string& infographic_text);
    std::string render_svg(const UnifiedInfographic& infographic);
    
    // Flex 运行时渲染
    flex::Group* to_flex(const UnifiedInfographic& infographic, flex::Instance& instance);
    flex::Group* to_flex(std::string_view source, flex::Instance& instance);
    
    // ========== 主题管理 ==========
    void set_theme(const Theme& theme);
    const Theme& get_theme() const;
    
    // ========== 模板信息 (静态方法) ==========
    static std::vector<std::string> get_available_templates();
    static std::vector<std::string> get_templates_by_category(TemplateCategory category);
    static std::string get_template_description(TemplateType type);
    static bool validate_template_name(const std::string& template_name);
    static std::string get_template_requirements(TemplateType type);
    
private:
    std::unique_ptr<UnifiedParser> parser_;
    Theme current_theme_;
    bool has_theme_override_ = false;
};

// 便利函数
std::string quick_infographic(const std::string& template_name, 
                             const std::vector<std::string>& labels,
                             const std::vector<double>& values = {});

std::string create_kpi_dashboard(const std::vector<std::pair<std::string, std::string>>& kpis);
std::string create_timeline(const std::vector<std::pair<std::string, std::string>>& events);
std::string create_comparison(const std::string& title,
                             const std::string& option_a, const std::vector<std::string>& features_a,
                             const std::string& option_b, const std::vector<std::string>& features_b);
std::string create_swot_analysis(const std::vector<std::string>& strengths,
                                const std::vector<std::string>& weaknesses,
                                const std::vector<std::string>& opportunities,
                                const std::vector<std::string>& threats);

} // namespace flex::modules::infographic
