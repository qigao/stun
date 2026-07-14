#include "md_block_parser.h"
#include <algorithm>
#include <cctype>

namespace md {

namespace {

// 跳过前导空格，返回空格数
int skip_spaces(std::string_view& line) {
    int count = 0;
    while (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
        count += (line[0] == '\t') ? 4 : 1;
        line.remove_prefix(1);
    }
    return count;
}

// 检查是否全是空白
bool is_blank(std::string_view line) {
    return std::all_of(line.begin(), line.end(), [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    });
}

// 检查 ATX header (# Header)
bool check_atx_header(std::string_view line, int& level, std::string_view& content) {
    if (line.empty() || line[0] != '#') return false;
    
    size_t i = 0;
    while (i < line.size() && line[i] == '#' && i < 6) i++;
    level = static_cast<int>(i);
    
    if (i >= line.size()) {
        content = {};
        return true;
    }
    if (line[i] != ' ' && line[i] != '\t') return false;
    
    // 跳过空格
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
    content = line.substr(i);
    
    // 去除尾部 #
    while (!content.empty() && content.back() == '#') content.remove_suffix(1);
    while (!content.empty() && (content.back() == ' ' || content.back() == '\t')) content.remove_suffix(1);
    
    return true;
}

// 检查代码围栏 (``` 或 ~~~)
bool check_code_fence(std::string_view line, std::string_view& marker, std::string_view& info) {
    if (line.size() < 3) return false;
    
    char fence_char = line[0];
    if (fence_char != '`' && fence_char != '~') return false;
    
    size_t fence_len = 0;
    while (fence_len < line.size() && line[fence_len] == fence_char) fence_len++;
    if (fence_len < 3) return false;
    
    marker = line.substr(0, fence_len);
    info = line.substr(fence_len);
    
    // 去除 info 前后空白
    while (!info.empty() && (info.front() == ' ' || info.front() == '\t')) info.remove_prefix(1);
    while (!info.empty() && (info.back() == ' ' || info.back() == '\t' || info.back() == '\r' || info.back() == '\n')) info.remove_suffix(1);
    
    // ``` 不能出现在 info 中
    if (fence_char == '`' && info.find('`') != std::string_view::npos) return false;
    
    return true;
}

// 检查列表标记
bool check_list_marker(std::string_view line, bool& ordered, std::string_view& marker, std::string_view& content) {
    if (line.empty()) return false;
    
    // 无序列表: - + *
    if ((line[0] == '-' || line[0] == '+' || line[0] == '*') && 
        line.size() > 1 && (line[1] == ' ' || line[1] == '\t')) {
        ordered = false;
        marker = line.substr(0, 2);
        content = line.substr(2);
        return true;
    }
    
    // 有序列表: 1. 或 1)
    size_t i = 0;
    while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) i++;
    if (i == 0 || i > 9) return false;  // 最多9位数字
    if (i >= line.size()) return false;
    if (line[i] != '.' && line[i] != ')') return false;
    if (i + 1 >= line.size() || (line[i + 1] != ' ' && line[i + 1] != '\t')) return false;
    
