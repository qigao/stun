#include <flexui/screen.h>

int main() {
    flexui::Screen screen(550, 500, "FlexUI Animation & EventBus Demo");

    screen.loadCSS(R"(
        #main-panel {
            display: flex;
            flex-direction: column;
            gap: 15px;
            padding: 25px;
            width: 450px;
            height: 440px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            border-radius: 16px;
            position: absolute;
            top: 30px;
            left: 50px;
        }
        
        #title {
            width: 400px;
            height: 40px;
            color: #ffffff;
            font-size: 20px;
            font-weight: bold;
        }
        
        .section {
            display: flex;
            flex-direction: column;
            gap: 10px;
            padding: 15px;
            background: rgba(255, 255, 255, 0.95);
            border-radius: 12px;
        }
        
        .section-title {
            width: 370px;
            height: 28px;
            font-weight: bold;
            color: #667eea;
        }
        
        #counter-display {
            width: 370px;
            height: 40px;
            font-size: 18px;
        }
        
        .button-row {
            display: flex;
            flex-direction: row;
            gap: 10px;
            width: 370px;
            height: 40px;
        }
        
        button {
            flex: 1;
            height: 40px;
            background: #667eea;
            color: #ffffff;
            border-radius: 8px;
            font-weight: bold;
        }
        
        button:hover {
            background: #5568d3;
        }
        
        #progress-bar {
            width: 370px;
            height: 30px;
        }
        
        #progress-label {
            width: 370px;
            height: 28px;
            font-size: 16px;
        }
        
        #status {
            width: 370px;
            height: 32px;
            color: #666666;
            font-style: italic;
        }
    )");

    screen.loadXML(R"(
        <div id="main-panel">
            <label id="title">🎨 Animation & EventBus Demo</label>
            
            <div class="section">
                <label class="section-title">Counter with Events</label>
                <label id="counter-display">Count: 0</label>
                
                <div class="button-row">
                    <button onclick="increment">+1</button>
                    <button onclick="decrement">-1</button>
                    <button onclick="reset">Reset</button>
                </div>
                
                <button onclick="smoothIncrement">Smooth +10</button>
            </div>
            
            <div class="section">
                <label class="section-title">Animated Progress Bar</label>
                <progressbar id="progress-bar" value="0" max="100"/>
                <label id="progress-label">Progress: 0%</label>
                
                <button onclick="animateProgress">Random Animate</button>
            </div>
            
            <div class="section">
                <label class="section-title">EventBus Test</label>
                <button onclick="testEventBus">Test Event Listener</button>
            </div>
            
            <label id="status">Ready - Try the buttons!</label>
        </div>
    )");

    // Load module with FlexUI standard library
    screen.loadJSModule("js_modules/animation_demo.js");

    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
