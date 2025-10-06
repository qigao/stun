# NanoGUI Material Design 3 (M3) Theme

A modern Material Design 3 (Material You) theme implementation for NanoGUI based on the official [Material Design 3 specifications](https://m3.material.io).

## Overview

This theme implements Google's latest Material Design 3 system with:
- **Dynamic Color System** - Tonal palettes with 13 color roles
- **Adaptive Layouts** - Responsive components that adapt to screen size
- **Enhanced Accessibility** - WCAG 2.1 AA compliant color contrasts
- **Motion & Interaction** - Smooth animations with Material easing curves
- **Typography Scale** - 5 display + 9 text styles

## Key Differences from Material Theme

The M3 theme differs from the existing material theme by implementing:

1. **Color System**: Uses tonal palettes (primary, secondary, tertiary, error, neutral) with 13 color roles
2. **Shape System**: Rounded corners with 4 shape families (none, small, medium, large, extra-large)
3. **Elevation**: Uses color overlays instead of shadows for elevation
4. **State Layers**: Hover/focus/press states use color overlays at specific opacities
5. **Typography**: Updated type scale with display styles and refined text styles

## Components

### Core (1)
- `M3Theme` - Material Design 3 color and style system with dynamic color generation

### Implemented Components (13)

#### Buttons & Actions (2)
- `M3Button` - 5 variants (Filled, Outlined, Text, Elevated, Tonal)
- `M3FAB` - Floating Action Button (3 sizes: Small, Regular, Large)

#### Containment (2)
- `M3Card` - 3 variants (Elevated, Filled, Outlined)
- `M3Dialog` - Modal dialog with scrim and elevation

#### Input (4)
- `M3TextField` - 2 variants (Filled, Outlined) with label and helper text
- `M3Switch` - Toggle switch with state layers
- `M3Checkbox` - Checkbox with label support
- `M3Slider` - Range slider with value display

#### Selection (1)
- `M3Chip` - 4 variants (Assist, Filter, Input, Suggestion)

#### Feedback (2)
- `M3ProgressBar` - Linear progress indicator
- `M3Tooltip` - Hover tooltip with inverse colors

#### Display (2)
- `M3Divider` - Horizontal/vertical content separator
- `M3Badge` - Notification badge (dot or count)

### Planned Components

Additional M3 components will be added in future releases:
- M3Switch, M3Checkbox, M3Radio
- M3Slider, M3ProgressBar
- M3Dialog, M3Menu, M3Tooltip
- M3Tabs, M3NavigationBar
- And more...

## Building

### As Part of NanoGUI

```bash
cmake -DBUILD_M3_THEME=ON ..
cmake --build .
```

### Standalone

```bash
cd theme/m3
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

```cpp
#include <nanogui/nanogui.h>
#include <nanogui/m3.h>  // Includes all M3 components

using namespace nanogui;

int main() {
    nanogui::init();
    
    Screen *screen = new Screen(Vector2i(800, 600), "M3 Demo");
    
    // Create M3 theme with seed color
    auto *theme = new M3Theme(screen->nvg_context(), 
                              Color(0.4f, 0.2f, 0.8f, 1.0f)); // Purple seed
    screen->set_theme(theme);
    
    // Create M3 components
    auto *button = new M3Button(screen, "Click Me", 0, M3Button::Style::Filled);
    auto *fab = new M3FAB(screen, 0xf067, M3FAB::Size::Regular);
    auto *card = new M3Card(screen, M3Card::Style::Elevated);
    auto *textfield = new M3TextField(screen, "Hello", M3TextField::Style::Filled);
    auto *chip = new M3Chip(screen, "Filter", 0, M3Chip::Style::Filter);
    
    screen->perform_layout();
    nanogui::mainloop();
    nanogui::shutdown();
    
    return 0;
}
```

## Color System

M3 uses a tonal palette system with 13 color roles:

### Light Scheme
- Primary, OnPrimary, PrimaryContainer, OnPrimaryContainer
- Secondary, OnSecondary, SecondaryContainer, OnSecondaryContainer
- Tertiary, OnTertiary, TertiaryContainer, OnTertiaryContainer
- Error, OnError, ErrorContainer, OnErrorContainer
- Background, OnBackground
- Surface, OnSurface, SurfaceVariant, OnSurfaceVariant
- Outline, OutlineVariant
- Inverse colors (InverseSurface, InverseOnSurface, InversePrimary)

### Dark Scheme
Same roles with adjusted tones for dark mode.

## Requirements

- C++17 or later
- NanoGUI core library
- OpenGL 3.3+ or OpenGL ES 2.0+

## License

BSD-style license (same as NanoGUI). See LICENSE.txt in root directory.

## Version

**Version**: 1.0.0  
**Material Design**: Material Design 3 (Material You)  
**Specification**: https://m3.material.io  
**Status**: In Development
