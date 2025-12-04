#include <flexui/screen.h>

int main() {
    flexui::Screen screen(400, 300, "Hello FlexUI + JS");

    screen.loadCSS(R"(
        #container {
            display: flex;
            flex-direction: column;
            gap: 20px;
            padding: 40px;
            width: 320px;
            height: 220px;
            background: #f5f5f5;
            border-radius: 12px;
            position: absolute;
            top: 40px;
            left: 40px;
        }
        
        button {
            width: 320px;
            height: 50px;
            background: #4CAF50;
            color: white;
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
        }
        
        button:hover {
            background: #45a049;
        }
        
        #reset-btn {
            background: #f44336;
        }
        
        #reset-btn:hover {
            background: #da190b;
        }
        
        #message {
            width: 320px;
            height: 40px;
            font-size: 18px;
            text-align: center;
        }
    )");

    screen.loadXML(R"(
        <div id="container">
            <button onclick="handleClick">Click Me!</button>
            <label id="message">Click the button above!</label>
            <button id="reset-btn" onclick="handleReset">Reset</button>
        </div>
    )");

    // Load ES6 module with FlexUI standard library
    screen.loadJSModule("js_modules/hello.js");

    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
