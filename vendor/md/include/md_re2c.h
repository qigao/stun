#pragma once

#include <string>
#include <vector>
#include <memory>
#include <stack>
#include "memory_pool.h"

namespace md_re2c {

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
    Checkbox
};

struct Node {
    NodeType type;
    std::string text;
    std::vector<Node*> children;
    int level = 0; // For headers

    Node(NodeType t) : type(t) {}
};

struct ParseContext {
    MemoryPool* pool;
    Node* root;
    std::stack<Node*> node_stack;

    ParseContext(size_t pool_size = 2 * 1024 * 1024) 
        : pool(pool_create(pool_size)) 
    {
        root = create_node(NodeType::Document);
        node_stack.push(root);
    }

    ~ParseContext() {
        if (pool) pool_destroy(pool);
    }

    // Helper to create a node using the pool
    Node* create_node(NodeType t) {
        void* mem = pool_alloc(pool, sizeof(Node));
        if (!mem) return nullptr;
        return new (mem) Node(t);
    }
};

struct LexerState {
    const char* start;
    const char* cursor;
    const char* marker;
    int last_token = 0;  // Track the last token returned
};

struct ParseResult {
    std::unique_ptr<ParseContext> ctx;
    Node* root;
};

ParseResult parse(const std::string& input);

// Block processor
void process_blocks(Node* node, MemoryPool* pool);

// Inline emphasis processor
void process_inline_emphasis(Node* node, MemoryPool* pool);

} // namespace md_re2c
