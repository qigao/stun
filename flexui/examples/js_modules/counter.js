// counter.js - Reusable counter module
export class Counter {
    constructor(displayId) {
        this.count = 0;
        this.displayId = displayId;
        this.updateDisplay();
    }

    increment() {
        this.count++;
        this.updateDisplay();
        console.log("Counter incremented to " + this.count);
    }

    decrement() {
        this.count--;
        this.updateDisplay();
        console.log("Counter decremented to " + this.count);
    }

    reset() {
        this.count = 0;
        this.updateDisplay();
        console.log("Counter reset");
    }

    updateDisplay() {
        const display = getWidget(this.displayId);
        if (display) {
            display.setText("Count: " + this.count);
        }
    }

    getValue() {
        return this.count;
    }
}
