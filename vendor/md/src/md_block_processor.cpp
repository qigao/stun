#include "md_re2c.h"
#include "md_extension.h"
#include <vector>
#include <string>
#include <algorithm>

namespace md_re2c {

static std::string get_raw_text(Node* n) {
    if (!n) return "";
    if (n->type == NodeType::Text || n->type == NodeType::HtmlEntity) return n->text;
    std::string res;
    for (auto child : n->children) res += get_raw_text(child);
    return res;
}

static bool is_table_delimiter(const std::string& line) {
    if (line.empty()) return false;
    bool has_dash = false;
    bool has_pipe = false;
    // GFM: A delimiter row consists of | and - and :
    // Must contain at least one dash and one pipe
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i == line.size()) return false;
    
    for (; i < line.size(); ++i) {
        char c = line[i];
        if (c == '-') has_dash = true;
        else if (c == '|') has_pipe = true;
        else if (c == ':' || c == ' ' || c == '\t') continue;
        else return false;
    }
    return has_dash && has_pipe;
}

static std::vector<std::string> split_cells(const std::string& line) {
    std::vector<std::string> cells;
    std::string current;
    bool escaped = false;
    
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) start++;
    size_t end = line.size();
    while (end > start && (line[end-1] == ' ' || line[end-1] == '\t' || line[end-1] == '\r')) end--;

    if (start < end && line[start] == '|') start++;
    if (end > start && line[end-1] == '|') end--;
    
    bool in_code = false;
    for (size_t i = start; i < end; ++i) {
        if (line[i] == '`' && !escaped) {
            in_code = !in_code;
            current += line[i];
        } else if (line[i] == '\\' && !escaped) {
            escaped = true;
            // Keep backslash if it's protecting a pipe
            if (i + 1 < end && line[i+1] == '|') {
                current += '\\';
            } else {
                current += line[i];
            }
        } else if (line[i] == '|' && !escaped && !in_code) {
            cells.push_back(current);
            current.clear();
        } else {
            current += line[i];
            escaped = false;
        }
    }
    cells.push_back(current);
    
    // Trim whitespace from cells
    for (auto& cell : cells) {
        size_t s = 0;
        while (s < cell.size() && isspace(static_cast<unsigned char>(cell[s]))) s++;
        size_t e = cell.size();
        while (e > s && isspace(static_cast<unsigned char>(cell[e-1]))) e--;
        cell = cell.substr(s, e - s);
    }
    
    return cells;
}

void process_blocks(Node* node, MemoryPool* pool) {
    if (!node) return;
    
    // 1. Recurse first (depth-first)
    for (auto child : node->children) {
        process_blocks(child, pool);
    }
    
    // 2. Process CodeBlocks with registered handlers
    for (auto& child : node->children) {
        if (child->type == NodeType::CodeBlock && !child->text.empty()) {
            auto result = ExtensionRegistry::instance().process(child->text, get_raw_text(child));
            if (result) {
                // 转换为 Diagram 节点
                child->type = NodeType::Diagram;
                child->children.clear();
                
                // 存储渲染结果
                auto* svg_copy = new std::string(std::move(result->content));
                child->diagram_data = svg_copy;
            }
        }
    }
    
    // 3. Identify tables and merge paragraphs in container nodes
    if (node->type == NodeType::Document || node->type == NodeType::ListItem) {
        std::vector<Node*> new_children;
        
        for (size_t i = 0; i < node->children.size(); ++i) {
            auto current = node->children[i];
            
            // Candidate for table start: a paragraph that's followed by a delimiter
            if (current->type == NodeType::Paragraph && i + 1 < node->children.size()) {
                auto next = node->children[i+1];
                if (next->type == NodeType::Paragraph) {
                    std::string next_text = get_raw_text(next);
                    if (is_table_delimiter(next_text)) {
                        void* t_mem = pool_alloc(pool, sizeof(Node));
                        auto table = new (t_mem) Node(NodeType::Table);
                        
                        // Header
                        void* h_mem = pool_alloc(pool, sizeof(Node));
                        auto header = new (h_mem) Node(NodeType::TableRow);
                        auto head_cells = split_cells(get_raw_text(current));
                        for (const auto& cell_text : head_cells) {
                            void* c_mem = pool_alloc(pool, sizeof(Node));
                            auto cell = new (c_mem) Node(NodeType::TableCell);
                            void* txt_mem = pool_alloc(pool, sizeof(Node));
                            auto txt = new (txt_mem) Node(NodeType::Text);
                            txt->text = cell_text;
                            cell->children.push_back(txt);
                            header->children.push_back(cell);
                        }
                        table->children.push_back(header);
                        
                        i++; // Skip delimiter
                        
                        // Data rows
                        while (i + 1 < node->children.size()) {
                            auto row_node = node->children[i+1];
                            if (row_node->type != NodeType::Paragraph) break;
                            std::string row_text = get_raw_text(row_node);
                            if (row_text.find('|') == std::string::npos) break;
                            
                            void* r_mem = pool_alloc(pool, sizeof(Node));
                            auto row = new (r_mem) Node(NodeType::TableRow);
                            auto row_cells = split_cells(row_text);
                            for (const auto& cell_text : row_cells) {
                                void* rc_mem = pool_alloc(pool, sizeof(Node));
                                auto cell = new (rc_mem) Node(NodeType::TableCell);
                                void* rtxt_mem = pool_alloc(pool, sizeof(Node));
                                auto txt = new (rtxt_mem) Node(NodeType::Text);
                                txt->text = cell_text;
                                cell->children.push_back(txt);
                                row->children.push_back(cell);
                            }
                            table->children.push_back(row);
                            i++;
                        }
                        new_children.push_back(table);
                        continue;
                    }
                }
            }
            
            // Paragraph merging: if this is a paragraph and the previous was a paragraph, merge.
            if (current->type == NodeType::Paragraph && !new_children.empty() && new_children.back()->type == NodeType::Paragraph) {
                void* sp_mem = pool_alloc(pool, sizeof(Node));
                auto space = new (sp_mem) Node(NodeType::Text);
                space->text = " ";
                new_children.back()->children.push_back(space);
                for (auto p_child : current->children) {
                    new_children.back()->children.push_back(p_child);
                }
            } else {
                new_children.push_back(current);
            }
        }
        node->children = std::move(new_children);
    }
}

} // namespace md_re2c
