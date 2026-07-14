# Infographic Module - Consolidated Design

## 架构概览

```
┌─────────────────────────────────────────────────────────────────────┐
│                         User Input                                   │
│  infographic list-grid-badge-card                                   │
│  data { title: "..." items: [...] }                                 │
│  theme { palette: #xxx #yyy }                                       │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Compiler (re2c + lemon)                          │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐          │
│  │ Lexer (re2c) │ -> │Parser(lemon) │ -> │     AST      │          │
│  └──────────────┘    └──────────────┘    └──────────────┘          │
└─────────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         AST Structure                                │
│  AstInfographic                                                      │
│  ├── template_type: TemplateType                                    │
│  ├── title: string                                                  │
│  ├── desc: string                                                   │
│  ├── items: vector<AstItem>                                         │
│  │   └── AstItem { label, desc, value, icon, illus, children }     │
│  └── theme: AstTheme { palette, preset, stylize }                  │
└─────────────────────────────────────────────────────────────────────┘
                                │
                    ┌───────────┴───────────┐
                    ▼                       ▼
┌──────────────────────────┐  ┌──────────────────────────┐
│      SVG Renderer        │  │    Flex Runtime          │
│  (Direct SVG output)     │  │  (Interactive UI)        │
└──────────────────────────┘  └──────────────────────────┘
```

## 1. DSL 语法设计

### 1.1 基本语法

```infographic
infographic <template-name>

data {
    title: "Chart Title"
    desc: "Description"
    items: [
        { label: "Item 1", desc: "Desc", value: 100, icon: "mdi/star" }
        { label: "Item 2", desc: "Desc", value: 200 }
    ]
}

theme {
    palette: #3b82f6 #8b5cf6 #f97316
    preset: dark
    stylize: rough
}
```

### 1.2 语法特点

- **花括号块** - 替代 YAML 缩进，更清晰
- **冒号分隔** - `key: value` 格式
- **数组语法** - `[item1, item2]` 或多行 `[ item1 \n item2 ]`
- **颜色字面量** - `#rrggbb` 直接支持
- **嵌套结构** - children 支持递归

## 2. 文件结构

```
infographic/
├── compiler/
│   ├── infographic_lexer.re       # re2c lexer 定义
│   ├── infographic_grammar.y      # lemon grammar 定义
│   ├── infographic_token.h        # token 类型定义
│   ├── infographic_context.h      # parser context
│   ├── infographic_ast.h          # AST 节点定义
│   └── infographic_parser.cpp     # parser 入口
├── include/infographic/
│   ├── infographic.h              # 公共 API
│   ├── ast.h                      # AST 导出
│   └── renderer/
│       └── template_renderer.h    # 渲染器基类
├── src/
│   ├── infographic_component.cpp  # flex 组件
│   ├── flexinfographic_new.cpp    # 对外入口
│   ├── layout/layout_engine.cpp   # 布局引擎（libcola）
│   └── renderer/                  # SVG 渲染器
│       ├── template_renderer.cpp
│       ├── list_renderer.cpp
│       ├── sequence_renderer.cpp
│       ├── compare_renderer.cpp
│       └── chart_renderer.cpp
├── grid/      # 布局模块：Grid
├── timeline/  # 布局模块：Timeline
├── funnel/    # 布局模块：Funnel
├── pie/       # 布局模块：Pie/Donut
├── bar/       # 布局模块：Bar/Column
├── swot/      # 布局模块：SWOT/Compare
├── tree/      # 布局模块：Tree
├── zigzag/    # 布局模块：Zigzag
├── circular/  # 布局模块：Circular
├── roadmap/   # 布局模块：Roadmap
└── CMakeLists.txt
```

## 3. Token 定义

