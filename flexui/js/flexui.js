// FlexUI Standard Library
// This module provides a clean API wrapper around global functions

export const FlexUI = {
    // Widget access
    getWidget: (id) => getWidget(id),
    
    // Timing
    setTimeout: (fn, ms) => setTimeout(fn, ms),
    clearTimeout: (id) => clearTimeout(id),
    
    // Console (re-export for convenience)
    log: (...args) => console.log(...args),
    info: (...args) => console.info(...args),
    warn: (...args) => console.warn(...args),
    error: (...args) => console.error(...args)
};

// Widget helper utilities
export class WidgetHelper {
    static setText(id, text) {
        const widget = getWidget(id);
        if (widget) widget.setText(text);
    }
    
    static getText(id) {
        const widget = getWidget(id);
        return widget ? widget.getText() : '';
    }
    
    static setValue(id, value) {
        const widget = getWidget(id);
        if (widget) widget.setValue(value);
    }
    
    static getValue(id) {
        const widget = getWidget(id);
        return widget ? widget.getValue() : null;
    }
    
    static setChecked(id, checked) {
        const widget = getWidget(id);
        if (widget) widget.setChecked(checked);
    }
    
    static isChecked(id) {
        const widget = getWidget(id);
        return widget ? widget.isChecked() : false;
    }
}

// Animation utilities
export class Animation {
    static lerp(start, end, t) {
        return start + (end - start) * t;
    }
    
    static easeInOut(t) {
        return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t;
    }
    
    static animate(duration, callback) {
        const startTime = Date.now();
        
        function tick() {
            const elapsed = Date.now() - startTime;
            const progress = Math.min(elapsed / duration, 1);
            
            callback(progress);
            
            if (progress < 1) {
                setTimeout(tick, 16); // ~60fps
            }
        }
        
        tick();
    }
}

// Event utilities
export class EventBus {
    constructor() {
        this.listeners = {};
    }
    
    on(event, callback) {
        if (!this.listeners[event]) {
            this.listeners[event] = [];
        }
        this.listeners[event].push(callback);
    }
    
    off(event, callback) {
        if (!this.listeners[event]) return;
        this.listeners[event] = this.listeners[event].filter(cb => cb !== callback);
    }
    
    emit(event, data) {
        if (!this.listeners[event]) return;
        this.listeners[event].forEach(cb => cb(data));
    }
}

// Export default for convenience
export default FlexUI;
