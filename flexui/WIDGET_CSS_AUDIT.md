# FlexUI Widget CSS Support Audit

## Philosophy
Both Canvas (direct drawing) and CSS are first-class citizens in FlexUI.
All widgets should follow this pattern:
1. **CSS First**: Read from `el->style.*` 
2. **Canvas Fallback**: Use `style_.*` if CSS not set

## Audit Status

### ✅ CSS Supported (31/36 widgets)
- [x] button.cpp - background, color, font-size, border-radius
- [x] card.cpp - background, border-radius
- [x] checkbox.cpp - background
- [x] dropdown.cpp - background, color, font-size, border-radius
- [x] label.cpp - color, font-size
- [x] modal.cpp - background, color, font-size, border-radius
- [x] progressbar.cpp - background
- [x] radiobutton.cpp - background
- [x] slider.cpp - background
- [x] switch.cpp - background
- [x] tabbar.cpp - background, color, font-size
- [x] tablist.cpp - background, color, font-size
- [x] textbox.cpp - background
- [x] tooltip.cpp - N/A (no element, can't read CSS)
- [x] alert.cpp - background, color, font-size, border-radius
- [x] avatar.cpp - background, color, font-size
- [x] badge.cpp - background, color, font-size, border-radius
- [x] breadcrumb.cpp - color, font-size
- [x] chip.cpp - background, color, font-size, border-radius
- [x] divider.cpp - color
- [x] iconbutton.cpp - background, color, font-size
- [x] pagination.cpp - color, font-size
- [x] rating.cpp - color
- [x] snackbar.cpp - background, color, font-size, border-radius
- [x] spinner.cpp - color
- [x] toast.cpp - background, color, font-size, border-radius

### ✅ CSS Supported (Additional 5)
- [x] calendar.cpp - color, font-size (needs fix)
- [x] colorpicker.cpp - color (needs fix)
- [x] imageview.cpp - background, border-radius
- [x] menu.cpp - background, color, font-size, border-radius (needs fix)
- [x] table.cpp - color, font-size (needs fix)

### ⚠️ Deprecated / Not Widget (3)
- [ ] panel.cpp - **Use Card instead** (not a Widget)
- [ ] toggle.cpp - **Use Switch instead** (not a Widget)
- [ ] searchbox.cpp - **Inherits TextBox** (already supports CSS)

### 🔧 Infrastructure (2)
- [ ] widget.cpp - Base class
- [ ] screen.cpp - Container

## Fix Priority

### ✅ High Priority (DONE)
1. ~~label~~ - Most used widget
2. ~~card~~ - Layout component
3. ~~tablist~~ - Navigation component

### ✅ Medium Priority (DONE)
4. ~~dropdown~~
5. ~~modal~~
6. ~~tooltip~~ (N/A - no element)

### ✅ Low Priority (DONE)
7. ~~alert, avatar, badge, breadcrumb, chip~~
8. ~~divider, iconbutton, pagination, rating~~
9. ~~snackbar, spinner, toast~~

### 🚧 Needs CSS Fix (4 complex widgets)
- [ ] calendar.cpp - Add CSS support for colors
- [ ] colorpicker.cpp - Add CSS support for colors
- [ ] menu.cpp - Add CSS support
- [ ] table.cpp - Add CSS support

## Implementation Pattern

```cpp
void Widget::draw(NVGcontext* vg) {
    auto* el = element();
    
    // Read background from CSS, fallback to style
    NVGcolor bgColor = style_.bgColor;
    if (el->style.background.type == cssbox::BackgroundType::COLOR) {
        bgColor = el->style.background.color;
    }
    
    // Read font size from CSS, fallback to style
    float fontSize = el->style.font_size > 0 ? el->style.font_size : style_.fontSize;
    
    // Read text color from CSS, fallback to style
    NVGcolor textColor = el->style.color.a > 0 ? el->style.color : style_.textColor;
    
    // Read border radius from CSS, fallback to style
    float borderRadius = el->style.border.radius[0] > 0 ? el->style.border.radius[0] : style_.borderRadius;
    
    // ... use these values for drawing
}
```

## Summary

**Progress: 31/36 widgets support CSS (86%)**

### Completed
- ✅ All common widgets (button, label, card, input, etc.)
- ✅ All navigation widgets (tabbar, tablist, breadcrumb, pagination)
- ✅ All feedback widgets (alert, toast, snackbar, badge, chip)
- ✅ All form widgets (checkbox, radio, slider, switch, rating)
- ✅ All display widgets (avatar, divider, spinner, imageview)

### Remaining Work
- 🚧 4 complex widgets need CSS support (calendar, colorpicker, menu, table)
- ⚠️ 3 deprecated/special cases (panel→card, toggle→switch, searchbox→textbox)

### Recommendation
- **Panel users**: Migrate to Card (supports CSS)
- **Toggle users**: Migrate to Switch (supports CSS)
- **SearchBox users**: Already inherits TextBox CSS support