```cpp
// infographic_token.h

// Keywords
TOK_INFOGRAPHIC     // infographic
TOK_DATA            // data
TOK_THEME           // theme
TOK_ITEMS           // items
TOK_CHILDREN        // children

// Data fields
TOK_TITLE           // title
TOK_DESC            // desc
TOK_LABEL           // label
TOK_VALUE           // value
TOK_ICON            // icon
TOK_ILLUS           // illus
TOK_DONE            // done

// Theme fields
TOK_PALETTE         // palette
TOK_PRESET          // preset
TOK_STYLIZE         // stylize

// Literals
TOK_STRING          // "..."
TOK_NUMBER          // 123, 45.6
TOK_BOOL            // true, false
TOK_COLOR           // #rrggbb
TOK_TEMPLATE_NAME   // list-grid-badge-card

// Symbols
TOK_LBRACE          // {
TOK_RBRACE          // }
TOK_LBRACKET        // [
TOK_RBRACKET        // ]
TOK_COLON           // :
TOK_COMMA           // ,
```

## 4. AST 定义

```cpp
// infographic_ast.h

namespace flex::modules::infographic {

using AstValue = std::variant<double, std::string, bool>;

struct AstItem {
    std::string label;
    std::optional<std::string> desc;
    std::optional<double> value;
    std::optional<std::string> icon;
    std::optional<std::string> illus;
    std::optional<bool> done;
    std::vector<std::shared_ptr<AstItem>> children;
    std::map<std::string, AstValue> properties;
};

struct AstTheme {
    std::vector<std::string> palette;
    std::optional<std::string> preset;   // dark, hand-drawn
    std::optional<std::string> stylize;  // rough, pattern
};

struct AstInfographic {
    TemplateType template_type;
    std::optional<std::string> title;
    std::optional<std::string> desc;
    std::vector<std::shared_ptr<AstItem>> items;
    AstTheme theme;
};

} // namespace
```

## 5. 布局引擎设计

### 5.1 核心思想

**模板名 = 布局算法 + 样式配置**

```
list-grid-badge-card
│    │    │     │
│    │    │     └── style: card
│    │    └──────── variant: badge
│    └───────────── layout: grid
└────────────────── category: list
```

### 5.2 布局类型 (~15 种)

| Layout | 模板前缀 | 描述 |
|--------|---------|------|
| GridLayout | list-grid-* | 网格排列 |
| RowLayout | list-row-* | 水平行 |
| ColumnLayout | list-column-* | 垂直列 |
| ZigzagLayout | list-zigzag-*, sequence-zigzag-* | 之字形 |
| TimelineLayout | sequence-timeline-* | 时间线 |
| SnakeLayout | sequence-snake-* | 蛇形 |
| StairsLayout | sequence-stairs-* | 阶梯 |
| FunnelLayout | sequence-funnel-*, sequence-filter-* | 漏斗 |
| PyramidLayout | sequence-pyramid-* | 金字塔 |
| CircularLayout | sequence-circular-*, relation-circle-* | 圆形 |
| RoadmapLayout | sequence-roadmap-* | 路线图 |
| TreeLayout | hierarchy-tree-* | 树形 |
| QuadrantLayout | compare-swot, quadrant-* | 四象限 |
| VsLayout | compare-binary-* | 对比 |
| PieLayout | chart-pie-* | 饼图 |
| BarLayout | chart-bar-*, chart-column-* | 柱状图 |

### 5.3 样式配置

```cpp
struct StyleConfig {
    // Card style
    int card_radius = 8;
    bool show_border = true;
    float opacity = 1.0f;
    
    // Content
    bool show_icon = false;
    bool show_badge = false;
    bool show_illus = false;
    std::string icon_position = "left";  // left, top, center
    
    // Typography
    int title_size = 16;
    int desc_size = 12;
    bool title_bold = true;
};

// 从模板名解析样式
StyleConfig parse_style_from_template(const std::string& template_name);
```

## 6. 渲染流程

