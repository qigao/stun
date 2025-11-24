# DDF Expression Language Guide

## Overview

The DDF Expression Language allows you to create dynamic, data-driven diagrams by embedding expressions in shape properties, styles, and text. Expressions are evaluated at runtime and can access data, perform calculations, and apply transformations.

## Table of Contents

1. [Basic Syntax](#basic-syntax)
2. [Variables](#variables)
3. [Operators](#operators)
4. [Filters](#filters)
5. [Functions](#functions)
6. [Advanced Usage](#advanced-usage)
7. [Examples](#examples)

## Basic Syntax

### Expression Delimiters

Expressions are enclosed in double curly braces:

```
{{expression}}
```

### Simple Examples

```json
{
  "text": "{{data.name}}",
  "fill": "{{data.color}}",
  "width": "{{data.count * 10}}"
}
```

### Literal Values

You can mix expressions with literal text:

```json
{
  "text": "Name: {{data.name}}, Age: {{data.age}}"
}
```

## Variables

### Data Variables

Access properties from bound data nodes:

```
{{data.name}}           // Data node property
{{data.properties.age}} // Nested property
```

### Parent Variables

Access parent shape properties:

```
{{parent.width}}        // Parent's width
{{parent.x}}            // Parent's x position
{{parent.style.fill}}   // Parent's fill color
```

### Index Variable

Access the current index in iterations:

```
{{index}}               // Current index (0-based)
{{index + 1}}           // 1-based index
```

### Canvas Variables

Access canvas dimensions and properties:

```
{{canvas.width}}        // Canvas width
{{canvas.height}}       // Canvas height
{{canvas.zoom}}         // Current zoom level
```

### Selection Variables

Access selection state:

```
{{selected}}            // Boolean: is this shape selected?
{{selection.count}}     // Number of selected shapes
```

### Built-in Constants

```
{{PI}}                  // Math.PI (3.14159...)
{{E}}                   // Math.E (2.71828...)
{{true}}                // Boolean true
{{false}}               // Boolean false
{{null}}                // Null value
```

## Operators

### Arithmetic Operators

```
{{a + b}}               // Addition
{{a - b}}               // Subtraction
{{a * b}}               // Multiplication
{{a / b}}               // Division
{{a % b}}               // Modulo (remainder)
{{a ** b}}              // Exponentiation
```

### Comparison Operators

```
{{a == b}}              // Equal
{{a != b}}              // Not equal
{{a > b}}               // Greater than
{{a < b}}               // Less than
{{a >= b}}              // Greater than or equal
{{a <= b}}              // Less than or equal
```

### Logical Operators

```
{{a && b}}              // Logical AND
{{a || b}}              // Logical OR
{{!a}}                  // Logical NOT
```

### Ternary Operator

```
{{condition ? true_value : false_value}}
```

Examples:

```
{{data.status == "online" ? "#27ae60" : "#e74c3c"}}
{{data.count > 10 ? "Many" : "Few"}}
{{selected ? 3 : 1}}
```

### String Concatenation

```
{{data.firstName + " " + data.lastName}}
{{"Total: " + data.count}}
```

### Operator Precedence

From highest to lowest:

1. `()` - Parentheses
2. `**` - Exponentiation
3. `!` - Logical NOT
4. `*`, `/`, `%` - Multiplication, Division, Modulo
5. `+`, `-` - Addition, Subtraction
6. `>`, `<`, `>=`, `<=` - Comparison
7. `==`, `!=` - Equality
8. `&&` - Logical AND
9. `||` - Logical OR
10. `? :` - Ternary

## Filters

Filters transform values using the pipe operator `|`.

### Basic Filter Syntax

```
{{value | filter}}
{{value | filter(arg1, arg2)}}
```

### String Filters

#### uppercase

Convert to uppercase:

```
{{data.name | uppercase}}
// "john doe" → "JOHN DOE"
```

#### lowercase

Convert to lowercase:

```
{{data.name | lowercase}}
// "JOHN DOE" → "john doe"
```

#### capitalize

Capitalize first letter:

```
{{data.name | capitalize}}
// "john doe" → "John doe"
```

#### truncate

Truncate to specified length:

```
{{data.description | truncate(20)}}
// "This is a long description" → "This is a long desc..."
```

#### trim

Remove leading/trailing whitespace:

```
{{data.name | trim}}
// "  John  " → "John"
```

### Number Filters

#### round

Round to nearest integer:

```
{{data.value | round}}
// 3.7 → 4
```

#### floor

Round down:

```
{{data.value | floor}}
// 3.7 → 3
```

#### ceil

Round up:

```
{{data.value | ceil}}
// 3.2 → 4
```

#### abs

Absolute value:

```
{{data.value | abs}}
// -5 → 5
```

#### fixed

Format to fixed decimal places:

```
{{data.value | fixed(2)}}
// 3.14159 → "3.14"
```

### Color Filters

#### color_map

Map values to colors:

```
{{data.department | color_map}}
// "Engineering" → "#3498db"
// "Sales" → "#2ecc71"
// "Marketing" → "#e74c3c"
```

#### lighten

Lighten a color:

```
{{data.color | lighten(20)}}
// "#3498db" → "#5dade2"
```

#### darken

Darken a color:

```
{{data.color | darken(20)}}
// "#3498db" → "#2874a6"
```

#### opacity

Set color opacity:

```
{{data.color | opacity(0.5)}}
// "#3498db" → "rgba(52, 152, 219, 0.5)"
```

### Date Filters

#### format

Format date:

```
{{data.created | format("YYYY-MM-DD")}}
// Date object → "2025-10-19"

{{data.created | format("MMM DD, YYYY")}}
// Date object → "Oct 19, 2025"
```

#### relative

Relative time:

```
{{data.created | relative}}
// Date object → "2 hours ago"
```

### Array Filters

#### length

Get array length:

```
{{data.items | length}}
// [1, 2, 3] → 3
```

#### join

Join array elements:

```
{{data.tags | join(", ")}}
// ["tag1", "tag2"] → "tag1, tag2"
```

#### first

Get first element:

```
{{data.items | first}}
// [1, 2, 3] → 1
```

#### last

Get last element:

```
{{data.items | last}}
// [1, 2, 3] → 3
```

### Utility Filters

#### default

Provide default value:

```
{{data.name | default("Unknown")}}
// null → "Unknown"
// "John" → "John"
```

#### json

Convert to JSON string:

```
{{data.object | json}}
// {a: 1} → '{"a":1}'
```

### Chaining Filters

You can chain multiple filters:

```
{{data.name | uppercase | truncate(10)}}
// "john doe smith" → "JOHN DOE S..."

{{data.value | abs | round | fixed(2)}}
// -3.7 → "4.00"
```

## Functions

### Math Functions

```
{{sqrt(data.value)}}        // Square root
{{pow(data.value, 2)}}      // Power
{{min(a, b, c)}}            // Minimum value
{{max(a, b, c)}}            // Maximum value
{{random()}}                // Random number 0-1
{{random(min, max)}}        // Random number in range
```

### String Functions

```
{{length(data.name)}}       // String length
{{substring(data.name, 0, 5)}} // Substring
{{indexOf(data.name, "o")}} // Index of character
{{replace(data.name, "a", "b")}} // Replace
```

### Conditional Functions

```
{{if(condition, true_val, false_val)}}
{{coalesce(val1, val2, val3)}}  // First non-null value
```

### Type Conversion

```
{{toNumber(data.value)}}    // Convert to number
{{toString(data.value)}}    // Convert to string
{{toBoolean(data.value)}}   // Convert to boolean
```

## Advanced Usage

### Nested Expressions

```
{{data.items[index].name}}
{{parent.children[0].width}}
```

### Complex Conditions

```
{{(data.status == "online" && data.load < 80) ? "#27ae60" : "#e74c3c"}}
```

### Mathematical Formulas

```
{{(data.value - min) / (max - min) * 100}}
{{sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2))}}
```

### Dynamic Positioning

```json
{
  "geometry": {
    "x": "{{index * 150 + 50}}",
    "y": "{{parent.y + 100}}",
    "width": "{{data.count * 10}}",
    "height": "{{max(data.value, 20)}}"
  }
}
```

### Conditional Styling

```json
{
  "style": {
    "fill": "{{data.value > 100 ? '#e74c3c' : '#3498db'}}",
    "stroke-width": "{{selected ? 3 : 1}}",
    "opacity": "{{data.enabled ? 1 : 0.5}}"
  }
}
```

### Data-Driven Text

```json
{
  "text": "{{data.name | uppercase}} ({{data.count}} items)"
}
```

### Responsive Sizing

```json
{
  "geometry": {
    "width": "{{canvas.width < 600 ? 100 : 150}}",
    "font-size": "{{canvas.zoom > 1 ? 14 : 12}}"
  }
}
```

## Examples

### Example 1: Status Indicator

```json
{
  "shapes": [
    {
      "type": "circle",
      "geometry": {
        "cx": 10,
        "cy": 10,
        "r": 5
      },
      "style": {
        "fill": "{{data.status == 'online' ? '#27ae60' : data.status == 'offline' ? '#e74c3c' : '#f39c12'}}"
      }
    }
  ]
}
```

### Example 2: Progress Bar

```json
{
  "shapes": [
    {
      "type": "rect",
      "geometry": {
        "x": 0,
        "y": 0,
        "width": "{{data.progress * 200}}",
        "height": 20
      },
      "style": {
        "fill": "{{data.progress < 0.5 ? '#e74c3c' : data.progress < 0.8 ? '#f39c12' : '#27ae60'}}"
      }
    }
  ]
}
```

### Example 3: Dynamic Grid

```json
{
  "shapes": [
    {
      "type": "rect",
      "geometry": {
        "x": "{{(index % 4) * 150}}",
        "y": "{{floor(index / 4) * 100}}",
        "width": 120,
        "height": 80
      }
    }
  ]
}
```

### Example 4: Temperature Color Map

```json
{
  "style": {
    "fill": "{{data.temp < 0 ? '#3498db' : data.temp < 20 ? '#2ecc71' : data.temp < 30 ? '#f39c12' : '#e74c3c'}}"
  }
}
```

### Example 5: Formatted Label

```json
{
  "text": "{{data.name | capitalize}} - {{data.value | fixed(2)}}% ({{data.status | uppercase}})"
}
```

### Example 6: Conditional Visibility

```json
{
  "style": {
    "opacity": "{{data.visible && canvas.zoom > 0.5 ? 1 : 0}}"
  }
}
```

## Best Practices

1. **Keep It Simple**: Avoid overly complex expressions
2. **Use Filters**: Leverage filters for common transformations
3. **Default Values**: Always provide defaults for optional data
4. **Type Safety**: Be aware of data types in comparisons
5. **Performance**: Cache expensive calculations
6. **Readability**: Break complex expressions into multiple properties
7. **Testing**: Test expressions with various data values

## Common Patterns

### Safe Property Access

```
{{data.user.name | default("Unknown")}}
```

### Percentage Calculation

```
{{(data.value / data.total * 100) | fixed(1)}}%
```

### Conditional Class

```
{{data.active ? "active" : "inactive"}}
```

### Range Mapping

```
{{(data.value - min) / (max - min) * targetRange + targetMin}}
```

### Color Interpolation

```
{{data.value > 50 ? '#e74c3c' : '#3498db'}}
```

## Troubleshooting

### Expression Not Evaluating

1. Check syntax (double curly braces)
2. Verify variable names
3. Check for typos
4. Ensure data is available

### Unexpected Results

1. Check operator precedence
2. Verify data types
3. Use parentheses for clarity
4. Test with simple values first

### Performance Issues

1. Avoid complex nested expressions
2. Cache calculated values
3. Use simpler filters
4. Minimize expression count

## Next Steps

- Review [User Guide](USER_GUIDE.md)
- Learn about [CSS Styling](CSS_STYLING.md)
- Study [Component System](COMPONENTS.md)
- Explore [Examples](../../examples/ddf/)
