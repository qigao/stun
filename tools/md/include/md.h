#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stack>
#include "memory_pool.h"

namespace md {

enum class NodeType {
    Document,
    Paragraph,
    Header,
    List,
    OrderedList,
    ListItem,
    Text,
    Emphasis,
    Strong,
    Link,
    Image,
    CodeBlock,
    CodeSpan,
    Table,
    TableRow,
    TableCell,
    Strikethrough,
    Underline,
    HorizontalRule,
    HtmlEntity,
    LineBreak,
    TaskItem,
    Checkbox,
    BlockQuote,
    Math,           // 行内数学 $...$
    MathBlock,      // 块级数学 $$...$$
    Highlight,      // ==高亮==
    Superscript,    // ^上标^
    Subscript,      // ~下标~
    Footnote,       // [^1] 脚注引用
    FootnoteDef,    // [^1]: 脚注定义
    DefList,        // 定义列表
    DefTerm,        // 定义术语
    DefDesc,        // 定义描述
    Diagram         // 可渲染的图表节点 (mermaid/flexchart/infographic)
};

struct Node {
    NodeType type;
    std::string text;
    std::string language;          // For CodeBlock: "cpp", "js", etc.
    std::vector<Node*> children;
    int level = 0;                 // For headers
    void* diagram_data = nullptr;  // For Diagram nodes

    Node(NodeType t) : type(t) {}
};

struct ParseContext {
    MemoryPool* pool;
    Node* root = nullptr;

    ParseContext(size_t pool_size = 2 * 1024 * 1024) 
        : pool(pool_create(pool_size)) {}

    ~ParseContext() {
        if (pool) pool_destroy(pool);
    }

    Node* create_node(NodeType t) {
        void* mem = pool_alloc(pool, sizeof(Node));
        if (!mem) return nullptr;
        return new (mem) Node(t);
    }
};

struct ParseResult {
    std::unique_ptr<ParseContext> ctx;
    Node* root;
};

// 主入口
ParseResult parse(const std::string& input);

// 扩展处理 (图表等)
void process_blocks(Node* node, MemoryPool* pool);

} // namespace md
