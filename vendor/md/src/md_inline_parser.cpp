#include "md_inline_parser.h"
#include <vector>
#include <cctype>
#include <algorithm>

namespace md {

Node* InlineParser::create_node(NodeType type) {
    void* mem = pool_alloc(pool_, sizeof(Node));
    return new (mem) Node(type);
}

Node* InlineParser::create_text(std::string_view text) {
    if (text.empty()) return nullptr;
    Node* node = create_node(NodeType::Text);
    node->text = std::string(text);
    return node;
}

Node* InlineParser::transform(const Block& block) {
    switch (block.type) {
        case BlockType::Document: {
            Node* node = create_node(NodeType::Document);
            for (const auto& child : block.children) {
                if (Node* n = transform(*child)) node->children.push_back(n);
            }
            return node;
        }
        case BlockType::Paragraph: {
            Node* node = create_node(NodeType::Paragraph);
            parse_inlines(block.content, node);
            return node;
        }
        case BlockType::Header: {
            Node* node = create_node(NodeType::Header);
            node->level = block.level;
            parse_inlines(block.content, node);
            return node;
        }
        case BlockType::CodeBlock: {
            Node* node = create_node(NodeType::CodeBlock);
            node->language = block.info;  // Store language identifier
            if (!block.content.empty()) {
                node->children.push_back(create_text(block.content));
            }
            return node;
        }
        case BlockType::List: {
            Node* node = create_node(block.ordered ? NodeType::OrderedList : NodeType::List);
            for (const auto& child : block.children) {
                if (Node* n = transform(*child)) node->children.push_back(n);
            }
            return node;
        }
        case BlockType::ListItem: {
            std::string_view content = block.content;
            bool is_task = false;
            bool checked = false;
            
            // 检查 task list: [ ] 或 [x] 或 [X]
            if (content.size() >= 3 && content[0] == '[' && content[2] == ']') {
                char mark = content[1];
                if (mark == ' ' || mark == 'x' || mark == 'X') {
                    is_task = true;
                    checked = (mark != ' ');
                    content = content.substr(3);
                    // 跳过空格
                    while (!content.empty() && content[0] == ' ') content.remove_prefix(1);
                }
            }
            
            Node* node = create_node(is_task ? NodeType::TaskItem : NodeType::ListItem);
            
            if (is_task) {
                // 添加 Checkbox 节点
                Node* checkbox = create_node(NodeType::Checkbox);
                checkbox->text = checked ? "x" : " ";
                node->children.push_back(checkbox);
            }
            
            if (!content.empty()) parse_inlines(content, node);
            for (const auto& child : block.children) {
                if (Node* n = transform(*child)) node->children.push_back(n);
            }
            return node;
        }
        case BlockType::HorizontalRule:
            return create_node(NodeType::HorizontalRule);
        case BlockType::BlockQuote: {
            Node* node = create_node(NodeType::BlockQuote);
            for (const auto& child : block.children) {
                if (Node* n = transform(*child)) node->children.push_back(n);
            }
            return node;
        }
        case BlockType::FootnoteDef: {
            Node* node = create_node(NodeType::FootnoteDef);
            node->text = block.info;
            parse_inlines(block.content, node);
            return node;
        }
        case BlockType::DefList: {
            Node* node = create_node(NodeType::DefList);
            for (const auto& child : block.children) {
                if (Node* n = transform(*child)) node->children.push_back(n);
            }
            return node;
        }
        case BlockType::DefTerm: {
            Node* node = create_node(NodeType::DefTerm);
            parse_inlines(block.content, node);
            return node;
        }
        case BlockType::DefDesc: {
            Node* node = create_node(NodeType::DefDesc);
            parse_inlines(block.content, node);
            return node;
        }
        case BlockType::Table: {
            Node* node = create_node(NodeType::Table);
            for (const auto& child : block.children) {
                if (Node* n = transform(*child)) node->children.push_back(n);
            }
            return node;
        }
        case BlockType::TableRow: {
            Node* node = create_node(NodeType::TableRow);
            for (const auto& child : block.children) {
                if (Node* n = transform(*child)) node->children.push_back(n);
            }
            return node;
        }
        case BlockType::TableCell: {
            Node* node = create_node(NodeType::TableCell);
            parse_inlines(block.content, node);
            return node;
        }
        case BlockType::BlankLine:
            return nullptr;
    }
    return nullptr;
}

// Delimiter 信息 (用于 emphasis 处理)
struct DelimiterInfo {
    size_t node_idx;
    size_t len;
    char ch;
    bool can_open;
    bool can_close;
};

void InlineParser::parse_inlines(std::string_view text, Node* parent) {
    std::vector<Node*> nodes;
    std::vector<DelimiterInfo> delims;
    
    size_t pos = 0;
    size_t text_start = 0;
    
    auto flush_text = [&](size_t end) {
        if (end > text_start) {
            if (Node* t = create_text(text.substr(text_start, end - text_start))) {
                nodes.push_back(t);
            }
        }
    };
    
    while (pos < text.size()) {
        char c = text[pos];
        
        // 转义字符
        if (c == '\\' && pos + 1 < text.size()) {
            char next = text[pos + 1];
            if (std::ispunct(static_cast<unsigned char>(next))) {
                flush_text(pos);
                nodes.push_back(create_text(text.substr(pos + 1, 1)));
                pos += 2;
                text_start = pos;
                continue;
            }
        }
        
        // HTML tags: <u>, </u>, <br>, <hr>, autolinks
        if (c == '<') {
            // Autolink: <https://...> or <http://...>
            if (pos + 8 < text.size()) {
                bool is_https = (text.substr(pos + 1, 8) == "https://");
                bool is_http = (text.substr(pos + 1, 7) == "http://");
                if (is_https || is_http) {
                    size_t url_start = pos + 1;
                    size_t url_end = text.find('>', url_start);
                    if (url_end != std::string_view::npos) {
                        flush_text(pos);
                        Node* link = create_node(NodeType::Link);
                        std::string_view url = text.substr(url_start, url_end - url_start);
                        link->text = std::string(url);
                        if (Node* t = create_text(url)) link->children.push_back(t);
                        nodes.push_back(link);
                        pos = url_end + 1;
                        text_start = pos;
                        continue;
                    }
                }
            }
            // <u>
            if (pos + 2 < text.size() && text[pos + 1] == 'u' && text[pos + 2] == '>') {
                // 查找 </u>
                size_t close = text.find("</u>", pos + 3);
                if (close != std::string_view::npos) {
                    flush_text(pos);
                    Node* underline = create_node(NodeType::Underline);
                    std::string_view content = text.substr(pos + 3, close - pos - 3);
                    parse_inlines(content, underline);
                    nodes.push_back(underline);
                    pos = close + 4;
                    text_start = pos;
                    continue;
                }
            }
            // <br> or <br/>
            if (pos + 3 < text.size() && text[pos + 1] == 'b' && text[pos + 2] == 'r') {
                if (text[pos + 3] == '>') {
                    flush_text(pos);
                    nodes.push_back(create_node(NodeType::LineBreak));
                    pos += 4;
                    text_start = pos;
                    continue;
                }
                if (pos + 4 < text.size() && text[pos + 3] == '/' && text[pos + 4] == '>') {
                    flush_text(pos);
                    nodes.push_back(create_node(NodeType::LineBreak));
                    pos += 5;
                    text_start = pos;
                    continue;
                }
            }
        }
        
        // Math: $...$ (inline) or $$...$$ (block)
        if (c == '$') {
            bool is_block = (pos + 1 < text.size() && text[pos + 1] == '$');
            size_t delim_len = is_block ? 2 : 1;
            size_t content_start = pos + delim_len;
            
            // 查找结束标记
            size_t search = content_start;
            while (search < text.size()) {
                size_t found = text.find('$', search);
                if (found == std::string_view::npos) break;
                
                if (is_block) {
                    if (found + 1 < text.size() && text[found + 1] == '$') {
                        flush_text(pos);
                        Node* math = create_node(NodeType::MathBlock);
                        math->text = std::string(text.substr(content_start, found - content_start));
                        nodes.push_back(math);
                        pos = found + 2;
                        text_start = pos;
                        goto next_char;
                    }
                    search = found + 1;
                } else {
                    // 单 $ 不能紧跟另一个 $（那是 $$）
                    if (found > content_start) {
                        flush_text(pos);
                        Node* math = create_node(NodeType::Math);
                        math->text = std::string(text.substr(content_start, found - content_start));
                        nodes.push_back(math);
                        pos = found + 1;
                        text_start = pos;
                        goto next_char;
                    }
                    break;
                }
            }
        }
        
        // Code span
        if (c == '`') {
            flush_text(pos);
            size_t start = pos;
            size_t backticks = 0;
            while (pos < text.size() && text[pos] == '`') { backticks++; pos++; }
            
            // 查找匹配
            size_t search = pos;
            while (search < text.size()) {
                if (text[search] == '`') {
                    size_t end_start = search;
                    size_t end_count = 0;
                    while (search < text.size() && text[search] == '`') { end_count++; search++; }
                    if (end_count == backticks) {
                        Node* code = create_node(NodeType::CodeSpan);
                        std::string_view content = text.substr(pos, end_start - pos);
                        if (content.size() >= 2 && content.front() == ' ' && content.back() == ' ' &&
                            content.find_first_not_of(' ') != std::string_view::npos) {
                            content.remove_prefix(1);
                            content.remove_suffix(1);
                        }
                        if (Node* t = create_text(content)) code->children.push_back(t);
                        nodes.push_back(code);
                        pos = search;
                        text_start = pos;
                        goto next_char;
                    }
                } else {
                    search++;
                }
            }
            // 没找到匹配
            if (Node* t = create_text(text.substr(start, backticks))) nodes.push_back(t);
            text_start = pos;
            continue;
        }
        
        // Highlight: ==text==
        if (c == '=' && pos + 1 < text.size() && text[pos + 1] == '=') {
            size_t content_start = pos + 2;
            size_t close = text.find("==", content_start);
            if (close != std::string_view::npos) {
                flush_text(pos);
                Node* hl = create_node(NodeType::Highlight);
                std::string_view content = text.substr(content_start, close - content_start);
                parse_inlines(content, hl);
                nodes.push_back(hl);
                pos = close + 2;
                text_start = pos;
                continue;
            }
        }
        
        // Superscript: ^text^ (单个 ^，不是 ^^)
        if (c == '^') {
            size_t content_start = pos + 1;
            size_t close = text.find('^', content_start);
            if (close != std::string_view::npos && close > content_start) {
                // 检查内容不含空格
                std::string_view content = text.substr(content_start, close - content_start);
                if (content.find(' ') == std::string_view::npos) {
                    flush_text(pos);
                    Node* sup = create_node(NodeType::Superscript);
                    if (Node* t = create_text(content)) sup->children.push_back(t);
                    nodes.push_back(sup);
                    pos = close + 1;
                    text_start = pos;
                    continue;
                }
            }
        }
        
        // Emphasis: * _ ~
        if (c == '*' || c == '_' || c == '~') {
            flush_text(pos);
            size_t run_start = pos;
            while (pos < text.size() && text[pos] == c) pos++;
            size_t run_len = pos - run_start;
            
            // ~ 作为 strikethrough (GFM 规范：单个或双个都可以)
            if (c == '~') {
                // 查找匹配的结束标记
                size_t search = pos;
                while (search < text.size()) {
                    size_t found = text.find('~', search);
                    if (found == std::string_view::npos) break;
                    
                    // 计算结束标记长度
                    size_t end_len = 0;
                    while (found + end_len < text.size() && text[found + end_len] == '~') end_len++;
                    
                    // GFM: 1~1, 2~2, 或 1~2, 2~1 都可以匹配
                    if (end_len >= 1 && found > pos) {
                        Node* strike = create_node(NodeType::Strikethrough);
                        std::string_view content = text.substr(pos, found - pos);
                        parse_inlines(content, strike);
                        nodes.push_back(strike);
                        pos = found + std::min(run_len, end_len);
                        text_start = pos;
                        goto next_char;
                    }
                    search = found + end_len;
                }
                // 没找到匹配，当作普通文本
                if (Node* t = create_text(text.substr(run_start, run_len))) nodes.push_back(t);
                text_start = pos;
                continue;
            }
            
            char before = (run_start > 0) ? text[run_start - 1] : ' ';
            char after = (pos < text.size()) ? text[pos] : ' ';
            
            bool before_ws = std::isspace(static_cast<unsigned char>(before));
            bool after_ws = std::isspace(static_cast<unsigned char>(after));
            bool before_punct = std::ispunct(static_cast<unsigned char>(before));
            bool after_punct = std::ispunct(static_cast<unsigned char>(after));
            
            bool left_flanking = !after_ws && (!after_punct || before_ws || before_punct);
            bool right_flanking = !before_ws && (!before_punct || after_ws || after_punct);
            
            bool can_open = (c == '_') ? (left_flanking && (!right_flanking || before_punct)) : left_flanking;
            bool can_close = (c == '_') ? (right_flanking && (!left_flanking || after_punct)) : right_flanking;
            
            if (Node* t = create_text(text.substr(run_start, run_len))) {
                nodes.push_back(t);
                if (can_open || can_close) {
                    delims.push_back({nodes.size() - 1, run_len, c, can_open, can_close});
                }
            }
            text_start = pos;
            continue;
        }
        
        // Footnote reference: [^id]
        if (c == '[' && pos + 2 < text.size() && text[pos + 1] == '^') {
            size_t close = text.find(']', pos + 2);
            if (close != std::string_view::npos && close > pos + 2) {
                // 确保不是脚注定义 [^id]:
                if (close + 1 >= text.size() || text[close + 1] != ':') {
                    flush_text(pos);
                    Node* fn = create_node(NodeType::Footnote);
                    fn->text = std::string(text.substr(pos + 2, close - pos - 2));
                    nodes.push_back(fn);
                    pos = close + 1;
                    text_start = pos;
                    continue;
                }
            }
        }
        
        // Image: ![alt](url)
        if (c == '!' && pos + 1 < text.size() && text[pos + 1] == '[') {
            size_t start = pos;
            pos += 2;
            size_t alt_start = pos;
            int depth = 1;
            while (pos < text.size() && depth > 0) {
                if (text[pos] == '[') depth++;
                else if (text[pos] == ']') depth--;
                pos++;
            }
            if (depth == 0 && pos < text.size() && text[pos] == '(') {
                size_t alt_end = pos - 1;
                pos++;
                size_t url_start = pos;
                int paren_depth = 1;
                while (pos < text.size() && paren_depth > 0) {
                    if (text[pos] == '(') paren_depth++;
                    else if (text[pos] == ')') paren_depth--;
                    if (paren_depth > 0) pos++;
                }
                if (paren_depth == 0) {
                    flush_text(start);
                    Node* img = create_node(NodeType::Image);
                    img->text = std::string(text.substr(url_start, pos - url_start));
                    if (Node* t = create_text(text.substr(alt_start, alt_end - alt_start))) {
                        img->children.push_back(t);
                    }
                    nodes.push_back(img);
                    pos++;
                    text_start = pos;
                    continue;
                }
            }
            pos = start + 1;
        }
        
        // Bare URL autolink: https://... or http://...
        if (c == 'h' && pos + 7 < text.size()) {
            bool is_https = (text.substr(pos, 8) == "https://");
            bool is_http = (text.substr(pos, 7) == "http://");
            if (is_https || is_http) {
                flush_text(pos);
                size_t url_start = pos;
                // URL 结束于空白或常见分隔符
                while (pos < text.size()) {
                    char ch = text[pos];
                    if (std::isspace(static_cast<unsigned char>(ch)) || 
                        ch == '<' || ch == '>' || ch == '"' || ch == '\'' ||
                        ch == ')' || ch == ']') break;
                    pos++;
                }
                // 去除尾部标点
                while (pos > url_start && (text[pos-1] == '.' || text[pos-1] == ',' || 
                       text[pos-1] == '!' || text[pos-1] == '?' || text[pos-1] == ';')) {
                    pos--;
                }
                Node* link = create_node(NodeType::Link);
                std::string_view url = text.substr(url_start, pos - url_start);
                link->text = std::string(url);
                if (Node* t = create_text(url)) link->children.push_back(t);
                nodes.push_back(link);
                text_start = pos;
                continue;
            }
        }
        
        // Link: [text](url)
        if (c == '[') {
            size_t start = pos;
            pos++;
            size_t link_text_start = pos;
            int depth = 1;
            while (pos < text.size() && depth > 0) {
                if (text[pos] == '[') depth++;
                else if (text[pos] == ']') depth--;
                pos++;
            }
            if (depth == 0 && pos < text.size() && text[pos] == '(') {
                size_t link_text_end = pos - 1;
                pos++;
                size_t url_start = pos;
                int paren_depth = 1;
                while (pos < text.size() && paren_depth > 0) {
                    if (text[pos] == '(') paren_depth++;
                    else if (text[pos] == ')') paren_depth--;
                    if (paren_depth > 0) pos++;
                }
                if (paren_depth == 0) {
                    flush_text(start);
                    Node* link = create_node(NodeType::Link);
                    link->text = std::string(text.substr(url_start, pos - url_start));
                    // 递归解析 link 文本
                    std::string_view link_content = text.substr(link_text_start, link_text_end - link_text_start);
                    parse_inlines(link_content, link);
                    nodes.push_back(link);
                    pos++;
                    text_start = pos;
                    continue;
                }
            }
            pos = start + 1;
        }
        
        pos++;
        next_char:;
    }
    
    flush_text(text.size());
    process_emphasis(nodes, delims);
    
    for (Node* n : nodes) {
        if (n) parent->children.push_back(n);
    }
}

void InlineParser::process_emphasis(std::vector<Node*>& nodes, std::vector<DelimiterInfo>& delims) {
    // CommonMark emphasis 算法 (简化版)
    // 从后向前查找 closer，然后向前查找匹配的 opener
    
    for (int closer_idx = static_cast<int>(delims.size()) - 1; closer_idx >= 0; closer_idx--) {
        DelimiterInfo& closer = delims[closer_idx];
        if (!closer.can_close || closer.len == 0) continue;
        
        // 向前查找 opener
        for (int opener_idx = closer_idx - 1; opener_idx >= 0; opener_idx--) {
            DelimiterInfo& opener = delims[opener_idx];
            if (!opener.can_open || opener.len == 0) continue;
            if (opener.ch != closer.ch) continue;
            
            // 确定使用的长度 (1 或 2)
            size_t use_len = (opener.len >= 2 && closer.len >= 2) ? 2 : 1;
            if (closer.ch == '~') use_len = 2;  // ~~ 必须成对
            
            if (opener.len < use_len || closer.len < use_len) continue;
            
            // 创建 emphasis 节点
            NodeType type;
            if (closer.ch == '~') type = NodeType::Strikethrough;
            else if (use_len == 2) type = NodeType::Strong;
            else type = NodeType::Emphasis;
            
            Node* emph = create_node(type);
            
            // 收集 opener 和 closer 之间的节点
            for (size_t k = opener.node_idx + 1; k < closer.node_idx; k++) {
                if (nodes[k]) {
                    emph->children.push_back(nodes[k]);
                    nodes[k] = nullptr;
                }
            }
            
            // 更新 opener 节点
            Node* opener_node = nodes[opener.node_idx];
            if (opener.len == use_len) {
                nodes[opener.node_idx] = emph;
                opener.len = 0;
            } else {
                opener_node->text = opener_node->text.substr(0, opener.len - use_len);
                opener.len -= use_len;
                // 在 opener 后插入 emph
                nodes.insert(nodes.begin() + opener.node_idx + 1, emph);
                // 调整所有后续 delimiter 的 node_idx
                for (auto& d : delims) {
                    if (d.node_idx > opener.node_idx) d.node_idx++;
                }
                closer.node_idx++;
            }
            
            // 更新 closer 节点
            Node* closer_node = nodes[closer.node_idx];
            if (closer.len == use_len) {
                nodes[closer.node_idx] = nullptr;
                closer.len = 0;
            } else {
                closer_node->text = closer_node->text.substr(use_len);
                closer.len -= use_len;
            }
            
            break;  // 处理下一个 closer
        }
    }
    
    // 移除 nullptr
    nodes.erase(std::remove(nodes.begin(), nodes.end(), nullptr), nodes.end());
}

Node* blocks_to_ast(const Block& root, MemoryPool* pool) {
    InlineParser parser(pool);
    return parser.transform(root);
}

} // namespace md
