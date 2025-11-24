# NanoGUI Fluent iOS Theme

An experimental Fluent UI **iOS**-inspired component set for NanoGUI. The goal is to mirror Fluent 2 guidance for the Apple ecosystem (https://fluent2.microsoft.design/components/ios). This first drop focuses on scaffolding, color tokens, and a few representative controls so further work can iterate quickly.

## Included Pieces

- FluentIOSTheme - Tokenised palette for light/dark schemes with iOS spacing, shadows, accent handling, and semantic color tokens (`color(SemanticColor)`).
- FluentIOSButton - Rounded system button supporting Filled / Gray / Outline styles and destructive variants.
- FluentIOSSegmentedControl - segmented toggle styled like iOS.
- FluentIOSListCell - list/table cell with title, subtitle, icon, and accessory chevron.
- FluentIOSSwitch - adaptive toggle with Fluent spacing, drag gestures, and keyboard toggling.
- FluentIOSTextField - token-aware text entry with accent focus ring and subdued placeholder styling.
- FluentIOSNavigationBar - Simple navigation header with optional back affordance, subtitle support, and trailing action container.
- FluentIOSTheme token accessors for typography, spacing, and motion curves.
- `example_fluent_ios_showcase` sample featuring navigation, segmented control, switch, and text field.

These components are intentionally lightweight and reuse NanoGUI primitives so they can serve as a foundation for broader Fluent UI coverage (segmented controls, lists, sheets, etc.).

### Semantic Color Tokens

`FluentIOSTheme::SemanticColor` exposes Fluent-style backgrounds, text roles, accent tiers, and status colors for light/dark schemes. Use `theme->color(SemanticColor::AccentPrimary)` (or the success/warning/danger tokens) to keep widgets aligned with the Fluent iOS palette without reaching into internal theme members.

Controls like the button, segmented control, switch, list cell, text field, and navigation bar now consume these semantic tokens, so palette tweaks flow automatically to the showcase components.

### Elevation Tokens

`FluentIOSTheme::ElevationLevel` returns soft shadow presets (offset, blur, opacity) tailored for light/dark schemes via `theme->elevation(level)`. The navigation bar and switch knob now draw their shadows from these presets so raised surfaces match Fluent depth guidance out of the box.

Levels align with the Fluent 2 iOS tiers (2, 4, 8, 16, 28, 64) as documented in *Microsoft Fluent 2 iOS.pdf*, combining the key and ambient shadow components the spec lists for each tier.

Filled/gray buttons and focused text fields also consume these elevation tokens, so their raised states inherit the same key+ambient shadows as the specification.

## Showcase Example

Run `cmake --build build --target example_fluent_ios_showcase` (or the equivalent in your generator) to launch the showcase screen. It pairs the navigation bar, segmented control, list cells, and the new FluentIOSSwitch and FluentIOSTextField in a grouped layout. The switch reacts to pointer drag or keyboard input while the text field demonstrates Fluent spacing, placeholder styling, and accent focus states tied to the FluentIOSTheme tokens.

## Roadmap Ideas

- Add Fluent iOS typography ramp (Large Title vs Inline) and integrate with FluentIOSTheme token accessors.
- Implement segmented control, list cell, people picker, and sheet panels inspired by the Fluent iOS catalog.
- Expand button styles (tonal, prominent) and add pressed/disabled animations.
- Provide Lottie-backed status indicators and acrylic materials for immersive surfaces.
- Extend FluentIOSListCell with toggle/accessory affordances and integrate text field validation states.

## Building

```
cmake --build build --target nanogui_fluent_ios_theme
```

The library is added to the NanoGUI build via theme/fluent-ios/CMakeLists.txt and exported as NanoGUI::FluentIOSTheme.
