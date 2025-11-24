# DDF Quick Reference Guide

## Importing Diagrams

1. **Click "Import DDF"** in the top menu
2. **Select a `.json` file** from `whiteboard/examples/ddf/`
3. **View your diagram** - it renders automatically!

## Example Files

### flowchart.json
User authentication flowchart with:
- Decision diamonds (orange)
- Process boxes (blue)
- Start/End ovals (green)
- Labeled arrows

### network_diagram.json
Network topology diagram

## DDF File Structure

```json
{
  "version": "1.0",
  "metadata": { "title": "My Diagram" },
  "shapes": [
    {
      "id": "shape1",
      "type": "rect",
      "geometry": { "x": 100, "y": 100, "width": 120, "height": 60 },
      "classes": ["my-class"],
      "text": "My Label"
    }
  ],
  "connectors": [
    {
      "id": "conn1",
      "from": { "shape_id": "shape1", "connection_point": "bottom" },
      "to": { "shape_id": "shape2", "connection_point": "top" },
      "arrow_end": "arrow"
    }
  ],
  "styles": {
    "rules": [
      {
        "selector": ".my-class",
        "properties": { "fill": "#3498db", "stroke": "#2c3e50" }
      }
    ]
  }
}
```

## Shape Types

- **rect** - Rectangle (with optional `rx` for rounded corners)
- **ellipse** - Oval (use `cx`, `cy`, `rx`, `ry`)
- **circle** - Circle (use `cx`, `cy`, `r`)
- **path** - Custom shape (use `d` in style for SVG path)
- **text** - Text label (use `x`, `y` for position)

## Connection Points

- **top** - Top center
- **bottom** - Bottom center
- **left** - Left center
- **right** - Right center

## Styling

### CSS-like Selectors
- `.class-name` - Class selector
- `#shape-id` - ID selector
- `.class:hover` - Pseudo-state

### Common Properties
- `fill` - Fill color (hex: `#3498db`)
- `stroke` - Border color
- `stroke-width` - Border width (number as string)
- `font-size` - Text size
- `opacity` - Transparency (0-1)

## Tips

- **Auto-generation**: If you only provide `data` nodes, shapes are generated automatically
- **Text color**: Automatically adapts to background (dark text on light, light on dark)
- **Multi-line text**: Use `\n` in text for line breaks
- **Hover effects**: Define `:hover` styles for interactive feedback
- **Optional IDs**: Shape and connector IDs are auto-generated if omitted

## Troubleshooting

### Shapes not appearing?
- Check console for validation errors
- Ensure geometry has numeric values (not strings)
- Path `d` attribute should be in `style`, not `geometry`

### Text not visible?
- Check if `text` field is set on shape
- Verify text color contrasts with background

### Connectors not drawing?
- Ensure both `from` and `to` shapes exist
- Check connection point names match shape's connection points
- Verify `arrow_end` is set to "arrow" if you want arrowheads

## Advanced Features

### Data Binding
```json
{
  "data": {
    "nodes": [{ "id": "node1", "properties": { "name": "John" } }]
  },
  "shapes": [{
    "data_binding": {
      "node_id": "node1",
      "property_mappings": { "text": "{{name}}" }
    }
  }]
}
```

### Components (Coming Soon)
Reusable shape templates with parameters

### Events (Coming Soon)
Interactive behaviors on click/hover

## Resources

- Full documentation: `whiteboard/docs/ddf/`
- API reference: `whiteboard/docs/ddf/API_REFERENCE.md`
- Examples: `whiteboard/examples/ddf/`