```cpp
class InfographicRenderer {
public:
    // SVG 输出
    std::string to_svg(const AstInfographic& ast);
    
    // Flex 运行时输出
    flex::Group* to_flex(const AstInfographic& ast, flex::Instance& instance);
    
private:
    // 1. 解析模板名 -> 获取 Layout + Style
    LayoutType get_layout_type(TemplateType type);
    StyleConfig get_style_config(TemplateType type);
    
    // 2. 创建布局引擎
    std::unique_ptr<LayoutEngine> create_layout(LayoutType type);
    
    // 3. 计算布局
    LayoutResult compute_layout(LayoutEngine& engine, 
                                const AstInfographic& ast,
                                int width, int height);
    
    // 4. 渲染元素
    void render_items(const LayoutResult& layout,
                      const StyleConfig& style,
                      const AstInfographic& ast);
};
```

## 7. 与 Flex 运行时集成

### 7.1 使用 Flex 布局

```cpp
flex::Group* InfographicRenderer::to_flex(const AstInfographic& ast, 
                                          flex::Instance& instance) {
    auto& arena = instance.arena();
    auto* root = flex::Group::create(arena);
    
    // 根据模板类型设置布局
    if (is_grid_layout(ast.template_type)) {
        root->set_layout(flex::LayoutMode::Flex);
        root->set_flex_direction(flex::FlexDirection::Row);
        root->set_flex_wrap(flex::FlexWrap::Wrap);
        root->set_gap(20);
    }
    
    // 创建子元素
    for (const auto& item : ast.items) {
        auto* card = create_card(item, arena);
        root->add_child(card);
    }
    
    return root;
}
```

### 7.2 组件注册

```cpp
void InfographicComponent::register_component() {
    auto comp = flex::Component::create("Infographic");
    
    comp->add_prop("source", std::string(""));
    comp->add_prop("width", 800.0f);
    comp->add_prop("height", 600.0f);
    
    comp->set_builder([](const flex::Props& props) {
        std::string source = flex::get_prop_string(props, "source");
        
        FlexInfographic infographic;
        auto result = infographic.parse(source);
        if (!result.success) return nullptr;
        
        // 返回渲染后的节点树
        return infographic.to_flex(*result.ast);
    });
    
    flex::ComponentRegistry::instance().register_component(comp);
}
```

## 8. 实施计划

### Phase 1: Parser 重构 (Week 1)
- [ ] 创建 `compiler/` 目录结构
- [ ] 实现 `infographic_lexer.re`
- [ ] 实现 `infographic_grammar.y`
- [ ] 实现 `infographic_ast.h`
- [ ] 更新 CMakeLists.txt

### Phase 2: 布局引擎 (Week 2)
- [ ] 实现 `LayoutEngine` 基类
- [ ] 实现 `StyleConfig` 解析
- [ ] 实现 5 个核心布局: Grid, Timeline, Funnel, Tree, Pie

### Phase 3: 渲染器整合 (Week 3)
- [ ] 重构现有渲染器使用新布局引擎
- [ ] 添加 icon/illus 渲染支持
- [ ] 实现 Flex 运行时输出

### Phase 4: 模板覆盖 (Week 4)
- [ ] 实现剩余布局类型
- [ ] 验证所有 SKILL.md 模板
- [ ] 性能优化

## 9. 测试策略

```cpp
// 每个模板类型一个测试用例
TEST(InfographicParser, ListGridBadgeCard) {
    const char* source = R"(
        infographic list-grid-badge-card
        data {
            title: "Test"
            items: [
                { label: "A", value: 100 }
                { label: "B", value: 200 }
            ]
        }
    )";
    
    FlexInfographic infographic;
    auto result = infographic.parse(source);
    
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.ast->template_type, TemplateType::ListGridBadgeCard);
    ASSERT_EQ(result.ast->items.size(), 2);
}
```

---

**设计原则**:
1. **数据驱动** - 模板名决定布局+样式，不是代码分支
2. **单一职责** - Parser 只解析，Layout 只布局，Renderer 只渲染
3. **零特殊情况** - 所有模板走同一条代码路径
4. **可扩展** - 新模板只需配置，不需要新类
