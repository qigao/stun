#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace md_re2c {

// 块类型
enum class BlockType {
    Document,
    Paragraph,
    Header,
    CodeBlock,
    List,
    ListItem,
    Table,
    TableRow,
    TableCell,
    HorizontalRule,
    BlockQuote,
    FootnoteDef,
    DefList,        // 定义列表
    DefTerm,        // 术语
    DefDesc,        // 描述
    BlankLine
};

// 块节点 - 第一阶段输出
struct Block {
    BlockType type;
    int level = 0;              // header level 或 list indent level
    bool ordered = false;       // 有序列表?
    bool tight = true;          // 紧凑列表?
    std::string content;        // 原始文本内容 (代码块/段落)
    std::string info;           // 代码块语言 / 表格对齐
    std::vector<std::unique_ptr<Block>> children;
    
    Block(BlockType t) : type(t) {}
};

// 行类型 (内部使用)
enum class LineType {
    Blank,
    ATXHeader,
    SetextHeader,
    CodeFence,
    IndentedCode,
    ListMarker,
    TableDelimiter,
    HorizontalRule,
    BlockQuote,
    FootnoteDef,
    DefDesc,            // : definition
    Paragraph
};

// 行信息
struct LineInfo {
    LineType type;
    int indent = 0;             // 前导空格数
    int level = 0;              // header level
    bool ordered = false;       // 有序列表?
    std::string_view content;   // 去除标记后的内容
    std::string_view marker;    // 列表标记 (如 "- " 或 "1. ")
    std::string_view info;      // 代码块语言
};

// Block Parser
class BlockParser {
public:
    std::unique_ptr<Block> parse(std::string_view input);
    
private:
    // 行分析
    LineInfo analyze_line(std::string_view line);
    
    // 块构建
    void close_block();
    void add_line_to_block(const LineInfo& line);
    
    // 列表处理
    int calculate_list_indent(std::string_view line);
    bool can_continue_list(const LineInfo& line, int current_indent);
    
    // 表格处理
    bool is_table_delimiter(std::string_view line);
    std::vector<std::string_view> split_table_row(std::string_view line);
    
    // 状态
    std::unique_ptr<Block> root_;
    std::vector<Block*> stack_;     // 当前打开的块栈
    bool in_code_block_ = false;
    std::string code_fence_marker_; // 记录开始的 fence (``` 或 ~~~)
};

} // namespace md_re2c
