# FlexUI Widget Library

Complete widget library for FlexUI with CSS styling and interactive controls.

## Widget Overview

### Core Widgets (Existing)

#### Button
Interactive button with hover and click states.
```cpp
FlexButtonProps button;
button.id = "my-button";
button.text = "Click Me";
button.classes.push_back("button-primary");
FlexWidgetFactory::createButton(document, button);
```

#### Text Input
Single-line text input field.
```cpp
FlexTextInputProps input;
input.id = "my-input";
input.placeholder = "Enter text...";
FlexWidgetFactory::createTextInput(document, input);
```

#### Checkbox
Toggle checkbox with checked/unchecked states.
```cpp
FlexCheckboxProps checkbox;
checkbox.id = "my-checkbox";
checkbox.checked = true;
FlexWidgetFactory::createCheckbox(document, checkbox);
```

#### Radio Button
Radio button for mutually exclusive selections within a group.
```cpp
FlexRadioProps radio;
radio.id = "radio-option-1";
FlexWidgetFactory::createRadio(document, radio);
```

#### Slider
Draggable slider for selecting values in a range.
```cpp
FlexSliderProps slider;
slider.track_id = "slider-track";
slider.fill_id = "slider-fill";
slider.thumb_id = "slider-thumb";
slider.value = 0.5f; // 0.0 to 1.0
FlexWidgetFactory::createSlider(document, slider);
```

#### Toggle/Switch
On/off toggle switch.
```cpp
FlexToggleProps toggle;
toggle.id = "my-toggle";
toggle.initial_on = true;
FlexWidgetFactory::createToggle(document, toggle);
```

#### Dropdown
Dropdown menu with selectable options.
```cpp
FlexDropdownProps dropdown;
dropdown.id = "my-dropdown";
dropdown.placeholder = "Select option";
dropdown.options = {
  {"opt1", "Option 1"},
  {"opt2", "Option 2"}
};
FlexWidgetFactory::createDropdown(document, dropdown);
```

---

### New Widgets

#### Progress Bar
Animated progress indicator showing completion percentage.

**Features:**
- Smooth animations
- Customizable colors via CSS
- Programmatic value updates

**Usage:**
```cpp
FlexProgressBarProps progress;
progress.track_id = "progress-track";
progress.fill_id = "progress-fill";
progress.width = 300;
progress.height = 10;
progress.value = 0.75f; // 75% complete
FlexWidgetFactory::createProgressBar(document, progress);

// Controller for dynamic updates
auto progress_controller = std::make_shared<FlexProgressBar>();
FlexProgressBarBinding binding;
binding.track_id = "progress-track";
binding.fill_id = "progress-fill";
binding.track_width = 300;
binding.initial_value = 0.75f;
progress_controller->registerProgressBar(binding);

// Update value programmatically
progress_controller->setValue("progress-track", 0.9f);
```

**CSS Classes:**
- `.progress-track` - Background track
- `.progress-fill` - Filled portion

---

#### Tabs
Tab control for organizing content into switchable panels.

**Features:**
- Multiple tabs with associated panels
- Click to switch between tabs
- Active tab highlighting
- Callback on tab change

**Usage:**
```cpp
FlexTabsProps tabs;
tabs.container_id = "tabs-main";
tabs.tabs = {
  {"tab-1", "panel-1", "First Tab"},
  {"tab-2", "panel-2", "Second Tab"},
  {"tab-3", "panel-3", "Third Tab"}
};
FlexWidgetFactory::createTabs(document, tabs);

// Controller for interactivity
auto tabs_controller = std::make_shared<FlexTabs>();
FlexTabsBinding binding;
binding.container_id = "tabs-main";
binding.tabs = {
  {"tab-1", "panel-1", "First Tab"},
  {"tab-2", "panel-2", "Second Tab"},
  {"tab-3", "panel-3", "Third Tab"}
};
binding.initial_active = 0;
binding.on_change = [](int index) {
  logi("Tab {} activated", index);
};
tabs_controller->registerTabs(binding);
```

**CSS Classes:**
- `.tabs-container` - Main container
- `.tab-bar` - Tab button container
- `.tab` - Individual tab button
- `.tab-active` - Active tab
- `.tab-content` - Content area
- `.tab-panel` - Individual panel
- `.tab-panel-active` - Visible panel
- `.tab-panel-hidden` - Hidden panel

---

#### Badge
Small label for status indicators, counts, or tags.

**Features:**
- Compact design
- Multiple color variants
- Text content

**Usage:**
```cpp
FlexBadgeProps badge;
badge.id = "badge-new";
badge.text = "New";
badge.classes.push_back("badge-primary");
FlexWidgetFactory::createBadge(document, badge);
```

**CSS Classes:**
- `.badge` - Base badge style
- `.badge-primary` - Primary color variant
- `.badge-success` - Success/green variant
- `.badge-danger` - Danger/red variant
- `.badge-warning` - Warning/yellow variant
- `.badge-info` - Info/blue variant

---

#### Card
Container widget for grouping related content.

