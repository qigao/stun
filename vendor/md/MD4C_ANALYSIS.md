# MD4C Architecture Analysis

## 🏗️ **Overall Architecture**

MD4C uses a **hand-written recursive descent parser** with a sophisticated **two-phase approach**:

### **Phase 1: Block Analysis**
- Scans the document line-by-line
- Identifies block-level structures (headers, lists, tables, code blocks)
- Builds a hierarchy of blocks and lines

### **Phase 2: Inline Analysis**
- Processes text within each block
- Resolves inline elements (emphasis, links, images)
- Uses a **mark-based system** with opener/closer stacks

---

## 🔑 **Key Design Decisions**

### 1. **No Lexer/Parser Generator**
- **No re2c, no Lemon, no Yacc**
- Everything is hand-written C code
- **Why?** Maximum flexibility for context-sensitive rules

### 2. **Character-Based Processing**
- Direct character access via `CH(offset)` macro
- No tokenization step
- Processes the input string directly with offset tracking

### 3. **Mark-Based Inline Parsing**
```c
typedef struct MD_MARK_tag {
    OFF beg;          // Start offset
    OFF end;          // End offset
    unsigned char prev;  // Previous mark index
    unsigned char next;  // Next mark index (for stacks)
    unsigned char ch;    // The character (*, _, ~, etc.)
    unsigned char flags; // OPENER, CLOSER, etc.
} MD_MARK;
```

**How it works:**
1. **Collect marks**: Scan text and record all potential emphasis markers
2. **Build stacks**: Organize marks into opener/closer stacks
3. **Resolve pairs**: Match openers with closers using stack-based algorithm
4. **Process**: Convert matched pairs into spans

### 4. **Separate Stacks for Different Emphasis Types**
```c
#define ASTERISK_OPENERS_oo_mod3_0   // Opener-only, length % 3 == 0
#define ASTERISK_OPENERS_oo_mod3_1   // Opener-only, length % 3 == 1
#define ASTERISK_OPENERS_oo_mod3_2   // Opener-only, length % 3 == 2
#define ASTERISK_OPENERS_oc_mod3_0   // Opener/closer, length % 3 == 0
// ... and so on for UNDERSCORE, TILDE, BRACKET, DOLLAR
```

**Why mod 3?** To handle the CommonMark rule about emphasis nesting:
- `*` = 1 asterisk → italic
- `**` = 2 asterisks → bold
- `***` = 3 asterisks → bold+italic
- Length modulo 3 determines compatibility

---

## 📊 **Data Structures**

### **MD_CTX (Context)**
The main context object that holds:
- Input text and size
- Parser callbacks
- Mark arrays (for inline parsing)
- Opener stacks (16 different stacks!)
- Block bytes (for block structure)
- Container stack (for nested blocks)
- Reference definitions (for links)

### **MD_LINE_ANALYSIS**
```c
typedef struct MD_LINE_ANALYSIS_tag {
    MD_LINETYPE type;     // BLANK, HR, ATXHEADER, TEXT, TABLE, etc.
    unsigned data;        // Type-specific data
    int enforce_new_block;
    OFF beg;              // Line start offset
    OFF end;              // Line end offset
    unsigned indent;      // Indentation level
} MD_LINE_ANALYSIS;
```

### **MD_BLOCK**
Represents block-level structures (paragraphs, lists, code blocks, etc.)

---

## 🔄 **Parsing Flow**

### **Main Entry Point: `md_parse()`**
```
1. md_analyze_doc()
   ├─ md_analyze_line() for each line
   │  └─ Determine line type (header, list, table, text, etc.)
   │
   ├─ md_process_blocks()
   │  ├─ Build block hierarchy
   │  ├─ Handle containers (lists, quotes)
   │  └─ For each block:
   │     └─ md_process_normal_block_contents()
   │        └─ md_analyze_inlines()  ← INLINE PARSING
   │           ├─ md_collect_marks()
   │           ├─ md_analyze_marks()
   │           └─ md_resolve_links()
   │
   └─ Fire callbacks (enter_block, leave_block, enter_span, leave_span, text)
```

