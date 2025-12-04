// main.js - Main application using modules
import { Counter } from './counter.js';
import { Timer } from './timer.js';

// Create counter instance
const counter = new Counter('counter-display');

// Export functions for onclick handlers
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
        getWidget('auto-btn').setText('Auto +1 (every 500ms)');
        getWidget('status').setText('Auto-increment stopped');
    }
};

// Create timer for auto-increment
const timer = new Timer(function() {
    counter.increment();
}, 500);

globalThis.startAutoIncrement = function() {
    if (timer.isRunning()) {
        timer.stop();
        getWidget('auto-btn').setText('Auto +1 (every 500ms)');
        getWidget('status').setText('Auto-increment stopped');
    } else {
        timer.start();
        getWidget('auto-btn').setText('Stop Auto');
        getWidget('status').setText('Auto-incrementing...');
    }
};

globalThis.greet = function() {
    const nameInput = getWidget('name-input');
    const name = nameInput.getText();

    if (name === '') {
        getWidget('status').setText('Please enter your name!');
    } else {
        getWidget('status').setText('Hello, ' + name + '!');
        console.log('Greeting: Hello, ' + name);
    }
};

// Initial log
console.log('JavaScript Modular Counter Demo loaded!');
console.log('Modules: Counter, Timer');