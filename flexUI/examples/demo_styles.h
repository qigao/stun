#pragma once

#include <string>

namespace flexUI {
namespace examples {

const std::string DEMO_CSS = R"(
    /* =========================================
       Variables & Theme (Dark Mode)
       ========================================= */
    /* =========================================
       Variables & Theme (Dark Mode)
       ========================================= */
    #root {
        /* Palette: Tailwind Slate & Blue */
        --bg-app: 15, 23, 42, 255;          /* slate-900 */
        --bg-panel: 30, 41, 59, 255;        /* slate-800 */
        --bg-card: 51, 65, 85, 255;         /* slate-700 */
        --bg-input: 15, 23, 42, 255;        /* slate-900 */
        
        --text-main: 248, 250, 252, 255;    /* slate-50 */
        --text-muted: 148, 163, 184, 255;   /* slate-400 */
        
        --primary: 59, 130, 246, 255;       /* blue-500 */
        --primary-hover: 37, 99, 235, 255;  /* blue-600 */
        
        --success: 34, 197, 94, 255;        /* green-500 */
        --danger: 239, 68, 68, 255;         /* red-500 */
        --warning: 234, 179, 8, 255;        /* yellow-500 */
        
        --border: 71, 85, 105, 255;         /* slate-600 */
        
        font-family: NotoSansSC;
        font-size: 14;
        
        /* Layout */
        width: 1200;
        height: 900;
        display: flex;
        flex-direction: row; /* Sidebar + Main */
        --bg: 15, 23, 42, 255; /* Use --bg for background */
    }

