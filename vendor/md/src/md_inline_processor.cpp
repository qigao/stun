#include "md_re2c.h"
#include <vector>
#include <string>
#include <algorithm>

namespace md_re2c {

// Mark represents a potential emphasis or link delimiter
struct Mark {
    size_t pos;           // Position in text
    size_t len;           // Length of delimiter
    char ch;              // Character ('*', '_', '~', '[', '!', ']')
    bool can_open;        // Can open emphasis/link
    bool can_close;       // Can close emphasis/link
    int matched_with;     // Index of matching mark (-1 if unmatched)
    std::string link_url; // For links and images
    
    Mark(size_t p, size_t l, char c) 
        : pos(p), len(l), ch(c), can_open(false), can_close(false), matched_with(-1) {}
};

// Check if character is whitespace
static bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Check if character is punctuation
static bool is_punctuation(char c) {
    return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) || 
           (c >= 91 && c <= 96) || (c >= 123 && c <= 126);
}

// Determine if a mark can open/close emphasis
static void classify_mark(Mark& mark, const std::string& text) {
    if (mark.ch == '[' || mark.ch == '!' || mark.ch == ']') {
        return; // Brackets are handled by their presence
    }
    
    char before = (mark.pos > 0) ? text[mark.pos - 1] : ' ';
    char after = (mark.pos + mark.len < text.size()) ? text[mark.pos + mark.len] : ' ';
    
    bool before_whitespace = is_whitespace(before);
    bool after_whitespace = is_whitespace(after);
    bool before_punct = is_punctuation(before);
    bool after_punct = is_punctuation(after);
    
    bool left_flanking = !after_whitespace && 
                        (!after_punct || before_whitespace || before_punct);
    bool right_flanking = !before_whitespace && 
                         (!before_punct || after_whitespace || after_punct);
    
    if (mark.ch == '*') {
        mark.can_open = left_flanking;
        mark.can_close = right_flanking;
    } else if (mark.ch == '_') {
        mark.can_open = left_flanking && !isalnum(static_cast<unsigned char>(before));
        mark.can_close = right_flanking && !isalnum(static_cast<unsigned char>(after));
    } else if (mark.ch == '~') {
        mark.can_open = left_flanking;
        mark.can_close = right_flanking;
    }
}

// Collect all marks from text
static std::vector<Mark> collect_marks(const std::string& text) {
    std::vector<Mark> marks;
    
    for (size_t i = 0; i < text.size(); ) {
        char ch = text[i];
        
        if (ch == '*' || ch == '_' || ch == '~') {
            size_t start = i;
            while (i < text.size() && text[i] == ch) i++;
            size_t len = i - start;
            
            if (ch == '~' && len >= 2) {
                marks.emplace_back(start, 2, ch);
                classify_mark(marks.back(), text);
            } else if (ch == '*' || ch == '_') {
                while (len >= 2) {
                    marks.emplace_back(start, 2, ch);
                    classify_mark(marks.back(), text);
                    start += 2; len -= 2;
                }
                if (len == 1) {
                    marks.emplace_back(start, 1, ch);
                    classify_mark(marks.back(), text);
                }
            }
        } else if (ch == '[') {
            marks.emplace_back(i, 1, '[');
            marks.back().can_open = true;
            i++;
        } else if (ch == '!' && i + 1 < text.size() && text[i+1] == '[') {
            marks.emplace_back(i, 2, '!');
            marks.back().can_open = true;
            i += 2;
        } else if (ch == ']') {
            size_t j = i + 1;
            if (j < text.size() && text[j] == '(') {
                size_t k = text.find(')', j);
                if (k != std::string::npos) {
                    marks.emplace_back(i, (k + 1) - i, ']');
                    marks.back().can_close = true;
                    marks.back().link_url = text.substr(j + 1, k - j - 1);
                    i = k + 1;
                    continue;
                }
            }
            i++;
        } else {
            i++;
        }
    }
    return marks;
}

// Match openers with closers
static void resolve_marks(std::vector<Mark>& marks) {
    // Phase 1: Match Links and Images
    for (size_t closer_idx = 0; closer_idx < marks.size(); closer_idx++) {
        Mark& closer = marks[closer_idx];
        if (closer.ch != ']' || closer.matched_with >= 0) continue;
        
        for (int opener_idx = (int)closer_idx - 1; opener_idx >= 0; opener_idx--) {
            Mark& opener = marks[opener_idx];
            if ((opener.ch == '[' || opener.ch == '!') && opener.matched_with < 0) {
                opener.matched_with = (int)closer_idx;
                closer.matched_with = opener_idx;
                break;
            }
        }
    }
    
    // Phase 2: Match Emphasis
    for (size_t closer_idx = 0; closer_idx < marks.size(); closer_idx++) {
        Mark& closer = marks[closer_idx];
        if (!closer.can_close || closer.matched_with >= 0) continue;
        
        for (int opener_idx = (int)closer_idx - 1; opener_idx >= 0; opener_idx--) {
            Mark& opener = marks[opener_idx];
            if (!opener.can_open || opener.matched_with >= 0) continue;
            if (opener.ch != closer.ch || opener.len != closer.len) continue;
            
            opener.matched_with = (int)closer_idx;
            closer.matched_with = opener_idx;
            break;
        }
    }
}

