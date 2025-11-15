# Enhanced SVG Shape Features

## Overview
Leverage SVG capabilities to create a powerful, flexible shape system that's easy to group, manage, edit, and combine.

## 1. Smart Grouping & Hierarchy

### Nested Groups
```cpp
struct SVGGroup {
    std::string id;
    std::string name;
    std::vector<int> child_indices;  // Can contain shapes or other groups
    int parent_group_id = -1;
    bool collapsed = false;  // For layer panel
    nanogui::Color group_color;  // Visual indicator
};
```

**Features:**
- Drag shapes into groups in layer panel
- Collapse/expand groups
- Color-coded groups
- Multi-level nesting (groups within groups)
- Group transformations affect all children

### Smart Selection
- **Ctrl+Click**: Add to selection
- **Shift+Click**: Select range
- **Double-click group**: Select all children
- **Alt+Click**: Select through (ignore groups)

## 2. SVG Component Library

### Reusable Components
```cpp
struct SVGComponent {
    std::string id;
    std::string name;
    std::string svg_template;
    std::map<std::string, Parameter> parameters;
    std::vector<std::string> tags;  // For searching
    std::string category;
    Thumbnail preview;
};
```

**Features:**
- Save any shape/group as reusable component
- Component library panel
- Drag-and-drop from library
- Parameter inheritance
- Version control for components

### Component Instances
- Link to master component
- Override specific parameters
- Update all instances when master changes
- Break link to make independent

## 3. Advanced Editing Features

### Multi-Parameter Editing
```cpp
// Edit multiple shapes at once
void batch_edit_parameters(
    std::vector<int> shape_indices,
    std::string parameter_name,
    std::string new_value
);
```

**Features:**
- Select multiple SVG shapes
- Edit common parameters together
- Preview changes in real-time
- Undo/redo for batch operations

### Parameter Binding
```cpp
struct ParameterBinding {
    int source_shape;
    std::string source_param;
    int target_shape;
    std::string target_param;
    std::function<std::string(std::string)> transform;  // Optional
};
```

**Features:**
- Link parameters between shapes
- When one changes, others update automatically
- Example: Link all text colors in a diagram
- Transform functions (e.g., uppercase, add prefix)

### Smart Connectors
```cpp
struct Connector {
    int from_shape;
    std::string from_anchor;  // "top", "bottom", "left", "right", "center"
    int to_shape;
    std::string to_anchor;
    ConnectorStyle style;  // straight, curved, orthogonal
    bool auto_route = true;  // Avoid other shapes
};
```

**Features:**
- Attach connectors to shapes
- Connectors follow shapes when moved
- Auto-routing around obstacles
- Multiple anchor points per shape
- Curved, straight, or orthogonal lines

## 4. Template System

### Shape Templates
```json
{
  "id": "uml_class",
  "name": "UML Class",
  "base_shape": "rectangle",
  "sections": [
    {"name": "className", "type": "text", "style": "bold"},
    {"name": "attributes", "type": "list", "separator": "\n"},
    {"name": "methods", "type": "list", "separator": "\n"}
  ],
  "layout": "vertical",
  "auto_resize": true
}
```

**Features:**
- Define custom shape templates
- Structured data (not just text)
- Auto-layout sections
- Type-specific editors (text, list, table)
- Validation rules

### Layout Templates
```cpp
enum class LayoutType {
    Vertical,
    Horizontal,
    Grid,
    Tree,
    Radial,
    Force_Directed
};

struct LayoutTemplate {
    LayoutType type;
    float spacing;
    Alignment alignment;
    bool auto_arrange = true;
};
```

**Features:**
- Apply layouts to groups
- Auto-arrange shapes
- Maintain layout on changes
- Snap to layout grid

## 5. Data-Driven Shapes

### Data Binding
```cpp
struct DataSource {
    std::string id;
    std::map<std::string, std::string> data;
    std::vector<int> bound_shapes;
};

// Bind shape parameters to data
void bind_to_data(int shape_index, std::string data_source_id);
```

**Features:**
- Import data from JSON/CSV
- Bind shape parameters to data fields
- Generate multiple shapes from data
- Update shapes when data changes

### Dynamic Content
```cpp
// Example: Generate org chart from data
void generate_from_data(
    std::string template_id,
    DataSource data,
    LayoutTemplate layout
);
```

**Features:**
- Auto-generate diagrams from data
- Update diagrams when data changes
- Export data from diagrams
- Two-way data binding

## 6. Combination & Merging

### Boolean Operations
```cpp
enum class BooleanOp {
    Union,      // Combine shapes
    Subtract,   // Cut out
    Intersect,  // Keep overlap only
    Exclude     // Remove overlap
};

Stroke combine_shapes(
    std::vector<int> shape_indices,
    BooleanOp operation
);
```

**Features:**
- Combine multiple SVG shapes
- Non-destructive operations
- Preview before applying
- Maintain editability

### Path Operations
```cpp
// Simplify complex paths
void simplify_path(int shape_index, float tolerance);

// Convert shapes to paths
void convert_to_path(int shape_index);

// Merge paths
void merge_paths(std::vector<int> shape_indices);
```

## 7. Style Management

### Style Presets
```cpp
struct StylePreset {
    std::string name;
    nanogui::Color stroke_color;
    nanogui::Color fill_color;
    float stroke_width;
    std::string font_family;
    float font_size;
    // ... other style properties
};
```

