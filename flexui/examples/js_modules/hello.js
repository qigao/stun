// hello.js - Minimal example using FlexUI standard library
import { WidgetHelper, FlexUI } from 'flexui';

let clickCount = 0;

globalThis.handleClick = function() {
    clickCount++;
    WidgetHelper.setText('message', 'Hello! Clicked ' + clickCount + ' times');
    FlexUI.log('Button clicked:', clickCount);
};

globalThis.handleReset = function() {
    clickCount = 0;
    WidgetHelper.setText('message', 'Click the button above!');
    FlexUI.log('Reset');
};

FlexUI.log('Hello demo loaded!');