    ordered = true;
    marker = line.substr(0, i + 2);
    content = line.substr(i + 2);
    return true;
}

// 检查水平线 (---, ***, ___)
bool check_horizontal_rule(std::string_view line) {
    if (line.size() < 3) return false;
    
    char rule_char = 0;
    int count = 0;
    
    for (char c : line) {
        if (c == ' ' || c == '\t') continue;
        if (c == '\r' || c == '\n') break;
        
        if (rule_char == 0) {
            if (c != '-' && c != '*' && c != '_') return false;
            rule_char = c;
        }
        if (c != rule_char) return false;
        count++;
    }
    
    return count >= 3;
}

// 检查表格分隔行
bool check_table_delimiter(std::string_view line) {
    bool has_pipe = false;
    bool has_dash = false;
    
    for (char c : line) {
        if (c == '|') has_pipe = true;
        else if (c == '-') has_dash = true;
        else if (c != ' ' && c != '\t' && c != ':' && c != '\r' && c != '\n') return false;
    }
    
    return has_pipe && has_dash;
}

// 检查 BlockQuote (> prefix)
bool check_block_quote(std::string_view line, std::string_view& content) {
    if (line.empty() || line[0] != '>') return false;
    
    content = line.substr(1);
    // 跳过可选的一个空格
    if (!content.empty() && content[0] == ' ') content.remove_prefix(1);
    return true;
}

// 检查脚注定义 [^id]: content
bool check_footnote_def(std::string_view line, std::string_view& id, std::string_view& content) {
    if (line.size() < 5 || line[0] != '[' || line[1] != '^') return false;
    
    size_t close = line.find("]:");
    if (close == std::string_view::npos || close < 3) return false;
    
    id = line.substr(2, close - 2);
    content = line.substr(close + 2);
    while (!content.empty() && content[0] == ' ') content.remove_prefix(1);
    return true;
}

// 检查定义描述 : description
bool check_def_desc(std::string_view line, std::string_view& content) {
    if (line.empty() || line[0] != ':') return false;
    if (line.size() < 2 || line[1] != ' ') return false;
    
    content = line.substr(2);
    return true;
}

} // anonymous namespace

LineInfo BlockParser::analyze_line(std::string_view line) {
    LineInfo info;
    
    // 去除行尾换行
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.remove_suffix(1);
    }
    
    // 空行
    if (is_blank(line)) {
        info.type = LineType::Blank;
        return info;
    }
    
    // 计算缩进
    std::string_view trimmed = line;
    info.indent = skip_spaces(trimmed);
    
    // 缩进代码块 (4空格)
    if (info.indent >= 4 && !in_code_block_) {
        info.type = LineType::IndentedCode;
        info.content = line.substr(4);  // 保留超过4空格的部分
        return info;
    }
    
    // 代码围栏
    std::string_view fence_marker, fence_info;
    if (info.indent < 4 && check_code_fence(trimmed, fence_marker, fence_info)) {
        info.type = LineType::CodeFence;
        info.marker = fence_marker;
        info.info = fence_info;
        return info;
    }
    
    // ATX Header
    int header_level;
    std::string_view header_content;
    if (info.indent < 4 && check_atx_header(trimmed, header_level, header_content)) {
        info.type = LineType::ATXHeader;
        info.level = header_level;
        info.content = header_content;
        return info;
    }
    
    // 水平线
    if (info.indent < 4 && check_horizontal_rule(trimmed)) {
        info.type = LineType::HorizontalRule;
        return info;
    }
    
    // BlockQuote
    std::string_view quote_content;
    if (info.indent < 4 && check_block_quote(trimmed, quote_content)) {
        info.type = LineType::BlockQuote;
        info.content = quote_content;
        return info;
    }
    
    // 脚注定义 [^id]: content
    std::string_view fn_id, fn_content;
    if (info.indent < 4 && check_footnote_def(trimmed, fn_id, fn_content)) {
        info.type = LineType::FootnoteDef;
        info.marker = fn_id;
        info.content = fn_content;
        return info;
    }
    
    // 定义描述 : description
    std::string_view def_content;
    if (info.indent < 4 && check_def_desc(trimmed, def_content)) {
        info.type = LineType::DefDesc;
        info.content = def_content;
        return info;
    }
    
    // 列表标记
    std::string_view list_marker, list_content;
    if (info.indent < 4 && check_list_marker(trimmed, info.ordered, list_marker, list_content)) {
        info.type = LineType::ListMarker;
        info.marker = list_marker;
        info.content = list_content;
        return info;
    }
    
    // 表格分隔行
    if (check_table_delimiter(trimmed)) {
        info.type = LineType::TableDelimiter;
        info.content = trimmed;
        return info;
    }
    
    // 默认: 段落
    info.type = LineType::Paragraph;
    info.content = trimmed;
    return info;
}