**Features:**
- Title and content areas
- Hover effects
- Shadow and border styling

**Usage:**
```cpp
FlexCardProps card;
card.id = "my-card";
card.title = "Card Title";
card.content = "Card content goes here.";
card.classes.push_back("card");
FlexWidgetFactory::createCard(document, card);
```

**CSS Classes:**
- `.card` - Main card container
- `.card-title` - Card title text
- `.card-content` - Card content text

---

#### Tooltip
Contextual help text that appears on hover.

**Features:**
- Delayed appearance
- Follows mouse cursor
- Auto-hide on mouse leave
- Customizable delay

**Usage:**
```cpp
// Create tooltip element
FlexTooltipProps tooltip;
tooltip.tooltip_id = "tooltip-1";
tooltip.text = "This is helpful information";
FlexWidgetFactory::createTooltip(document, tooltip);

// Controller for hover behavior
auto tooltip_controller = std::make_shared<FlexTooltip>();
FlexTooltipBinding binding;
binding.target_id = "my-button"; // Element to attach tooltip to
binding.tooltip_id = "tooltip-1";
binding.text = "This is helpful information";
binding.delay = 0.5f; // Show after 0.5 seconds
tooltip_controller->registerTooltip(binding);
```

**CSS Classes:**
- `.tooltip` - Tooltip container
- `.tooltip-visible` - Visible state
- `.tooltip-hidden` - Hidden state

---

#### Spinner
Animated loading indicator.

**Features:**
- Continuous rotation animation
- Configurable rotation speed
- Active/inactive states
- Customizable size

**Usage:**
```cpp
FlexSpinnerProps spinner;
spinner.id = "spinner-loading";
spinner.size = 40;
spinner.classes.push_back("spinner");
FlexWidgetFactory::createSpinner(document, spinner);

// Controller for animation
auto spinner_controller = std::make_shared<FlexSpinner>();
FlexSpinnerBinding binding;
binding.spinner_id = "spinner-loading";
binding.rotation_speed = 360.0f; // Degrees per second
binding.active = true;
spinner_controller->registerSpinner(binding);

// Control spinner state
spinner_controller->setActive("spinner-loading", false); // Stop
spinner_controller->setActive("spinner-loading", true);  // Start
```

**CSS Classes:**
- `.spinner` - Spinner element
- `.spinner-active` - Active/spinning state
- `.spinner-inactive` - Inactive/stopped state

---

#### Scrollbar
Customizable scrollbar for scrolling content.

**Features:**
- Vertical and Horizontal orientation
- Draggable thumb
- Programmatic value updates
- Customizable styling

**Usage:**
```cpp
FlexScrollbarProps scrollbar;
scrollbar.track_id = "scrollbar-track";
scrollbar.thumb_id = "scrollbar-thumb";
scrollbar.orientation = FlexScrollbarOrientation::Vertical;
scrollbar.width = 12;
scrollbar.height = 200;
scrollbar.thumb_size = 40;
scrollbar.value = 0.0f;
FlexWidgetFactory::createScrollbar(document, scrollbar);

// Controller
auto scrollbar_controller = std::make_shared<FlexScrollbar>();
FlexScrollbarBinding binding;
binding.track_id = "scrollbar-track";
binding.thumb_id = "scrollbar-thumb";
binding.orientation = ScrollbarOrientation::Vertical;
binding.track_length = 200;
binding.thumb_size = 40;
binding.value = 0.0f;
binding.on_change = [](float value) {
    // Update content position based on value (0.0 to 1.0)
};
scrollbar_controller->registerScrollbar(binding);
```

**CSS Classes:**
- `.scrollbar-track` - Background track
- `.scrollbar-thumb` - Draggable thumb

---

#### Flexbox Container (NEW)
Flexible box layout container for arranging child elements in rows or columns.

**Features:**
- Flexbox layout (CSS Flexbox Module Level 1)
- Row, column, row-reverse, column-reverse directions
- Wrap control (nowrap, wrap, wrap-reverse)
- Justify content (flex-start, flex-end, center, space-between, space-around, space-evenly)
- Align items (flex-start, flex-end, center, baseline, stretch)
- Gap between items

**Usage:**
```cpp
FlexContainerProps container;
container.id = "flex-container";
container.direction = "row"; // "row", "column", "row-reverse", "column-reverse"
container.wrap = "wrap"; // "nowrap", "wrap", "wrap-reverse"
container.justify = "space-between"; // justify-content
container.align = "center"; // align-items
container.gap = "10px";
container.styles["width"] = "600px";
container.styles["background"] = "#ecf0f1";
FlexWidgetFactory::createFlexContainer(document, container);
```

**CSS Classes:**
- `.flex-container` - Container element

---

#### Flex Item (NEW)
Child element in a flexbox container with flex properties.

**Features:**
- Flex grow factor (expansion)
- Flex shrink factor (compression)
- Flex basis (initial size)
- Align self (override container alignment)

