# Whiteboard Documentation

Welcome to the Modern Whiteboard documentation. This guide will help you understand and use the whiteboard application's features.

## Documentation Structure

### 📘 User Documentation (`user/`)
End-user guides for using the whiteboard application:
- **DDF User Guide** - Working with Diagram Definition Format
- **SVG Editing Guide** - Creating and editing SVG shapes
- **Export Guide** - Exporting your work to various formats

### 🔧 API Reference (`api/`)
Technical API documentation for developers:
- **DDF API Reference** - Complete DDF API documentation
- **SVG Shape Format** - SVG shape file format specification
- **Format Comparison** - Comparison of different diagram formats

### 👨‍💻 Developer Guides (`dev/`)
Guides for developers working on the codebase:
- **DDF Components** - Component system architecture
- **DDF CSS Styling** - CSS-like styling system
- **DDF Expressions** - Expression language reference
- **SVG Generator** - SVG generation system
- **Shape Library** - Shape library implementation

### 📦 Archive (`archive/`)
Historical implementation notes and completed feature documentation:
- Implementation guides for completed features
- Bug fix documentation
- Testing and debugging guides

## Quick Start

### For Users
1. **Understand the formats**: [DDF as Project Format](DDF_AS_PROJECT_FORMAT.md) - How to save, share, and open projects
2. Start with the [DDF User Guide](user/DDF_USER_GUIDE.md)
3. Learn about [SVG Editing](user/SVG_EDITING_GUIDE.md)
4. Check out [Export Options](user/EXPORT_GUIDE.md)

### For Developers
1. **Understand the architecture**: [Architecture Summary](ARCHITECTURE_SUMMARY.md) - DDF and SVGShape relationship
2. Read the [DDF API Reference](api/DDF_API_REFERENCE.md)
3. Understand [DDF Components](dev/DDF_COMPONENTS.md)
4. Learn about [CSS Styling](dev/DDF_CSS_STYLING.md)
5. Review [Rendering Architecture](RENDERING_ARCHITECTURE.md)

## Key Features

### Diagram Definition Format (DDF)
**The universal document format** for both in-memory representation and file storage:
- Complete diagram structure (shapes, connectors, data, events)
- Hierarchical shape trees
- CSS-like styling with pseudo-states
- Data binding and expressions
- Component system for reusable elements
- Smart connectors with routing algorithms
- Can reference SVGShape templates

### SVG Shape Library
**Reusable visual element templates** used as building blocks:
- `.svgshape` format for shape definitions
- Template parameters (`{{placeholders}}`)
- Style customization
- Shape library management
- Referenced by DDF shapes

### Interactive Canvas
- Pan and zoom
- Shape selection and editing
- Hover effects and interactions
- Export to multiple formats

## File Formats

### DDF (.json) - The Document Format
**Purpose**: Universal format for in-memory representation AND file storage

Complete diagram documents with shapes, connectors, data, and interactivity.
```json
{
  "version": "1.0",
  "shapes": [
    {
      "id": "shape1",
      "type": "svg",
      "svg_shape_id": "flowchart.process"  // References SVGShape
    }
  ],
  "connectors": [...],
  "data": {...},
  "styles": {...}
}
```

### SVGShape (.svgshape) - Visual Element Template
**Purpose**: Reusable building blocks for shapes

Shape templates used by the shape library and referenced by DDF.
```json
{
  "type": "rect",
  "geometry": {...},
  "style": {...},
  "text": "{{label}}"  // Parametric
}
```

**See [Architecture Summary](ARCHITECTURE_SUMMARY.md) for the relationship between DDF and SVGShape.**

## Examples

Example files are located in `whiteboard/examples/`:
- `ddf/flowchart.json` - User authentication flowchart
- `ddf/network_diagram.json` - Network topology diagram
- `ddf/org_chart.json` - Organization chart

## Contributing

When adding new documentation:
- **User guides** → `user/`
- **API docs** → `api/`
- **Developer guides** → `dev/`
- **Implementation notes** → `archive/` (after feature is complete)

Keep documentation:
- Clear and concise
- Up-to-date with code changes
- Well-organized with examples
- Properly cross-referenced

## Support

For issues or questions:
1. Check the relevant documentation section
2. Review example files
3. Check the archive for historical context
4. Consult the API reference

---

Last updated: October 2025
