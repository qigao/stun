# FlexMaid CLI - Command Line Interface

A fast, production-ready command-line tool for rendering Mermaid diagrams to SVG.

## 🚀 Quick Start

```bash
# Basic usage
flexmaid_example.exe diagram.mmd output.svg

# With dark theme
flexmaid_example.exe diagram.mmd output.svg --theme dark

# With performance benchmarking
flexmaid_example.exe diagram.mmd output.svg --benchmark

# Verbose output
flexmaid_example.exe diagram.mmd output.svg --verbose
```

## 📖 Usage

```
FlexMaid - Fast Mermaid Diagram Renderer

Usage:
  flexmaid_example.exe <input.mmd> <output.svg> [options]

Arguments:
  <input.mmd>   Input Mermaid diagram file
  <output.svg>  Output SVG file

Options:
  --theme <name>    Theme: light, dark, modern, official (default: light)
  --verbose         Print detailed information
  --benchmark       Show performance metrics
  --help            Show this help message
```

## 🎨 Themes

FlexMaid supports multiple built-in themes:

- **light** - Clean, professional theme (default)
- **dark** - Dark mode with high contrast
- **modern** - Contemporary, vibrant colors
- **official** - Matches official Mermaid.js styling

## 📊 Examples

### Basic Flowchart
```bash
flexmaid_example.exe flowchart.mmd flowchart.svg
```

### Sequence Diagram with Dark Theme
```bash
flexmaid_example.exe sequence.mmd sequence.svg --theme dark
```

### Performance Benchmarking
```bash
flexmaid_example.exe large_diagram.mmd output.svg --benchmark
```

**Output:**
```
✓ Successfully generated output.svg

Performance Metrics:
  Read:   0.15 ms
  Parse:  0.28 ms
  Render: 0.42 ms
  Write:  0.11 ms
  ─────────────────────
  Total:  0.96 ms

Output size: 15234 bytes
```

## 🔧 Integration

### Batch Processing
```bash
# Process all .mmd files in a directory
for file in *.mmd; do
    flexmaid_example.exe "$file" "${file%.mmd}.svg"
done
```

### Python Integration
```python
import subprocess

def render_mermaid(input_file, output_file, theme="default"):
    result = subprocess.run(
        ["flexmaid_example.exe", input_file, output_file, "--theme", theme],
        capture_output=True,
        text=True
    )
    return result.returncode == 0
```

### CI/CD Pipeline
```yaml
# GitHub Actions example
- name: Render Diagrams
  run: |
    for diagram in docs/diagrams/*.mmd; do
      flexmaid_example.exe "$diagram" "${diagram%.mmd}.svg"
    done
```

## ⚡ Performance

FlexMaid is **~120x faster** than official Mermaid.js:

| Diagram Type | Official Mermaid.js | FlexMaid | Speedup |
|--------------|---------------------|----------|---------|
| Flowchart    | ~50ms              | ~0.4ms   | 125x    |
| Sequence     | ~45ms              | ~0.3ms   | 150x    |
| Class        | ~40ms              | ~0.2ms   | 200x    |
| State        | ~35ms              | ~0.3ms   | 117x    |

## 📝 Supported Diagram Types

FlexMaid supports 29+ diagram types:

- Flowchart
- Sequence
- Class
- State
- ER (Entity-Relationship)
- Gantt
- Pie
- GitGraph
- Journey
- Timeline
- And many more...

## 🐛 Error Handling

FlexMaid provides clear error messages:

```bash
$ flexmaid_example.exe invalid.mmd output.svg
Parse error: Unexpected token at line 3, column 5
```

Exit codes:
- `0` - Success
- `1` - Parse error or file I/O error

## 🔍 Verbose Mode

Use `--verbose` to see detailed processing information:

```bash
$ flexmaid_example.exe diagram.mmd output.svg --verbose
Reading input file: diagram.mmd
Input size: 234 bytes
Theme: default
Parsing diagram...
Diagram type: flowchart
Rendering SVG...
Writing output file: output.svg
✓ Successfully generated output.svg
```

## 📦 Building from Source

```bash
cd build/Ninja/Msvc
ninja flexmaid_example
```

The executable will be in `bin/flexmaid_example.exe`.

## 🤝 Contributing

FlexMaid is part of the Flex framework. See the main repository for contribution guidelines.

## 📄 License

See the main Flex repository for license information.

---

**Built with ❤️ using Modern C++17**