std::unique_ptr<Block> BlockParser::parse(std::string_view input) {
    root_ = std::make_unique<Block>(BlockType::Document);
    stack_.clear();
    stack_.push_back(root_.get());
    in_code_block_ = false;
    code_fence_marker_.clear();
    
    // 按行处理
    size_t pos = 0;
    while (pos < input.size()) {
        size_t end = input.find('\n', pos);
        if (end == std::string_view::npos) end = input.size();
        
        std::string_view line = input.substr(pos, end - pos);
        add_line_to_block(analyze_line(line), line);
        
        pos = end + 1;
    }
    
    // 关闭所有打开的块
    while (stack_.size() > 1) close_block();
    
    return std::move(root_);
}

void BlockParser::close_block() {
    if (stack_.size() <= 1) return;
    stack_.pop_back();
}

void BlockParser::add_line_to_block(const LineInfo& line, std::string_view raw_line) {
    Block* current = stack_.back();
    
    // 代码块内部: 只检查结束标记
    if (in_code_block_) {
        if (line.type == LineType::CodeFence && 
            line.marker.size() >= code_fence_marker_.size() &&
            line.marker[0] == code_fence_marker_[0]) {
            in_code_block_ = false;
            code_fence_marker_.clear();
            close_block();
        } else {
            // 添加原始行到代码块（保留原始内容）
            if (current->type == BlockType::CodeBlock) {
                current->content += std::string(raw_line);
                current->content += '\n';
            }
        }
        return;
    }
    
    switch (line.type) {
        case LineType::Blank:
            // 空行可能结束段落
            if (current->type == BlockType::Paragraph) {
                close_block();
            }
            break;
            
        case LineType::ATXHeader: {
            if (current->type == BlockType::Paragraph) close_block();
            // Break list if header is at indent 0
            if (line.indent == 0) {
                while (stack_.size() > 1) {
                    BlockType t = stack_.back()->type;
                    if (t == BlockType::ListItem || t == BlockType::List) {
                        close_block();
                    } else {
                        break;
                    }
                }
            }
            auto header = std::make_unique<Block>(BlockType::Header);
            header->level = line.level;
            header->content = std::string(line.content);
            stack_.back()->children.push_back(std::move(header));
            break;
        }
        
        case LineType::CodeFence: {
            if (current->type == BlockType::Paragraph) close_block();
            // Break list if code fence is at indent 0
            if (line.indent == 0) {
                while (stack_.size() > 1) {
                    BlockType t = stack_.back()->type;
                    if (t == BlockType::ListItem || t == BlockType::List) {
                        close_block();
                    } else {
                        break;
                    }
                }
            }
            auto code = std::make_unique<Block>(BlockType::CodeBlock);
            code->info = std::string(line.info);
            stack_.back()->children.push_back(std::move(code));
            stack_.push_back(stack_.back()->children.back().get());
            in_code_block_ = true;
            code_fence_marker_ = std::string(line.marker);
            break;
        }
        
        case LineType::IndentedCode: {
            if (current->type == BlockType::CodeBlock && current->info.empty()) {
                // 继续缩进代码块
                current->content += std::string(line.content);
                current->content += '\n';
            } else {
                if (current->type == BlockType::Paragraph) close_block();
                auto code = std::make_unique<Block>(BlockType::CodeBlock);
                code->content = std::string(line.content) + '\n';
                stack_.back()->children.push_back(std::move(code));
                stack_.push_back(stack_.back()->children.back().get());
            }
            break;
        }
        
        case LineType::HorizontalRule: {
            if (current->type == BlockType::Paragraph) close_block();

            // Break list if HR is at indent 0
            if (line.indent == 0) {
                while (stack_.size() > 1) {
                    BlockType t = stack_.back()->type;
                    if (t == BlockType::ListItem || t == BlockType::List) {
                        close_block();
                    } else {
                        break;
                    }
                }
            }

            auto hr = std::make_unique<Block>(BlockType::HorizontalRule);
            stack_.back()->children.push_back(std::move(hr));
            break;
        }
        
        case LineType::BlockQuote: {
            if (current->type == BlockType::Paragraph) close_block();
            current = stack_.back();
            
            // 如果当前不在 BlockQuote 中，创建一个
            if (current->type != BlockType::BlockQuote) {
                auto quote = std::make_unique<Block>(BlockType::BlockQuote);
                stack_.back()->children.push_back(std::move(quote));
                stack_.push_back(stack_.back()->children.back().get());
                current = stack_.back();
            }
            
            // 检查是否有嵌套 blockquote (> > text)
            std::string_view content = line.content;
            while (!content.empty() && content[0] == '>') {
                content.remove_prefix(1);
                if (!content.empty() && content[0] == ' ') content.remove_prefix(1);
                
                // 创建嵌套 BlockQuote
                if (current->children.empty() || 
                    current->children.back()->type != BlockType::BlockQuote) {
                    auto nested = std::make_unique<Block>(BlockType::BlockQuote);
                    current->children.push_back(std::move(nested));
                }
                current = current->children.back().get();
            }
            
            // 添加内容
            if (!content.empty()) {
                auto para = std::make_unique<Block>(BlockType::Paragraph);
                para->content = std::string(content);
                current->children.push_back(std::move(para));
            }
            break;
        }
        
        case LineType::FootnoteDef: {
            if (current->type == BlockType::Paragraph) close_block();
            auto fn = std::make_unique<Block>(BlockType::FootnoteDef);
            fn->info = std::string(line.marker);
            fn->content = std::string(line.content);
            stack_.back()->children.push_back(std::move(fn));
            break;
        }
        
        case LineType::DefDesc: {
            // 定义描述：前一个块应该是段落（术语）或已有的定义列表
            Block* parent = stack_.back();
            
            // 查找或创建定义列表
            if (current->type == BlockType::Paragraph) {
                // 把段落转换为术语
                std::string term_content = current->content;
                close_block();
                
                // 移除段落
                if (!parent->children.empty() && 
                    parent->children.back()->type == BlockType::Paragraph) {
                    parent->children.pop_back();
                }
                
                // 创建定义列表
                auto dl = std::make_unique<Block>(BlockType::DefList);
                auto term = std::make_unique<Block>(BlockType::DefTerm);
                term->content = term_content;
                dl->children.push_back(std::move(term));
                
                auto desc = std::make_unique<Block>(BlockType::DefDesc);
                desc->content = std::string(line.content);
                dl->children.push_back(std::move(desc));
                
                parent->children.push_back(std::move(dl));
                stack_.push_back(parent->children.back().get());
            } else if (current->type == BlockType::DefList) {
                // 添加到现有定义列表
                auto desc = std::make_unique<Block>(BlockType::DefDesc);
                desc->content = std::string(line.content);
                current->children.push_back(std::move(desc));
            }
            break;
        }
        
        case LineType::ListMarker: {
            if (current->type == BlockType::Paragraph) close_block();
            current = stack_.back();  // 刷新 current
            
            // 检查是否可以继续当前列表
            bool need_new_list = true;
            if (current->type == BlockType::ListItem) {
                Block* list = stack_[stack_.size() - 2];
                if (list->type == BlockType::List && list->ordered == line.ordered) {
                    need_new_list = false;
                    close_block();  // 关闭当前 ListItem
                } else {
                    // 列表类型不同，关闭当前列表
                    close_block();  // 关闭 ListItem
                    close_block();  // 关闭 List
                }
            } else if (current->type == BlockType::List) {
                if (current->ordered == line.ordered) {
                    need_new_list = false;
                } else {
                    close_block();  // 关闭旧列表
                }
            }
            
            if (need_new_list) {
                auto list = std::make_unique<Block>(BlockType::List);
                list->ordered = line.ordered;
                stack_.back()->children.push_back(std::move(list));
                stack_.push_back(stack_.back()->children.back().get());
            }
            
            // 创建 ListItem
            auto item = std::make_unique<Block>(BlockType::ListItem);
            item->content = std::string(line.content);
            stack_.back()->children.push_back(std::move(item));
            stack_.push_back(stack_.back()->children.back().get());
            break;
        }
        
        case LineType::TableDelimiter: {
            // 表格分隔行: 前一个块必须是段落，转换为表格头
            if (current->type == BlockType::Paragraph) {
                std::string header_content = current->content;
                close_block();
                
                Block* parent = stack_.back();
                if (!parent->children.empty() && 
                    parent->children.back()->type == BlockType::Paragraph) {
                    parent->children.pop_back();
                }
                
                // 解析对齐信息
                std::string alignments;
                std::string_view delim = line.content;
                size_t dpos = (delim[0] == '|') ? 1 : 0;
                while (dpos < delim.size()) {
                    size_t dnext = delim.find('|', dpos);
                    if (dnext == std::string_view::npos) dnext = delim.size();
                    std::string_view col = delim.substr(dpos, dnext - dpos);
                    
                    // trim
                    while (!col.empty() && col[0] == ' ') col.remove_prefix(1);
                    while (!col.empty() && col.back() == ' ') col.remove_suffix(1);
                    
                    if (!col.empty()) {
                        bool left = (col[0] == ':');
                        bool right = (col.back() == ':');
                        if (left && right) alignments += 'c';      // center
                        else if (right) alignments += 'r';         // right
                        else alignments += 'l';                    // left (default)
                    }
                    dpos = dnext + 1;
                }
                
                auto table = std::make_unique<Block>(BlockType::Table);
                table->info = alignments;  // 存储对齐信息
                
                // 解析表头
                auto header_row = std::make_unique<Block>(BlockType::TableRow);
                size_t pos = 0;
                if (!header_content.empty() && header_content[0] == '|') pos = 1;
                size_t end = header_content.size();
                if (end > 0 && header_content[end-1] == '|') end--;
                
                while (pos < end) {
                    size_t next = header_content.find('|', pos);
                    if (next == std::string::npos || next > end) next = end;
                    
                    auto cell = std::make_unique<Block>(BlockType::TableCell);
                    std::string cell_content = header_content.substr(pos, next - pos);
                    size_t s = cell_content.find_first_not_of(" \t");
                    size_t e = cell_content.find_last_not_of(" \t");
                    if (s != std::string::npos && e != std::string::npos) {
                        cell->content = cell_content.substr(s, e - s + 1);
                    }
                    header_row->children.push_back(std::move(cell));
                    pos = next + 1;
                }
                
                table->children.push_back(std::move(header_row));
                stack_.back()->children.push_back(std::move(table));
                stack_.push_back(stack_.back()->children.back().get());
            }
            break;
        }
            
        case LineType::Paragraph:
        default:
            // 检查是否是表格数据行
            if (current->type == BlockType::Table && line.content.find('|') != std::string_view::npos) {
                auto row = std::make_unique<Block>(BlockType::TableRow);
                std::string row_content(line.content);
                size_t pos = 0;
                if (!row_content.empty() && row_content[0] == '|') pos = 1;
                size_t end = row_content.size();
                if (end > 0 && row_content[end-1] == '|') end--;
                
                while (pos < end) {
                    size_t next = row_content.find('|', pos);
                    if (next == std::string::npos || next > end) next = end;
                    
                    auto cell = std::make_unique<Block>(BlockType::TableCell);
                    std::string cell_content = row_content.substr(pos, next - pos);
                    size_t s = cell_content.find_first_not_of(" \t");
                    size_t e = cell_content.find_last_not_of(" \t");
                    if (s != std::string::npos && e != std::string::npos) {
                        cell->content = cell_content.substr(s, e - s + 1);
                    }
                    row->children.push_back(std::move(cell));
                    pos = next + 1;
                }
                
                current->children.push_back(std::move(row));
            } else if (current->type == BlockType::Paragraph) {
                // 继续当前段落
                current->content += ' ';
                current->content += std::string(line.content);
            } else if (current->type == BlockType::ListItem && current->content.empty()) {
                // ListItem 的第一行内容
                current->content = std::string(line.content);
            } else {
                // 新段落
                if (current->type == BlockType::Table) close_block();  // 结束表格
                auto para = std::make_unique<Block>(BlockType::Paragraph);
                para->content = std::string(line.content);
                stack_.back()->children.push_back(std::move(para));
                stack_.push_back(stack_.back()->children.back().get());
            }
            break;
    }
}

} // namespace md