### **Inline Parsing Detail**
```
md_analyze_inlines():
  1. Reset mark stack
  2. md_collect_marks() - Scan and collect all potential markers
  3. md_analyze_marks() for links: "[]!"
  4. md_resolve_links()
  5. md_analyze_marks() for emphasis: "*_~$"
  6. md_analyze_marks() for autolinks: "@:."
  7. Resolve all opener/closer pairs
```

---

## 🎯 **How Emphasis Parsing Works**

### **Step 1: Collect Marks**
Scan the text and create MD_MARK entries for every `*`, `_`, `~`, etc.

### **Step 2: Classify Marks**
Each mark is classified as:
- **OPENER**: Can open an emphasis span
- **CLOSER**: Can close an emphasis span
- **BOTH**: Can be either (context-dependent)

Classification rules:
- Check surrounding whitespace
- Check surrounding punctuation
- Apply CommonMark rules

### **Step 3: Build Stacks**
Push openers onto appropriate stacks based on:
- Character type (`*` vs `_`)
- Length (1, 2, 3+)
- Length mod 3 (for nesting rules)

### **Step 4: Resolve Pairs**
For each potential closer:
1. Find matching opener from appropriate stack
2. Check compatibility (length, nesting rules)
3. Mark the pair as resolved
4. Remove from stack

### **Step 5: Process Spans**
Walk through resolved marks and fire callbacks:
- `enter_span(EM)` for `*...*`
- `enter_span(STRONG)` for `**...**`
- `text()` for content between spans

---

## 🚀 **Why MD4C is Fast**

1. **Single-pass for blocks**: Each line analyzed once
2. **Direct character access**: No string copying
3. **Efficient mark resolution**: Stack-based O(n) algorithm
4. **Minimal allocations**: Reuses buffers between blocks
5. **No backtracking**: Greedy left-to-right processing

---

## 🎓 **Key Lessons for md-re2c**

### **What We Can Learn:**

1. **Two-Phase Approach is Essential**
   - Block parsing and inline parsing are fundamentally different
   - Don't try to handle both with the same grammar

2. **Mark-Based Inline Parsing**
   - Collect all potential markers first
   - Resolve them with a separate algorithm
   - Much more flexible than grammar-based approach

3. **Context Matters**
   - Emphasis markers need surrounding context (whitespace, punctuation)
   - Can't be determined by lexer alone

4. **Stack-Based Resolution**
   - Multiple stacks for different marker types
   - Enables proper nesting and precedence

5. **Offset-Based Processing**
   - Work with offsets into original string
   - Avoid string copying and tokenization overhead

---

## 💡 **Recommended Approach for md-re2c**

### **Option A: Hybrid (Recommended)**
Keep what works, fix what doesn't:

1. **Keep current block parser** (85%+ success rate)
   - re2c lexer for block-level tokens
   - Lemon parser for block structure
   - This is working well!

2. **Add mark-based inline processor**
   - After parsing blocks, process TEXT nodes
   - Implement md4c-style mark collection and resolution
   - Replace TEXT nodes with formatted spans

### **Option B: Full Rewrite**
Implement md4c-style parser from scratch:
- Hand-written recursive descent
- Mark-based inline parsing
- More work, but maximum flexibility

### **Option C: Simplified Mark System**
Implement a simpler version of md4c's approach:
- Only handle `*`, `**`, `_`, `__` (no complex nesting)
- Single pass through TEXT nodes
- Regex-based or simple state machine

---

## 📈 **Complexity Comparison**

| Approach | LOC | Complexity | Flexibility | Performance |
|----------|-----|------------|-------------|-------------|
| **md-re2c (current)** | ~500 | Low | Low | Fast |
| **md4c (full)** | ~6500 | Very High | Very High | Very Fast |
| **Hybrid (recommended)** | ~1000 | Medium | High | Fast |

---

## 🎯 **Next Steps**

1. **Implement mark-based inline processor** for md-re2c
2. **Keep existing block parser** (it's working!)
3. **Add post-processing** for TEXT nodes
4. **Test against CommonMark spec**

This gives us the best of both worlds:
- ✅ Simple, maintainable block parsing (re2c + Lemon)
- ✅ Flexible, correct inline parsing (mark-based)
- ✅ Good performance
- ✅ High spec compliance
