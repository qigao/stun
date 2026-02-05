# Standalone Chart DSL Specification

This document defines the independent declarative Chart DSL, inspired by **D2** and **Vega-Lite**. Files typically use the `.chart` extension.

## 1. Document Structure

A `.chart` file is a collection of root-level properties, data definitions, and mark blocks.

```chart
// Root properties
title: "Global Temperature Trends"
width: 600
height: 400

// Data definition
data climate {
  source: "temp.json"
}

// Mark definitions
line {
  data: climate
  x: year
  y: temp_anomaly
}
```

## 2. Root Properties
These properties apply to the entire chart canvas. By default, child elements use **relative units** (e.g., `50%`) based on these dimensions.

| Property | Type | Description | Default |
|---|---|---|---|
| `title` | String | The main title of the chart. | - |
| `width` | Unit | Overall width (e.g., `800px`, `100%`). | `100%` |
| `height` | Unit | Overall height (e.g., `400px`, `50%`). | `400px` |
| `margin` | Unit/Object | Spacing around drawing area (e.g., `20px`, `5%`). | `0` |
| `theme` | String | Reference to a predefined theme. | `"light"` |

## 3. Data Blocks

Data can be defined once and reused across multiple marks.

### External Source
```chart
data [name] {
  source: "path/to/data.json"
  format: "json" | "csv"
}
```

### Inline Data
```chart
data [name] {
  values: [
    { x: 1, y: 10 },
    { x: 2, y: 20 }
  ]
}
```

## 4. Mark Blocks

Marks represent the visual items on the chart.

### Supported Marks & Encodings

| Mark | Description | Primary Encodings |
|---|---|---|
| `bar` | Rectangular bars | `x`, `y`, `color` |
| `line` | Connected paths | `x`, `y`, `color`, `stroke-width` |
| `point` | Scatter plot dots | `x`, `y`, `color`, `size`, `shape` |
| `area` | Filled regions | `x`, `y`, `y2`, `color` (for stacked) |
| `arc` | Pie/Donut segments | `theta` (value), `color`, `radius` |
| `radar` | Polar area/line | `angle`, `radius`, `color` |
| `rect` | Heatmap/Matrix | `x`, `y`, `color` (value) |
| `rule` | Line segments | `x`, `y`, `x2`, `y2`, `color` |
| `tick` | Thin lines | `x`, `y`, `color` |
| `text` | Text labels | `x`, `y`, `text`, `color`, `size` |
| `trail` | Variable width line | `x`, `y`, `size`, `color` |
| `boxplot`| Composite box plot | `x` (ordinal), `y` (quantitative) |
| `errorbar`| Error bars | `x`, `y`, `y_min`, `y_max` |
| `errorband`| Error bands | `x`, `y`, `y_min`, `y_max` |
| `geoshape`| Geographic maps | `shape` (geojson), `color` |
| `image` | Image marks | `x`, `y`, `url` |

### Syntax
```chart
[mark_type] [optional_name] {
  data: [data_name]
  [encodings]
  [style]
}
```

## 5. Encodings

Channels map data fields to visual attributes.

### Position Channels
- `x`, `y`: Cartesian coordinates.
- `x2`, `y2`: Secondary coordinates for ranges/segments.
- `theta`, `theta2`: Angular positions for arcs.
- `radius`, `radius2`: Radial distances for arcs/radar.
- `angle`: Angular axis for radar charts.
- `latitude`, `longitude`: Geospatial coordinates.

### Visual Attribute Channels
- `color`: Categorical or quantitative color mapping.
- `size`: Quantitative size.
- `shape`: Categorical symbols or GeoJSON shapes.
- `opacity`: Transparency mapping.
- `text`: Label content.
- `url`: Image source for `image` marks.

## 6. View Composition

Combine multiple views into a single visualization.

### Layering
The root level acts as a default layer. Multiple mark blocks create layers.

### Concatenation
Combine independent charts vertically or horizontally.

```chart
vconcat [name] {
  chart_a { ... }
  chart_b { ... }
}

hconcat [name] {
  chart_list: [view1, view2]
  resolve: { scale: { y: "shared" } }
}
```

### Faceting
Partition data into a grid of charts.

```chart
facet [name] {
  data: main_data
  column: "category_field"
  row: "group_field"
  spec: {
    mark: bar
    encoding: { x: month, y: revenue }
  }
}
```

### Repeating
Repeat a chart template with different fields.

```chart
repeat [name] {
  data: main_data
  columns: ["temp", "humidity", "wind"]
  spec: {
    mark: line
    encoding: { x: time, y: { repeat: "column" } }
  }
}
```

## 7. Geospatial & Projections

For mapping and geographic visualizations.

```chart
projection my_map {
  type: "albersUsa" | "mercator" | "equirectangular"
  scale: 1000
  translate: [400, 200]
}

geoshape {
  data: states_geojson
  projection: my_map
  encoding: { color: population }
}
```

### Channel Shorthand
```chart
x: "field_name"
color: "category_field"
```

### Channel Block (for detailed config)
```chart
y: "revenue" {
  type: "quantitative"
  title: "Annual Revenue ($)"
  scale: { nice: true, zero: true }
}
```

## 6. Styles (D2 Style)

Apply styles using blocks or dot-notation within a mark.

```chart
bar {
  style {
    fill: "#4a90e2"
    stroke: "#2a70c2"
    stroke-width: 1
    corner-radius: 4
  }
}

// Or shorthand
line.style.stroke: "red"
```

## 7. Data Transformations (Vega Style)

Data blocks support transformation pipelines to process raw data before rendering.

```chart
data climate {
  source: "temp.json"
  transform: [
    { type: "filter", expr: "datum.value > 0" }
    { type: "aggregate", groupby: "year", op: "mean", field: "temp", as: "avg_temp" }
    { type: "stack", groupby: ["year"], sort: "month", field: "yield" }
  ]
}
```

## 8. Reactive Signals

Signals are dynamic variables that respond to event streams.

```chart
signal hover_id {
  value: null
  on: [
    { events: "rect:mouseover", update: "datum.id" }
    { events: "rect:mouseout", update: "null" }
  ]
}

signal chart_width {
  value: 500
  on: [{ events: "window:resize", update: "containerSize().width" }]
}
```

## 9. Mark Production Rules (States)

Marks can define encodings for different lifecycle states, enabling rich interactivity.

```chart
bar {
  data: climate
  
  // Applied on initial creation
  enter {
    fill: "#ccc"
    opacity: 0
    y: { value: 0 }
  }
  
  // Applied on data/signal updates
  update {
    x: "year"
    y: "avg_temp"
    fill: { signal: "hover_id == datum.id ? 'red' : 'steelblue'" }
    opacity: 1
  }
  
  // Applied on mouse hover
  hover {
    fill: "firebrick"
  }
}
```

## 10. Explicit Scales & Axes (Shared)

For precise control, scales and axes can be defined at the root level and referenced by marks.

```chart
scale color_scale {
  type: "linear"
  domain: [0, 100]
  range: { scheme: "viridis" }
}

axis x_axis {
  orient: "bottom"
  title: "Fiscal Year"
  grid: true
}
```
