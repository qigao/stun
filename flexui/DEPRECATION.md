# FlexUI Deprecation Notice

## Deprecated Components

### Panel → Card

**Status:** Deprecated  
**Reason:** Panel does not inherit from Widget and cannot use CSS styling.  
**Replacement:** Use `Card` instead.

#### Migration Guide

**Old Code (Panel):**
```cpp
#include <flexui/panel.h>

PanelStyle style;
style.bgColor = nvgRGB(255, 255, 255);
style.borderRadius = 8;

Panel panel(100, 100, style);
panel.draw(vg);
```

**New Code (Card):**
```cpp
#include <flexui/card.h>

// XML approach (recommended)
screen.loadXML(R"(
    <card id="my-card" style="
        background: #ffffff;
        border-radius: 8px;
        width: 300px;
        height: 200px;
    "/>
)");

// Or C++ approach
auto* card = screen.createWidget<Card>(renderer, "my-card");
```

**Benefits:**
- ✅ Full CSS support
- ✅ Responsive layout with flexbox
- ✅ Consistent with other widgets
- ✅ Can be styled via XML/CSS

---

### Toggle → Switch

**Status:** Deprecated  
**Reason:** Toggle does not inherit from Widget and cannot use CSS styling.  
**Replacement:** Use `Switch` instead.

#### Migration Guide

**Old Code (Toggle):**
```cpp
#include <flexui/toggle.h>

ToggleStyle style;
style.colorOn = nvgRGB(76, 175, 80);

Toggle toggle(50, 50, false, style);
toggle.setChangeCallback([](bool on) {
    // Handle change
});
toggle.draw(vg);
toggle.handleClick(mx, my);
```

**New Code (Switch):**
```cpp
#include <flexui/switch.h>

// XML approach (recommended)
screen.loadXML(R"(
    <switch id="my-switch" on="false"/>
)");

auto* sw = screen.findWidget("my-switch");
if (auto* switchWidget = dynamic_cast<Switch*>(sw)) {
    switchWidget->setChangeCallback([](bool on) {
        // Handle change
    });
}

// Or C++ approach
auto* switchWidget = screen.createWidget<Switch>(renderer, "my-switch", false);
switchWidget->setChangeCallback([](bool on) {
    // Handle change
});
```

**Benefits:**
- ✅ Full CSS support
- ✅ Integrated with FlexUI event system
- ✅ Consistent with other widgets
- ✅ Can be styled via XML/CSS

---

## Timeline

- **Now:** Deprecation warnings added
- **Next Release:** Panel and Toggle marked as deprecated in documentation
- **Future Release:** Panel and Toggle may be removed

## Questions?

If you have questions about migration, please open an issue on GitHub.
