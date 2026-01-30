#pragma once

namespace LayoutStyles {
    static const char* CSS = R"(
.root {
    background-color: #0d0d0d;
    color: #cccccc;
    width: 100%;
    height: 100%;
    display: flex;
    flex-direction: column;
}

/* --- TOP: Static Header --- */
.header {
    height: 24px;
    background-color: #1a1a1a;
    border-bottom: 1px solid #333333;
    display: flex;
    flex-direction: row;
    align-items: center;
    justify-content: space-between;
    padding: 0 16px; 
}
.header-title {
    color: #e0e0e0;
    font-weight: bold;
}
.header-right {
    color: #666666;
    font-size: 12px;
}

/* --- MIDDLE: Scrollable Conversation Area --- */
.content {
    flex-grow: 1;
    display: flex;
    flex-direction: column;
    padding: 8px 16px;
    background-color: #0d0d0d;
}

/* Chat List */
.chat-list {
    flex-grow: 1;
    overflow-y: scroll;
    width: 100%;
    border: 1px solid #333333;
}

/* Message Styles */
.message-row {
    height: auto;
    padding: 10px 12px;
    margin-bottom: 12px;
    display: flex;
    flex-direction: column;
    border-radius: 6px;
}

.user-msg {
    background-color: #161616;
    border-left: 3px solid #007acc;
}

.ai-msg {
    background-color: #1e1e1e;
    border-left: 3px solid #f97316;
}

.sys-msg {
    background-color: #1a1a1a;
    border-left: 3px solid #666666;
}

.msg-header {
    height: 20px;
    margin-bottom: 4px;
    display: flex;
    flex-direction: row;
}

.role-badge {
    font-weight: bold;
    margin-right: 8px;
    font-size: 12px;
}

.msg-content {
    color: #d4d4d4;
    margin-left: 0; 
    flex-grow: 1;
    width: 100%;
    border: 0px;
}

/* --- BOTTOM: Status + Input --- */
.footer {
    display: flex;
    flex-direction: column;
    height: auto;
    background-color: #1a1a1a;
    border-top: 1px solid #333333;
}

.status-bar {
    height: 24px;
    display: flex;
    flex-direction: row;
    align-items: center;
    justify-content: flex-start;
    padding: 0 12px;
    background-color: #212121;
    color: #9e9e9e;
    font-size: 11px;
    gap: 16px;
    border-top: 1px solid #333333;
}

.status-item {
    display: flex;
    flex-direction: row;
    align-items: center;
    gap: 4px;
}

.status-label {
    color: #666666;
    text-transform: uppercase;
}

.status-value {
    color: #e0e0e0;
    font-weight: bold;
}

.status-value.active {
    color: #4caf50;
}

.input-area {
    padding: 8px 16px;
    display: flex;
    flex-direction: column;
    background-color: #111111;
    width: 100%;
}

/* Input Styles */
.input-box {
    width: 100%;
    height: 32px;
    border: 1px solid #444444;
    background-color: #000000;
    color: #ffffff;
    padding: 0 8px; 
    
    --input-bg: #000000;
    --input-text: #ffffff;
    --input-placeholder: #555555;
    --input-cursor: #007acc;
    --input-selection-bg: #264f78;
    --input-border: #444444;
}

.input-hint {
    color: #555555;
    font-size: 10px;
    margin-top: 4px;
    align-self: flex-end;
}

/* Markdown Styles */
.h1 { font-size: 24px; font-weight: bold; color: #569cd6; margin-top: 12px; margin-bottom: 12px; border-bottom: 2px solid #3e3e42; }
.h2 { font-size: 20px; font-weight: bold; color: #4ec9b0; margin-top: 10px; margin-bottom: 8px; }
.h3 { font-size: 16px; font-weight: bold; color: #9cdcfe; margin-top: 8px; margin-bottom: 6px; }

.md-p { margin-bottom: 10px; color: #e0e0e0; line-height: 1.4; }
.md-strong { font-weight: bold; color: #fac863; }
.md-em { font-style: italic; color: #c594c5; }

.md-code-block {
    display: flex;
    flex-direction: column;
    background-color: #1e1e1e;
    border: 1px solid #444444;
    padding: 10px;
    margin-top: 4px;
    margin-bottom: 12px;
    font-family: "Consolas", "Courier New", monospace;
    color: #dcdcaa;
    white-space: pre-wrap;
    border-radius: 4px;
    max-height: 200px;
    overflow-y: auto;
}
.md-code {
    background-color: #2d2d2d;
    padding: 2px 6px;
    border-radius: 3px;
    color: #ce9178;
    font-family: monospace;
}
.md-quote {
    border-left: 4px solid #6a9955;
    padding-left: 12px;
    color: #b5cea8;
    font-style: italic;
    margin-bottom: 12px;
    background-color: #1a2a1a;
}
.md-ul, .md-ol { margin-bottom: 10px; padding-left: 20px; }
.md-li { margin-bottom: 6px; }

/* --- Floating Model Selector --- */
.selector-overlay {
    position: absolute;
    left: 0;
    top: 0;
    width: 100%;
    height: 100%;
    /* background-color: rgba(0, 0, 0, 0.5); - Turn off for TUI to avoid covering UI */
    display: flex;
    justify-content: center;
    align-items: center;
    z-index: 2000;
}

.selector-panel {
    width: 360px;
    background-color: #1a1a1b;
    border: 1px solid #007acc;
    border-top: 4px solid #007acc;
    padding: 16px;
    display: flex;
    flex-direction: column;
}

.selector-header {
    margin-bottom: 12px;
    border-bottom: 1px solid #333333;
    padding-bottom: 8px;
}

.selector-title {
    color: #007acc;
    font-weight: bold;
    text-transform: uppercase;
}

.model-option {
    padding: 10px 12px;
    margin-bottom: 4px;
    background-color: #252526;
    border-radius: 4px;
    display: flex;
    flex-direction: column;
    cursor: pointer;
}

.model-option:hover {
    background-color: #37373d;
    border-left: 4px solid #4caf50;
}

.model-option.active {
    background-color: #264f78;
    border-left: 4px solid #007acc;
}

.opt-name {
    color: #ffffff;
    font-weight: bold;
}

.opt-desc {
    color: #858585;
    font-size: 10px;
}

.selector-hint {
    margin-top: 8px;
    color: #666666;
    font-size: 10px;
    text-align: center;
}

/* --- Command Autocomplete --- */
.cmd-popup {
    position: absolute;
    right: 16px;
    top: 40px;
    width: 280px;
    height: 160px;
    background-color: #252526;
    border: 1px solid #007acc;
    display: flex;
    flex-direction: column;
    z-index: 1500;
    /* box-shadow: 0 4px 20px rgba(0,0,0,0.8); */
}

.cmd-header {
    height: 24px;
    background-color: #007acc;
    display: flex;
    flex-direction: row;
    align-items: center;
    justify-content: space-between;
    padding: 0 8px;
    cursor: default;
}

.cmd-title {
    color: #ffffff;
    font-weight: bold;
    font-size: 11px;
}

.cmd-close {
    color: #ffffff;
    font-weight: bold;
    cursor: pointer;
    padding: 0 4px;
}

.cmd-close:hover {
    background-color: #ff4d4d;
}

.cmd-container {
    display: flex;
    flex-direction: column;
    padding: 4px 0;
}

.cmd-item {
    padding: 8px 12px;
    display: flex;
    flex-direction: row;
    align-items: center;
    cursor: pointer;
}

.cmd-item:hover {
    background-color: #094771;
}

.cmd-item.active {
    background-color: #094771;
    border-left: 2px solid #007acc;
}

.cmd-name {
    color: #ce9178;
    font-weight: bold;
}

.cmd-desc {
    color: #cccccc;
    font-size: 10px;
    margin-left: 8px;
}

.visible { display: flex; }
.hidden { display: none; }
)";
}
