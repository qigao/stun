// main_with_stdlib.js - Using FlexUI standard library
import { FlexUI, WidgetHelper, Animation } from 'flexui';
import { Counter } from './counter.js';
import { Timer } from './timer.js';

// Use standard library instead of global functions
const counter = new Counter('counter-display');

globalThis.increment = function() {
    counter.increment();
};

globalThis.decrement = function() {
    counter.decrement();
};

globalThis.reset = function() {
    counter.reset();
    if (timer.isRunning()) {
        timer.stop();
        WidgetHelper.setText('auto-btn', 'Auto +1 (every 500ms)');
        WidgetHelper.setText('status', 'Auto-increment stopped');
    }
};

const timer = new Timer(function() {
    counter.increment();
}, 500);

globalThis.startAutoIncrement = function() {
    if (timer.isRunning()) {
        timer.stop();
        WidgetHelper.setText('auto-btn', 'Auto +1 (every 500ms)');
        WidgetHelper.setText('status', 'Auto-increment stopped');
    } else {
        timer.start();
        WidgetHelper.setText('auto-btn', 'Stop Auto');
        WidgetHelper.setText('status', 'Auto-incrementing...');
    }
};

globalThis.greet = function() {
    const name = WidgetHelper.getText('name-input');

    if (name === '') {
        WidgetHelper.setText('status', 'Please enter your name!');
    } else {
        WidgetHelper.setText('status', 'Hello, ' + name + '!');
        FlexUI.log('Greeting: Hello, ' + name);
    }
};

// Demo: Animate counter display on increment
const originalIncrement = counter.increment.bind(counter);
counter.increment = function() {
    originalIncrement();
    
    // Animate the display (scale effect simulation via text)
    const display = FlexUI.getWidget('counter-display');
    if (display) {
        Animation.animate(200, function(progress) {
            // Simple pulse effect by changing text temporarily
            const scale = 1 + Animation.easeInOut(progress) * 0.1;
            // Note: Real scale would need CSS animation support
        });
    }
};

FlexUI.log('JavaScript Modular Counter Demo with Standard Library loaded!');
FlexUI.log('Using: FlexUI, WidgetHelper, Animation, Counter, Timer');
