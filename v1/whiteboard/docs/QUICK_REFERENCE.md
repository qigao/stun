# Documentation Quick Reference

Quick links to commonly accessed documentation.

## 🚀 Getting Started

| Document | Description |
|----------|-------------|
| [Main README](README.md) | Documentation overview and structure |
| [Architecture Summary](ARCHITECTURE_SUMMARY.md) | **START HERE** - Understand DDF and SVGShape |
| [Format Relationship](FORMAT_RELATIONSHIP.md) | Visual guide to how formats work together |
| [DDF User Guide](user/DDF_USER_GUIDE.md) | Complete guide to using DDF |
| [SVG Editing Guide](user/SVG_EDITING_GUIDE.md) | How to create and edit SVG shapes |

## 📚 By Role

### For End Users
- [DDF User Guide](user/DDF_USER_GUIDE.md) - Working with diagrams
- [SVG Editing Guide](user/SVG_EDITING_GUIDE.md) - Creating shapes
- [Export Guide](user/EXPORT_GUIDE.md) - Exporting your work
- [SVG Enhanced Features](user/SVG_ENHANCED_FEATURES.md) - Advanced SVG features
- [SVG Shape Interactions](user/SVG_SHAPE_INTERACTIONS.md) - Interactive shapes

### For Developers
- [DDF API Reference](api/DDF_API_REFERENCE.md) - Complete API docs
- [DDF Components](dev/DDF_COMPONENTS.md) - Component system
- [DDF CSS Styling](dev/DDF_CSS_STYLING.md) - Styling system
- [DDF Expressions](dev/DDF_EXPRESSIONS.md) - Expression language
- [SVG Generator Guide](dev/SVG_GENERATOR_GUIDE.md) - SVG generation
- [SVGShape Implementation](dev/SVGSHAPE_IMPLEMENTATION.md) - Shape format

### For API Users
- [DDF API Reference](api/DDF_API_REFERENCE.md) - DDF API
- [SVG Shape Format](api/SVG_SHAPE_FORMAT.md) - Shape file format
- [Format Comparison](api/FORMAT_COMPARISON.md) - Format differences

## 📖 By Topic

### DDF (Diagram Definition Format)
- **User**: [DDF User Guide](user/DDF_USER_GUIDE.md)
- **API**: [DDF API Reference](api/DDF_API_REFERENCE.md)
- **Dev**: [Components](dev/DDF_COMPONENTS.md) | [CSS Styling](dev/DDF_CSS_STYLING.md) | [Expressions](dev/DDF_EXPRESSIONS.md)

### SVG Shapes
- **User**: [SVG Editing Guide](user/SVG_EDITING_GUIDE.md) | [Enhanced Features](user/SVG_ENHANCED_FEATURES.md)
- **API**: [SVG Shape Format](api/SVG_SHAPE_FORMAT.md)
- **Dev**: [Generator Guide](dev/SVG_GENERATOR_GUIDE.md) | [Implementation](dev/SVGSHAPE_IMPLEMENTATION.md) | [Styling Guide](dev/SVGSHAPE_STYLING_GUIDE.md)

### Format Comparison
- **[SVGShape vs DDF](SVGSHAPE_VS_DDF_COMPARISON.md)** - Complete comparison of both formats
- **[Format Comparison](api/FORMAT_COMPARISON.md)** - Quick reference table

### Export & Import
- **User**: [Export Guide](user/EXPORT_GUIDE.md)
- **Archive**: [Import Implementation](archive/ADDING_IMPORT_BUTTON.md)

### Interactive Features
- **User**: [SVG Shape Interactions](user/SVG_SHAPE_INTERACTIONS.md) | [Inline SVG Editing](user/INLINE_SVG_EDITING.md)
- **Dev**: [DDF Expressions](dev/DDF_EXPRESSIONS.md)

## 🔍 Common Tasks

| Task | Document |
|------|----------|
| Create a flowchart | [DDF User Guide](user/DDF_USER_GUIDE.md) |
| Design custom shapes | [SVG Editing Guide](user/SVG_EDITING_GUIDE.md) |
| Export to PNG/SVG | [Export Guide](user/EXPORT_GUIDE.md) |
| Use the API | [DDF API Reference](api/DDF_API_REFERENCE.md) |
| Understand components | [DDF Components](dev/DDF_COMPONENTS.md) |
| Style shapes with CSS | [DDF CSS Styling](dev/DDF_CSS_STYLING.md) |
| Create parametric shapes | [SVGShape Implementation](dev/SVGSHAPE_IMPLEMENTATION.md) |

## 📦 Examples

Example files are located in `whiteboard/examples/`:
- `ddf/flowchart.json` - User authentication flowchart
- `ddf/network_diagram.json` - Network topology
- `ddf/org_chart.json` - Organization chart
- `shapes/*.svgshape` - Shape library examples

## 🗂️ Directory Structure

```
docs/
├── README.md              # Main documentation index
├── QUICK_REFERENCE.md     # This file
├── user/                  # End-user guides
├── api/                   # API reference docs
├── dev/                   # Developer guides
└── archive/               # Historical docs
```

## 🔗 External Resources

- [NanoVG Documentation](https://github.com/memononen/nanovg)
- [JSON Schema](https://json-schema.org/)
- [SVG Specification](https://www.w3.org/TR/SVG2/)

---

💡 **Tip**: Use your IDE's search to find specific topics across all documentation files.
