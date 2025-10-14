# Apple Human Interface Guidelines Theme for NanoGUI

This theme implements Apple's Human Interface Guidelines (HIG) design system for macOS, iOS, and visionOS.

## Overview

The Apple theme provides native-looking components following Apple's design principles:

- **Visual Hierarchy**: Clear information architecture with proper spacing and typography
- **Vibrancy**: Translucent materials and blur effects
- **Depth**: Subtle shadows and layering
- **Color**: System colors that adapt to light/dark mode
- **Typography**: SF Pro font system with dynamic type scales
- **Motion**: Smooth, natural animations

## Design Specifications

Based on Apple's Human Interface Guidelines:
- https://developer.apple.com/design/human-interface-guidelines/
- https://developer.apple.com/design/resources/

## Components

### Buttons
- **Primary**: Filled with accent color
- **Secondary**: Bordered with subtle background
- **Tertiary**: Text-only, minimal style
- **Destructive**: Red accent for dangerous actions

### Controls
- **Toggle**: iOS-style switch
- **Checkbox**: macOS-style checkbox
- **Radio**: Circular radio buttons
- **Slider**: Continuous value selection
- **Stepper**: Increment/decrement control
- **Segmented Control**: Mutually exclusive options

### Navigation
- **Tab Bar**: Bottom navigation (iOS)
- **Sidebar**: Collapsible navigation (macOS)
- **Navigation Bar**: Top bar with title and actions
- **Toolbar**: Action buttons at top/bottom

### Content
- **List**: Grouped or plain list styles
- **Card**: Rounded content containers
- **Sheet**: Modal presentation
- **Popover**: Contextual content
- **Alert**: System alerts and dialogs

### Input
- **Text Field**: Single-line text input
- **Text View**: Multi-line text input
- **Search Field**: Search with icon
- **Date Picker**: Date and time selection

### Feedback
- **Progress**: Linear and circular progress
- **Activity Indicator**: Loading spinner
- **Badge**: Notification counts
- **Label**: Text display with semantic colors

## Color System

### System Colors
- **Accent**: User-selected tint color (default blue)
- **Label**: Primary text color
- **Secondary Label**: Secondary text
- **Tertiary Label**: Tertiary text
- **Quaternary Label**: Watermark text

### Background Colors
- **System Background**: Primary background
- **Secondary System Background**: Grouped content
- **Tertiary System Background**: Grouped content within grouped content

### Semantic Colors
- **Red**: Destructive actions, errors
- **Orange**: Warnings
- **Yellow**: Caution
- **Green**: Success, positive actions
- **Blue**: Default accent, links
- **Purple**: Creative, premium
- **Pink**: Playful, social
- **Gray**: Neutral, disabled

## Typography

### SF Pro Font System
- **Large Title**: 34pt, Regular
- **Title 1**: 28pt, Regular
- **Title 2**: 22pt, Regular
- **Title 3**: 20pt, Regular
- **Headline**: 17pt, Semibold
- **Body**: 17pt, Regular
- **Callout**: 16pt, Regular
- **Subheadline**: 15pt, Regular
- **Footnote**: 13pt, Regular
- **Caption 1**: 12pt, Regular
- **Caption 2**: 11pt, Regular

## Spacing

### Standard Spacing Scale
- **4pt**: Tight spacing
- **8pt**: Default spacing
- **12pt**: Comfortable spacing
- **16pt**: Section spacing
- **20pt**: Large spacing
- **24pt**: Extra large spacing

## Corner Radius

### Standard Radius Values
- **Small**: 4pt (buttons, badges)
- **Medium**: 8pt (cards, text fields)
- **Large**: 12pt (sheets, popovers)
- **Extra Large**: 16pt (large cards)
- **Continuous**: Apple's continuous corner curve

## Usage

```cpp
#include <nanogui/apple_theme.h>
#include <nanogui/apple_button.h>

// Create Apple theme
auto theme = new AppleTheme(ctx);
theme->set_accent_color(AppleTheme::AccentColor::Blue);
theme->set_appearance(AppleTheme::Appearance::Light);

// Create button with Apple styling
auto button = new AppleButton(window, "Click Me", AppleButton::Style::Primary);
button->set_callback([]() {
    std::cout << "Button clicked!" << std::endl;
});
```

## Building

The Apple theme is built as part of the NanoGUI theme system:

```bash
cmake -B build -DBUILD_APPLE_THEME=ON
cmake --build build
```

## License

BSD-style license (see LICENSE.txt)
