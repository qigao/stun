// animation_demo.js - Demonstrates FlexUI standard library features
import { FlexUI, WidgetHelper, Animation, EventBus } from 'flexui';

// Event bus for component communication
const eventBus = new EventBus();

// Progress bar controller
class ProgressController {
    constructor(progressId, labelId) {
        this.progressId = progressId;
        this.labelId = labelId;
        this.value = 0;
    }
    
    setValue(value) {
        this.value = Math.max(0, Math.min(100, value));
        WidgetHelper.setValue(this.progressId, this.value);
        WidgetHelper.setText(this.labelId, 'Progress: ' + Math.round(this.value) + '%');
    }
    
    animateTo(target, duration) {
        const start = this.value;
        const end = target;
        
        Animation.animate(duration, (progress) => {
            const eased = Animation.easeInOut(progress);
            const current = Animation.lerp(start, end, eased);
            this.setValue(current);
        });
    }
}

// Counter with event emission
class EventCounter {
    constructor(displayId) {
        this.count = 0;
        this.displayId = displayId;
        this.updateDisplay();
    }
    
    increment() {
        this.count++;
        this.updateDisplay();
        eventBus.emit('counterChanged', this.count);
        FlexUI.log('Counter:', this.count);
    }
    
    decrement() {
        this.count--;
        this.updateDisplay();
        eventBus.emit('counterChanged', this.count);
        FlexUI.log('Counter:', this.count);
    }
    
    reset() {
        this.count = 0;
        this.updateDisplay();
        eventBus.emit('counterChanged', this.count);
        FlexUI.log('Counter reset');
    }
    
    updateDisplay() {
        WidgetHelper.setText(this.displayId, 'Count: ' + this.count);
    }
}

// Initialize components
const counter = new EventCounter('counter-display');
const progress = new ProgressController('progress-bar', 'progress-label');

// Link counter to progress bar via event bus
eventBus.on('counterChanged', (count) => {
    const percentage = Math.abs(count) % 101;
    progress.animateTo(percentage, 300);
});

// Export handlers for XML onclick
globalThis.increment = () => counter.increment();
globalThis.decrement = () => counter.decrement();
globalThis.reset = () => counter.reset();

globalThis.animateProgress = () => {
    const target = Math.random() * 100;
    progress.animateTo(target, 1000);
    WidgetHelper.setText('status', 'Animating to ' + Math.round(target) + '%...');
};

globalThis.testEventBus = () => {
    let clicks = 0;
    
    const handler = (count) => {
        clicks++;
        WidgetHelper.setText('status', 'Event received ' + clicks + ' times (count=' + count + ')');
    };
    
    eventBus.on('counterChanged', handler);
    
    FlexUI.setTimeout(() => {
        eventBus.off('counterChanged', handler);
        WidgetHelper.setText('status', 'Event listener removed');
    }, 5000);
    
    WidgetHelper.setText('status', 'Event listener added (will auto-remove in 5s)');
};

globalThis.smoothIncrement = () => {
    const start = counter.count;
    const end = start + 10;
    
    Animation.animate(1000, (progress) => {
        const current = Math.round(Animation.lerp(start, end, progress));
        if (current > counter.count) {
            counter.count = current;
            counter.updateDisplay();
        }
    });
};

FlexUI.log('Animation Demo loaded!');
FlexUI.log('Features: EventBus, Animation, WidgetHelper');
