#pragma once

namespace flexui {
namespace themes {
namespace shadcn {

inline const char* css = R"(
/* ============================================================================
   shadcn/ui Theme for FlexUI
   A clean, minimal design system inspired by shadcn/ui
   ============================================================================ */

:root {
    /* Colors */
    --background: #ffffff;
    --foreground: #0a0a0a;
    --card: #ffffff;
    --card-foreground: #0a0a0a;
    --primary: #18181b;
    --primary-foreground: #fafafa;
    --secondary: #f4f4f5;
    --secondary-foreground: #18181b;
    --muted: #f4f4f5;
    --muted-foreground: #71717a;
    --accent: #f4f4f5;
    --accent-foreground: #18181b;
    --destructive: #ef4444;
    --destructive-foreground: #fafafa;
    --border: #e4e4e7;
    --input: #e4e4e7;
    --ring: #18181b;
    
    /* Sizing */
    --radius: 6px;
    --radius-sm: 4px;
    --radius-lg: 8px;
    
    /* Spacing */
    --space-1: 4px;
    --space-2: 8px;
    --space-3: 12px;
    --space-4: 16px;
    --space-5: 20px;
    --space-6: 24px;
    --space-8: 32px;
}

/* ============================================================================
   Card Component
   ============================================================================ */
.card {
    display: flex;
    flex-direction: column;
    background: var(--card);
    border-radius: var(--radius);
    border: 1px solid var(--border);
    box-shadow: 0 1px 3px rgba(0,0,0,0.08);
}

.card-header {
    display: flex;
    flex-direction: column;
    padding: var(--space-5);
    gap: var(--space-1);
}

.card-title {
    font-size: 16px;
    font-weight: 600;
    color: var(--card-foreground);
}

.card-description {
    font-size: 13px;
    color: var(--muted-foreground);
}

.card-content {
    display: flex;
    flex-direction: column;
    padding: 0 var(--space-5) var(--space-5) var(--space-5);
    gap: var(--space-4);
}

.card-footer {
    display: flex;
    flex-direction: row;
    padding: var(--space-4) var(--space-5);
    gap: var(--space-3);
    border-top: 1px solid var(--border);
    background: var(--muted);
}

/* ============================================================================
   Button Component
   ============================================================================ */
.btn {
    height: 36px;
    padding: 0 16px;
    border-radius: var(--radius);
    font-size: 13px;
    font-weight: 500;
    transition: background-color 0.15s, opacity 0.15s;
}

.btn-default {
    background: var(--primary);
    color: var(--primary-foreground);
}
.btn-default:hover {
    background: #27272a;
}

.btn-secondary {
    background: var(--secondary);
    color: var(--secondary-foreground);
}
.btn-secondary:hover {
    background: #e4e4e7;
}

.btn-destructive {
    background: var(--destructive);
    color: var(--destructive-foreground);
}
.btn-destructive:hover {
    background: #dc2626;
}

.btn-outline {
    background: var(--background);
    color: var(--foreground);
    border: 1px solid var(--input);
}
.btn-outline:hover {
    background: var(--accent);
}

.btn-ghost {
    background: transparent;
    color: var(--foreground);
}
.btn-ghost:hover {
    background: var(--accent);
}

.btn-link {
    background: transparent;
    color: var(--primary);
    text-decoration: underline;
}

.btn-sm {
    height: 32px;
    padding: 0 12px;
    font-size: 12px;
}

.btn-lg {
    height: 42px;
    padding: 0 24px;
    font-size: 14px;
}

.btn-icon {
    width: 36px;
    padding: 0;
}

/* ============================================================================
   Input Component
   ============================================================================ */
.input {
    height: 36px;
    width: 100%;
    padding: 8px 12px;
    background: var(--background);
    border: 1px solid var(--input);
    border-radius: var(--radius);
    font-size: 13px;
    color: var(--foreground);
    transition: border-color 0.15s;
}

.input:focus {
    border-color: var(--ring);
}

.input:hover {
    border-color: #a1a1aa;
}

.input-error {
    border-color: var(--destructive);
}

/* ============================================================================
   Label Component
   ============================================================================ */
.label {
    font-size: 13px;
    font-weight: 500;
    color: var(--foreground);
}

.label-muted {
    color: var(--muted-foreground);
}

/* ============================================================================
   Badge Component
   ============================================================================ */
.badge {
    display: inline-flex;
    align-items: center;
    height: 22px;
    padding: 0 10px;
    border-radius: 11px;
    font-size: 11px;
    font-weight: 500;
}

.badge-default {
    background: var(--primary);
    color: var(--primary-foreground);
}

.badge-secondary {
    background: var(--secondary);
    color: var(--secondary-foreground);
}

.badge-destructive {
    background: var(--destructive);
    color: var(--destructive-foreground);
}

.badge-outline {
    background: transparent;
    color: var(--foreground);
    border: 1px solid var(--border);
}

/* ============================================================================
   Alert Component
   ============================================================================ */
.alert {
    display: flex;
    flex-direction: column;
    padding: var(--space-4);
    border-radius: var(--radius);
    border: 1px solid var(--border);
    gap: var(--space-1);
}

.alert-default {
    background: var(--background);
}

.alert-destructive {
    background: #fef2f2;
    border-color: #fecaca;
}

.alert-success {
    background: #f0fdf4;
    border-color: #bbf7d0;
}

.alert-warning {
    background: #fffbeb;
    border-color: #fde68a;
}

.alert-title {
    font-size: 13px;
    font-weight: 600;
    color: var(--foreground);
}

.alert-description {
    font-size: 13px;
    color: var(--muted-foreground);
}

.alert-destructive .alert-title {
    color: var(--destructive);
}

.alert-success .alert-title {
    color: #16a34a;
}

.alert-warning .alert-title {
    color: #ca8a04;
}

/* ============================================================================
   Avatar Component
   ============================================================================ */
.avatar {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 40px;
    height: 40px;
    border-radius: 9999px;
    background: var(--muted);
    font-size: 14px;
    font-weight: 500;
    color: var(--muted-foreground);
    overflow: hidden;
}

.avatar-sm {
    width: 32px;
    height: 32px;
    font-size: 12px;
}

.avatar-lg {
    width: 56px;
    height: 56px;
    font-size: 18px;
}

/* ============================================================================
   Separator Component
   ============================================================================ */
.separator {
    width: 100%;
    height: 1px;
    background: var(--border);
}

.separator-vertical {
    width: 1px;
    height: 100%;
    background: var(--border);
}

/* ============================================================================
   Form Utilities
   ============================================================================ */
.form-field {
    display: flex;
    flex-direction: column;
    gap: var(--space-2);
    width: 100%;
}

.form-row {
    display: flex;
    flex-direction: row;
    gap: var(--space-4);
    width: 100%;
}

/* ============================================================================
   Layout Utilities
   ============================================================================ */
.flex { display: flex; }
.flex-col { flex-direction: column; }
.flex-row { flex-direction: row; }
.flex-wrap { flex-wrap: wrap; }
.flex-1 { flex: 1; }
.items-center { align-items: center; }
.justify-center { justify-content: center; }
.justify-between { justify-content: space-between; }

.gap-1 { gap: var(--space-1); }
.gap-2 { gap: var(--space-2); }
.gap-3 { gap: var(--space-3); }
.gap-4 { gap: var(--space-4); }
.gap-6 { gap: var(--space-6); }

.p-2 { padding: var(--space-2); }
.p-4 { padding: var(--space-4); }
.p-6 { padding: var(--space-6); }

/* ============================================================================
   Typography
   ============================================================================ */
.text-xs { font-size: 12px; }
.text-sm { font-size: 13px; }
.text-base { font-size: 14px; }
.text-lg { font-size: 16px; }
.text-xl { font-size: 18px; }
.text-2xl { font-size: 24px; }

.font-normal { font-weight: 400; }
.font-medium { font-weight: 500; }
.font-semibold { font-weight: 600; }
.font-bold { font-weight: 700; }

.text-muted { color: var(--muted-foreground); }
.text-primary { color: var(--foreground); }
.text-destructive { color: var(--destructive); }
)";

} // namespace shadcn
} // namespace themes
} // namespace flexui
