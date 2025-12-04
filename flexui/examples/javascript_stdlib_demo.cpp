#include <flexui/screen.h>

int main() {
    flexui::Screen screen(500, 400, "JavaScript Standard Library Demo");

    screen.loadCSS(R"(
        #main-panel {
            display: flex;
            flex-direction: column;
            gap: 15px;
            padding: 25px;
            width: 400px;
            height: 340px;
            background: #ffffff;
            border: 2px solid #2196F3;
            border-radius: 12px;
            position: absolute;
            top: 30px;
            left: 50px;
        }
        #title {
            width: 350px;
            height: 36px;
        }
        #counter-display {
            width: 350px;
            height: 50px;
        }
        .button-row {
            display: flex;
            flex-direction: row;
            gap: 15px;
            width: 350px;
            height: 50px;
        }
        button {
            width: 110px;
            height: 45px;
        }
        #status {
            width: 350px;
            height: 30px;
        }
        input {
            width: 350px;
            height: 40px;
        }
    )");

    screen.loadXML(R"(
        <div id="main-panel">
            <label id="title">FlexUI Standard Library Demo</label>
            <label id="counter-display">Count: 0</label>

            <div class="button-row">
                <button id="inc-btn" onclick="increment">+1</button>
                <button id="dec-btn" onclick="decrement">-1</button>
                <button id="reset-btn" onclick="reset">Reset</button>
            </div>

            <button id="auto-btn" onclick="startAutoIncrement">Auto +1 (every 500ms)</button>

            <input id="name-input" placeholder="Enter your name"/>
            <button id="greet-btn" onclick="greet">Greet</button>

            <label id="status">Ready</label>
        </div>
    )");

    // Load module that uses FlexUI standard library
    screen.loadJSModule("js_modules/main_with_stdlib.js");

    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
