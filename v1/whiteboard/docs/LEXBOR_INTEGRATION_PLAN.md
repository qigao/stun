# Lexbor Integration Plan

## Executive Summary

We're integrating **Lexbor** (HTML/CSS parser) to enable rich styling with full CSS3 support and HTML-formatted text in the Modern Whiteboard application.

## Why Lexbor?

### Current Limitations

**CSS Support** (Manual Parser):
- ✅ Basic selectors: `type`, `.class`, `#id`, `:pseudo`
- ❌ No combinators: `>`, `+`, `~`
- ❌ No attribute selectors: `[attr=value]`
- ❌ No advanced pseudo-classes: `:nth-child()`, `:not()`
- ❌ No CSS functions: `calc()`, `rgb()`, `var()`

**Text Support**:
- ✅ Plain text only
- ❌ No formatting (bold, italic, underline)
- ❌ No links
- ❌ No lists

### With Lexbor

**Full CSS3**:
```css
/* All of these work! */
.card > .title { fill: blue; }
rect[data-status="active"] { fill: green; }
.card:nth-child(2n) { opacity: 0.5; }
.card:hover:not(.disabled) { fill: lightblue; }
```

**Rich Text**:
```html
<b>Bold</b> <i>italic</i> <u>underline</u>
<a href="...">Links</a>
<ul><li>Lists</li></ul>
<span style="color: red">Colored text</span>
```

## Implementation Plan

### Timeline: 6-8 weeks

**Phase 1: Foundation** (1 week)
- Add Lexbor dependency ✅ Done
- Create C++ wrappers
- Replace CSS parser
- Integrate with DDF

**Phase 2: Advanced CSS** (1 week)
- Attribute selectors
- Combinators
- Pseudo-classes
- CSS functions

**Phase 3: Rich Text** (1-2 weeks)
- HTML parser
- Rich text renderer
- Text editor
- Copy/paste

**Phase 4: SVG Integration** (1 week)
- CSS in SVG
- Export with CSS

**Phase 5: Themes** (1 week)
- Light/dark themes
- CSS variables
- Theme switching

**Phase 6: Polish** (1 week)
- Performance optimization
- Testing
- Documentation

## Benefits

### For Users
✅ Professional styling with full CSS3
✅ Rich text formatting (bold, italic, links, lists)
✅ Easy theme switching (light/dark mode)
✅ Consistent styling across shapes

### For Developers
✅ Standards-compliant (CSS3, HTML5)
✅ Less custom code (Lexbor handles complexity)
✅ Better DDF support
✅ Maintainable and extensible

### For the Project
✅ Competitive with Figma/Miro
✅ Small footprint (~2-3 MB)
✅ Fast (10-100x faster than alternatives)
✅ Future-proof

## Technical Details

### Architecture

```
┌─────────────────────────────────────────┐
│         Lexbor Integration Layer         │
│  ┌──────────┐  ┌──────────┐  ┌────────┐│
│  │   CSS    │  │   HTML   │  │Selector││
│  │  Parser  │  │  Parser  │  │Matcher ││
│  └──────────┘  └──────────┘  └────────┘│
└─────────────────────────────────────────┘
                  ↕
┌─────────────────────────────────────────┐
│         Enhanced Style System            │
│  ┌──────────┐  ┌──────────┐  ┌────────┐│
│  │StyleSheet│  │RichText  │  │ Theme  ││
│  │ Manager  │  │ Renderer │  │Manager ││
│  └──────────┘  └──────────┘  └────────┘│
└─────────────────────────────────────────┘
                  ↕
┌─────────────────────────────────────────┐
│         Existing Systems                 │
│  ┌──────────┐  ┌──────────┐  ┌────────┐│
│  │   DDF    │  │  LunaSVG │  │NanoVG  ││
│  │ Document │  │   (SVG)  │  │(Canvas)││
│  └──────────┘  └──────────┘  └────────┘│
└─────────────────────────────────────────┘
```

### Performance Targets