    /* =========================================
       Structure: Sidebar & Main
       ========================================= */
    #sidebar {
        width: 260;
        height: 900;
        padding: 20;
        gap: 10;
        display: flex;
        flex-direction: column;
        --bg: 30, 41, 59, 255; /* slate-800 */
        box-shadow: 2 0 10 rgba(0,0,0,0.3);
    }

    #main-content {
        width: 940; 
        height: 900;
        padding: 40;
        gap: 24;
        display: flex;
        flex-direction: column;
        --bg: 15, 23, 42, 255;
    }

    /* header title */
    .brand {
        font-size: 24;
        --text-color: var(--text-main);
        height: 60;
        padding: 0 0 20 0;
        width: 220;
        display: flex;
        align-items: center;
    }

    #view-container {
        width: 860;
        height: 800;
        display: flex;
        position: relative;
    }

    .view-pane {
        width: 860;
        height: 800;
        display: flex;
        flex-direction: column;
        gap: 24;
    }

    /* =========================================
       Components: Cards
       ========================================= */
    .gallery-grid {
        width: 860; /* 940 - 40*2 padding */
        height: 800; /* enough space */
        display: flex;
        flex-direction: row;
        gap: 20;
    }

    .col {
        display: flex;
        flex-direction: column;
        gap: 20;
        width: 420;
    }

    .card {
        padding: 20;
        border-radius: 12;
        gap: 16;
        display: flex;
        flex-direction: column;
        --bg: var(--bg-card);
        box-shadow: 0 4 6 rgba(0,0,0,0.2);
        width: 420; /* Fixed width to avoid layout issues */
    }

    .card-title {
        font-size: 16;
        --text-color: var(--text-muted);
        height: 20;
        width: 380; /* Fixed width */
    }

    .wide-card {
        width: 860;
    }
    
    .card-row {
        display: flex;
        flex-direction: row;
        align-items: center;
        gap: 12;
        height: 40; /* consistent row height */
    }

    /* =========================================
       Widgets: Buttons
       ========================================= */
    button {
        height: 36;
        padding: 0 16 0 16; 
        border-radius: 6;
        font-size: 13;
        --bg: var(--primary);
        --text-color: 255,255,255,255;
        box-shadow: 0 1 2 rgba(0,0,0,0.1);
    }
    
    button.ghost {
        --bg: 255,255,255,10;
        --text-color: var(--text-main);
    }

    button.success { --bg: var(--success); }
    button.danger  { --bg: var(--danger); }
    
    /* =========================================
       Widgets: Form Elements
       ========================================= */
    label {
        font-size: 13;
        --text-color: var(--text-muted);
        width: 100;
        height: 24;
    }

    #backend-status {
        --text-overflow: ellipsis;
    }

    input {
        background-color: var(--bg-input);
        --text-color: var(--text-main);
        border-radius: 6;
        height: 36;
        width: 200;
        padding: 8 12 8 12;
        font-size: 13;
        /* Manually expanding var(--border) to rgba for engine safety */
        box-shadow: 0 0 0 1 rgba(71,85,105,1.0); 
        
        /* Widget specific variables */
        --input-bg: var(--bg-input);
        --input-text: var(--text-main);
    }

    textarea {
        background-color: var(--bg-input);
        color: var(--text-main);
        border-width: 1px;
        border-style: solid;
        border-color: var(--border);
        border-radius: 6;
        padding: 8 12 8 12;
        font-size: 13;
        --textarea-bg: var(--bg-input);
        --textarea-border: var(--border);
        --textarea-text: var(--text-main);
        --textarea-placeholder: var(--text-muted);
    }

    /* =========================================
       Widgets: Complex
       ========================================= */
       
    /* Sidebar Links (using Buttons style) */
    .nav-item {
        width: 220;
        height: 40;
        border-radius: 8;
        --bg: 255,255,255,0; /* transparent */
        --text-color: var(--text-muted);
        display: flex;
        align-items: center;
        padding: 0 12 0 12;
    }
    
    .nav-item.active {
        --bg: var(--primary);
        --text-color: 255,255,255,255;
    }

    divider {
        width: 220; /* Fixed width matching nav items */
        height: 1;
        --bg: var(--border);
        margin: 10 0 10 0;
    }


    /* =========================================
       Widgets: Toggles
       ========================================= */
    checkbox {
        height: 20;
        --checkbox-size: 20;
        border-radius: 4;
        border-width: 1;
        border-color: var(--border);
        background-color: var(--bg-input);
        margin: 0;
    }
    checkbox:checked {
        background-color: var(--primary);
        border-color: var(--primary);
        --text-color: 255,255,255,255;
    }

    switch {
        height: 24;
        border-radius: 12;
        background-color: var(--bg-panel);
        border-width: 1;
        border-color: var(--border);
        transition: all 0.2s;
        
        /* Widget specific variables */
        --switch-width: 44;
        --switch-height: 24;
        --switch-bg-off: var(--bg-panel);
        --switch-bg-on: var(--success);
        --switch-thumb: 255,255,255,255;
    }
    switch:checked {
        background-color: var(--success);
        border-color: var(--success);
    }

    /* =========================================
       Widgets: Valuators & Progress
       ========================================= */
    slider {
        width: 100%;
        height: 32;
    }

    slider > track {
        background-color: var(--bg-panel);
    }

    slider > fill,
    slider > thumb {
        background-color: var(--primary);
    }

    progressbar {
        width: 100%;
        height: 6;
        border-radius: 3;
    }

    progressbar > track {
        background-color: var(--bg-panel);
    }

    progressbar > fill {
        background-color: var(--primary);
    }

    spinner {
        width: 32;
        height: 32;
        flex-shrink: 0;
        --spinner-color: var(--primary);
        --spinner-stroke: 4;
    }

    badge {
        padding: 4 10 4 10;
        border-radius: 12;
        font-size: 11;
        flex-grow: 0;
        flex-shrink: 0;
        background-color: var(--danger);
        --text-color: 255,255,255,255;
    }

    avatar {
        width: 56;
        height: 56;
        --avatar-size: 48;
        --avatar-bg: var(--primary);
        --avatar-text: 255,255,255,255;
        --avatar-border: 15,23,42,255;
        --avatar-border-width: 2;
        --status-size: 12;
    }

    .avatar-row {
        display: flex;
        flex-direction: row;
        align-items: center;
        gap: 12;
        height: 64;
    }

    /* =========================================
       Widgets: Data & Trees
       ========================================= */
    tree {
        width: 100%;
        height: 160;
        background-color: var(--bg-input);
        border-radius: 6;
        padding: 8;
        --text-color: var(--text-main);
        --hover-color: var(--bg-panel);
        --selected-color: var(--primary);
        overflow-y: scroll;
    }
    
    dropdown {
        width: 100%;
        height: 36;
        border-radius: 6;
        border-width: 1;
        border-color: var(--border);
        background-color: var(--bg-input);
        --bg: var(--bg-input);
        padding: 0 12 0 12;
        display: flex;
        align-items: center;
        --text-color: var(--text-main);
    }
    
    table {
        width: 100%;
        height: 240;
        background-color: var(--bg-input);
        border-radius: 8;
        border-width: 1;
        border-color: var(--border);
        
        /* Professional Table Variables */
        --table-bg: var(--bg-input);
        --table-header-bg: 30, 41, 59, 255;  /* slate-800 */
        --table-header-text: 148, 163, 184, 255; /* slate-400 */
        --table-text: var(--text-main);
        --table-border: var(--border);
        --table-divider: 71, 85, 105, 80; /* slate-600 with alpha */
        --table-hover: 255, 255, 255, 12;
        --table-selected: 59, 130, 246, 80; /* blue-500 with alpha */
        --table-stripe: 255, 255, 255, 5;
    }

    calendar {
        width: 320;
        height: 340;
        background-color: var(--bg-input);
        border-radius: 8;
        border-width: 1;
        border-color: var(--border);
        display: flex;
    }

    /* =========================================
       Widgets: Interactive
       ========================================= */
    /* =========================================
       View Specific Controls
       ========================================= */
    .tabs-view {
        width: 100%;
        height: 100%;
        display: flex;
        flex-direction: column;
        gap: 0;
    }

    .tab-content {
        width: 100%;
        height: 700;
        position: relative;
    }

    .tab-page {
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        padding: 20;
        border-radius: 12;
        gap: 16;
        display: flex;
        flex-direction: column;
        --bg: var(--bg-card);
        box-shadow: 0 4 6 rgba(0,0,0,0.2);
    }

    tabs {
        width: 100%;
        height: 48;
        display: flex;
        flex-direction: row;
        border-bottom-width: 1;
        border-color: var(--border);
        background-color: var(--bg-panel);
        --tabs-bg: var(--bg-panel);
        --tabs-text: var(--text-muted);
        --tabs-active-text: var(--primary);
        --tabs-indicator: var(--primary);
    }

    colorpicker {
        width: 100%;
        height: 160;
        margin-top: 10;
        border-radius: 6;
        border-width: 1;
        border-color: var(--border);
        background-color: var(--bg-input);
    }

    /* =========================================
       Widgets: Overlays
       ========================================= */
    toast {
        position: absolute;
        bottom: 30;
        right: 30;
        min-width: 280;
        padding: 16;
        border-radius: 8;
        background-color: var(--bg-panel);
        box-shadow: 0 10 15 rgba(0,0,0,0.3);
        border-left-width: 4;
        border-color: var(--success);
        --text-color: var(--text-main);
        z-index: 1000;
        opacity: 0;
    }

    modal {
        position: absolute;
        top: 0;
        left: 0;
        width: 100%;
        height: 100%;
        background-color: rgba(0,0,0,0.6);
        display: flex;
        align-items: center;
        justify-content: center;
        z-index: 2000;
        visibility: hidden;
    }

    /* Utility: Hidden */
    .hidden {
        display: none;
    }
)";

const std::string DEMO_STATE_CSS = R"(
    .hidden {
        display: none;
    }

    .tab-page[data-state=inactive] {
        display: none;
    }

    .live-task-row {
        border: 1px solid var(--border);
        color: var(--foreground);
        cursor: pointer;
    }

    .live-task-row:hover {
        background-color: var(--accent);
    }

    .live-task-row[data-state=complete] {
        color: var(--muted-foreground);
    }
)";

} // namespace examples
} // namespace flexUI