**Features:**
- Save/load style presets
- Apply preset to multiple shapes
- Style library
- Import/export styles

### Theme System
```cpp
struct Theme {
    std::string name;
    std::map<std::string, StylePreset> styles;
    std::map<std::string, nanogui::Color> colors;
};
```

**Features:**
- Define color themes
- Switch themes for entire document
- Dark/light mode support
- Custom theme creation

## 8. Smart Alignment & Distribution

### Advanced Alignment
```cpp
enum class AlignMode {
    Left, Right, Top, Bottom, Center_H, Center_V,
    Distribute_H, Distribute_V,
    Match_Width, Match_Height, Match_Size,
    Align_To_Grid, Align_To_Guide
};
```

**Features:**
- Align to selection bounds
- Align to canvas
- Align to grid
- Distribute evenly
- Match sizes

### Smart Guides
```cpp
struct SmartGuide {
    GuideType type;  // alignment, spacing, size
    float position;
    std::vector<int> affected_shapes;
    bool temporary = true;  // Show only while dragging
};
```

**Features:**
- Show alignment guides while dragging
- Snap to other shapes
- Show spacing measurements
- Highlight aligned edges

## 9. Version Control & History

### Shape History
```cpp
struct ShapeVersion {
    int shape_index;
    Stroke snapshot;
    std::string timestamp;
    std::string description;
};
```

**Features:**
- Track shape changes
- Revert to previous version
- Compare versions
- Branch and merge

### Collaborative Features
```cpp
struct ShapeAnnotation {
    int shape_index;
    std::string author;
    std::string comment;
    nanogui::Vector2f position;
    std::string timestamp;
};
```

**Features:**
- Add comments to shapes
- Track who edited what
- Review mode
- Approval workflow

## 10. Export & Integration

### Enhanced Export
```cpp
struct ExportOptions {
    bool include_metadata = true;
    bool embed_fonts = true;
    bool optimize_paths = true;
    bool preserve_editability = true;
    std::vector<std::string> export_layers;
};
```

**Features:**
- Export with metadata
- Preserve editability
- Layer-based export
- Multiple formats (SVG, PDF, PNG)

### Code Generation
```cpp
// Generate code from diagrams
std::string generate_code(
    int shape_index,
    CodeLanguage language,
    CodeTemplate template
);
```

**Features:**
- Generate class code from UML
- Generate HTML from wireframes
- Generate database schema from ER diagrams
- Custom code templates

## Implementation Priority

### Phase 1: Foundation (High Priority)
1. ✅ Basic SVG shape support
2. ✅ Parameter editing
3. 🔄 Smart grouping
4. 🔄 Component library
5. 🔄 Style presets

### Phase 2: Advanced Editing (Medium Priority)
6. Parameter binding
7. Smart connectors
8. Template system
9. Advanced alignment

### Phase 3: Data & Automation (Lower Priority)
10. Data binding
11. Boolean operations
12. Code generation
13. Collaborative features

## Technical Architecture

### Data Structure
```cpp
struct EnhancedStroke : public Stroke {
    // Grouping
    std::vector<int> children;
    int parent_id = -1;
    
    // Components
    std::string component_id;
    bool is_component_instance = false;
    
    // Bindings
    std::vector<ParameterBinding> bindings;
    std::string data_source_id;
    
    // Connectors
    std::vector<Connector> connectors;
    
    // Metadata
    std::map<std::string, std::string> metadata;
    std::vector<ShapeAnnotation> annotations;
    
    // Style
    std::string style_preset_id;
    std::string theme_id;
};
```

### Manager Classes
```cpp
class SVGComponentManager {
    void save_component(const Stroke& shape);
    Stroke load_component(const std::string& id);
    std::vector<SVGComponent> get_library();
};

class SVGGroupManager {
    void create_group(std::vector<int> shape_indices);
    void ungroup(int group_id);
    std::vector<int> get_group_children(int group_id);
};

class SVGConnectorManager {
    void create_connector(int from, int to);
    void update_connectors(int shape_index);
    void auto_route_connector(Connector& conn);
};

class SVGDataManager {
    void bind_data(int shape_index, DataSource data);
    void update_from_data(const std::string& data_source_id);
    void generate_shapes(const std::string& template_id, DataSource data);
};
```

## User Interface Enhancements

### Context Menu Extensions
- "Save as Component"
- "Create Connector"
- "Bind to Data"
- "Apply Style Preset"
- "Group Selection"
- "Align & Distribute"

### New Panels
- **Component Library**: Browse and insert components
- **Data Panel**: Manage data sources and bindings
- **Style Panel**: Manage presets and themes
- **Connector Panel**: Configure connector routing

### Keyboard Shortcuts
- **Ctrl+G**: Group selection
- **Ctrl+Shift+G**: Ungroup
- **Ctrl+L**: Create connector
- **Ctrl+Shift+S**: Save as component
- **Ctrl+B**: Bind to data
- **Ctrl+Shift+A**: Advanced alignment dialog

## Benefits

1. **Productivity**: Reuse components, batch edit, auto-layout
2. **Consistency**: Themes, style presets, linked parameters
3. **Flexibility**: Data-driven, templates, boolean operations
4. **Collaboration**: Annotations, version control, review mode
5. **Integration**: Code generation, data import/export
6. **Professional**: Advanced alignment, smart connectors, polish

## Next Steps

1. Review and prioritize features
2. Design detailed API for each feature
3. Implement Phase 1 features
4. Create user documentation
5. Gather feedback and iterate