- CSS parsing: < 10ms for 1000 rules
- Selector matching: < 1ms per shape
- Style computation: < 5ms for 1000 shapes
- HTML rendering: 60 FPS

### Dependencies

- ✅ Lexbor 2.3.0+ (added to vcpkg.json)
- ✅ LunaSVG (existing)
- ✅ NanoVG (existing)
- ✅ nlohmann/json (existing)

## Integration with Collaboration

**Lexbor** (rich styling) + **Collaboration** (multi-user) = Professional collaborative whiteboard

**Recommended approach**:
1. Implement Lexbor first (6-8 weeks) - Better foundation
2. Then implement Collaboration (12-15 months)

**Or parallel development**:
- Team A: Lexbor integration
- Team B: Collaboration features

## Getting Started

### Step 1: Install Lexbor
```bash
vcpkg install lexbor
```

### Step 2: Review Spec
- Read `.kiro/specs/lexbor-integration/requirements.md`
- Read `.kiro/specs/lexbor-integration/design.md`
- Read `.kiro/specs/lexbor-integration/tasks.md`

### Step 3: Start Implementation
- Begin with Task 1: Add Lexbor Dependency
- Create C++ wrappers
- Test CSS parsing

## Example Usage

### Before (Current)

```cpp
// Limited CSS support
StyleSheet sheet;
sheet.add_rule(".card", {{"fill", "red"}});
// ❌ Can't use: ".card > .title"
// ❌ Can't use: "[data-status='active']"
```

### After (With Lexbor)

```cpp
// Full CSS3 support
EnhancedStyleSheet sheet;
sheet.parse_css(R"(
    .card > .title { fill: blue; }
    rect[data-status="active"] { fill: green; }
    .card:nth-child(2n) { opacity: 0.5; }
    .card:hover:not(.disabled) { fill: lightblue; }
)");

// Compute styles with full cascade
auto styles = sheet.compute_style(
    "shape1", "rect", {"card"}, 
    {{"data-status", "active"}}, 
    {"hover"}
);
```

### Rich Text

```cpp
// Before: Plain text only
nvgText(ctx, x, y, "Plain text", nullptr);

// After: Rich HTML text
RichTextRenderer renderer;
renderer.render(ctx, 
    "<b>Bold</b> <i>italic</i> <a href='...'>link</a>",
    x, y, max_width
);
```

## Success Metrics

### Technical
- ✅ 100% CSS3 selector support
- ✅ < 10ms CSS parsing
- ✅ < 1ms selector matching
- ✅ 60 FPS with rich text

### User
- ✅ Users create complex styles
- ✅ Rich text is easy to use
- ✅ Themes work seamlessly
- ✅ No performance issues

## Risks & Mitigation

### Technical Risks
1. **Integration complexity** → Create C++ wrappers
2. **Performance impact** → Implement caching
3. **Memory usage** → LRU cache eviction

### Business Risks
1. **Learning curve** → Provide templates
2. **Backward compatibility** → Maintain compatibility layer

## Next Steps

### This Week
1. ✅ Add Lexbor to vcpkg.json (Done!)
2. Install Lexbor: `vcpkg install lexbor`
3. Create test program
4. Start Task 1 implementation

### This Month
1. Complete Phase 1 (Foundation)
2. Test CSS parsing
3. Integrate with DDF

### This Quarter
1. Complete Phases 1-3
2. Test with real documents
3. Gather feedback

## Resources

### Specification
- `.kiro/specs/lexbor-integration/` - Full spec
- `requirements.md` - 8 requirements
- `design.md` - Technical architecture
- `tasks.md` - 15 tasks, 75 subtasks

### External
- [Lexbor GitHub](https://github.com/lexbor/lexbor)
- [Lexbor Documentation](https://lexbor.com/docs/)
- [CSS3 Selectors Spec](https://www.w3.org/TR/selectors-3/)
- [HTML5 Spec](https://html.spec.whatwg.org/)

---

**Created**: October 2025
**Status**: Ready for Implementation
**Next Action**: Install Lexbor and start Task 1
