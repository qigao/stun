# Inline SVG Text Editing

## Overview

The whiteboard now supports **inline text editing** for SVG shapes with text parameters. This provides a more intuitive editing experience similar to modern design tools like Figma, Sketch, or Adobe XD.

## Features

### Inline Text Editor
- **Double-click** on an SVG shape to edit text directly
- Text input appears **on the canvas** at the shape's location
- **Enter** to commit changes
- **Esc** to cancel
- **Auto-focus** with text pre-selected for easy replacement
- **Live updates** - SVG regenerates immediately

### Fallback to Full Editor
- If shape has **no text parameters**, opens the full parameter editor dialog
- If shape has **multiple parameters**, inline editor handles the first text parameter
- Full editor still accessible for advanced editing

## Usage

### Quick Edit (Inline)

1. **Double-click** on an SVG shape (e.g., UML class, flowchart node)
2. Text input appears on the shape
3. Type the new text
4. Press **Enter** to save or **Esc** to cancel

### Full Edit (Dialog)

1. **Double-click** on an SVG shape with no text parameters
2. Parameter editor dialog opens
3. Edit all parameters
4. Click **OK** to save or **Cancel** to discard

## Supported Shapes

Any SVG shape with text parameters supports inline editing:

- **UML Diagrams**
  - Class names
  - Method names
  - Attribute names
  
- **Flowcharts**
  - Process labels
  - Decision text
  - Data labels

- **Network Diagrams**
  - Node labels
  - Connection labels

- **Custom Shapes**
  - Any shape with `type: "text"` parameters

## Technical Details

### Parameter Types

SVG shapes can have three parameter types:

1. **text** - Inline editable (e.g., class names, labels)
2. **number** - Numeric values (e.g., width, height)
3. **list** - Dropdown selections (e.g., alignment, style)

Only **text** parameters support inline editing.

### SVG Template System

Shapes use placeholder syntax in SVG templates:

```xml
<svg>
  <text id="className">{{className}}</text>
  <text id="methodName">{{methodName}}</text>
</svg>
```

Parameters are defined in shape definitions:

```json
{
  "id": "uml.class",
  "name": "Class",
  "parameters": [
    {
      "name": "className",
      "type": "text",
      "default_value": "MyClass",
      "label": "Class Name"
    }
  ]
}
```

### Editing Flow

```
User double-clicks SVG shape
    ↓
CanvasController detects double-click
    ↓
Check if shape has text parameters
    ↓
┌─────────────────┬─────────────────┐
│ Has text param  │ No text param   │
│                 │                 │
│ InlineTextEditor│ SVGParameterEditor
│ (on canvas)     │ (dialog)        │
└─────────────────┴─────────────────┘
    ↓                   ↓
User edits text     User edits all params
    ↓                   ↓
Press Enter/Esc     Click OK/Cancel
    ↓                   ↓
Update parameter value
    ↓
Regenerate SVG with new parameters
    ↓
Update stroke in document
    ↓
Canvas redraws with updated shape
```

## Implementation

### Key Classes

1. **InlineTextEditor** (`inline_text_editor.h/cpp`)
   - Extends `nanogui::TextBox`
   - Positioned on canvas at shape location
   - Handles Enter/Esc keyboard events
   - Commits changes to document

2. **CanvasController** (`canvas_controller.cpp`)
   - Detects double-click on SVG shapes
   - Determines which editor to use
   - Creates and activates inline editor

3. **SVGShapeLibrary** (`svg_shape_library.h/cpp`)
   - Manages shape definitions
   - Provides parameter metadata
   - Regenerates SVG with new parameters

### Code Example

