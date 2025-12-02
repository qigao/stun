# Tailwind CSS Examples

This directory contains example applications demonstrating Tailwind CSS-style functionality in FlexUI.

## 🎨 Examples Overview

### 1. **tailwind_button_gallery.cpp** - Button Components Gallery
**Features Demonstrated:**
- ✅ **CSS Variables** - Complete color palette using `:root` variables
- ✅ **Atomic Utility Classes** - Tailwind-style utilities (`.flex`, `.gap-4`, `.px-6`, etc.)
- ✅ **Pseudo-class Variants** - `:hover`, `:active`, `:focus` states
- ✅ **Responsive Design** - `@media` queries for mobile/tablet/desktop layouts
- ✅ **Semantic Color System** - Primary, Success, Danger, Warning buttons
- ✅ **Component Variants** - Solid, Outline, and Ghost button styles

**Key Classes:**
```css
/* Color Tokens */
--blue-500, --green-500, --red-500, --yellow-500

/* Layout */
.flex, .flex-col, .gap-4

/* Buttons with States */
.btn-primary:hover { background-color: var(--blue-600); }
.btn-primary:active { background-color: var(--blue-700); }

/* Responsive */
@media (min-width: 768px) {
    .button-group { flex-direction: row; }
}
```

---

### 2. **tailwind_card_dashboard.cpp** - Card & Dashboard Layout
**Features Demonstrated:**
- ✅ **Card Components** - Reusable card patterns with hover effects
- ✅ **Responsive Grid** - Mobile-first 1/2/4 column layouts
- ✅ **Design Tokens** - Consistent spacing and color system
- ✅ **Typography Scale** - From `.text-xs` to `.text-3xl`
- ✅ **Stat Cards** - Dashboard metrics components
- ✅ **Badge Components** - Small status indicators

**Key Patterns:**
```css
/* Card with Hover */
.card {
    background: var(--white);
    border: 1px solid var(--gray-300);
    border-radius: 12px;
}
.card:hover {
    border-color: var(--blue-500);
}

/* Responsive Grid */
.stats-grid { flex-direction: column; }

@media (min-width: 768px) {
    .stats-grid { flex-direction: row; }
    .stat-card { width: 48%; }
}

@media (min-width: 1024px) {
    .stat-card { width: 23%; }
}
```

---

### 3. **tailwind_form_demo.cpp** - Form Components & Validation
**Features Demonstrated:**
- ✅ **Form Inputs** - Styled input fields with focus states
- ✅ **Validation States** - Error styling and helper text
- ✅ **Label Patterns** - Required field indicators
- ✅ **Button States** - Primary/Secondary with hover/active
- ✅ **Responsive Forms** - Stack on mobile, side-by-side on desktop
- ✅ **JavaScript Integration** - Client-side validation

**Key Patterns:**
```css
/* Input with Focus State */
.input {
    border: 1px solid var(--gray-300);
}
.input:focus {
    border-color: var(--blue-500);
    border-width: 2px;
}

/* Error State */
.input-error {
    border-color: var(--red-500);
}

/* Required Label */
.label-required::after {
    content: " *";
    color: var(--red-500);
}
```

---

## 🚀 Running the Examples

### Build All Examples
```bash
cd build/Ninja/Msvc
ninja                           # Build all targets including examples
```

### Run Specific Example
```bash
cd build/Ninja/Msvc/bin
./tailwind_button_gallery.exe
./tailwind_card_dashboard.exe
./tailwind_form_demo.exe
```

---

## 📚 Implemented Tailwind CSS Features

### ✅ Core Features
| Feature | Status | Description |
|---------|--------|-------------|
| **CSS Variables** | ✅ Fully Implemented | `:root { --color: value; }` with `var()` references |
| **Variable Chaining** | ✅ Fully Implemented | `--blue: var(--primary)` with cycle detection |
| **Atomic Utilities** | ✅ Fully Implemented | `.w-20`, `.p-4`, `.flex`, `.gap-6`, etc. |
| **Pseudo-classes** | ✅ Fully Implemented | `:hover`, `:focus`, `:active`, `:disabled` |
| **@media Queries** | ✅ Fully Implemented | Responsive breakpoints (sm, md, lg) |
| **Escaped Classes** | ✅ Fully Implemented | `.focus\:ring-2` → class name `"focus:ring-2"` |