**Usage:**
```cpp
FlexItemProps item;
item.id = "flex-item-1";
item.parent_id = "flex-container";
item.text = "Flexible Item";
item.flex_grow = 1.0f; // Grows to fill space
item.flex_shrink = 1.0f; // Shrinks if needed
item.flex_basis = "auto"; // Initial size
item.align_self = "center"; // Override container alignment
item.styles["background"] = "#3498db";
FlexWidgetFactory::createFlexItem(document, item);
```

**CSS Classes:**
- `.flex-item` - Item element

---

#### Grid Container (NEW)
CSS Grid layout container for two-dimensional layouts.

**Features:**
- Grid template columns and rows
- Auto-placement and auto-sizing
- Gap between rows and columns
- Grid auto-flow control
- Responsive grid (auto-fill, auto-fit with minmax)

**Usage:**
```cpp
FlexGridProps grid;
grid.id = "grid-container";
grid.template_columns = "repeat(3, 1fr)"; // 3 equal columns
grid.template_rows = "auto"; // Auto-sized rows
grid.gap = "10px"; // Gap between items
grid.auto_rows = "100px"; // Size of implicit rows
grid.auto_flow = "row"; // "row", "column", "row dense", "column dense"
grid.styles["width"] = "600px";
grid.styles["background"] = "#ecf0f1";
FlexWidgetFactory::createGrid(document, grid);

// Example: Responsive auto-fill grid
FlexGridProps responsive_grid;
responsive_grid.id = "responsive-grid";
responsive_grid.template_columns = "repeat(auto-fill, minmax(150px, 1fr))";
responsive_grid.gap = "15px";
FlexWidgetFactory::createGrid(document, responsive_grid);
```

**CSS Classes:**
- `.grid-container` - Container element

---

#### Grid Item (NEW)
Child element in a grid container with grid placement properties.

**Features:**
- Column and row spanning
- Explicit column/row start and end
- Justify self and align self
- Support for grid-column and grid-row syntax

**Usage:**
```cpp
FlexGridItemProps item;
item.id = "grid-item-1";
item.parent_id = "grid-container";
item.text = "Grid Item";
item.column_span = 2; // Spans 2 columns
item.row_span = 1; // Spans 1 row
item.styles["background"] = "#e74c3c";
FlexWidgetFactory::createGridItem(document, item);

// Example: Explicit positioning
FlexGridItemProps positioned_item;
positioned_item.id = "positioned-item";
positioned_item.parent_id = "grid-container";
positioned_item.column_start = "1";
positioned_item.column_end = "3"; // Spans columns 1-2
positioned_item.row_start = "2";
positioned_item.row_end = "4"; // Spans rows 2-3
positioned_item.justify_self = "center"; // Horizontal alignment
positioned_item.align_self = "stretch"; // Vertical alignment
FlexWidgetFactory::createGridItem(document, positioned_item);
```

**CSS Classes:**
- `.grid-item` - Item element

---

## Widget Controllers

All interactive widgets require a controller to handle events and state:

### FlexPointer
Handles hover and click pseudo-classes for visual feedback.

### FlexCheckbox
Manages checkbox state and click events.

### FlexRadio
Manages radio button groups and mutual exclusivity.

### FlexSlider
Handles slider dragging and value updates.

### FlexToggle
Manages toggle switch state.

### FlexProgressBar (NEW)
Handles progress bar animations and value updates.

### FlexTabs (NEW)
Manages tab switching and panel visibility.

### FlexTooltip (NEW)
Handles tooltip hover delays and positioning.

### FlexSpinner (NEW)
Manages spinner rotation animation.

### FlexScrollbar (NEW)
Handles scrollbar dragging and value updates.

---

## Complete Example

See the following examples for comprehensive demonstrations:

- `flexui/examples/example_flexui_all_widgets.cpp` - All widgets with layout organization
- `flexui/examples/example_flexui_layouts.cpp` - **Layout systems showcase** (Flexbox & Grid)
- `flexui/examples/example_nanovg_css_basics_flexui.cpp` - Basic CSS features

To build and run:
```bash
cmake --build build --target example_flexui_all_widgets
cmake --build build --target example_flexui_layouts
./build/bin/example_flexui_all_widgets
./build/bin/example_flexui_layouts
```

---

## CSS Styling

All widgets can be styled using CSS. Common properties:

- `background-color` - Background color
- `color` - Text color
- `border-radius` - Rounded corners
- `padding` - Internal spacing
- `margin` - External spacing
- `width`, `height` - Dimensions
- `box-shadow` - Drop shadow
- `transition` - Smooth animations

Pseudo-classes:
- `:hover` - Mouse hover state
- `:active` - Mouse click/press state
- `:focus` - Keyboard focus state

---

## Architecture

FlexUI follows a clean separation of concerns:

1. **Widget Factory** (`FlexWidgetFactory`) - Creates widget DOM structure
2. **Controllers** (`Flex*` classes) - Handle interactivity and state
3. **Document** (`FlexDocument`) - Manages the widget tree
4. **CSS** - Handles all visual styling

This architecture allows:
- Easy widget creation with factory methods
- Flexible styling without code changes
- Reusable interactive behaviors
- Clean separation of structure, behavior, and presentation
