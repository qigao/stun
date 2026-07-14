#pragma once

#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <memory>
#include <optional>
#include <vector>

namespace md {

// 代码块处理结果
struct BlockResult {
    enum class Type { Text, Svg, FlexGroup };
    
    Type type = Type::Text;
    std::string content;      // SVG 字符串或原始文本
    void* flex_group = nullptr; // 如果是 FlexGroup，指向 flex::Group*
};

// 代码块处理器签名
// 输入: 代码块内容
// 输出: 处理结果
using BlockHandler = std::function<BlockResult(std::string_view content)>;

// 扩展注册表 - 单例
class ExtensionRegistry {
public:
    static ExtensionRegistry& instance();
    
    // 注册处理器
    void register_handler(const std::string& language, BlockHandler handler);
    
    // 查找处理器
    BlockHandler* find_handler(const std::string& language);
    
    // 检查是否有处理器
    bool has_handler(const std::string& language) const;
    
    // 处理代码块 - 如果没有处理器，返回 nullopt
    std::optional<BlockResult> process(const std::string& language, std::string_view content);

private:
    ExtensionRegistry() = default;
    std::unordered_map<std::string, BlockHandler> handlers_;
};

// 便利宏 - 自动注册
#define MD_REGISTER_HANDLER(lang, handler) \
    static bool _md_reg_##lang = (md::ExtensionRegistry::instance().register_handler(#lang, handler), true)

} // namespace md