### ✅ Utility Categories
- **Layout:** `flex`, `flex-col`, `flex-row`, `gap-*`
- **Sizing:** `w-*`, `h-*`, `min-w-*`, `max-w-*`
- **Spacing:** `p-*`, `px-*`, `py-*`, `m-*`, `mb-*`
- **Typography:** `text-*`, `font-*`
- **Colors:** `text-*`, `bg-*`, `border-*`
- **Borders:** `rounded-*`, `border-*`

### ✅ Component Patterns
- **Buttons:** Primary, Secondary, Outline, Ghost
- **Cards:** Basic, Hover, Stats
- **Forms:** Inputs, Labels, Validation
- **Badges:** Status indicators
- **Layouts:** Grid, Flex, Responsive

---

## 🎓 Learning Path

### Beginner: Start Here
1. **tailwind_button_gallery.cpp** - Learn basic utilities and states
2. Review CSS variables and color tokens
3. Experiment with hover effects

### Intermediate
1. **tailwind_card_dashboard.cpp** - Learn layout patterns
2. Understand responsive breakpoints
3. Study component composition

### Advanced
1. **tailwind_form_demo.cpp** - Master complex components
2. Add JavaScript interactivity
3. Implement custom validation

---

## 💡 Tips & Best Practices

### 1. **Use CSS Variables for Consistency**
```css
:root {
    --blue-500: #3b82f6;
    --spacing-4: 16px;
}
.btn { background: var(--blue-500); }
```

### 2. **Mobile-First Responsive Design**
```css
/* Mobile (default) */
.container { flex-direction: column; }

/* Tablet and up */
@media (min-width: 768px) {
    .container { flex-direction: row; }
}
```

### 3. **Compose Utility Classes**
```xml
<button class="btn-primary px-6 py-3 rounded-lg font-medium">
    Submit
</button>
```

### 4. **Leverage Pseudo-classes**
```css
.btn:hover { background-color: var(--blue-600); }
.input:focus { border-color: var(--blue-500); }
```

---

## 🧪 Testing

All features demonstrated in these examples are covered by unit tests:
```bash
cd build/Ninja/Msvc/bin
./test_nanovg_css_tailwind.exe
```

**Test Coverage:**
- ✅ 10/10 test cases passing
- ✅ 83/83 assertions passing
- ✅ CSS variable resolution (including chains)
- ✅ Pseudo-class selectors
- ✅ @media query evaluation
- ✅ Responsive behavior

---

## 📖 Reference

### Tailwind CSS Documentation
- [Tailwind CSS Official Docs](https://tailwindcss.com/docs)
- [Utility-First Fundamentals](https://tailwindcss.com/docs/utility-first)
- [Responsive Design](https://tailwindcss.com/docs/responsive-design)

### FlexUI-Specific
- All examples use pure CSS (no build step required)
- JavaScript integration via `loadJS()` for interactivity
- XML-based UI structure with Tailwind class composition

---

## 🔧 Customization

### Adding Custom Colors
```cpp
screen.loadCSS(R"(
    :root {
        --brand-primary: #FF6B6B;
        --brand-secondary: #4ECDC4;
    }
    .btn-brand {
        background-color: var(--brand-primary);
        color: var(--white);
    }
)");
```

### Creating Custom Components
```cpp
screen.loadCSS(R"(
    .my-card {
        background: var(--white);
        padding: var(--spacing-6);
        border-radius: 12px;
        border: 1px solid var(--gray-300);
    }
    .my-card:hover {
        border-color: var(--blue-500);
        transform: translateY(-2px);
    }
)");
```

---

## 🎯 Next Steps

1. **Experiment** - Modify the examples to create your own designs
2. **Combine** - Mix utility classes to create new components
3. **Extend** - Add more color tokens and spacing values
4. **Optimize** - Use CSS variables for theme switching

Happy coding! 🚀