// Build AST from text with resolved marks
static Node* build_inline_tree(const std::string& text, const std::vector<Mark>& marks, MemoryPool* pool) {
    void* r_mem = pool_alloc(pool, sizeof(Node));
    auto root = new (r_mem) Node(NodeType::Text);
    size_t pos = 0;
    
    for (size_t i = 0; i < marks.size(); i++) {
        const Mark& mark = marks[i];
        if (mark.matched_with >= 0 && mark.matched_with < (int)i) continue;
        
        if (pos < mark.pos) {
            void* t_mem = pool_alloc(pool, sizeof(Node));
            auto text_node = new (t_mem) Node(NodeType::Text);
            text_node->text = text.substr(pos, mark.pos - pos);
            root->children.push_back(text_node);
        }
        
        if (mark.matched_with >= 0 && mark.matched_with > (int)i) {
            const Mark& closer = marks[mark.matched_with];
            NodeType type;
            if (mark.ch == '[') type = NodeType::Link;
            else if (mark.ch == '!') type = NodeType::Image;
            else if (mark.ch == '~' && mark.len == 2) type = NodeType::Strikethrough;
            else if (mark.len == 2) type = NodeType::Strong;
            else type = NodeType::Emphasis;
            
            void* n_mem = pool_alloc(pool, sizeof(Node));
            auto node = new (n_mem) Node(type);
            if (type == NodeType::Link || type == NodeType::Image) {
                node->text = closer.link_url;
            }
            
            size_t content_start = mark.pos + mark.len;
            size_t content_end = closer.pos;
            if (content_start < content_end) {
                void* c_mem = pool_alloc(pool, sizeof(Node));
                auto content_node = new (c_mem) Node(NodeType::Text);
                content_node->text = text.substr(content_start, content_end - content_start);
                node->children.push_back(content_node);
            }
            root->children.push_back(node);
            pos = closer.pos + closer.len;
        } else {
            void* t_mem = pool_alloc(pool, sizeof(Node));
            auto text_node = new (t_mem) Node(NodeType::Text);
            text_node->text = text.substr(mark.pos, mark.len);
            root->children.push_back(text_node);
            pos = mark.pos + mark.len;
        }
    }
    
    if (pos < text.size()) {
        void* t_mem = pool_alloc(pool, sizeof(Node));
        auto text_node = new (t_mem) Node(NodeType::Text);
        text_node->text = text.substr(pos);
        root->children.push_back(text_node);
    }
    return root;
}

// Merge adjacent Text/HtmlEntity nodes into single Text nodes
static void merge_text_nodes(Node* node) {
    if (!node || node->children.empty()) return;

    std::vector<Node*> merged;
    for (auto child : node->children) {
        if (!merged.empty() && 
            (merged.back()->type == NodeType::Text || merged.back()->type == NodeType::HtmlEntity) &&
            (child->type == NodeType::Text || child->type == NodeType::HtmlEntity)) {
            merged.back()->type = NodeType::Text;
            merged.back()->text += child->text;
        } else {
            merge_text_nodes(child);
            merged.push_back(child);
        }
    }
    node->children = std::move(merged);
}

// Main entry point
void process_inline_emphasis(Node* node, MemoryPool* pool) {
    if (!node) return;
    
    // First pass: Merge adjacent text nodes at this level
    merge_text_nodes(node);
    
    // Tasklist detection for ListItems
    if (node->type == NodeType::ListItem) {
        if (!node->children.empty() && node->children[0]->type == NodeType::Text) {
            std::string& t = node->children[0]->text;
            // Check for [ ] , [x] , [X] 
            if (t.size() >= 4 && t[0] == '[' && (t[1] == ' ' || t[1] == 'x' || t[1] == 'X') && t[2] == ']' && t[3] == ' ') {
                bool checked = (t[1] != ' ');
                node->type = NodeType::TaskItem;
                
                void* cb_mem = pool_alloc(pool, sizeof(Node));
                auto checkbox = new (cb_mem) Node(NodeType::Checkbox);
                checkbox->text = checked ? "x" : " ";
                
                // Keep the rest of the text
                t = t.substr(4);
                
                // Insert checkbox at the beginning
                node->children.insert(node->children.begin(), checkbox);
            }
        }
    }
    
    // Second pass: Process emphasis and flatten results
    std::vector<Node*> new_children;
    for (auto child : node->children) {
        if (child->type == NodeType::Text && !child->text.empty()) {
            auto marks = collect_marks(child->text);
            if (!marks.empty()) {
                resolve_marks(marks);
                auto new_tree = build_inline_tree(child->text, marks, pool);
                for (auto split_child : new_tree->children) {
                    new_children.push_back(split_child);
                }
            } else {
                new_children.push_back(child);
            }
        } else {
            process_inline_emphasis(child, pool);
            new_children.push_back(child);
        }
    }
    node->children = std::move(new_children);
}

} // namespace md_re2c
