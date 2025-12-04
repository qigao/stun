// timer.js - Auto-increment timer module
export class Timer {
    constructor(callback, intervalMs) {
        this.callback = callback;
        this.intervalMs = intervalMs;
        this.timerId = null;
        this.running = false;
    }

    start() {
        if (this.running) return;
        
        this.running = true;
        this.tick();
        console.log("Timer started");
    }

    stop() {
        if (!this.running) return;
        
        if (this.timerId !== null) {
            clearTimeout(this.timerId);
            this.timerId = null;
        }
        this.running = false;
        console.log("Timer stopped");
    }

    tick() {
        if (!this.running) return;
        
        this.callback();
        
        const self = this;
        this.timerId = setTimeout(function() {
            self.tick();
        }, this.intervalMs);
    }

    isRunning() {
        return this.running;
    }
}
