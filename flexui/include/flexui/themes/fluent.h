#pragma once

namespace flexui {
namespace themes {
namespace fluent {

inline const char* css = R"(
/* ============================================================================
   Fluent UI 2 Theme for FlexUI
   Microsoft's design system with vibrant colors and smooth transitions
   ============================================================================ */

:root {
    /* Brand Colors */
    --brand: #0078d4;
    --brand-hover: #106ebe;
    --brand-active: #005a9e;
    
    /* Neutral Colors */
    --bg-page: #fafafa;
    --bg-card: #ffffff;
    --bg-hover: #f5f5f5;
    --bg-active: #ebebeb;
    
    /* Text Colors */
    --text-primary: #323130;
    --text-secondary: #605e5c;
    --text-disabled: #a19f9d;
    
    /* Border Colors */
    --stroke: #e1dfdd;
    --stroke-focus: #0078d4;
    
    /* Status Colors */
    --success: #107c10;
    --success-bg: #dff6dd;
    --warning: #ffaa44;
    --warning-bg: #fff4ce;
    --error: #d13438;
    --error-bg: #fde7e9;
    --info: #0078d4;
    --info-bg: #cce4f6;
    
    /* Sizing */
    --radius: 4px;
    --radius-lg: 8px;
    
    /* Spacing */
    --space-xs: 4px;
    --space-sm: 8px;
    --space-md: 12px;
    --space-lg: 16px;
    --space-xl: 24px;
    --space-2xl: 32px;
}

/* ============================================================================
   Card Component
   ============================================================================ */
.card {
    display: flex;
    flex-direction: column;
    background: var(--bg-card);
    border-radius: var(--radius-lg);
    border: 1px solid var(--stroke);
    box-shadow: 0 2px 4px rgba(0,0,0,0.04), 0 0 2px rgba(0,0,0,0.06);
    transition: box-shadow 0.2s ease-out;
}

.card:hover {
    box-shadow: 0 4px 8px rgba(0,0,0,0.08), 0 0 4px rgba(0,0,0,0.08);
}

.card-header {
    display: flex;
    flex-direction: column;
    padding: var(--space-lg);
    gap: var(--space-xs);
}

.card-title {
    font-size: 16px;
    font-weight: 600;
    color: var(--text-primary);
}

.card-description {
    font-size: 13px;
    color: var(--text-secondary);
}

.card-content {
    display: flex;
    flex-direction: column;
    padding: 0 var(--space-lg) var(--space-lg) var(--space-lg);
    gap: var(--space-lg);
}

.card-footer {
    display: flex;
    flex-direction: row;
    padding: var(--space-md) var(--space-lg);
    gap: var(--space-sm);
    border-top: 1px solid var(--stroke);
    background: var(--bg-hover);
}

/* ============================================================================
   Stat Card (Colored variant)
   ============================================================================ */
.stat-card {
    display: flex;
    flex-direction: column;
    padding: var(--space-lg);
    background: #e3f2fd;
    border-radius: var(--radius-lg);
    border: 2px solid #1976d2;
    gap: var(--space-sm);
    transition: background 0.2s ease-out, border-color 0.2s ease-out;
}

.stat-card:hover {
    background: #bbdefb;
    border-color: #0d47a1;
}

.stat-label {
    font-size: 13px;
    color: #1565c0;
}

.stat-value {
    font-size: 28px;
    font-weight: 600;
    color: #0d47a1;
}

.stat-change {
    font-size: 12px;
}

.change-positive {
    color: var(--success);
    background: var(--success-bg);
    padding: 2px 8px;
    border-radius: var(--radius);
}

.change-negative {
    color: var(--error);
    background: var(--error-bg);
    padding: 2px 8px;
    border-radius: var(--radius);
}

/* ============================================================================
   Action Card (Orange variant)
   ============================================================================ */
.action-card {
    display: flex;
    flex-direction: column;
    padding: var(--space-lg);
    background: #fff3e0;
    border-radius: var(--radius-lg);
    border: 2px solid #ff9800;
    gap: var(--space-md);
    transition: background 0.2s ease-out, border-color 0.2s ease-out;
}

.action-card:hover {
    background: #ffe0b2;
    border-color: #e65100;
}

.action-title {
    font-size: 15px;
    font-weight: 600;
    color: #e65100;
}

.action-desc {
    font-size: 13px;
    color: #bf360c;
}

/* ============================================================================
   Button Component
   ============================================================================ */
.btn {
    height: 36px;
    padding: 0 16px;
    border-radius: var(--radius);
    font-size: 14px;
    font-weight: 600;
    transition: background 0.15s ease-out, border-color 0.15s ease-out;
}

.btn-primary {
    background: var(--brand);
    color: white;
}
.btn-primary:hover {
    background: var(--brand-hover);
}
.btn-primary:active {
    background: var(--brand-active);
}

.btn-secondary {
    background: var(--bg-hover);
    color: var(--text-primary);
}
.btn-secondary:hover {
    background: var(--bg-active);
}

.btn-outline {
    background: transparent;
    color: var(--brand);
    border: 1px solid var(--stroke);
}
.btn-outline:hover {
    background: var(--bg-hover);
    border-color: var(--brand);
}

.btn-ghost {
    background: transparent;
    color: var(--text-primary);
}
.btn-ghost:hover {
    background: var(--bg-hover);
}

.btn-danger {
    background: var(--error);
    color: white;
}
.btn-danger:hover {
    background: #c62828;
}

.btn-sm {
    height: 28px;
    padding: 0 12px;
    font-size: 12px;
}

.btn-lg {
    height: 44px;
    padding: 0 24px;
    font-size: 15px;
}

/* ============================================================================
   Input Component
   ============================================================================ */
.input {
    height: 36px;
    width: 100%;
    padding: 8px 12px;
    background: var(--bg-card);
    border: 1px solid var(--stroke);
    border-radius: var(--radius);
    font-size: 14px;
    color: var(--text-primary);
    transition: border-color 0.2s ease-out, box-shadow 0.2s ease-out;
}

.input:hover {
    border-color: var(--text-secondary);
}

.input:focus {
    border-color: var(--brand);
    box-shadow: 0 0 0 1px var(--brand);
}

.input-error {
    border-color: var(--error);
}

.input-error:focus {
    box-shadow: 0 0 0 1px var(--error);
}

/* Purple themed input */
.form-input {
    height: 40px;
    width: 100%;
    padding: 8px 12px;
    background: #f3e5f5;
    border: 2px solid #9c27b0;
    border-radius: var(--radius);
    font-size: 14px;
    color: #4a148c;
    transition: border-color 0.2s ease-out, background 0.2s ease-out;
}

.form-input:hover {
    border-color: #7b1fa2;
}

.form-input:focus {
    border-color: #7b1fa2;
    border-width: 3px;
    background: #e1bee7;
}

/* ============================================================================
   Label Component
   ============================================================================ */
.label {
    font-size: 14px;
    font-weight: 400;
    color: var(--text-primary);
}

.label-secondary {
    color: var(--text-secondary);
}

.form-label {
    font-size: 14px;
    font-weight: 600;
    color: #4a148c;
}

/* ============================================================================
   Badge / Persona Component
   ============================================================================ */
.badge {
    display: inline-flex;
    align-items: center;
    height: 20px;
    padding: 0 8px;
    border-radius: 10px;
    font-size: 11px;
    font-weight: 600;
}

.badge-primary {
    background: var(--brand);
    color: white;
}

.badge-success {
    background: var(--success-bg);
    color: var(--success);
}

.badge-warning {
    background: var(--warning-bg);
    color: #996600;
}

.badge-error {
    background: var(--error-bg);
    color: var(--error);
}

.badge-neutral {
    background: var(--bg-hover);
    color: var(--text-secondary);
}

/* ============================================================================
   Avatar / Persona Component
   ============================================================================ */
.avatar {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 40px;
    height: 40px;
    border-radius: 50%;
    background: var(--brand);
    color: white;
    font-size: 14px;
    font-weight: 600;
}

.avatar-sm {
    width: 28px;
    height: 28px;
    font-size: 11px;
}

.avatar-lg {
    width: 56px;
    height: 56px;
    font-size: 20px;
}

/* ============================================================================
   Message Bar / Alert Component
   ============================================================================ */
.message-bar {
    display: flex;
    flex-direction: row;
    padding: var(--space-md) var(--space-lg);
    border-radius: var(--radius);
    gap: var(--space-sm);
    align-items: center;
}

.message-bar-info {
    background: var(--info-bg);
    border-left: 4px solid var(--info);
}

.message-bar-success {
    background: var(--success-bg);
    border-left: 4px solid var(--success);
}

.message-bar-warning {
    background: var(--warning-bg);
    border-left: 4px solid var(--warning);
}

.message-bar-error {
    background: var(--error-bg);
    border-left: 4px solid var(--error);
}

.message-bar-text {
    font-size: 13px;
    color: var(--text-primary);
}

/* ============================================================================
   Table Component
   ============================================================================ */
.table-container {
    display: flex;
    flex-direction: column;
    background: #e8f5e9;
    border-radius: var(--radius-lg);
    border: 2px solid #4caf50;
    padding: var(--space-lg);
    gap: var(--space-lg);
    transition: border-color 0.2s ease-out;
}

.table-container:hover {
    border-color: #2e7d32;
}

.table-row {
    display: flex;
    flex-direction: row;
    width: 100%;
    height: 48px;
    padding: 0 12px;
    border-bottom: 1px solid #81c784;
    align-items: center;
    background: #c8e6c9;
    transition: background 0.15s ease-out;
}

.table-row:hover {
    background: #a5d6a7;
}

.table-header {
    font-weight: 600;
    color: #2e7d32;
    background: #a5d6a7;
}

.table-cell {
    flex-grow: 1;
    font-size: 14px;
    color: #1b5e20;
}

/* ============================================================================
   Header / Navigation
   ============================================================================ */
.header {
    display: flex;
    flex-direction: row;
    width: 100%;
    height: 48px;
    background: var(--bg-card);
    border-bottom: 1px solid var(--stroke);
    align-items: center;
    padding: 0 var(--space-lg);
}

.nav-item {
    height: 48px;
    padding: 0 var(--space-lg);
    font-size: 14px;
    color: var(--text-secondary);
    transition: color 0.15s ease-out, background 0.15s ease-out;
}

.nav-item:hover {
    color: var(--text-primary);
    background: var(--bg-hover);
}

.nav-item-active {
    color: var(--brand);
    border-bottom: 2px solid var(--brand);
}

/* ============================================================================
   Form Utilities
   ============================================================================ */
.form-group {
    display: flex;
    flex-direction: column;
    gap: var(--space-sm);
    width: 100%;
}

.form-row {
    display: flex;
    flex-direction: row;
    gap: var(--space-lg);
    width: 100%;
}

/* ============================================================================
   Layout Utilities
   ============================================================================ */
.flex { display: flex; }
.flex-col { flex-direction: column; }
.flex-row { flex-direction: row; }
.flex-1 { flex: 1; }
.items-center { align-items: center; }
.justify-center { justify-content: center; }
.justify-between { justify-content: space-between; }

.gap-xs { gap: var(--space-xs); }
.gap-sm { gap: var(--space-sm); }
.gap-md { gap: var(--space-md); }
.gap-lg { gap: var(--space-lg); }
.gap-xl { gap: var(--space-xl); }

.p-sm { padding: var(--space-sm); }
.p-md { padding: var(--space-md); }
.p-lg { padding: var(--space-lg); }
.p-xl { padding: var(--space-xl); }

/* ============================================================================
   Grid Layout (CSS Grid)
   ============================================================================ */
.grid {
    display: grid;
    gap: var(--space-lg);
}

.grid-cols-2 {
    grid-template-columns: 1fr 1fr;
}

.grid-cols-3 {
    grid-template-columns: 1fr 1fr 1fr;
}

.grid-cols-4 {
    grid-template-columns: 1fr 1fr 1fr 1fr;
}

/* ============================================================================
   Typography
   ============================================================================ */
.text-xs { font-size: 11px; }
.text-sm { font-size: 13px; }
.text-base { font-size: 14px; }
.text-lg { font-size: 16px; }
.text-xl { font-size: 20px; }
.text-2xl { font-size: 24px; }

.font-regular { font-weight: 400; }
.font-semibold { font-weight: 600; }
.font-bold { font-weight: 700; }

.text-primary { color: var(--text-primary); }
.text-secondary { color: var(--text-secondary); }
.text-brand { color: var(--brand); }
.text-success { color: var(--success); }
.text-error { color: var(--error); }

/* ============================================================================
   Animation Utilities
   ============================================================================ */
@keyframes pulse {
    0% { opacity: 1; }
    50% { opacity: 0.6; }
    100% { opacity: 1; }
}

@keyframes shimmer {
    0% { background-position: -200% 0; }
    100% { background-position: 200% 0; }
}

.skeleton {
    background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
    background-size: 200% 100%;
    animation: shimmer 1.5s infinite;
    border-radius: var(--radius);
}

.loading {
    animation: pulse 1.2s ease-in-out infinite;
}
)";

} // namespace fluent
} // namespace themes
} // namespace flexui