```cpp
// In canvas_controller.cpp
void CanvasController::handle_svg_double_click(int stroke_index, const nanogui::Vector2f &pos) {
  // Get shape definition
  const ShapeDefinition *shape = m_shape_library->get_shape(stroke.svg_shape_id);
  
  // Find first text parameter
  std::string text_param_name;
  for (const auto &param : shape->parameters) {
    if (param.type == "text") {
      text_param_name = param.name;
      break;
    }
  }
  
  if (!text_param_name.empty()) {
    // Use inline editor
    auto *editor = new InlineTextEditor(
      m_view->parent(),
      m_document,
      m_shape_library,
      stroke_index,
      text_param_name,
      pos
    );
    editor->activate();
  } else {
    // Use full parameter editor
    auto *editor = new SVGParameterEditor(...);
    editor->show();
  }
}
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| **Double-click** | Start editing |
| **Enter** | Commit changes |
| **Esc** | Cancel editing |
| **Tab** | (Future) Next parameter |
| **Shift+Tab** | (Future) Previous parameter |

## User Experience

### Before (Dialog-based)
1. Double-click shape
2. Dialog pops up
3. Find text field
4. Edit text
5. Click OK
6. Dialog closes

### After (Inline)
1. Double-click shape
2. Text input appears **on shape**
3. Edit text directly
4. Press Enter
5. Done!

**Result**: Faster, more intuitive, less context switching

## Future Enhancements

- [ ] Multi-line text editing
- [ ] Rich text formatting (bold, italic)
- [ ] Tab to cycle through multiple text parameters
- [ ] Auto-resize text box based on content
- [ ] Syntax highlighting for code parameters
- [ ] Autocomplete for common values
- [ ] Undo/redo within editor
- [ ] Drag to reposition editor
- [ ] Click outside to commit (optional)

## Comparison with Other Tools

| Feature | Figma | Sketch | Adobe XD | Whiteboard |
|---------|-------|--------|----------|------------|
| Inline text edit | ✅ | ✅ | ✅ | ✅ |
| Double-click to edit | ✅ | ✅ | ✅ | ✅ |
| Enter to commit | ✅ | ✅ | ✅ | ✅ |
| Esc to cancel | ✅ | ✅ | ✅ | ✅ |
| Auto-select text | ✅ | ✅ | ✅ | ✅ |
| On-canvas editing | ✅ | ✅ | ✅ | ✅ |

## Testing

### Manual Test

1. Load a whiteboard with SVG shapes
2. Double-click on a UML class shape
3. Verify inline editor appears
4. Type new class name
5. Press Enter
6. Verify shape updates with new text
7. Press Ctrl+Z to undo
8. Verify shape reverts to original text

### Edge Cases

- [ ] Empty text parameter
- [ ] Very long text (overflow)
- [ ] Special characters (quotes, brackets)
- [ ] Unicode characters
- [ ] Shape with no text parameters
- [ ] Shape with multiple text parameters
- [ ] Rapid double-clicks
- [ ] Click outside editor
- [ ] Delete shape while editing

## Troubleshooting

**Issue**: Editor doesn't appear
- Check if shape has text parameters
- Verify shape library is loaded
- Check console for errors

**Issue**: Text doesn't update
- Verify SVG template has correct placeholders
- Check parameter name matches template
- Ensure SVG regeneration succeeds

**Issue**: Editor appears in wrong position
- Check canvas coordinate transformation
- Verify zoom and pan are accounted for

## Files Modified

1. **whiteboard/include/whiteboard/canvas/inline_text_editor.h** (NEW)
2. **whiteboard/src/canvas/inline_text_editor.cpp** (NEW)
3. **whiteboard/src/canvas/canvas_controller.cpp** (MODIFIED)
4. **whiteboard/CMakeLists.txt** (MODIFIED)

## Summary

✅ **Inline text editing** for SVG shapes
✅ **Double-click** to edit
✅ **Enter/Esc** keyboard shortcuts
✅ **Auto-focus** and text selection
✅ **Live SVG regeneration**
✅ **Fallback** to full parameter editor
✅ **Professional UX** comparable to Figma

Your whiteboard now has **modern, intuitive text editing** for SVG shapes! 🎉
